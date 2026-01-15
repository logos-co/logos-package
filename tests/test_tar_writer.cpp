#include <gtest/gtest.h>
#include "core/tar_writer.h"
#include "core/tar_reader.h"

using namespace lgx;

// =============================================================================
// Basic Entry Tests
// =============================================================================

TEST(TarWriterTest, AddFile_Simple) {
    DeterministicTarWriter writer;
    
    std::vector<uint8_t> content = {'H', 'e', 'l', 'l', 'o'};
    writer.addFile("test.txt", content);
    
    EXPECT_EQ(writer.entryCount(), 1);
    
    auto tarData = writer.finalize();
    EXPECT_FALSE(tarData.empty());
    EXPECT_TRUE(TarReader::isValidTar(tarData));
}

TEST(TarWriterTest, AddFile_FromString) {
    DeterministicTarWriter writer;
    
    writer.addFile("readme.txt", "Hello World");
    
    auto tarData = writer.finalize();
    
    // Read back and verify content
    auto fileData = TarReader::readFile(tarData, "readme.txt");
    ASSERT_TRUE(fileData.has_value());
    
    std::string content(fileData->begin(), fileData->end());
    EXPECT_EQ(content, "Hello World");
}

TEST(TarWriterTest, AddDirectory) {
    DeterministicTarWriter writer;
    
    writer.addDirectory("variants");
    writer.addDirectory("variants/linux");
    writer.addFile("variants/linux/lib.so", "binary content");
    
    auto tarData = writer.finalize();
    
    auto result = TarReader::read(tarData);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.entries.size(), 3);
}

TEST(TarWriterTest, AddEntry) {
    DeterministicTarWriter writer;
    
    TarEntry entry;
    entry.path = "custom/path/file.bin";
    entry.data = {0x00, 0x01, 0x02, 0x03};
    entry.isDirectory = false;
    
    writer.addEntry(entry);
    
    auto tarData = writer.finalize();
    
    auto fileData = TarReader::readFile(tarData, "custom/path/file.bin");
    ASSERT_TRUE(fileData.has_value());
    EXPECT_EQ(*fileData, entry.data);
}

// =============================================================================
// Determinism Tests
// =============================================================================

TEST(TarWriterTest, Determinism_SameEntries) {
    auto createTar = []() {
        DeterministicTarWriter writer;
        writer.addFile("file1.txt", "content1");
        writer.addFile("file2.txt", "content2");
        writer.addDirectory("dir");
        return writer.finalize();
    };
    
    auto tar1 = createTar();
    auto tar2 = createTar();
    
    EXPECT_EQ(tar1, tar2);
}

TEST(TarWriterTest, Determinism_DifferentOrder) {
    // Entries added in different order should produce same output (sorted)
    DeterministicTarWriter writer1;
    writer1.addFile("zzz.txt", "last");
    writer1.addFile("aaa.txt", "first");
    writer1.addFile("mmm.txt", "middle");
    auto tar1 = writer1.finalize();
    
    DeterministicTarWriter writer2;
    writer2.addFile("aaa.txt", "first");
    writer2.addFile("mmm.txt", "middle");
    writer2.addFile("zzz.txt", "last");
    auto tar2 = writer2.finalize();
    
    EXPECT_EQ(tar1, tar2);
}

TEST(TarWriterTest, Determinism_SortedEntries) {
    DeterministicTarWriter writer;
    writer.addFile("c.txt", "c");
    writer.addFile("a.txt", "a");
    writer.addFile("b.txt", "b");
    
    auto tarData = writer.finalize();
    
    // Read entries and verify order
    auto entries = TarReader::readInfo(tarData);
    ASSERT_GE(entries.size(), 3);
    
    // Should be sorted: a.txt, b.txt, c.txt
    EXPECT_EQ(entries[0].path, "a.txt");
    EXPECT_EQ(entries[1].path, "b.txt");
    EXPECT_EQ(entries[2].path, "c.txt");
}

// =============================================================================
// Fixed Metadata Tests
// =============================================================================

TEST(TarWriterTest, FixedMetadata) {
    DeterministicTarWriter writer;
    writer.addFile("test.txt", "content");
    writer.addDirectory("testdir");
    
    auto tarData = writer.finalize();
    auto entries = TarReader::readInfo(tarData);
    
    for (const auto& entry : entries) {
        // UID and GID should be 0
        EXPECT_EQ(entry.uid, 0);
        EXPECT_EQ(entry.gid, 0);
        
        // Mtime should be 0
        EXPECT_EQ(entry.mtime, 0);
        
        // Mode should be 0644 for files, 0755 for directories
        if (entry.isDirectory) {
            EXPECT_EQ(entry.mode, 0755);
        } else {
            EXPECT_EQ(entry.mode, 0644);
        }
    }
}

// =============================================================================
// Roundtrip Tests
// =============================================================================

TEST(TarWriterTest, Roundtrip_MultipleFiles) {
    DeterministicTarWriter writer;
    
    std::map<std::string, std::vector<uint8_t>> files = {
        {"file1.txt", {'a', 'b', 'c'}},
        {"dir/file2.bin", {0x00, 0x01, 0x02, 0xFF}},
        {"dir/subdir/file3.dat", {'x', 'y', 'z'}}
    };
    
    for (const auto& [path, data] : files) {
        writer.addFile(path, data);
    }
    
    auto tarData = writer.finalize();
    auto result = TarReader::read(tarData);
    
    ASSERT_TRUE(result.success);
    
    // Verify each file
    for (const auto& [path, expectedData] : files) {
        auto fileData = TarReader::readFile(tarData, path);
        ASSERT_TRUE(fileData.has_value()) << "File not found: " << path;
        EXPECT_EQ(*fileData, expectedData) << "Content mismatch for: " << path;
    }
}

TEST(TarWriterTest, Roundtrip_LargeFile) {
    DeterministicTarWriter writer;
    
    // 100KB file
    std::vector<uint8_t> largeData(100 * 1024);
    for (size_t i = 0; i < largeData.size(); ++i) {
        largeData[i] = static_cast<uint8_t>(i % 256);
    }
    
    writer.addFile("large.bin", largeData);
    
    auto tarData = writer.finalize();
    auto result = TarReader::readFile(tarData, "large.bin");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, largeData);
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST(TarWriterTest, Clear) {
    DeterministicTarWriter writer;
    
    writer.addFile("file1.txt", "content1");
    writer.addFile("file2.txt", "content2");
    EXPECT_EQ(writer.entryCount(), 2);
    
    writer.clear();
    EXPECT_EQ(writer.entryCount(), 0);
    
    writer.addFile("file3.txt", "content3");
    EXPECT_EQ(writer.entryCount(), 1);
    
    auto tarData = writer.finalize();
    auto result = TarReader::read(tarData);
    
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.entries.size(), 1);
    EXPECT_EQ(result.entries[0].path, "file3.txt");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(TarWriterTest, EmptyFile) {
    DeterministicTarWriter writer;
    writer.addFile("empty.txt", std::vector<uint8_t>{});
    
    auto tarData = writer.finalize();
    auto result = TarReader::readFile(tarData, "empty.txt");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(TarWriterTest, DeepDirectory) {
    DeterministicTarWriter writer;
    writer.addFile("a/b/c/d/e/f/g/deep.txt", "deep content");
    
    auto tarData = writer.finalize();
    auto result = TarReader::readFile(tarData, "a/b/c/d/e/f/g/deep.txt");
    
    ASSERT_TRUE(result.has_value());
    std::string content(result->begin(), result->end());
    EXPECT_EQ(content, "deep content");
}
