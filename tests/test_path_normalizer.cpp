#include <gtest/gtest.h>
#include "core/path_normalizer.h"

using namespace lgx;

// =============================================================================
// NFC Normalization Tests
// =============================================================================

TEST(PathNormalizerTest, NFCNormalization_ASCII) {
    auto result = PathNormalizer::toNFC("hello/world.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello/world.txt");
}

TEST(PathNormalizerTest, NFCNormalization_Unicode) {
    // Test with Unicode characters that are already NFC
    auto result = PathNormalizer::toNFC("héllo/wörld.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "héllo/wörld.txt");
}

TEST(PathNormalizerTest, NFCNormalization_NFDtoNFC) {
    // NFD: é as e + combining acute accent (U+0065 U+0301)
    // NFC: é as single character (U+00E9)
    std::string nfd = "e\xCC\x81";  // e + combining acute accent (UTF-8)
    std::string nfc = "\xC3\xA9";   // é as single character (UTF-8)
    
    auto result = PathNormalizer::toNFC(nfd);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, nfc);
}

TEST(PathNormalizerTest, IsNFC_True) {
    EXPECT_TRUE(PathNormalizer::isNFC("hello"));
    EXPECT_TRUE(PathNormalizer::isNFC("héllo"));  // NFC form
}

TEST(PathNormalizerTest, IsNFC_False) {
    // NFD form: e + combining acute
    std::string nfd = "e\xCC\x81";
    EXPECT_FALSE(PathNormalizer::isNFC(nfd));
}

// =============================================================================
// Security Validation Tests
// =============================================================================

TEST(PathNormalizerTest, ValidatePath_Valid) {
    auto result = PathNormalizer::validateArchivePath("variants/linux/lib.so");
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error.empty());
}

TEST(PathNormalizerTest, ValidatePath_RejectEmpty) {
    auto result = PathNormalizer::validateArchivePath("");
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.error.find("empty"), std::string::npos);
}

TEST(PathNormalizerTest, ValidatePath_RejectAbsolute) {
    auto result = PathNormalizer::validateArchivePath("/absolute/path");
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.error.find("absolute"), std::string::npos);
}

TEST(PathNormalizerTest, ValidatePath_RejectDotDot) {
    auto result = PathNormalizer::validateArchivePath("variants/../etc/passwd");
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.error.find(".."), std::string::npos);
}

TEST(PathNormalizerTest, ValidatePath_RejectBackslash) {
    auto result = PathNormalizer::validateArchivePath("variants\\windows\\file.dll");
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.error.find("backslash"), std::string::npos);
}

TEST(PathNormalizerTest, ValidatePath_RejectNonNFC) {
    // NFD form
    std::string nfd = "file_e\xCC\x81.txt";
    auto result = PathNormalizer::validateArchivePath(nfd);
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.error.find("NFC"), std::string::npos);
}

// =============================================================================
// Path Utility Tests
// =============================================================================

TEST(PathNormalizerTest, NormalizeSeparators) {
    EXPECT_EQ(PathNormalizer::normalizeSeparators("a\\b/c\\d"), "a/b/c/d");
    EXPECT_EQ(PathNormalizer::normalizeSeparators("a//b///c"), "a/b/c");
    EXPECT_EQ(PathNormalizer::normalizeSeparators("path/"), "path");
    EXPECT_EQ(PathNormalizer::normalizeSeparators("/root"), "/root");
}

TEST(PathNormalizerTest, ToLowercase_ASCII) {
    EXPECT_EQ(PathNormalizer::toLowercase("HELLO"), "hello");
    EXPECT_EQ(PathNormalizer::toLowercase("Hello World"), "hello world");
    EXPECT_EQ(PathNormalizer::toLowercase("Linux-AMD64"), "linux-amd64");
}

TEST(PathNormalizerTest, ToLowercase_Unicode) {
    // German uppercase ß -> ss (or ß depending on locale)
    // Simple test with accented characters
    EXPECT_EQ(PathNormalizer::toLowercase("HÉLLO"), "héllo");
}

TEST(PathNormalizerTest, JoinPath_Vector) {
    std::vector<std::string> components = {"variants", "linux", "lib.so"};
    EXPECT_EQ(PathNormalizer::joinPath(components), "variants/linux/lib.so");
    
    std::vector<std::string> empty = {};
    EXPECT_EQ(PathNormalizer::joinPath(empty), "");
    
    std::vector<std::string> single = {"file.txt"};
    EXPECT_EQ(PathNormalizer::joinPath(single), "file.txt");
}

TEST(PathNormalizerTest, JoinPath_TwoStrings) {
    EXPECT_EQ(PathNormalizer::joinPath("variants", "linux"), "variants/linux");
    EXPECT_EQ(PathNormalizer::joinPath("variants/", "linux"), "variants/linux");
    EXPECT_EQ(PathNormalizer::joinPath("", "linux"), "linux");
    EXPECT_EQ(PathNormalizer::joinPath("variants", ""), "variants");
}

TEST(PathNormalizerTest, Basename) {
    EXPECT_EQ(PathNormalizer::basename("path/to/file.txt"), "file.txt");
    EXPECT_EQ(PathNormalizer::basename("file.txt"), "file.txt");
    EXPECT_EQ(PathNormalizer::basename("/absolute/path/file"), "file");
    EXPECT_EQ(PathNormalizer::basename("path/to/dir/"), "dir");
}

TEST(PathNormalizerTest, Dirname) {
    EXPECT_EQ(PathNormalizer::dirname("path/to/file.txt"), "path/to");
    EXPECT_EQ(PathNormalizer::dirname("file.txt"), "");
    EXPECT_EQ(PathNormalizer::dirname("/absolute/path/file"), "/absolute/path");
    EXPECT_EQ(PathNormalizer::dirname("/root"), "/");
}

TEST(PathNormalizerTest, IsAbsolute) {
    EXPECT_TRUE(PathNormalizer::isAbsolute("/absolute/path"));
    EXPECT_TRUE(PathNormalizer::isAbsolute("/"));
    EXPECT_FALSE(PathNormalizer::isAbsolute("relative/path"));
    EXPECT_FALSE(PathNormalizer::isAbsolute(""));
    
    // Windows-style (if supported)
    EXPECT_TRUE(PathNormalizer::isAbsolute("C:\\Windows"));
    EXPECT_TRUE(PathNormalizer::isAbsolute("C:/Windows"));
}

TEST(PathNormalizerTest, SplitPath) {
    auto parts = PathNormalizer::splitPath("variants/linux/lib/file.so");
    ASSERT_EQ(parts.size(), 4);
    EXPECT_EQ(parts[0], "variants");
    EXPECT_EQ(parts[1], "linux");
    EXPECT_EQ(parts[2], "lib");
    EXPECT_EQ(parts[3], "file.so");
    
    // Should skip empty components and "."
    auto parts2 = PathNormalizer::splitPath("./path//to/./file");
    ASSERT_EQ(parts2.size(), 3);
    EXPECT_EQ(parts2[0], "path");
    EXPECT_EQ(parts2[1], "to");
    EXPECT_EQ(parts2[2], "file");
}

TEST(PathNormalizerTest, GetRootComponent) {
    EXPECT_EQ(PathNormalizer::getRootComponent("variants/linux/file"), "variants");
    EXPECT_EQ(PathNormalizer::getRootComponent("manifest.json"), "manifest.json");
    EXPECT_EQ(PathNormalizer::getRootComponent(""), "");
}
