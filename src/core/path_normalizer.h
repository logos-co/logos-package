#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <vector>

namespace lgx {

/**
 * PathNormalizer handles Unicode NFC normalization and security validation
 * for archive paths according to the LGX specification.
 */
class PathNormalizer {
public:
    /**
     * Result of path validation
     */
    struct ValidationResult {
        bool valid;
        std::string error;
        
        static ValidationResult ok() { return {true, ""}; }
        static ValidationResult fail(const std::string& msg) { return {false, msg}; }
    };

    /**
     * Normalize a path to Unicode NFC form.
     * @param path The input path (UTF-8 encoded)
     * @return NFC-normalized path, or std::nullopt if normalization fails
     */
    static std::optional<std::string> toNFC(const std::string& path);
    
    /**
     * Check if a string is already in NFC form.
     */
    static bool isNFC(const std::string& str);
    
    /**
     * Validate an archive path according to LGX security rules:
     * - Not absolute
     * - No ".." segments after normalization
     * - Not empty
     * - No backslashes
     * 
     * @param archivePath The path within the archive
     * @return ValidationResult indicating success or failure with error message
     */
    static ValidationResult validateArchivePath(const std::string& archivePath);
    
    /**
     * Normalize path separators (convert backslashes to forward slashes)
     * and collapse redundant separators.
     */
    static std::string normalizeSeparators(const std::string& path);
    
    /**
     * Convert a path to lowercase (for variant names, package names, etc.)
     */
    static std::string toLowercase(const std::string& str);
    
    /**
     * Join path components with forward slash
     */
    static std::string joinPath(const std::vector<std::string>& components);
    static std::string joinPath(const std::string& base, const std::string& relative);
    
    /**
     * Get the basename of a path
     */
    static std::string basename(const std::string& path);
    
    /**
     * Get the directory name of a path
     */
    static std::string dirname(const std::string& path);
    
    /**
     * Check if path is absolute
     */
    static bool isAbsolute(const std::string& path);
    
    /**
     * Split path into components
     */
    static std::vector<std::string> splitPath(const std::string& path);
    
    /**
     * Get the root component of an archive path (first directory)
     */
    static std::string getRootComponent(const std::string& archivePath);
};

} // namespace lgx
