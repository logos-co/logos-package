#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace lgx {

/**
 * A package dependency. Lives inside Manifest::dependencies.
 *
 * Two on-disk forms are accepted:
 *   - Plain string: "waku_module"           -> Dependency{ name = "waku_module" }
 *   - Object:       { "name": "...",
 *                     "version"?: "^1.2.0",
 *                     "signer"?: "did:jwk:..." }
 *
 * The struct is implicitly constructible from a std::string so existing code
 * that does `dependencies.push_back("dep")` keeps working unchanged.
 */
struct Dependency {
    std::string name;                       // required, lowercase canonical name
    std::optional<std::string> version;     // npm/Cargo-style semver range
    std::optional<std::string> signer;      // did:jwk:... DID, when pinned

    Dependency() = default;
    Dependency(std::string n) : name(std::move(n)) {}     // NOLINT: intended implicit
    Dependency(const char* n) : name(n) {}                // NOLINT: intended implicit

    /// True when only the `name` field is set — the value can serialize back
    /// as a plain string.
    bool isSimple() const {
        return !version.has_value() && !signer.has_value();
    }

    /// Human-readable single-line form, used for `lgx manifest` output and
    /// error messages.
    std::string toString() const {
        std::string s = name;
        if (version)  s += " " + *version;
        if (signer)   s += " [signer=" + *signer + "]";
        return s;
    }

    bool operator==(const Dependency& o) const {
        return name == o.name && version == o.version && signer == o.signer;
    }
    bool operator!=(const Dependency& o) const { return !(*this == o); }
};

/**
 * Manifest represents the manifest.json file in an LGX package.
 */
class Manifest {
public:
    // Current manifest version. Bumped to 0.3.0 to accommodate richer
    // dependency entries (semver ranges + optional signer DID). 0.2.x
    // manifests are still readable for backward compatibility.
    static constexpr const char* CURRENT_VERSION = "0.3.0";
    
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
    std::string icon;
    std::vector<Dependency> dependencies;

    // human-readable label; consumers fall back to `name` when unset.
    std::string displayName;
    
    // Main mapping: variant -> relative path to entry point
    std::map<std::string, std::string> main;

    // Merkle tree hashes (always present, recomputed on every content change)
    // Keys: "root", "variants", "variants/<name>", "docs", "licenses", etc.
    // Values: hex-encoded SHA-256
    std::map<std::string, std::string> hashes;

    // Path to the QML entry point, relative to the selected variant root
    // (e.g. under variants/<variant>/).
    // Required for type == "ui_qml" (enforced by validate()); empty for
    // other package types.
    std::string view;
    
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
     * Compare metadata fields with another manifest, ignoring variant-specific
     * fields (main). Returns a ValidationResult with an error for each
     * mismatching field.
     */
    ValidationResult compareMetadata(const Manifest& other) const;

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
