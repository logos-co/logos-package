#include "path_normalizer.h"

// ICU's C API only: Android's platform ICU (libicu.so, API 31+) exports
// nothing else.
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

#include <algorithm>
#include <sstream>

namespace lgx {

namespace {

static_assert(sizeof(UChar) == sizeof(char16_t), "UChar is UTF-16");

UChar* units(std::u16string& s) { return reinterpret_cast<UChar*>(s.data()); }
const UChar* units(const std::u16string& s) { return reinterpret_cast<const UChar*>(s.data()); }
int32_t length(const std::u16string& s) { return static_cast<int32_t>(s.size()); }

// Runs a preflight-then-fill ICU call; `fill(dest, capacity, status)` returns
// the full length. nullopt on any error but the preflight's overflow.
template <typename Fill>
std::optional<std::u16string> produce(Fill fill)
{
    UErrorCode status = U_ZERO_ERROR;
    const int32_t needed = fill(nullptr, 0, &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) return std::nullopt;
    std::u16string out(static_cast<size_t>(needed), u'\0');
    status = U_ZERO_ERROR;
    fill(units(out), needed, &status);
    if (U_FAILURE(status)) return std::nullopt;
    return out;
}

// Ill-formed input becomes U+FFFD, as icu::UnicodeString::fromUTF8 did.
std::u16string fromUtf8(const std::string& s)
{
    auto out = produce([&](UChar* dest, int32_t capacity, UErrorCode* status) {
        int32_t needed = 0;
        u_strFromUTF8WithSub(dest, capacity, &needed, s.data(), static_cast<int32_t>(s.size()),
                             0xFFFD, nullptr, status);
        return needed;
    });
    return out ? std::move(*out) : std::u16string();
}

// Unpaired surrogates become U+FFFD, as UnicodeString::toUTF8String did.
std::string toUtf8(const std::u16string& s)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t needed = 0;
    u_strToUTF8WithSub(nullptr, 0, &needed, units(s), length(s), 0xFFFD, nullptr, &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    status = U_ZERO_ERROR;
    u_strToUTF8WithSub(out.data(), needed, nullptr, units(s), length(s), 0xFFFD, nullptr, &status);
    return U_SUCCESS(status) ? out : std::string();
}

} // namespace

std::optional<std::string> PathNormalizer::toNFC(const std::string& path) {
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status)) {
        return std::nullopt;
    }

    const std::u16string source = fromUtf8(path);
    auto normalized = produce([&](UChar* dest, int32_t capacity, UErrorCode* st) {
        return unorm2_normalize(normalizer, units(source), length(source), dest, capacity, st);
    });
    if (!normalized) {
        return std::nullopt;
    }
    return toUtf8(*normalized);
}

bool PathNormalizer::isNFC(const std::string& str) {
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status)) {
        return false;
    }

    const std::u16string source = fromUtf8(str);
    const bool normalized = unorm2_isNormalized(normalizer, units(source), length(source), &status);
    return normalized && U_SUCCESS(status);
}

PathNormalizer::ValidationResult PathNormalizer::validateArchivePath(const std::string& archivePath) {
    // Check for empty path
    if (archivePath.empty()) {
        return ValidationResult::fail("Path is empty");
    }
    
    // Check for backslashes (forbidden in archive paths)
    if (archivePath.find('\\') != std::string::npos) {
        return ValidationResult::fail("Path contains backslashes");
    }
    
    // Check for absolute path
    if (isAbsolute(archivePath)) {
        return ValidationResult::fail("Path is absolute");
    }
    
    // Split and check for ".." segments
    auto components = splitPath(archivePath);
    for (const auto& component : components) {
        if (component == "..") {
            return ValidationResult::fail("Path contains '..' segment");
        }
    }
    
    // Verify NFC normalization
    if (!isNFC(archivePath)) {
        return ValidationResult::fail("Path is not NFC-normalized");
    }
    
    return ValidationResult::ok();
}

std::string PathNormalizer::normalizeSeparators(const std::string& path) {
    std::string result;
    result.reserve(path.size());
    
    bool lastWasSep = false;
    for (char c : path) {
        if (c == '\\' || c == '/') {
            if (!lastWasSep) {
                result += '/';
                lastWasSep = true;
            }
        } else {
            result += c;
            lastWasSep = false;
        }
    }
    
    // Remove trailing slash unless it's the root
    while (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    
    return result;
}

std::string PathNormalizer::toLowercase(const std::string& str) {
    const std::u16string source = fromUtf8(str);
    // A null locale is ICU's default locale, as UnicodeString::toLower() used.
    auto lowered = produce([&](UChar* dest, int32_t capacity, UErrorCode* status) {
        return u_strToLower(dest, capacity, units(source), length(source), nullptr, status);
    });
    return lowered ? toUtf8(*lowered) : std::string();
}

std::string PathNormalizer::joinPath(const std::vector<std::string>& components) {
    if (components.empty()) {
        return "";
    }
    
    std::string result;
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) {
            result += '/';
        }
        result += components[i];
    }
    return result;
}

std::string PathNormalizer::joinPath(const std::string& base, const std::string& relative) {
    if (base.empty()) {
        return relative;
    }
    if (relative.empty()) {
        return base;
    }
    
    std::string result = base;
    if (result.back() != '/') {
        result += '/';
    }
    
    // Skip leading slash in relative if present
    if (relative[0] == '/') {
        result += relative.substr(1);
    } else {
        result += relative;
    }
    
    return result;
}

std::string PathNormalizer::basename(const std::string& path) {
    auto normalized = normalizeSeparators(path);
    auto pos = normalized.rfind('/');
    if (pos == std::string::npos) {
        return normalized;
    }
    return normalized.substr(pos + 1);
}

std::string PathNormalizer::dirname(const std::string& path) {
    auto normalized = normalizeSeparators(path);
    auto pos = normalized.rfind('/');
    if (pos == std::string::npos) {
        return "";
    }
    if (pos == 0) {
        return "/";
    }
    return normalized.substr(0, pos);
}

bool PathNormalizer::isAbsolute(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    
    // Unix absolute path
    if (path[0] == '/') {
        return true;
    }
    
    // Windows absolute path (e.g., C:\)
    if (path.size() >= 3 && std::isalpha(path[0]) && path[1] == ':' && 
        (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    
    return false;
}

std::vector<std::string> PathNormalizer::splitPath(const std::string& path) {
    std::vector<std::string> components;
    auto normalized = normalizeSeparators(path);
    
    std::stringstream ss(normalized);
    std::string component;
    
    while (std::getline(ss, component, '/')) {
        if (!component.empty() && component != ".") {
            components.push_back(component);
        }
    }
    
    return components;
}

std::string PathNormalizer::getRootComponent(const std::string& archivePath) {
    auto components = splitPath(archivePath);
    if (components.empty()) {
        return "";
    }
    return components[0];
}

} // namespace lgx
