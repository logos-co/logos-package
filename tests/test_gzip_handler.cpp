#include <gtest/gtest.h>
#include "core/gzip_handler.h"

using namespace lgx;

// =============================================================================
// Compression/Decompression Roundtrip Tests
// =============================================================================

TEST(GzipHandlerTest, Roundtrip_SimpleData) {
    std::vector<uint8_t> original = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    
    auto compressed = GzipHandler::compress(original);
    ASSERT_FALSE(compressed.empty());
    EXPECT_TRUE(GzipHandler::isGzipData(compressed));
    
    auto decompressed = GzipHandler::decompress(compressed);
    EXPECT_EQ(decompressed, original);
}

TEST(GzipHandlerTest, Roundtrip_LargeData) {
    // Create 1MB of data
    std::vector<uint8_t> original(1024 * 1024);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>(i % 256);
    }
    
    auto compressed = GzipHandler::compress(original);
    ASSERT_FALSE(compressed.empty());
    
    // Compression should reduce size for patterned data
    EXPECT_LT(compressed.size(), original.size());
    
    auto decompressed = GzipHandler::decompress(compressed);
    EXPECT_EQ(decompressed, original);
}

TEST(GzipHandlerTest, Roundtrip_BinaryData) {
    // Binary data with null bytes
    std::vector<uint8_t> original = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0x00, 0x00, 0xAB};
    
    auto compressed = GzipHandler::compress(original);
    ASSERT_FALSE(compressed.empty());
    
    auto decompressed = GzipHandler::decompress(compressed);
    EXPECT_EQ(decompressed, original);
}

// =============================================================================
// Empty Data Handling
// =============================================================================

TEST(GzipHandlerTest, EmptyData_Compress) {
    std::vector<uint8_t> empty;
    
    auto compressed = GzipHandler::compress(empty);
    // Should produce valid gzip (with empty content)
    EXPECT_TRUE(GzipHandler::isGzipData(compressed));
}

TEST(GzipHandlerTest, EmptyData_Roundtrip) {
    std::vector<uint8_t> empty;
    
    auto compressed = GzipHandler::compress(empty);
    auto decompressed = GzipHandler::decompress(compressed);
    
    EXPECT_TRUE(decompressed.empty());
}

// =============================================================================
// Determinism Tests
// =============================================================================

TEST(GzipHandlerTest, Determinism_SameInput) {
    std::vector<uint8_t> data = {'T', 'e', 's', 't', ' ', 'd', 'a', 't', 'a'};
    
    auto compressed1 = GzipHandler::compress(data);
    auto compressed2 = GzipHandler::compress(data);
    
    // Same input MUST produce identical output
    EXPECT_EQ(compressed1, compressed2);
}

TEST(GzipHandlerTest, Determinism_MultipleRuns) {
    // More complex data
    std::vector<uint8_t> data(10000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }
    
    auto compressed1 = GzipHandler::compress(data);
    auto compressed2 = GzipHandler::compress(data);
    auto compressed3 = GzipHandler::compress(data);
    
    EXPECT_EQ(compressed1, compressed2);
    EXPECT_EQ(compressed2, compressed3);
}

TEST(GzipHandlerTest, Determinism_HeaderFormat) {
    std::vector<uint8_t> data = {'X'};
    auto compressed = GzipHandler::compress(data);
    
    // Check gzip header format
    ASSERT_GE(compressed.size(), 10);
    
    // Magic bytes
    EXPECT_EQ(compressed[0], 0x1f);
    EXPECT_EQ(compressed[1], 0x8b);
    
    // Compression method (deflate)
    EXPECT_EQ(compressed[2], 8);
    
    // Flags (should be 0 - no extra fields)
    EXPECT_EQ(compressed[3], 0);
    
    // Mtime should be 0 (bytes 4-7)
    EXPECT_EQ(compressed[4], 0);
    EXPECT_EQ(compressed[5], 0);
    EXPECT_EQ(compressed[6], 0);
    EXPECT_EQ(compressed[7], 0);
    
    // OS byte (we use 0xFF for unknown)
    EXPECT_EQ(compressed[9], 0xFF);
}

// =============================================================================
// Invalid Data Handling
// =============================================================================

TEST(GzipHandlerTest, IsGzipData_ValidMagic) {
    std::vector<uint8_t> valid = {0x1f, 0x8b, 0x08, 0x00};
    EXPECT_TRUE(GzipHandler::isGzipData(valid));
}

TEST(GzipHandlerTest, IsGzipData_InvalidMagic) {
    std::vector<uint8_t> invalid = {0x50, 0x4b, 0x03, 0x04};  // ZIP magic
    EXPECT_FALSE(GzipHandler::isGzipData(invalid));
}

TEST(GzipHandlerTest, IsGzipData_TooShort) {
    std::vector<uint8_t> tooShort = {0x1f};
    EXPECT_FALSE(GzipHandler::isGzipData(tooShort));
    
    std::vector<uint8_t> empty;
    EXPECT_FALSE(GzipHandler::isGzipData(empty));
}

TEST(GzipHandlerTest, Decompress_InvalidData) {
    std::vector<uint8_t> garbage = {'n', 'o', 't', ' ', 'g', 'z', 'i', 'p'};
    
    auto result = GzipHandler::decompress(garbage);
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(GzipHandler::getLastError().empty());
}

TEST(GzipHandlerTest, Decompress_TruncatedData) {
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    auto compressed = GzipHandler::compress(data);
    
    // Truncate the compressed data
    compressed.resize(compressed.size() / 2);
    
    auto result = GzipHandler::decompress(compressed);
    // Should fail gracefully
    EXPECT_TRUE(result.empty());
}

// =============================================================================
// Stream Decompression
// =============================================================================

TEST(GzipHandlerTest, DecompressStream) {
    std::vector<uint8_t> original = {'S', 't', 'r', 'e', 'a', 'm', ' ', 't', 'e', 's', 't'};
    auto compressed = GzipHandler::compress(original);

    std::vector<uint8_t> result;
    bool success = GzipHandler::decompressStream(compressed,
        [&result](const uint8_t* buffer, size_t size) {
            result.insert(result.end(), buffer, buffer + size);
            return true;
        });

    EXPECT_TRUE(success);
    EXPECT_EQ(result, original);
}

// =============================================================================
// Decompression Bomb Protection (F-007)
//
// A small, highly-compressible gzip stream can inflate to gigabytes. The
// decompress paths must enforce a hard cap on total output and reject the
// stream once it is exceeded, instead of growing an unbounded in-memory buffer
// and OOM-killing the host process.
// =============================================================================

// Build a "bomb": a small gzip stream that decompresses to `decompressedSize`
// bytes of zeros (DEFLATE achieves ~1000:1 on a run of identical bytes).
static std::vector<uint8_t> makeZeroBomb(size_t decompressedSize) {
    std::vector<uint8_t> zeros(decompressedSize, 0);
    return GzipHandler::compress(zeros);
}

TEST(GzipHandlerTest, Decompress_RejectsBombExceedingCap) {
    // 64 MiB of zeros compresses to a handful of KiB on disk...
    const size_t bombSize = 64 * 1024 * 1024;
    auto bomb = makeZeroBomb(bombSize);
    ASSERT_FALSE(bomb.empty());
    // ...the on-disk archive is tiny relative to its decompressed size.
    EXPECT_LT(bomb.size(), bombSize / 100);

    // Decompress with a 1 MiB cap: the loop must bail out long before
    // materializing the full 64 MiB, and must not return the payload.
    const size_t cap = 1 * 1024 * 1024;
    auto result = GzipHandler::decompress(bomb, cap);

    EXPECT_TRUE(result.empty());
    EXPECT_LE(result.size(), cap);
    EXPECT_FALSE(GzipHandler::getLastError().empty());
}

TEST(GzipHandlerTest, Decompress_AllowsOutputUpToCap) {
    // Data that decompresses to just under the cap must still succeed.
    std::vector<uint8_t> original(64 * 1024);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>((i * 31 + 7) % 256);
    }
    auto compressed = GzipHandler::compress(original);

    auto result = GzipHandler::decompress(compressed, 1 * 1024 * 1024);
    EXPECT_EQ(result, original);
}

TEST(GzipHandlerTest, Decompress_DefaultCapAllowsNormalData) {
    // The default cap must not interfere with ordinary, legitimately-sized data.
    std::vector<uint8_t> original(2 * 1024 * 1024);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>(i % 256);
    }
    auto compressed = GzipHandler::compress(original);

    auto result = GzipHandler::decompress(compressed);  // default cap
    EXPECT_EQ(result, original);
}

TEST(GzipHandlerTest, DecompressStream_RejectsBombExceedingCap) {
    const size_t bombSize = 64 * 1024 * 1024;
    auto bomb = makeZeroBomb(bombSize);
    ASSERT_FALSE(bomb.empty());

    const size_t cap = 1 * 1024 * 1024;
    size_t written = 0;
    bool success = GzipHandler::decompressStream(bomb,
        [&written](const uint8_t*, size_t size) {
            written += size;
            return true;
        },
        cap);

    EXPECT_FALSE(success);
    // The cap bounds how much is ever handed to the write callback.
    EXPECT_LE(written, cap);
    EXPECT_FALSE(GzipHandler::getLastError().empty());
}

TEST(GzipHandlerTest, DecompressStream_AllowsOutputUpToCap) {
    std::vector<uint8_t> original(64 * 1024);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>((i * 17 + 3) % 256);
    }
    auto compressed = GzipHandler::compress(original);

    std::vector<uint8_t> result;
    bool success = GzipHandler::decompressStream(compressed,
        [&result](const uint8_t* buffer, size_t size) {
            result.insert(result.end(), buffer, buffer + size);
            return true;
        },
        1 * 1024 * 1024);

    EXPECT_TRUE(success);
    EXPECT_EQ(result, original);
}

// =============================================================================
// Configurable Library-Wide Default Cap
//
// The decompression limit is also configurable for the whole library via
// GzipHandler::setDefaultMaxDecompressedSize(). Calls that pass no explicit
// maxOutputSize (including the .lgx load path) must honor the configured value.
//
// These tests mutate process-global state, so the fixture restores the factory
// default after each one to avoid leaking the setting into other tests.
// =============================================================================

class GzipDefaultCapTest : public ::testing::Test {
protected:
    void TearDown() override {
        GzipHandler::setDefaultMaxDecompressedSize(
            GzipHandler::DEFAULT_MAX_DECOMPRESSED_SIZE);
    }
};

TEST_F(GzipDefaultCapTest, FactoryDefaultIsOneGiB) {
    EXPECT_EQ(GzipHandler::getDefaultMaxDecompressedSize(),
              GzipHandler::DEFAULT_MAX_DECOMPRESSED_SIZE);
    EXPECT_EQ(GzipHandler::DEFAULT_MAX_DECOMPRESSED_SIZE,
              1024ull * 1024 * 1024);
}

TEST_F(GzipDefaultCapTest, SetAndGetRoundtrip) {
    GzipHandler::setDefaultMaxDecompressedSize(4 * 1024 * 1024);
    EXPECT_EQ(GzipHandler::getDefaultMaxDecompressedSize(), 4u * 1024 * 1024);
}

TEST_F(GzipDefaultCapTest, ConfiguredDefaultIsEnforced) {
    // Lower the library-wide default below the bomb's decompressed size...
    GzipHandler::setDefaultMaxDecompressedSize(1 * 1024 * 1024);

    auto bomb = makeZeroBomb(64 * 1024 * 1024);
    ASSERT_FALSE(bomb.empty());

    // ...and decompress WITHOUT an explicit cap: the configured default applies.
    auto result = GzipHandler::decompress(bomb);

    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(GzipHandler::getLastError().empty());
}

TEST_F(GzipDefaultCapTest, RaisedDefaultAllowsLargerData) {
    // Data larger than a deliberately-low base, allowed once the default is raised.
    std::vector<uint8_t> original(4 * 1024 * 1024);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>((i * 13 + 5) % 256);
    }
    auto compressed = GzipHandler::compress(original);

    // Default too low: rejected.
    GzipHandler::setDefaultMaxDecompressedSize(1 * 1024 * 1024);
    EXPECT_TRUE(GzipHandler::decompress(compressed).empty());

    // Raise the default above the payload: now accepted.
    GzipHandler::setDefaultMaxDecompressedSize(16 * 1024 * 1024);
    EXPECT_EQ(GzipHandler::decompress(compressed), original);
}

TEST_F(GzipDefaultCapTest, ExplicitArgOverridesConfiguredDefault) {
    // A low library default must not override a deliberately-higher per-call cap.
    GzipHandler::setDefaultMaxDecompressedSize(1024);  // 1 KiB

    std::vector<uint8_t> original(256 * 1024);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>((i * 7 + 1) % 256);
    }
    auto compressed = GzipHandler::compress(original);

    // Explicit cap is honored regardless of the (lower) configured default.
    auto result = GzipHandler::decompress(compressed, 1 * 1024 * 1024);
    EXPECT_EQ(result, original);
}

TEST_F(GzipDefaultCapTest, ZeroIsRejectedAndLeavesProtectionIntact) {
    // 0 is the USE_DEFAULT_MAX sentinel and must never become the stored cap,
    // otherwise it would silently disable bomb protection.
    GzipHandler::setDefaultMaxDecompressedSize(2 * 1024 * 1024);
    GzipHandler::setDefaultMaxDecompressedSize(0);  // must be ignored

    EXPECT_EQ(GzipHandler::getDefaultMaxDecompressedSize(), 2u * 1024 * 1024);

    // And the bomb is still rejected under the still-effective limit.
    auto bomb = makeZeroBomb(64 * 1024 * 1024);
    ASSERT_FALSE(bomb.empty());
    EXPECT_TRUE(GzipHandler::decompress(bomb).empty());
}
