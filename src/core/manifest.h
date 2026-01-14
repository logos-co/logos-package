#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <set>

namespace lgx {

/**
 * Manifest represents the manifest.json file in an LGX package.
 */
class Manifest {
public:
    // Current manifest version
    static constexpr const char* CURRENT_VERSION = "0.1.0";
    
    /**
     * Validation result for manifest.
     */
    struct ValidationResult {
        bool valid;
        std::vector<std::string> errors;
        
        static ValidationResult ok() { return {true, {}}; }
        static ValidationResult fail(const std::string& error) { 
            return {false, {error}}; 
        }
        static ValidationResult fail(const std::vector<std::string>& errors) {
            return {false, errors};
        }
        
        void addError(const std::string& error) {
            valid = false;
            errors.push_back(error);
        }
    };
    
    // Required fields
    std::string manifestVersion;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string type;
    std::string category;
    std::vector<std::string> dependencies;
    
    // Main mapping: variant -> relative path to entry point
    std::map<std::string, std::string> main;
    
    /**
     * Create a new empty manifest with default version.
     */
    Manifest();
    
    /**
     * Parse manifest from JSON string.
     * 
     * @param json JSON string
     * @return Manifest if parsing succeeds, nullopt on error
     */
    static std::optional<Manifest> fromJson(const std::string& json);
    
    /**
     * Serialize manifest to JSON string.
     * The output is deterministic (sorted keys, consistent formatting).
     * 
     * @return JSON string
     */
    std::string toJson() const;
    
    /**
     * Validate manifest fields.
     * Does NOT check completeness against actual variants.
     * 
     * @return ValidationResult with any errors
     */
    ValidationResult validate() const;
    
    /**
     * Validate completeness: every variant in main must exist,
     * every existing variant must have a main entry.
     * 
     * @param existingVariants Set of variant names that exist in the package
     * @return ValidationResult with any errors
     */
    ValidationResult validateCompleteness(const std::set<std::string>& existingVariants) const;
    
    /**
     * Normalize name to lowercase.
     */
    void normalizeName();
    
    /**
     * Normalize all variant keys in main to lowercase.
     */
    void normalizeVariantKeys();
    
    /**
     * Add or update a main entry for a variant.
     * Variant key is automatically lowercased.
     */
    void setMain(const std::string& variant, const std::string& path);
    
    /**
     * Remove a main entry for a variant.
     */
    void removeMain(const std::string& variant);
    
    /**
     * Get main entry for a variant (case-insensitive lookup).
     */
    std::optional<std::string> getMain(const std::string& variant) const;
    
    /**
     * Get all variant names from main.
     */
    std::set<std::string> getVariants() const;
    
    /**
     * Check if manifest version is supported.
     */
    static bool isVersionSupported(const std::string& version);
    
    /**
     * Get last parsing error.
     */
    static std::string getLastError();

private:
    static thread_local std::string lastError_;
};

} // namespace lgx
