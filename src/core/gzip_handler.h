#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <functional>
#include <atomic>

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
     * Factory default hard cap on decompressed output size (1 GiB).
     *
     * Decompression refuses to produce more than this many bytes from a single
     * gzip stream. This bounds the damage from a "decompression bomb" — a small,
     * highly-compressible archive (DEFLATE can reach ~1000:1) that would
     * otherwise inflate to gigabytes and exhaust host memory.
     *
     * This is the built-in default; the effective limit can be changed at
     * runtime for the whole library via setDefaultMaxDecompressedSize(), or
     * per call via the maxOutputSize argument to decompress() /
     * decompressStream().
     */
    static constexpr size_t DEFAULT_MAX_DECOMPRESSED_SIZE = 1024ull * 1024 * 1024;

    /**
     * Sentinel for the maxOutputSize arguments below: use the library-wide
     * configured default (see getDefaultMaxDecompressedSize()).
     */
    static constexpr size_t USE_DEFAULT_MAX = 0;

    /**
     * Set the library-wide default cap on decompressed output size, in bytes.
     *
     * Applies to every subsequent decompress() / decompressStream() call that
     * does not pass an explicit maxOutputSize (including the untrusted .lgx
     * load path in Package::load()). Thread-safe. Passing 0 is rejected and
     * leaves the current value unchanged — there is no "unlimited" setting, by
     * design, so a misconfiguration cannot silently disable bomb protection.
     *
     * @param maxBytes New default limit in bytes (must be > 0)
     */
    static void setDefaultMaxDecompressedSize(size_t maxBytes);

    /**
     * Get the current library-wide default cap on decompressed output size.
     *
     * Returns DEFAULT_MAX_DECOMPRESSED_SIZE unless changed via
     * setDefaultMaxDecompressedSize().
     */
    static size_t getDefaultMaxDecompressedSize();

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
     * Enforces a hard cap on total decompressed output to defend against
     * decompression bombs: if the inflated stream would exceed maxOutputSize,
     * decompression aborts and an empty vector is returned (getLastError()
     * reports the limit). This guards the untrusted .lgx load path, where a
     * tiny archive could otherwise inflate to gigabytes and OOM the host.
     *
     * @param data Gzip compressed data
     * @param maxOutputSize Maximum decompressed bytes to produce before
     *        rejecting the stream. Defaults to USE_DEFAULT_MAX, meaning the
     *        library-wide limit from getDefaultMaxDecompressedSize().
     * @return Decompressed data, or empty vector on failure / cap exceeded
     */
    static std::vector<uint8_t> decompress(
        const std::vector<uint8_t>& data,
        size_t maxOutputSize = USE_DEFAULT_MAX
    );

    /**
     * Decompress gzip data with streaming output.
     *
     * Like decompress(), this enforces maxOutputSize as a running total across
     * all chunks handed to writeCallback; the stream is rejected (returns false)
     * once the cap is exceeded, bounding memory even when the caller streams
     * output to disk.
     *
     * @param data Gzip compressed data
     * @param writeCallback Function that receives decompressed chunks
     * @param maxOutputSize Maximum total decompressed bytes before rejecting
     *        the stream. Defaults to USE_DEFAULT_MAX, meaning the library-wide
     *        limit from getDefaultMaxDecompressedSize().
     * @return true on success, false on failure / cap exceeded
     */
    static bool decompressStream(
        const std::vector<uint8_t>& data,
        std::function<bool(const uint8_t* buffer, size_t size)> writeCallback,
        size_t maxOutputSize = USE_DEFAULT_MAX
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

    // Library-wide configurable cap, initialized to the factory default.
    static std::atomic<size_t> defaultMaxDecompressedSize_;

    // Gzip header constants for determinism
    static constexpr uint8_t GZIP_MAGIC1 = 0x1f;
    static constexpr uint8_t GZIP_MAGIC2 = 0x8b;
    static constexpr uint8_t COMPRESSION_DEFLATE = 8;
    static constexpr uint8_t FLAGS_NONE = 0;
    static constexpr uint8_t OS_UNKNOWN = 0xff;  // Unknown OS for determinism
};

} // namespace lgx
