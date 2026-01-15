#pragma once

#include "tar_writer.h"

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
#include <map>

namespace lgx {

/**
 * TarReader reads and extracts entries from tar archives.
 */
class TarReader {
public:
    /**
     * Result of reading a tar archive.
     */
    struct ReadResult {
        bool success;
        std::string error;
        std::vector<TarEntry> entries;
        
        static ReadResult ok(std::vector<TarEntry> e) { 
            return {true, "", std::move(e)}; 
        }
        static ReadResult fail(const std::string& msg) { 
            return {false, msg, {}}; 
        }
    };
    
    /**
     * Information about a tar entry (without data).
     */
    struct EntryInfo {
        std::string path;
        bool isDirectory;
        bool isRegularFile;
        bool isSymlink;
        bool isHardlink;
        uint64_t size;
        uint32_t mode;
        uint32_t uid;
        uint32_t gid;
        uint64_t mtime;
        std::string linkTarget;  // For symlinks/hardlinks
        char typeFlag;
    };
    
    /**
     * Read all entries from tar data.
     * 
     * @param tarData Raw tar archive data
     * @return ReadResult containing entries or error
     */
    static ReadResult read(const std::vector<uint8_t>& tarData);
    
    /**
     * Read only entry info (without file contents).
     * Useful for validation and listing.
     */
    static std::vector<EntryInfo> readInfo(const std::vector<uint8_t>& tarData);
    
    /**
     * Read a single file from tar data by path.
     * 
     * @param tarData Raw tar archive data  
     * @param path Path to extract
     * @return File contents, or nullopt if not found
     */
    static std::optional<std::vector<uint8_t>> readFile(
        const std::vector<uint8_t>& tarData,
        const std::string& path
    );
    
    /**
     * Iterate over entries without loading all into memory.
     * 
     * @param tarData Raw tar archive data
     * @param callback Called for each entry; return false to stop iteration
     * @return true if iteration completed, false if stopped or error
     */
    static bool iterate(
        const std::vector<uint8_t>& tarData,
        std::function<bool(const TarEntry& entry)> callback
    );
    
    /**
     * Check if tar data appears valid (basic header check).
     */
    static bool isValidTar(const std::vector<uint8_t>& tarData);
    
    /**
     * Get last error message.
     */
    static std::string getLastError();

private:
    static thread_local std::string lastError_;
    
    static constexpr size_t BLOCK_SIZE = 512;
    static constexpr size_t NAME_SIZE = 100;
    static constexpr size_t PREFIX_SIZE = 155;
    
    /**
     * Parse a tar header at the given offset.
     */
    static std::optional<EntryInfo> parseHeader(
        const std::vector<uint8_t>& tarData,
        size_t offset
    );
    
    /**
     * Read octal value from buffer.
     */
    static uint64_t readOctal(const uint8_t* src, size_t size);
    
    /**
     * Verify header checksum.
     */
    static bool verifyChecksum(const uint8_t* header);
    
    /**
     * Check if block is all zeros.
     */
    static bool isZeroBlock(const uint8_t* block);
    
    /**
     * Reconstruct full path from name and prefix.
     */
    static std::string reconstructPath(const uint8_t* name, const uint8_t* prefix);
};

} // namespace lgx
