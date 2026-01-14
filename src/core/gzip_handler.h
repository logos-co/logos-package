#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

namespace lgx {

/**
 * GzipHandler provides deterministic gzip compression and decompression.
 * 
 * Determinism is achieved by:
 * - Setting mtime=0 in gzip header
 * - Omitting original filename
 * - Using fixed OS byte (0xFF = unknown)
 */
class GzipHandler {
public:
    /**
     * Compress data using deterministic gzip settings.
     * 
     * @param data Input data to compress
     * @return Compressed data in gzip format, or empty vector on failure
     */
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    
    /**
     * Compress data using deterministic gzip settings with streaming input.
     * 
     * @param readCallback Function that fills buffer and returns bytes read (0 = EOF)
     * @return Compressed data in gzip format, or empty vector on failure
     */
    static std::vector<uint8_t> compressStream(
        std::function<size_t(uint8_t* buffer, size_t maxSize)> readCallback
    );
    
    /**
     * Decompress gzip data.
     * 
     * @param data Gzip compressed data
     * @return Decompressed data, or empty vector on failure
     */
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
    
    /**
     * Decompress gzip data with streaming output.
     * 
     * @param data Gzip compressed data
     * @param writeCallback Function that receives decompressed chunks
     * @return true on success, false on failure
     */
    static bool decompressStream(
        const std::vector<uint8_t>& data,
        std::function<bool(const uint8_t* buffer, size_t size)> writeCallback
    );
    
    /**
     * Check if data appears to be gzip compressed (magic bytes check).
     */
    static bool isGzipData(const std::vector<uint8_t>& data);
    
    /**
     * Get the last error message (if any operation failed).
     */
    static std::string getLastError();

private:
    static thread_local std::string lastError_;
    
    // Gzip header constants for determinism
    static constexpr uint8_t GZIP_MAGIC1 = 0x1f;
    static constexpr uint8_t GZIP_MAGIC2 = 0x8b;
    static constexpr uint8_t COMPRESSION_DEFLATE = 8;
    static constexpr uint8_t FLAGS_NONE = 0;
    static constexpr uint8_t OS_UNKNOWN = 0xff;  // Unknown OS for determinism
};

} // namespace lgx
