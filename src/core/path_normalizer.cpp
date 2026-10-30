#include "path_normalizer.h"

#if defined(LGX_UNICODE_CF)
// CoreFoundation instead of ICU (LGX_UNICODE_CF): iOS has no public unorm2.
#include <CoreFoundation/CoreFoundation.h>
#include <memory>
#include <type_traits>
#else
// ICU's C API only: Android's platform ICU (libicu.so, API 31+) exports
// nothing else.
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>
#endif

#include <algorithm>
#include <sstream>

namespace lgx {

namespace {

#if defined(LGX_UNICODE_CF)

// Ill-formed input becomes one U+FFFD per maximal subpart, as ICU's
// u_strFromUTF8WithSub makes it.
std::u16string fromUtf8(const std::string& s)
{
    std::u16string out;
    out.reserve(s.size());
    const auto* bytes = reinterpret_cast<const unsigned char*>(s.data());
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char lead = bytes[i];
        size_t length = 0;
        unsigned char low = 0x80, high = 0xBF; // the second byte's range
        char32_t cp = 0;
        if (lead < 0x80) {
            length = 1;
            cp = lead;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            cp = lead & 0x1F;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            cp = lead & 0x0F;
            if (lead == 0xE0) low = 0xA0;
            if (lead == 0xED) high = 0x9F;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            cp = lead & 0x07;
            if (lead == 0xF0) low = 0x90;
            if (lead == 0xF4) high = 0x8F;
        }
        size_t n = 1;
        for (; n < length && i + n < s.size(); ++n) {
            const unsigned char c = bytes[i + n];
            if (c < (n == 1 ? low : 0x80) || c > (n == 1 ? high : 0xBF)) break;
            cp = (cp << 6) | (c & 0x3F);
        }
        i += n;
        if (n < length || length == 0) {
            out.push_back(0xFFFD);
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

struct CFReleaser {
    void operator()(CFTypeRef ref) const { CFRelease(ref); }
};
using MutableString = std::unique_ptr<std::remove_pointer_t<CFMutableStringRef>, CFReleaser>;

MutableString makeString(const std::u16string& s)
{
    MutableString str(CFStringCreateMutable(kCFAllocatorDefault, 0));
    if (str) {
        CFStringAppendCharacters(str.get(), reinterpret_cast<const UniChar*>(s.data()),
                                 static_cast<CFIndex>(s.size()));
    }
    return str;
}

// Lossless: what fromUtf8 builds, normalized or lowercased, has no lone surrogate.
std::string toUtf8(CFStringRef s)
{
    const CFRange all = CFRangeMake(0, CFStringGetLength(s));
    CFIndex size = 0;
    CFStringGetBytes(s, all, kCFStringEncodingUTF8, 0, false, nullptr, 0, &size);
    std::string out(static_cast<size_t>(size), '\0');
    CFStringGetBytes(s, all, kCFStringEncodingUTF8, 0, false,
                     reinterpret_cast<UInt8*>(out.data()), size, nullptr);
    return out;
}

} // namespace

std::optional<std::string> PathNormalizer::toNFC(const std::string& path) {
    MutableString normalized = makeString(fromUtf8(path));
    if (!normalized) {
        return std::nullopt;
    }
    CFStringNormalize(normalized.get(), kCFStringNormalizationFormC);
    return toUtf8(normalized.get());
}

bool PathNormalizer::isNFC(const std::string& str) {
    const std::u16string source = fromUtf8(str);
    MutableString original = makeString(source);
    MutableString normalized = makeString(source);
    if (!original || !normalized) {
        return false;
    }
    CFStringNormalize(normalized.get(), kCFStringNormalizationFormC);
    return CFEqual(original.get(), normalized.get());
}

#else

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

#endif

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

#if defined(LGX_UNICODE_CF)
std::string PathNormalizer::toLowercase(const std::string& str) {
    // A null locale is the locale-independent mapping.
    MutableString lowered = makeString(fromUtf8(str));
    if (!lowered) {
        return std::string();
    }
    CFStringLowercase(lowered.get(), nullptr);
    return toUtf8(lowered.get());
}
#else
std::string PathNormalizer::toLowercase(const std::string& str) {
    const std::u16string source = fromUtf8(str);
    // A null locale is ICU's default locale, as UnicodeString::toLower() used.
    auto lowered = produce([&](UChar* dest, int32_t capacity, UErrorCode* status) {
        return u_strToLower(dest, capacity, units(source), length(source), nullptr, status);
    });
    return lowered ? toUtf8(*lowered) : std::string();
}
#endif

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
