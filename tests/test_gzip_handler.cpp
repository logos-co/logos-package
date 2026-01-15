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
