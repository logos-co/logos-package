#pragma once

#include "manifest.h"
#include "tar_writer.h"
#include "tar_reader.h"
#include "../crypto/manifest_sig.h"
#include "../crypto/signing.h"

#include <string>
#include <vector>
#include <set>
#include <map>
#include <optional>
#include <filesystem>

namespace lgx {

/**
 * Package provides high-level operations for LGX package files.
 */
class Package {
public:
    /**
     * Result of package operations.
     */
    struct Result {
        bool success;
        std::string error;
        
        static Result ok() { return {true, ""}; }
        static Result fail(const std::string& msg) { return {false, msg}; }
    };
    
    /**
     * Verification result with detailed errors.
     */
    struct VerifyResult {
        bool valid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        
        static VerifyResult ok() { return {true, {}, {}}; }
    };
    
    /**
     * Allowed root entries in an LGX package.
     */
    static const std::set<std::string> ALLOWED_ROOT_ENTRIES;
    
    /**
     * Create a new skeleton package.
     * 
     * @param outputPath Path to write the .lgx file
     * @param name Package name (will be lowercased)
     * @return Result indicating success or failure
     */
    static Result create(
        const std::filesystem::path& outputPath,
        const std::string& name
    );
    
    /**
     * Load an existing package from file.
     * 
     * @param lgxPath Path to the .lgx file
     * @return Package instance, or nullopt on error
     */
    static std::optional<Package> load(const std::filesystem::path& lgxPath);
    
    /**
     * Save the package to a file.
     * 
     * @param lgxPath Path to write the .lgx file
     * @return Result indicating success or failure
     */
    Result save(const std::filesystem::path& lgxPath) const;
    
    /**
     * Verify a package file.
     * 
     * @param lgxPath Path to the .lgx file
     * @return VerifyResult with all validation errors/warnings
     */
    static VerifyResult verify(const std::filesystem::path& lgxPath);
    
    /**
     * Add files to a variant.
     * If the variant exists, it is completely replaced.
     * 
     * @param variant Variant name (will be lowercased)
     * @param filesPath Path to file or directory to add
     * @param mainPath Relative path to main file (required for directories)
     * @return Result indicating success or failure
     */
    Result addVariant(
        const std::string& variant,
        const std::filesystem::path& filesPath,
        const std::optional<std::string>& mainPath = std::nullopt
    );
    
    /**
     * Remove a variant.
     * 
     * @param variant Variant name (case-insensitive)
     * @return Result indicating success or failure
     */
    Result removeVariant(const std::string& variant);
    
    /**
     * Check if a variant exists.
     */
    bool hasVariant(const std::string& variant) const;
    
    /**
     * Get list of variant names.
     */
    std::set<std::string> getVariants() const;
    
    /**
     * Get the manifest.
     */
    Manifest& getManifest() { return manifest_; }
    const Manifest& getManifest() const { return manifest_; }
    
    /**
     * Check if the main path would change for a variant.
     */
    bool wouldMainChange(const std::string& variant, const std::string& newMain) const;
    
    /**
     * Extract a specific variant to an output directory.
     * 
     * @param variant Variant name (case-insensitive)
     * @param outputDir Directory to extract to (variant contents go to outputDir/variant/)
     * @return Result indicating success or failure
     */
    Result extractVariant(const std::string& variant, const std::filesystem::path& outputDir) const;
    
    /**
     * Extract all variants to an output directory.
     * 
     * @param outputDir Directory to extract to (each variant goes to outputDir/variant/)
     * @return Result indicating success or failure
     */
    Result extractAll(const std::filesystem::path& outputDir) const;
    
    /**
     * Get entry info for verification.
     */
    const std::vector<TarEntry>& getEntries() const { return entries_; }

    /**
     * Result of signature verification.
     */
    struct SignatureInfo {
        bool is_signed;           // manifest.sig present
        bool signature_valid;     // Ed25519 signature verifies
        bool package_valid;       // package structure and content hashes are valid
        std::string signer_did;   // did:jwk:... string
        std::string signer_name;  // from manifest.sig signer.name (self-asserted)
        std::string signer_url;   // from manifest.sig signer.url (self-asserted)
        std::string trusted_as;   // keyring name if DID is trusted, empty otherwise
        std::string error;        // error message if any
    };

    /**
     * Sign the package with the given secret key.
     * Validates the package first (structure + content hashes must be
     * valid), then signs manifest.toJson() bytes and stores the
     * signature in manifest.sig.
     *
     * @param sk Ed25519 secret key
     * @param signerName Optional display name for signer metadata
     * @param signerUrl Optional URL for signer metadata
     * @return Result indicating success or failure
     */
    Result signPackage(const crypto::SecretKey& sk,
                       const std::string& signerName = "",
                       const std::string& signerUrl = "");

    /**
     * Validate package structure and content hashes (non-static version).
     * Runs the same checks as verify() but on the already-loaded package.
     *
     * @return VerifyResult with all validation errors/warnings
     */
    VerifyResult validatePackage() const;

    /**
     * Verify the package signature.
     * First validates the package (structure + hashes), then checks
     * the Ed25519 signature. package_valid reflects whether the package
     * itself is valid; signature_valid reflects whether the signature
     * over manifest.json verifies.
     *
     * @return SignatureInfo with verification results
     */
    SignatureInfo verifySignature() const;

    /**
     * Check if the package has a signature.
     */
    bool isSigned() const { return manifestSig_.has_value(); }

    /**
     * Clear any existing signature.
     * Called automatically when package content is modified.
     * Hashes are NOT cleared — they are recomputed separately.
     */
    void clearSignature();

    /**
     * Recompute Merkle tree hashes over all package content.
     * Called automatically when package content is modified.
     * Hashes are always kept up to date in manifest.json.
     */
    void recomputeHashes();

    /**
     * Get last error message.
     */
    static std::string getLastError();

private:
    Manifest manifest_;
    std::vector<TarEntry> entries_;
    std::optional<crypto::ManifestSig> manifestSig_;
    
    static thread_local std::string lastError_;
    
    /**
     * Rebuild tar entries, ensuring manifest is included.
     */
    void rebuildTar();
    
    /**
     * Add filesystem entries recursively.
     */
    Result addFilesystemEntries(
        const std::filesystem::path& fsPath,
        const std::string& archiveBasePath
    );
    
    /**
     * Remove entries for a variant.
     */
    void removeVariantEntries(const std::string& variant);
    
    /**
     * Get directory entries that need to be created for a path.
     */
    static std::vector<std::string> getRequiredDirectories(const std::string& path);
};

} // namespace lgx
