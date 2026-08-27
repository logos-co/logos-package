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
// An intent a package can service.
//
// NAME ONLY. The author's metadata.json may also describe the payload shape
// (`provides[].params`), and the shell enforces that against the INSTALLED
// metadata.json — but none of it is carried here. The manifest copy exists for
// one question, asked before a package is installed: "which installable package
// provides X?" That is answered by the name alone.
struct ProvidedIntent {
    std::string intent;
};

class Manifest {
public:
    // Current manifest version. Bumped to 0.5.0 for `provides`: the intents a
    // package can service. It lives here, in the SIGNED manifest, rather than
    // only in the unsigned metadata.json on disk, so the capability claim is
    // covered by the package signature and is legible to a catalog before the
    // package is installed. 0.2.x-0.4.x manifests are still readable.
    static constexpr const char* CURRENT_VERSION = "0.5.0";

    // Canonical in-package icon location for 0.4.0+. The author's
    // metadata.json path stays free-form; the bundler normalises to this.
    static constexpr const char* ICON_PATH = "assets/icon.png";

    // Required icon dimensions, enforced for 0.4.0+ packages whose type
    // requires an icon. Exact, not a minimum — see plan.md §3.4.
    static constexpr int ICON_SIZE_PX = 256;

    // True when `version` is 0.4.0 or newer, i.e. the icon contract applies.
    // 0.2.x/0.3.x packages predate it and are exempt (plan.md §3.7): `icon`
    // was legal as "" back then, so applying the rule unconditionally would
    // make every already-published package uninstallable.
    static bool requiresIconContract(const std::string& version);
    
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

    // Intents this package can service, e.g. "chat.group.open". Names only.
    // Copied from the author's metadata.json at bundle time. Carried here so a
    // CATALOG can answer "which installable package provides X?" without the
    // package being installed — the registry inside a running shell reads the
    // installed metadata.json, but nothing outside can.
    std::vector<ProvidedIntent> provides;
    
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
