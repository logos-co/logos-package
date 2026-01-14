#include "gzip_handler.h"

#include <zlib.h>
#include <cstring>
#include <array>

namespace lgx {

thread_local std::string GzipHandler::lastError_;

std::vector<uint8_t> GzipHandler::compress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        // Return valid empty gzip for empty input
        std::vector<uint8_t> result;
        
        // Manual gzip header for determinism
        result.push_back(GZIP_MAGIC1);  // Magic number
        result.push_back(GZIP_MAGIC2);
        result.push_back(COMPRESSION_DEFLATE);  // Compression method
        result.push_back(FLAGS_NONE);   // Flags
        result.push_back(0); result.push_back(0); result.push_back(0); result.push_back(0);  // mtime = 0
        result.push_back(0);  // Extra flags
        result.push_back(OS_UNKNOWN);   // OS
        
        // Compress empty data
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));
        
        if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            lastError_ = "Failed to initialize deflate";
            return {};
        }
        
        std::array<uint8_t, 32> outBuf;
        strm.next_in = nullptr;
        strm.avail_in = 0;
        strm.next_out = outBuf.data();
        strm.avail_out = outBuf.size();
        
        deflate(&strm, Z_FINISH);
        result.insert(result.end(), outBuf.begin(), outBuf.begin() + (outBuf.size() - strm.avail_out));
        
        deflateEnd(&strm);
        
        // CRC32 and size (both 0 for empty input)
        uint32_t crc = crc32(0L, Z_NULL, 0);
        result.push_back(crc & 0xFF);
        result.push_back((crc >> 8) & 0xFF);
        result.push_back((crc >> 16) & 0xFF);
        result.push_back((crc >> 24) & 0xFF);
        result.push_back(0); result.push_back(0); result.push_back(0); result.push_back(0);
        
        return result;
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size() + 128);  // Reserve some extra space
    
    // Write deterministic gzip header
    result.push_back(GZIP_MAGIC1);  // Magic number
    result.push_back(GZIP_MAGIC2);
    result.push_back(COMPRESSION_DEFLATE);  // Compression method (deflate)
    result.push_back(FLAGS_NONE);   // Flags (none)
    // Modification time = 0 (4 bytes, little-endian)
    result.push_back(0);
    result.push_back(0);
    result.push_back(0);
    result.push_back(0);
    result.push_back(0);  // Extra flags
    result.push_back(OS_UNKNOWN);   // OS (unknown for determinism)
    
    // Initialize deflate with raw deflate (no zlib header)
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    
    int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          -MAX_WBITS,  // Negative for raw deflate
                          8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        lastError_ = "Failed to initialize deflate: " + std::to_string(ret);
        return {};
    }
    
    // Compress data
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_in = static_cast<uInt>(data.size());
    
    std::array<uint8_t, 32768> outBuf;
    
    do {
        strm.next_out = outBuf.data();
        strm.avail_out = outBuf.size();
        
        ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            lastError_ = "Deflate stream error";
            return {};
        }
        
        size_t have = outBuf.size() - strm.avail_out;
        result.insert(result.end(), outBuf.begin(), outBuf.begin() + have);
    } while (strm.avail_out == 0);
    
    deflateEnd(&strm);
    
    // Calculate CRC32
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, data.data(), static_cast<uInt>(data.size()));
    
    // Append CRC32 (little-endian)
    result.push_back(crc & 0xFF);
    result.push_back((crc >> 8) & 0xFF);
    result.push_back((crc >> 16) & 0xFF);
    result.push_back((crc >> 24) & 0xFF);
    
    // Append original size (little-endian, mod 2^32)
    uint32_t size = static_cast<uint32_t>(data.size());
    result.push_back(size & 0xFF);
    result.push_back((size >> 8) & 0xFF);
    result.push_back((size >> 16) & 0xFF);
    result.push_back((size >> 24) & 0xFF);
    
    return result;
}

std::vector<uint8_t> GzipHandler::compressStream(
    std::function<size_t(uint8_t* buffer, size_t maxSize)> readCallback
) {
    // Read all data first (for CRC calculation)
    std::vector<uint8_t> data;
    std::array<uint8_t, 32768> readBuf;
    
    while (true) {
        size_t bytesRead = readCallback(readBuf.data(), readBuf.size());
        if (bytesRead == 0) {
            break;
        }
        data.insert(data.end(), readBuf.begin(), readBuf.begin() + bytesRead);
    }
    
    return compress(data);
}

std::vector<uint8_t> GzipHandler::decompress(const std::vector<uint8_t>& data) {
    if (!isGzipData(data)) {
        lastError_ = "Not valid gzip data";
        return {};
    }
    
    std::vector<uint8_t> result;
    
    // Initialize inflate with gzip detection
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);  // 16 = gzip decoding
    if (ret != Z_OK) {
        lastError_ = "Failed to initialize inflate: " + std::to_string(ret);
        return {};
    }
    
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_in = static_cast<uInt>(data.size());
    
    std::array<uint8_t, 32768> outBuf;
    
    do {
        strm.next_out = outBuf.data();
        strm.avail_out = outBuf.size();
        
        ret = inflate(&strm, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || 
            ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            lastError_ = "Inflate error: " + std::to_string(ret);
            return {};
        }
        
        size_t have = outBuf.size() - strm.avail_out;
        result.insert(result.end(), outBuf.begin(), outBuf.begin() + have);
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&strm);
    return result;
}

bool GzipHandler::decompressStream(
    const std::vector<uint8_t>& data,
    std::function<bool(const uint8_t* buffer, size_t size)> writeCallback
) {
    if (!isGzipData(data)) {
        lastError_ = "Not valid gzip data";
        return false;
    }
    
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        lastError_ = "Failed to initialize inflate: " + std::to_string(ret);
        return false;
    }
    
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_in = static_cast<uInt>(data.size());
    
    std::array<uint8_t, 32768> outBuf;
    
    do {
        strm.next_out = outBuf.data();
        strm.avail_out = outBuf.size();
        
        ret = inflate(&strm, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || 
            ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            lastError_ = "Inflate error: " + std::to_string(ret);
            return false;
        }
        
        size_t have = outBuf.size() - strm.avail_out;
        if (have > 0) {
            if (!writeCallback(outBuf.data(), have)) {
                inflateEnd(&strm);
                lastError_ = "Write callback failed";
                return false;
            }
        }
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&strm);
    return true;
}

bool GzipHandler::isGzipData(const std::vector<uint8_t>& data) {
    return data.size() >= 2 && 
           data[0] == GZIP_MAGIC1 && 
           data[1] == GZIP_MAGIC2;
}

std::string GzipHandler::getLastError() {
    return lastError_;
}

} // namespace lgx
