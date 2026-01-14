#include "tar_writer.h"
#include "path_normalizer.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace lgx {

DeterministicTarWriter::DeterministicTarWriter() = default;
DeterministicTarWriter::~DeterministicTarWriter() = default;

void DeterministicTarWriter::addFile(const std::string& path, const std::vector<uint8_t>& data) {
    TarEntry entry;
    entry.path = path;
    entry.data = data;
    entry.isDirectory = false;
    entries_.push_back(std::move(entry));
}

void DeterministicTarWriter::addFile(const std::string& path, const std::string& content) {
    std::vector<uint8_t> data(content.begin(), content.end());
    addFile(path, data);
}

void DeterministicTarWriter::addDirectory(const std::string& path) {
    TarEntry entry;
    entry.path = path;
    entry.isDirectory = true;
    entries_.push_back(std::move(entry));
}

void DeterministicTarWriter::addEntry(const TarEntry& entry) {
    entries_.push_back(entry);
}

void DeterministicTarWriter::clear() {
    entries_.clear();
}

std::string DeterministicTarWriter::normalizeTarPath(const std::string& path, bool isDir) {
    std::string result = path;
    
    // Remove leading slashes
    while (!result.empty() && result[0] == '/') {
        result = result.substr(1);
    }
    
    // Remove trailing slashes
    while (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    
    // Add trailing slash for directories
    if (isDir && !result.empty() && result.back() != '/') {
        result += '/';
    }
    
    return result;
}

bool DeterministicTarWriter::splitPath(const std::string& path, std::string& name, std::string& prefix) {
    if (path.length() <= NAME_SIZE) {
        name = path;
        prefix = "";
        return true;
    }
    
    // Find a split point (must be at a '/')
    // prefix can be up to 155 chars, name up to 100 chars
    for (size_t i = path.length() - NAME_SIZE; i < path.length() && i <= PREFIX_SIZE; ++i) {
        if (path[i] == '/') {
            prefix = path.substr(0, i);
            name = path.substr(i + 1);
            if (prefix.length() <= PREFIX_SIZE && name.length() <= NAME_SIZE) {
                return true;
            }
        }
    }
    
    // Try from the other direction
    for (size_t i = std::min(PREFIX_SIZE, path.length() - 1); i > 0; --i) {
        if (path[i] == '/') {
            prefix = path.substr(0, i);
            name = path.substr(i + 1);
            if (name.length() <= NAME_SIZE) {
                return true;
            }
        }
    }
    
    return false;  // Path too long for USTAR
}

void DeterministicTarWriter::writeOctal(uint8_t* dest, size_t size, uint64_t value) {
    // Write octal value, space/null terminated
    char buf[24];
    snprintf(buf, sizeof(buf), "%0*llo", static_cast<int>(size - 1), 
             static_cast<unsigned long long>(value));
    std::memcpy(dest, buf, size - 1);
    dest[size - 1] = '\0';
}

uint32_t DeterministicTarWriter::calculateChecksum(const uint8_t* header) {
    uint32_t sum = 0;
    
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        // Checksum field (offset 148-155) treated as spaces
        if (i >= 148 && i < 156) {
            sum += ' ';
        } else {
            sum += header[i];
        }
    }
    
    return sum;
}

std::vector<uint8_t> DeterministicTarWriter::createHeader(const TarEntry& entry) {
    std::vector<uint8_t> header(BLOCK_SIZE, 0);
    
    std::string tarPath = normalizeTarPath(entry.path, entry.isDirectory);
    
    std::string name, prefix;
    if (!splitPath(tarPath, name, prefix)) {
        throw std::runtime_error("Path too long for USTAR format: " + tarPath);
    }
    
    // Name (0-99)
    std::memcpy(header.data(), name.c_str(), std::min(name.length(), NAME_SIZE));
    
    // Mode (100-107)
    writeOctal(header.data() + 100, 8, entry.isDirectory ? DIR_MODE : FILE_MODE);
    
    // UID (108-115)
    writeOctal(header.data() + 108, 8, UID);
    
    // GID (116-123)
    writeOctal(header.data() + 116, 8, GID);
    
    // Size (124-135)
    uint64_t size = entry.isDirectory ? 0 : entry.data.size();
    writeOctal(header.data() + 124, 12, size);
    
    // Mtime (136-147)
    writeOctal(header.data() + 136, 12, MTIME);
    
    // Checksum placeholder (148-155) - filled in later
    std::memset(header.data() + 148, ' ', 8);
    
    // Type flag (156)
    header[156] = entry.isDirectory ? '5' : '0';  // '5' = directory, '0' = regular file
    
    // Linkname (157-256) - empty
    
    // USTAR magic (257-262)
    std::memcpy(header.data() + 257, "ustar", 5);
    header[262] = '\0';
    
    // USTAR version (263-264)
    header[263] = '0';
    header[264] = '0';
    
    // Uname (265-296) - empty for determinism
    
    // Gname (297-328) - empty for determinism
    
    // Devmajor (329-336)
    writeOctal(header.data() + 329, 8, 0);
    
    // Devminor (337-344)
    writeOctal(header.data() + 337, 8, 0);
    
    // Prefix (345-499)
    if (!prefix.empty()) {
        std::memcpy(header.data() + 345, prefix.c_str(), 
                   std::min(prefix.length(), PREFIX_SIZE));
    }
    
    // Calculate and write checksum
    uint32_t checksum = calculateChecksum(header.data());
    snprintf(reinterpret_cast<char*>(header.data() + 148), 7, "%06o", checksum);
    header[154] = '\0';
    header[155] = ' ';
    
    return header;
}

std::vector<uint8_t> DeterministicTarWriter::finalize() {
    // Sort entries lexicographically by normalized path
    std::sort(entries_.begin(), entries_.end(), 
        [](const TarEntry& a, const TarEntry& b) {
            std::string pathA = normalizeTarPath(a.path, a.isDirectory);
            std::string pathB = normalizeTarPath(b.path, b.isDirectory);
            return pathA < pathB;
        });
    
    std::vector<uint8_t> result;
    
    for (const auto& entry : entries_) {
        // Write header
        auto header = createHeader(entry);
        result.insert(result.end(), header.begin(), header.end());
        
        // Write data (if file)
        if (!entry.isDirectory && !entry.data.empty()) {
            result.insert(result.end(), entry.data.begin(), entry.data.end());
            
            // Pad to block boundary
            size_t padding = (BLOCK_SIZE - (entry.data.size() % BLOCK_SIZE)) % BLOCK_SIZE;
            result.insert(result.end(), padding, 0);
        }
    }
    
    // End of archive: two zero blocks
    result.insert(result.end(), BLOCK_SIZE * 2, 0);
    
    return result;
}

} // namespace lgx
