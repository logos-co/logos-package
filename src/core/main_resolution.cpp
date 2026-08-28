#include "main_resolution.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lgx {

namespace fs = std::filesystem;

namespace {

// The directory IS the package, so a declared path leaving it does not name
// anything of the package's, whatever it names. Empty when it escapes.
// Lexical, never canonical: resolving symlinks here would refuse a legitimate
// symlinked payload, and the guard is about the DECLARED path.
std::string containedPath(const fs::path& dir, const std::string& declaredPath)
{
    std::error_code ec;
    const fs::path root = fs::absolute(dir, ec).lexically_normal();
    if (ec) {
        return {};
    }
    const fs::path candidate = (root / declaredPath).lexically_normal();
    const fs::path relative = candidate.lexically_relative(root);
    if (relative.empty() || *relative.begin() == "..") {
        return {};
    }
    return candidate.string();
}

MainFile locateMain(const fs::path& dir, const std::string& declaredPath,
                    const std::string& variant)
{
    MainFile result;
    result.declaredPath = declaredPath;
    result.variant = variant;

    const std::string candidate = containedPath(dir, declaredPath);
    if (candidate.empty()) {
        result.state = MainResolution::MalformedEntry;
        result.error = "main '" + declaredPath + "' resolves outside the module directory";
        return result;
    }

    // is_regular_file, not exists(): a directory named as `main` is not a
    // plugin, and exists() would hand the caller a path it cannot load.
    // Symlinks are followed, so a symlinked plugin still resolves.
    std::error_code ec;
    if (!fs::exists(candidate, ec) || ec) {
        result.state = MainResolution::FileMissing;
        result.error = "main '" + declaredPath + "' is not present in the module directory";
        return result;
    }
    if (!fs::is_regular_file(candidate, ec) || ec) {
        result.state = MainResolution::MalformedEntry;
        result.error = "main '" + declaredPath + "' is not a file";
        return result;
    }

    result.state = MainResolution::Resolved;
    result.path = candidate;
    return result;
}

} // namespace

MainFile resolveMain(const fs::path& dir,
                     const std::string& manifestBytes,
                     const std::vector<std::string>& variants)
{
    MainFile result;

    if (dir.empty()) {
        result.error = "no module directory given";
        return result;
    }

    json manifest;
    try {
        manifest = json::parse(manifestBytes);
    } catch (const std::exception& e) {
        result.error = std::string("manifest is not valid JSON: ") + e.what();
        return result;
    }
    if (!manifest.is_object()) {
        result.error = "manifest is not a JSON object";
        return result;
    }

    if (!manifest.contains("main")) {
        result.state = MainResolution::NotDeclared;
        return result;
    }
    const json& main = manifest["main"];

    if (main.is_object()) {
        std::string unusable;
        for (const std::string& variant : variants) {
            const auto declared = main.find(variant);
            if (declared == main.end()) {
                continue;
            }
            if (!declared->is_string() || declared->get<std::string>().empty()) {
                if (unusable.empty()) {
                    unusable = variant;
                }
                continue;
            }
            return locateMain(dir, declared->get<std::string>(), variant);
        }
        if (!unusable.empty()) {
            result.state = MainResolution::MalformedEntry;
            result.variant = unusable;
            result.error = "main['" + unusable + "'] is not a non-empty string";
            return result;
        }
        result.state = MainResolution::NoVariantMatch;
        result.error = "no candidate variant [";
        for (size_t i = 0; i < variants.size(); ++i) {
            if (i) result.error += ", ";
            result.error += variants[i];
        }
        result.error += "] is a key of main";
        return result;
    }

    if (main.is_string()) {
        const std::string declared = main.get<std::string>();
        if (declared.empty()) {
            result.state = MainResolution::MalformedEntry;
            result.error = "main is an empty string";
            return result;
        }
        return locateMain(dir, declared, std::string());
    }

    result.state = MainResolution::MalformedEntry;
    result.error = "main is neither a variant map nor a string";
    return result;
}

} // namespace lgx
