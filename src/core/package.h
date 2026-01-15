#pragma once

#include "manifest.h"
#include "tar_writer.h"
#include "tar_reader.h"

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
     * Get entry info for verification.
     */
    const std::vector<TarEntry>& getEntries() const { return entries_; }
    
    /**
     * Get last error message.
     */
    static std::string getLastError();

private:
    Manifest manifest_;
    std::vector<TarEntry> entries_;
    
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
