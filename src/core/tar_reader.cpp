#include "tar_reader.h"

#include <cstring>
#include <algorithm>
#include <array>
#include <limits>
#include <zlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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
        
        TarEntry entry(info.path, info.isDirectory, info.mode);
        
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

TarReader::ReadResult TarReader::readGzipFile(const std::filesystem::path& path,
                                               size_t maxOutputSize) {
#ifdef _WIN32
    // The MinGW zlib import library does not export gzopen_w. Open the wide
    // path with the CRT, then transfer ownership of its descriptor to zlib.
    const int fd = _wopen(path.c_str(), _O_RDONLY | _O_BINARY);
    if (fd < 0) return ReadResult::fail("Cannot open file: " + path.string());
    gzFile stream = gzdopen(fd, "rb");
    if (!stream) _close(fd);
#else
    gzFile stream = gzopen(path.c_str(), "rb");
#endif
    if (!stream) return ReadResult::fail("Cannot open file: " + path.string());

    // gzread passes non-gzip (and empty) files through verbatim; LGX files
    // must be gzip streams, as GzipHandler::decompress required.
    if (gzdirect(stream)) {
        gzclose(stream);
        return ReadResult::fail("Not valid gzip data");
    }

    // DEFLATE inflates at most 1032:1.
    std::error_code sizeError;
    const auto compressedSize = std::filesystem::file_size(path, sizeError);
    const size_t inflateBound =
        sizeError || compressedSize > std::numeric_limits<size_t>::max() / 1032
            ? std::numeric_limits<size_t>::max()
            : static_cast<size_t>(compressedSize) * 1032;

    size_t outputBytes = 0;
    std::string readError;
    // Reads until `length` bytes or the end of the stream, counting against
    // the cap. Returns the bytes read, or nullopt with readError set.
    auto readUpTo = [&](uint8_t* destination, size_t length) -> std::optional<size_t> {
        size_t done = 0;
        while (done < length) {
            const auto chunk = static_cast<unsigned>(std::min<size_t>(length - done, 32768));
            const int got = gzread(stream, destination + done, chunk);
            int zlibCode = Z_OK;
            const char* detail = gzerror(stream, &zlibCode);
            if (got < 0 || zlibCode != Z_OK) {
                readError = "Failed to decompress: " +
                            std::string(detail ? detail : "unknown zlib error");
                return std::nullopt;
            }
            if (got == 0) break;
            if (static_cast<size_t>(got) > maxOutputSize - outputBytes) {
                readError = "Decompressed size exceeds limit of " +
                            std::to_string(maxOutputSize) + " bytes";
                return std::nullopt;
            }
            outputBytes += static_cast<size_t>(got);
            done += static_cast<size_t>(got);
        }
        return done;
    };
    auto readExactly = [&](uint8_t* destination, size_t length) {
        const auto got = readUpTo(destination, length);
        if (got && *got != length) readError = "Truncated tar data";
        return got && *got == length;
    };

    std::vector<TarEntry> entries;
    std::vector<uint8_t> header(BLOCK_SIZE);
    std::array<uint8_t, 32768> scratch{};
    int zeroBlocks = 0;

    while (true) {
        const auto got = readUpTo(header.data(), header.size());
        if (!got || *got == 0) break;
        if (*got != header.size()) {
            readError = "Truncated tar data";
            break;
        }

        if (isZeroBlock(header.data())) {
            if (++zeroBlocks >= 2) {
                // Drain to the gzip trailer so CRC errors and an oversized
                // trailing stream cannot be accepted after the tar end marker.
                std::optional<size_t> drained;
                while ((drained = readUpTo(scratch.data(), scratch.size())) &&
                       *drained == scratch.size()) {}
                break;
            }
            continue;
        }
        zeroBlocks = 0;

        auto info = parseHeader(header, 0);
        if (!info) {
            readError = lastError_;
            break;
        }
        TarEntry entry(info->path, info->isDirectory, info->mode);
        if (info->isRegularFile && info->size > 0) {
            if (info->size > std::numeric_limits<size_t>::max() ||
                static_cast<size_t>(info->size) > maxOutputSize - outputBytes) {
                readError = "Decompressed size exceeds limit of " +
                            std::to_string(maxOutputSize) + " bytes";
                break;
            }
            // Allocate up front no more than the file could inflate to, so a
            // header alone cannot claim a cap-sized buffer; grow past that.
            const auto size = static_cast<size_t>(info->size);
            size_t want = std::min(size, std::max(inflateBound, scratch.size()));
            while (entry.data.size() < size) {
                const size_t have = entry.data.size();
                entry.data.resize(want);
                if (!readExactly(entry.data.data() + have, want - have)) break;
                want = std::min(size, want * 2);
            }
            if (!readError.empty()) break;

            const size_t padding = (BLOCK_SIZE - (size % BLOCK_SIZE)) % BLOCK_SIZE;
            if (padding && !readExactly(scratch.data(), padding)) break;
        }
        entries.push_back(std::move(entry));
    }

    const int closeResult = gzclose(stream);
    if (readError.empty() && closeResult != Z_OK)
        readError = "Failed to close gzip stream";
    if (!readError.empty()) return ReadResult::fail(readError);
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
        
        TarEntry entry(info.path, info.isDirectory, info.mode);

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
