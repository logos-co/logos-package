#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace lgx {

/**
 * TarEntry represents a single entry in a tar archive.
 */
struct TarEntry {
    std::string path;           // NFC-normalized archive path
    std::vector<uint8_t> data;  // File contents (empty for directories)
    bool isDirectory;
    
    TarEntry() : isDirectory(false) {}
    TarEntry(const std::string& p, bool isDir = false) 
        : path(p), isDirectory(isDir) {}
    TarEntry(const std::string& p, const std::vector<uint8_t>& d)
        : path(p), data(d), isDirectory(false) {}
};

/**
 * DeterministicTarWriter creates tar archives with deterministic output.
 * 
 * Determinism is achieved by:
 * - Lexicographic sorting of entries by NFC-normalized path bytes
 * - Fixed metadata: uid=0, gid=0, uname="", gname="", mtime=0
 * - Fixed modes: dirs=0755, files=0644
 * - USTAR format for consistency
 */
class DeterministicTarWriter {
public:
    DeterministicTarWriter();
    ~DeterministicTarWriter();
    
    /**
     * Add a file entry to the archive.
     * 
     * @param path NFC-normalized archive path (no leading slash)
     * @param data File contents
     */
    void addFile(const std::string& path, const std::vector<uint8_t>& data);
    
    /**
     * Add a file entry from string content.
     */
    void addFile(const std::string& path, const std::string& content);
    
    /**
     * Add a directory entry to the archive.
     * 
     * @param path NFC-normalized archive path (no leading slash, trailing slash optional)
     */
    void addDirectory(const std::string& path);
    
    /**
     * Add an entry (file or directory).
     */
    void addEntry(const TarEntry& entry);
    
    /**
     * Finalize and return the tar archive data.
     * Entries are sorted lexicographically before writing.
     * 
     * @return Complete tar archive data
     */
    std::vector<uint8_t> finalize();
    
    /**
     * Clear all entries.
     */
    void clear();
    
    /**
     * Get the number of entries.
     */
    size_t entryCount() const { return entries_.size(); }

private:
    std::vector<TarEntry> entries_;
    
    // Tar format constants
    static constexpr size_t BLOCK_SIZE = 512;
    static constexpr size_t NAME_SIZE = 100;
    static constexpr size_t PREFIX_SIZE = 155;
    
    // Fixed metadata values for determinism
    static constexpr uint32_t DIR_MODE = 0755;
    static constexpr uint32_t FILE_MODE = 0644;
    static constexpr uint32_t UID = 0;
    static constexpr uint32_t GID = 0;
    static constexpr uint64_t MTIME = 0;
    
    /**
     * Write a single tar header.
     */
    std::vector<uint8_t> createHeader(const TarEntry& entry);
    
    /**
     * Calculate tar checksum.
     */
    static uint32_t calculateChecksum(const uint8_t* header);
    
    /**
     * Write octal value to buffer.
     */
    static void writeOctal(uint8_t* dest, size_t size, uint64_t value);
    
    /**
     * Normalize path for tar (ensure proper format).
     */
    static std::string normalizeTarPath(const std::string& path, bool isDir);
    
    /**
     * Split long path into name and prefix for USTAR.
     */
    static bool splitPath(const std::string& path, std::string& name, std::string& prefix);
};

} // namespace lgx
