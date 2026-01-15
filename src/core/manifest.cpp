#include "manifest.h"
#include "path_normalizer.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>

using json = nlohmann::json;

namespace lgx {

thread_local std::string Manifest::lastError_;

Manifest::Manifest() 
    : manifestVersion(CURRENT_VERSION)
    , name("")
    , version("")
    , description("")
    , author("")
    , type("")
    , category("")
{
}

std::optional<Manifest> Manifest::fromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        Manifest m;
        
        // Required fields
        if (!j.contains("manifestVersion") || !j["manifestVersion"].is_string()) {
            lastError_ = "Missing or invalid 'manifestVersion' field";
            return std::nullopt;
        }
        m.manifestVersion = j["manifestVersion"].get<std::string>();
        
        if (!j.contains("name") || !j["name"].is_string()) {
            lastError_ = "Missing or invalid 'name' field";
            return std::nullopt;
        }
        m.name = j["name"].get<std::string>();
        
        if (!j.contains("version") || !j["version"].is_string()) {
            lastError_ = "Missing or invalid 'version' field";
            return std::nullopt;
        }
        m.version = j["version"].get<std::string>();
        
        if (!j.contains("description") || !j["description"].is_string()) {
            lastError_ = "Missing or invalid 'description' field";
            return std::nullopt;
        }
        m.description = j["description"].get<std::string>();
        
        if (!j.contains("author") || !j["author"].is_string()) {
            lastError_ = "Missing or invalid 'author' field";
            return std::nullopt;
        }
        m.author = j["author"].get<std::string>();
        
        if (!j.contains("type") || !j["type"].is_string()) {
            lastError_ = "Missing or invalid 'type' field";
            return std::nullopt;
        }
        m.type = j["type"].get<std::string>();
        
        if (!j.contains("category") || !j["category"].is_string()) {
            lastError_ = "Missing or invalid 'category' field";
            return std::nullopt;
        }
        m.category = j["category"].get<std::string>();
        
        if (!j.contains("dependencies") || !j["dependencies"].is_array()) {
            lastError_ = "Missing or invalid 'dependencies' field";
            return std::nullopt;
        }
        for (const auto& dep : j["dependencies"]) {
            if (!dep.is_string()) {
                lastError_ = "Invalid dependency entry (not a string)";
                return std::nullopt;
            }
            m.dependencies.push_back(dep.get<std::string>());
        }
        
        if (!j.contains("main") || !j["main"].is_object()) {
            lastError_ = "Missing or invalid 'main' field";
            return std::nullopt;
        }
        for (auto& [key, value] : j["main"].items()) {
            if (!value.is_string()) {
                lastError_ = "Invalid main entry for '" + key + "' (not a string)";
                return std::nullopt;
            }
            // Normalize key to lowercase
            std::string normalizedKey = PathNormalizer::toLowercase(key);
            m.main[normalizedKey] = value.get<std::string>();
        }
        
        return m;
    } catch (const json::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return std::nullopt;
    }
}

std::string Manifest::toJson() const {
    // Build JSON with sorted keys for determinism
    json j;
    
    j["manifestVersion"] = manifestVersion;
    j["name"] = name;
    j["version"] = version;
    j["description"] = description;
    j["author"] = author;
    j["type"] = type;
    j["category"] = category;
    j["dependencies"] = dependencies;
    
    // main as object with sorted keys
    json mainObj = json::object();
    for (const auto& [k, v] : main) {
        mainObj[k] = v;
    }
    j["main"] = mainObj;
    
    // Serialize with 2-space indent, sorted keys
    return j.dump(2);
}

Manifest::ValidationResult Manifest::validate() const {
    ValidationResult result = ValidationResult::ok();
    
    // Check version is supported
    if (!isVersionSupported(manifestVersion)) {
        result.addError("Unsupported manifest version: " + manifestVersion);
    }
    
    // Check required fields are not empty
    if (name.empty()) {
        result.addError("'name' field is empty");
    }
    
    if (version.empty()) {
        result.addError("'version' field is empty");
    }
    
    // Validate main entries
    for (const auto& [variant, path] : main) {
        // Check variant name is lowercase
        if (variant != PathNormalizer::toLowercase(variant)) {
            result.addError("Variant key '" + variant + "' is not lowercase");
        }
        
        // Check path is valid
        auto pathValidation = PathNormalizer::validateArchivePath(path);
        if (!pathValidation.valid) {
            result.addError("Invalid main path for '" + variant + "': " + pathValidation.error);
        }
    }
    
    return result;
}

Manifest::ValidationResult Manifest::validateCompleteness(
    const std::set<std::string>& existingVariants
) const {
    ValidationResult result = ValidationResult::ok();
    
    // Get variants from main (already lowercase)
    std::set<std::string> mainVariants = getVariants();
    
    // Normalize existing variants to lowercase for comparison
    std::set<std::string> normalizedExisting;
    for (const auto& v : existingVariants) {
        normalizedExisting.insert(PathNormalizer::toLowercase(v));
    }
    
    // Every main entry must have a corresponding variant directory
    for (const auto& variant : mainVariants) {
        if (normalizedExisting.find(variant) == normalizedExisting.end()) {
            result.addError("main[" + variant + "] has no corresponding variant directory");
        }
    }
    
    // Every variant directory must have a main entry
    for (const auto& variant : normalizedExisting) {
        if (mainVariants.find(variant) == mainVariants.end()) {
            result.addError("Variant '" + variant + "' has no main entry");
        }
    }
    
    return result;
}

void Manifest::normalizeName() {
    name = PathNormalizer::toLowercase(name);
}

void Manifest::normalizeVariantKeys() {
    std::map<std::string, std::string> normalized;
    for (const auto& [k, v] : main) {
        normalized[PathNormalizer::toLowercase(k)] = v;
    }
    main = std::move(normalized);
}

void Manifest::setMain(const std::string& variant, const std::string& path) {
    main[PathNormalizer::toLowercase(variant)] = path;
}

void Manifest::removeMain(const std::string& variant) {
    main.erase(PathNormalizer::toLowercase(variant));
}

std::optional<std::string> Manifest::getMain(const std::string& variant) const {
    std::string key = PathNormalizer::toLowercase(variant);
    auto it = main.find(key);
    if (it != main.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::set<std::string> Manifest::getVariants() const {
    std::set<std::string> variants;
    for (const auto& [k, v] : main) {
        variants.insert(k);
    }
    return variants;
}

bool Manifest::isVersionSupported(const std::string& version) {
    // Extract major version
    size_t dotPos = version.find('.');
    if (dotPos == std::string::npos) {
        return false;
    }
    
    std::string major = version.substr(0, dotPos);
    
    // Currently only major version 0 is supported
    return major == "0";
}

std::string Manifest::getLastError() {
    return lastError_;
}

} // namespace lgx
