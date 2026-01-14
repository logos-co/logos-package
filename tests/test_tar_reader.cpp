#include <gtest/gtest.h>
#include "core/tar_reader.h"
#include "core/tar_writer.h"

using namespace lgx;

// Helper function to create a test tar
static std::vector<uint8_t> createTestTar() {
    DeterministicTarWriter writer;
    writer.addDirectory("variants");
    writer.addDirectory("variants/linux");
    writer.addFile("variants/linux/lib.so", "library content");
    writer.addFile("manifest.json", "{\"name\": \"test\"}");
    return writer.finalize();
}

// =============================================================================
// Read Tests
// =============================================================================

TEST(TarReaderTest, Read_ValidTar) {
    auto tarData = createTestTar();
    
    auto result = TarReader::read(tarData);
    
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.entries.empty());
}

TEST(TarReaderTest, Read_EntriesContent) {
    auto tarData = createTestTar();
    
    auto result = TarReader::read(tarData);
    ASSERT_TRUE(result.success);
    
    // Find the manifest entry
    bool foundManifest = false;
    for (const auto& entry : result.entries) {
        if (entry.path == "manifest.json") {
            foundManifest = true;
            EXPECT_FALSE(entry.isDirectory);
            std::string content(entry.data.begin(), entry.data.end());
            EXPECT_EQ(content, "{\"name\": \"test\"}");
        }
    }
    EXPECT_TRUE(foundManifest);
}

TEST(TarReaderTest, Read_DirectoryEntries) {
    auto tarData = createTestTar();
    
    auto result = TarReader::read(tarData);
    ASSERT_TRUE(result.success);
    
    int dirCount = 0;
    for (const auto& entry : result.entries) {
        if (entry.isDirectory) {
            ++dirCount;
            EXPECT_TRUE(entry.data.empty());  // Directories have no data
        }
    }
    EXPECT_GE(dirCount, 2);  // At least variants and variants/linux
}

// =============================================================================
// ReadInfo Tests
// =============================================================================

TEST(TarReaderTest, ReadInfo_MetadataOnly) {
    auto tarData = createTestTar();
    
    auto entries = TarReader::readInfo(tarData);
    
    EXPECT_FALSE(entries.empty());
    
    for (const auto& info : entries) {
        EXPECT_FALSE(info.path.empty());
        // Check deterministic metadata
        EXPECT_EQ(info.uid, 0);
        EXPECT_EQ(info.gid, 0);
        EXPECT_EQ(info.mtime, 0);
    }
}

TEST(TarReaderTest, ReadInfo_TypeFlags) {
    auto tarData = createTestTar();
    
    auto entries = TarReader::readInfo(tarData);
    
    for (const auto& info : entries) {
        if (info.isDirectory) {
            EXPECT_EQ(info.typeFlag, '5');
            EXPECT_FALSE(info.isRegularFile);
        } else if (info.isRegularFile) {
            EXPECT_TRUE(info.typeFlag == '0' || info.typeFlag == '\0');
            EXPECT_FALSE(info.isDirectory);
        }
    }
}

// =============================================================================
// ReadFile Tests
// =============================================================================

TEST(TarReaderTest, ReadFile_ExistingFile) {
    auto tarData = createTestTar();
    
    auto result = TarReader::readFile(tarData, "manifest.json");
    
    ASSERT_TRUE(result.has_value());
    std::string content(result->begin(), result->end());
    EXPECT_EQ(content, "{\"name\": \"test\"}");
}

TEST(TarReaderTest, ReadFile_NestedFile) {
    auto tarData = createTestTar();
    
    auto result = TarReader::readFile(tarData, "variants/linux/lib.so");
    
    ASSERT_TRUE(result.has_value());
    std::string content(result->begin(), result->end());
    EXPECT_EQ(content, "library content");
}

TEST(TarReaderTest, ReadFile_NonExistent) {
    auto tarData = createTestTar();
    
    auto result = TarReader::readFile(tarData, "nonexistent.txt");
    
    EXPECT_FALSE(result.has_value());
}

TEST(TarReaderTest, ReadFile_NormalizedPath) {
    auto tarData = createTestTar();
    
    // Should work with leading slash stripped
    auto result1 = TarReader::readFile(tarData, "/manifest.json");
    // Note: Implementation may or may not handle this
    
    // Should work with trailing slash stripped (for directories)
    auto result2 = TarReader::readFile(tarData, "manifest.json/");
    // Note: This would not find a file
}

// =============================================================================
// Iterate Tests
// =============================================================================

TEST(TarReaderTest, Iterate_AllEntries) {
    auto tarData = createTestTar();
    
    std::vector<std::string> paths;
    bool success = TarReader::iterate(tarData, [&paths](const TarEntry& entry) {
        paths.push_back(entry.path);
        return true;  // Continue iteration
    });
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(paths.empty());
}

TEST(TarReaderTest, Iterate_StopEarly) {
    auto tarData = createTestTar();
    
    int count = 0;
    TarReader::iterate(tarData, [&count](const TarEntry& entry) {
        ++count;
        return count < 2;  // Stop after 2 entries
    });
    
    EXPECT_EQ(count, 2);
}

// =============================================================================
// Validity Tests
// =============================================================================

TEST(TarReaderTest, IsValidTar_Valid) {
    auto tarData = createTestTar();
    EXPECT_TRUE(TarReader::isValidTar(tarData));
}

TEST(TarReaderTest, IsValidTar_Empty) {
    std::vector<uint8_t> empty;
    EXPECT_FALSE(TarReader::isValidTar(empty));
}

TEST(TarReaderTest, IsValidTar_TooSmall) {
    std::vector<uint8_t> small(100, 0);
    EXPECT_FALSE(TarReader::isValidTar(small));
}

TEST(TarReaderTest, IsValidTar_InvalidChecksum) {
    auto tarData = createTestTar();
    
    // Corrupt a byte in the header (but not the magic)
    if (tarData.size() > 50) {
        tarData[50] ^= 0xFF;
    }
    
    EXPECT_FALSE(TarReader::isValidTar(tarData));
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST(TarReaderTest, Read_EmptyData) {
    std::vector<uint8_t> empty;
    
    auto result = TarReader::read(empty);
    
    // Should handle gracefully
    EXPECT_TRUE(result.entries.empty());
}

TEST(TarReaderTest, Read_GarbageData) {
    std::vector<uint8_t> garbage = {'n', 'o', 't', ' ', 'a', ' ', 't', 'a', 'r'};
    
    auto result = TarReader::read(garbage);
    
    // Should fail gracefully
    EXPECT_TRUE(result.entries.empty());
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(TarReaderTest, EmptyFileEntry) {
    DeterministicTarWriter writer;
    writer.addFile("empty.txt", std::vector<uint8_t>{});
    auto tarData = writer.finalize();
    
    auto result = TarReader::readFile(tarData, "empty.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(TarReaderTest, MultipleFilesWithSamePrefix) {
    DeterministicTarWriter writer;
    writer.addFile("file.txt", "file");
    writer.addFile("file.txt.bak", "backup");
    writer.addFile("file.txtx", "extended");
    auto tarData = writer.finalize();
    
    auto result1 = TarReader::readFile(tarData, "file.txt");
    auto result2 = TarReader::readFile(tarData, "file.txt.bak");
    auto result3 = TarReader::readFile(tarData, "file.txtx");
    
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result3.has_value());
    
    EXPECT_EQ(std::string(result1->begin(), result1->end()), "file");
    EXPECT_EQ(std::string(result2->begin(), result2->end()), "backup");
    EXPECT_EQ(std::string(result3->begin(), result3->end()), "extended");
}
