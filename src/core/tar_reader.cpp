#include "tar_reader.h"

#include <cstring>
#include <algorithm>

namespace lgx {

thread_local std::string TarReader::lastError_;

uint64_t TarReader::readOctal(const uint8_t* src, size_t size) {
    uint64_t value = 0;
    
    // Skip leading spaces
    size_t i = 0;
    while (i < size && (src[i] == ' ' || src[i] == '\0')) {
        ++i;
    }
    
    // Read octal digits
    while (i < size && src[i] >= '0' && src[i] <= '7') {
        value = (value * 8) + (src[i] - '0');
        ++i;
    }
    
    return value;
}

bool TarReader::verifyChecksum(const uint8_t* header) {
    uint32_t storedChecksum = static_cast<uint32_t>(readOctal(header + 148, 8));
    
    uint32_t calculatedChecksum = 0;
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        if (i >= 148 && i < 156) {
            calculatedChecksum += ' ';
        } else {
            calculatedChecksum += header[i];
        }
    }
    
    return storedChecksum == calculatedChecksum;
}

bool TarReader::isZeroBlock(const uint8_t* block) {
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        if (block[i] != 0) {
            return false;
        }
    }
    return true;
}

std::string TarReader::reconstructPath(const uint8_t* name, const uint8_t* prefix) {
    std::string result;
    
    // Read prefix (up to 155 chars)
    size_t prefixLen = 0;
    while (prefixLen < PREFIX_SIZE && prefix[prefixLen] != '\0') {
        ++prefixLen;
    }
    
    // Read name (up to 100 chars)
    size_t nameLen = 0;
    while (nameLen < NAME_SIZE && name[nameLen] != '\0') {
        ++nameLen;
    }
    
    if (prefixLen > 0) {
        result.assign(reinterpret_cast<const char*>(prefix), prefixLen);
        result += '/';
    }
    result.append(reinterpret_cast<const char*>(name), nameLen);
    
    return result;
}

std::optional<TarReader::EntryInfo> TarReader::parseHeader(
    const std::vector<uint8_t>& tarData,
    size_t offset
) {
    if (offset + BLOCK_SIZE > tarData.size()) {
        lastError_ = "Incomplete header at offset " + std::to_string(offset);
        return std::nullopt;
    }
    
    const uint8_t* header = tarData.data() + offset;
    
    // Check for end-of-archive (zero block)
    if (isZeroBlock(header)) {
        return std::nullopt;
    }
    
    // Verify checksum
    if (!verifyChecksum(header)) {
        lastError_ = "Invalid checksum at offset " + std::to_string(offset);
        return std::nullopt;
    }
    
    EntryInfo info;
    
    // Parse path
    info.path = reconstructPath(header, header + 345);
    
    // Parse mode
    info.mode = static_cast<uint32_t>(readOctal(header + 100, 8));
    
    // Parse uid/gid
    info.uid = static_cast<uint32_t>(readOctal(header + 108, 8));
    info.gid = static_cast<uint32_t>(readOctal(header + 116, 8));
    
    // Parse size
    info.size = readOctal(header + 124, 12);
    
    // Parse mtime
    info.mtime = readOctal(header + 136, 12);
    
    // Parse type flag
    info.typeFlag = static_cast<char>(header[156]);
    
    // Determine type
    info.isDirectory = (info.typeFlag == '5');
    info.isRegularFile = (info.typeFlag == '0' || info.typeFlag == '\0');
    info.isSymlink = (info.typeFlag == '2');
    info.isHardlink = (info.typeFlag == '1');
    
    // Parse link target (for symlinks/hardlinks)
    if (info.isSymlink || info.isHardlink) {
        size_t linkLen = 0;
        while (linkLen < 100 && header[157 + linkLen] != '\0') {
            ++linkLen;
        }
        info.linkTarget.assign(reinterpret_cast<const char*>(header + 157), linkLen);
    }
    
    return info;
}

TarReader::ReadResult TarReader::read(const std::vector<uint8_t>& tarData) {
    std::vector<TarEntry> entries;
    
    size_t offset = 0;
    int zeroBlockCount = 0;
    
    while (offset < tarData.size()) {
        auto infoOpt = parseHeader(tarData, offset);
        
        if (!infoOpt) {
            if (isZeroBlock(tarData.data() + offset)) {
                ++zeroBlockCount;
                offset += BLOCK_SIZE;
                if (zeroBlockCount >= 2) {
                    break;  // End of archive
                }
                continue;
            }
            return ReadResult::fail(lastError_);
        }
        
        zeroBlockCount = 0;
        const auto& info = *infoOpt;
        
        TarEntry entry;
        entry.path = info.path;
        entry.isDirectory = info.isDirectory;
        
        // Move past header
        offset += BLOCK_SIZE;
        
        // Read file data
        if (info.isRegularFile && info.size > 0) {
            if (offset + info.size > tarData.size()) {
                return ReadResult::fail("Incomplete file data for " + info.path);
            }
            
            entry.data.assign(
                tarData.begin() + offset,
                tarData.begin() + offset + info.size
            );
            
            // Move past data blocks (padded to block boundary)
            size_t dataBlocks = (info.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            offset += dataBlocks * BLOCK_SIZE;
        }
        
        entries.push_back(std::move(entry));
    }
    
    return ReadResult::ok(std::move(entries));
}

std::vector<TarReader::EntryInfo> TarReader::readInfo(const std::vector<uint8_t>& tarData) {
    std::vector<EntryInfo> entries;
    
    size_t offset = 0;
    int zeroBlockCount = 0;
    
    while (offset < tarData.size()) {
        auto infoOpt = parseHeader(tarData, offset);
        
        if (!infoOpt) {
            if (offset < tarData.size() && isZeroBlock(tarData.data() + offset)) {
                ++zeroBlockCount;
                offset += BLOCK_SIZE;
                if (zeroBlockCount >= 2) {
                    break;
                }
                continue;
            }
            break;
        }
        
        zeroBlockCount = 0;
        const auto& info = *infoOpt;
        entries.push_back(info);
        
        // Move past header
        offset += BLOCK_SIZE;
        
        // Skip data blocks
        if (info.isRegularFile && info.size > 0) {
            size_t dataBlocks = (info.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            offset += dataBlocks * BLOCK_SIZE;
        }
    }
    
    return entries;
}

std::optional<std::vector<uint8_t>> TarReader::readFile(
    const std::vector<uint8_t>& tarData,
    const std::string& path
) {
    size_t offset = 0;
    int zeroBlockCount = 0;
    
    // Normalize the search path (remove leading/trailing slashes)
    std::string searchPath = path;
    while (!searchPath.empty() && searchPath[0] == '/') {
        searchPath = searchPath.substr(1);
    }
    while (!searchPath.empty() && searchPath.back() == '/') {
        searchPath.pop_back();
    }
    
    while (offset < tarData.size()) {
        auto infoOpt = parseHeader(tarData, offset);
        
        if (!infoOpt) {
            if (offset < tarData.size() && isZeroBlock(tarData.data() + offset)) {
                ++zeroBlockCount;
                offset += BLOCK_SIZE;
                if (zeroBlockCount >= 2) {
                    break;
                }
                continue;
            }
            break;
        }
        
        zeroBlockCount = 0;
        const auto& info = *infoOpt;
        
        // Normalize entry path for comparison
        std::string entryPath = info.path;
        while (!entryPath.empty() && entryPath[0] == '/') {
            entryPath = entryPath.substr(1);
        }
        while (!entryPath.empty() && entryPath.back() == '/') {
            entryPath.pop_back();
        }
        
        // Move past header
        offset += BLOCK_SIZE;
        
        // Check if this is the file we're looking for
        if (entryPath == searchPath && info.isRegularFile) {
            if (info.size == 0) {
                return std::vector<uint8_t>{};
            }
            
            if (offset + info.size > tarData.size()) {
                lastError_ = "Incomplete file data";
                return std::nullopt;
            }
            
            return std::vector<uint8_t>(
                tarData.begin() + offset,
                tarData.begin() + offset + info.size
            );
        }
        
        // Skip data blocks
        if (info.isRegularFile && info.size > 0) {
            size_t dataBlocks = (info.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            offset += dataBlocks * BLOCK_SIZE;
        }
    }
    
    lastError_ = "File not found: " + path;
    return std::nullopt;
}

bool TarReader::iterate(
    const std::vector<uint8_t>& tarData,
    std::function<bool(const TarEntry& entry)> callback
) {
    size_t offset = 0;
    int zeroBlockCount = 0;
    
    while (offset < tarData.size()) {
        auto infoOpt = parseHeader(tarData, offset);
        
        if (!infoOpt) {
            if (offset < tarData.size() && isZeroBlock(tarData.data() + offset)) {
                ++zeroBlockCount;
                offset += BLOCK_SIZE;
                if (zeroBlockCount >= 2) {
                    return true;  // Normal end of archive
                }
                continue;
            }
            return false;  // Error
        }
        
        zeroBlockCount = 0;
        const auto& info = *infoOpt;
        
        TarEntry entry;
        entry.path = info.path;
        entry.isDirectory = info.isDirectory;
        
        // Move past header
        offset += BLOCK_SIZE;
        
        // Read file data
        if (info.isRegularFile && info.size > 0) {
            if (offset + info.size > tarData.size()) {
                lastError_ = "Incomplete file data for " + info.path;
                return false;
            }
            
            entry.data.assign(
                tarData.begin() + offset,
                tarData.begin() + offset + info.size
            );
            
            size_t dataBlocks = (info.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            offset += dataBlocks * BLOCK_SIZE;
        }
        
        if (!callback(entry)) {
            return false;  // Callback requested stop
        }
    }
    
    return true;
}

bool TarReader::isValidTar(const std::vector<uint8_t>& tarData) {
    if (tarData.size() < BLOCK_SIZE) {
        return false;
    }
    
    // Check for USTAR magic at offset 257
    const uint8_t* header = tarData.data();
    if (std::memcmp(header + 257, "ustar", 5) == 0) {
        return verifyChecksum(header);
    }
    
    // Old-style tar (no magic, but valid checksum)
    return verifyChecksum(header);
}

std::string TarReader::getLastError() {
    return lastError_;
}

} // namespace lgx
