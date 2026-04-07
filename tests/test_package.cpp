#include <gtest/gtest.h>
#include "core/package.h"
#include "crypto/signing.h"
#include "crypto/keyring.h"

#include <filesystem>
#include <fstream>

using namespace lgx;
namespace fs = std::filesystem;

// Test fixture with temp directory management
class PackageTest : public ::testing::Test {
protected:
    fs::path tempDir;
    
    void SetUp() override {
        // Create a unique temp directory for each test
        tempDir = fs::temp_directory_path() / ("lgx_test_" + std::to_string(rand()));
        fs::create_directories(tempDir);
    }
    
    void TearDown() override {
        // Clean up temp directory
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }
    
    // Helper to create a test file
    void createTestFile(const fs::path& path, const std::string& content) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << content;
    }
    
    // Helper to create a test directory with files
    void createTestDirectory(const fs::path& dir, 
                            const std::map<std::string, std::string>& files) {
        fs::create_directories(dir);
        for (const auto& [name, content] : files) {
            createTestFile(dir / name, content);
        }
    }
};

// =============================================================================
// Package Creation Tests
// =============================================================================

TEST_F(PackageTest, Create_SkeletonPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    
    auto result = Package::create(pkgPath, "testpkg");
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(pkgPath));
}

TEST_F(PackageTest, Create_NormalizesName) {
    fs::path pkgPath = tempDir / "test.lgx";
    
    Package::create(pkgPath, "MyPackage");
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    EXPECT_EQ(pkg->getManifest().name, "mypackage");
}

TEST_F(PackageTest, Create_ValidStructure) {
    fs::path pkgPath = tempDir / "test.lgx";
    
    Package::create(pkgPath, "testpkg");
    
    auto verifyResult = Package::verify(pkgPath);
    EXPECT_TRUE(verifyResult.valid) << "Errors: " << 
        (verifyResult.errors.empty() ? "none" : verifyResult.errors[0]);
}

// =============================================================================
// Package Loading Tests
// =============================================================================

TEST_F(PackageTest, Load_ValidPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    auto pkg = Package::load(pkgPath);
    
    ASSERT_TRUE(pkg.has_value());
    EXPECT_EQ(pkg->getManifest().name, "testpkg");
}

TEST_F(PackageTest, Load_NonExistent) {
    fs::path pkgPath = tempDir / "nonexistent.lgx";
    
    auto pkg = Package::load(pkgPath);
    
    EXPECT_FALSE(pkg.has_value());
}

TEST_F(PackageTest, Load_InvalidFile) {
    fs::path pkgPath = tempDir / "invalid.lgx";
    std::ofstream file(pkgPath);
    file << "not a valid lgx file";
    file.close();
    
    auto pkg = Package::load(pkgPath);
    
    EXPECT_FALSE(pkg.has_value());
}

// =============================================================================
// Add Single File Variant Tests
// =============================================================================

TEST_F(PackageTest, AddVariant_SingleFile) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Create test file
    fs::path testFile = tempDir / "lib.so";
    createTestFile(testFile, "library content");
    
    // Load and add variant
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    auto result = pkg->addVariant("linux-amd64", testFile);
    EXPECT_TRUE(result.success);
    
    // Save and verify
    pkg->save(pkgPath);
    
    auto verifyResult = Package::verify(pkgPath);
    EXPECT_TRUE(verifyResult.valid);
}

TEST_F(PackageTest, AddVariant_SingleFile_AutoMain) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path testFile = tempDir / "mylib.so";
    createTestFile(testFile, "content");
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    pkg->addVariant("linux-amd64", testFile);
    
    // Main should be set to the filename
    auto mainPath = pkg->getManifest().getMain("linux-amd64");
    ASSERT_TRUE(mainPath.has_value());
    EXPECT_EQ(*mainPath, "mylib.so");
}

// =============================================================================
// Add Directory Variant Tests
// =============================================================================

TEST_F(PackageTest, AddVariant_Directory) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Create test directory
    fs::path testDir = tempDir / "dist";
    createTestDirectory(testDir, {
        {"index.js", "console.log('hello')"},
        {"lib.js", "export {}"}
    });
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    auto result = pkg->addVariant("web", testDir, "index.js");
    EXPECT_TRUE(result.success);
    
    pkg->save(pkgPath);
    
    auto verifyResult = Package::verify(pkgPath);
    EXPECT_TRUE(verifyResult.valid);
}

TEST_F(PackageTest, AddVariant_Directory_RequiresMain) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path testDir = tempDir / "dist";
    createTestDirectory(testDir, {{"file.txt", "content"}});
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    // Adding directory without --main should fail
    auto result = pkg->addVariant("web", testDir);
    EXPECT_FALSE(result.success);
}

// =============================================================================
// Variant Replacement Tests (No Merge)
// =============================================================================

TEST_F(PackageTest, AddVariant_ReplacesExisting) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Add initial variant
    fs::path file1 = tempDir / "old.so";
    createTestFile(file1, "old content");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->save(pkgPath);
    
    // Add replacement variant
    fs::path file2 = tempDir / "new.so";
    createTestFile(file2, "new content");
    
    pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file2);
    pkg->save(pkgPath);
    
    // Verify - old content should be gone
    auto verifyResult = Package::verify(pkgPath);
    EXPECT_TRUE(verifyResult.valid);
    
    // Check main points to new file
    pkg = Package::load(pkgPath);
    auto mainPath = pkg->getManifest().getMain("linux-amd64");
    ASSERT_TRUE(mainPath.has_value());
    EXPECT_EQ(*mainPath, "new.so");
}

TEST_F(PackageTest, AddVariant_NoMerge) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Add first file to variant
    fs::path file1 = tempDir / "file1.so";
    createTestFile(file1, "content1");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->save(pkgPath);
    
    // Add second file (should replace, not merge)
    fs::path file2 = tempDir / "file2.so";
    createTestFile(file2, "content2");
    
    pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file2);
    pkg->save(pkgPath);
    
    // Load and check - only file2.so should exist
    pkg = Package::load(pkgPath);
    EXPECT_TRUE(pkg->hasVariant("linux-amd64"));
    
    // The variant should only have file2.so, not file1.so
    auto mainPath = pkg->getManifest().getMain("linux-amd64");
    EXPECT_EQ(*mainPath, "file2.so");
}

// =============================================================================
// Remove Variant Tests
// =============================================================================

TEST_F(PackageTest, RemoveVariant) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Add two variants
    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.so";
    createTestFile(file1, "content1");
    createTestFile(file2, "content2");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->addVariant("darwin-arm64", file2);
    pkg->save(pkgPath);
    
    // Remove one variant
    pkg = Package::load(pkgPath);
    auto result = pkg->removeVariant("linux-amd64");
    EXPECT_TRUE(result.success);
    pkg->save(pkgPath);
    
    // Verify
    pkg = Package::load(pkgPath);
    EXPECT_FALSE(pkg->hasVariant("linux-amd64"));
    EXPECT_TRUE(pkg->hasVariant("darwin-arm64"));
    
    auto verifyResult = Package::verify(pkgPath);
    EXPECT_TRUE(verifyResult.valid);
}

TEST_F(PackageTest, RemoveVariant_NonExistent) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    auto pkg = Package::load(pkgPath);
    auto result = pkg->removeVariant("nonexistent");
    
    EXPECT_FALSE(result.success);
}

// =============================================================================
// HasVariant Tests
// =============================================================================

TEST_F(PackageTest, HasVariant) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");
    
    auto pkg = Package::load(pkgPath);
    
    EXPECT_FALSE(pkg->hasVariant("linux-amd64"));
    
    pkg->addVariant("linux-amd64", file);
    
    EXPECT_TRUE(pkg->hasVariant("linux-amd64"));
    EXPECT_TRUE(pkg->hasVariant("Linux-AMD64"));  // Case-insensitive
}

// =============================================================================
// GetVariants Tests
// =============================================================================

TEST_F(PackageTest, GetVariants) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    pkg->addVariant("darwin-arm64", file);
    
    auto variants = pkg->getVariants();
    
    EXPECT_EQ(variants.size(), 2);
    EXPECT_TRUE(variants.count("linux-amd64") > 0);
    EXPECT_TRUE(variants.count("darwin-arm64") > 0);
}

// =============================================================================
// Verification Tests
// =============================================================================

TEST_F(PackageTest, Verify_ValidPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    pkg->save(pkgPath);
    
    auto result = Package::verify(pkgPath);
    
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(PackageTest, Verify_InvalidPackage_NonExistent) {
    fs::path pkgPath = tempDir / "nonexistent.lgx";
    
    auto result = Package::verify(pkgPath);
    
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.errors.empty());
}

// =============================================================================
// WouldMainChange Tests
// =============================================================================

TEST_F(PackageTest, WouldMainChange) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    
    // Main is "lib.so", checking with same value
    EXPECT_FALSE(pkg->wouldMainChange("linux-amd64", "lib.so"));
    
    // Main is "lib.so", checking with different value
    EXPECT_TRUE(pkg->wouldMainChange("linux-amd64", "other.so"));
    
    // Non-existent variant
    EXPECT_FALSE(pkg->wouldMainChange("nonexistent", "anything"));
}

// =============================================================================
// Save/Load Roundtrip Tests
// =============================================================================

TEST_F(PackageTest, SaveLoad_Roundtrip) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Add some content
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "binary content here");
    
    auto pkg = Package::load(pkgPath);
    pkg->getManifest().description = "Test description";
    pkg->getManifest().version = "2.0.0";
    pkg->addVariant("linux-amd64", file);
    pkg->save(pkgPath);
    
    // Load again and verify
    auto pkg2 = Package::load(pkgPath);
    ASSERT_TRUE(pkg2.has_value());
    
    EXPECT_EQ(pkg2->getManifest().description, "Test description");
    EXPECT_EQ(pkg2->getManifest().version, "2.0.0");
    EXPECT_TRUE(pkg2->hasVariant("linux-amd64"));
}

// =============================================================================
// Multiple Operations Tests
// =============================================================================

TEST_F(PackageTest, MultipleOperations) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    // Create test files
    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.so";
    fs::path file3 = tempDir / "lib3.dylib";
    createTestFile(file1, "content1");
    createTestFile(file2, "content2");
    createTestFile(file3, "content3");
    
    // Add variants
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->addVariant("linux-arm64", file2);
    pkg->addVariant("darwin-arm64", file3);
    pkg->save(pkgPath);
    
    // Verify
    auto result1 = Package::verify(pkgPath);
    EXPECT_TRUE(result1.valid);
    
    // Remove one
    pkg = Package::load(pkgPath);
    pkg->removeVariant("linux-arm64");
    pkg->save(pkgPath);
    
    // Verify again
    auto result2 = Package::verify(pkgPath);
    EXPECT_TRUE(result2.valid);
    
    // Check state
    pkg = Package::load(pkgPath);
    EXPECT_TRUE(pkg->hasVariant("linux-amd64"));
    EXPECT_FALSE(pkg->hasVariant("linux-arm64"));
    EXPECT_TRUE(pkg->hasVariant("darwin-arm64"));
    EXPECT_EQ(pkg->getVariants().size(), 2);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(PackageTest, AddVariant_EmptyFile) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path emptyFile = tempDir / "empty.txt";
    createTestFile(emptyFile, "");
    
    auto pkg = Package::load(pkgPath);
    auto result = pkg->addVariant("test", emptyFile);
    EXPECT_TRUE(result.success);
    
    pkg->save(pkgPath);
    
    auto verifyResult = Package::verify(pkgPath);
    EXPECT_TRUE(verifyResult.valid);
}

TEST_F(PackageTest, VariantName_CaseNormalization) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("Linux-AMD64", file);  // Mixed case
    pkg->save(pkgPath);
    
    pkg = Package::load(pkgPath);
    
    // Should be stored as lowercase
    EXPECT_TRUE(pkg->hasVariant("linux-amd64"));
    EXPECT_TRUE(pkg->hasVariant("LINUX-AMD64"));  // Case-insensitive lookup
    
    auto variants = pkg->getVariants();
    EXPECT_TRUE(variants.count("linux-amd64") > 0);
    EXPECT_TRUE(variants.count("Linux-AMD64") == 0);  // Not stored as mixed case
}

// =============================================================================
// Extract Variant Tests
// =============================================================================

TEST_F(PackageTest, ExtractVariant_SingleFile) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path testFile = tempDir / "lib.so";
    createTestFile(testFile, "library content");
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    pkg->addVariant("linux-amd64", testFile);
    pkg->save(pkgPath);
    
    pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    fs::path extractDir = tempDir / "extracted";
    auto result = pkg->extractVariant("linux-amd64", extractDir);
    
    EXPECT_TRUE(result.success) << result.error;
    
    fs::path extractedFile = extractDir / "linux-amd64" / "lib.so";
    EXPECT_TRUE(fs::exists(extractedFile)) << "Expected: " << extractedFile.string();
    
    std::ifstream file(extractedFile);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "library content");
}

TEST_F(PackageTest, ExtractVariant_Directory) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path testDir = tempDir / "dist";
    createTestDirectory(testDir, {
        {"index.js", "console.log('hello')"},
        {"lib.js", "export {}"}
    });
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    pkg->addVariant("web", testDir, "index.js");
    pkg->save(pkgPath);
    
    pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    fs::path extractDir = tempDir / "extracted";
    auto result = pkg->extractVariant("web", extractDir);
    
    EXPECT_TRUE(result.success) << result.error;
    
    EXPECT_TRUE(fs::exists(extractDir / "web" / "index.js"));
    EXPECT_TRUE(fs::exists(extractDir / "web" / "lib.js"));
}

TEST_F(PackageTest, ExtractVariant_NonExistent) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    fs::path extractDir = tempDir / "extracted";
    auto result = pkg->extractVariant("nonexistent", extractDir);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(PackageTest, ExtractAll_MultipleVariants) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.dylib";
    createTestFile(file1, "content1");
    createTestFile(file2, "content2");
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    pkg->addVariant("linux-amd64", file1);
    pkg->addVariant("darwin-arm64", file2);
    pkg->save(pkgPath);
    
    pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    fs::path extractDir = tempDir / "extracted";
    auto result = pkg->extractAll(extractDir);
    
    EXPECT_TRUE(result.success) << result.error;
    
    EXPECT_TRUE(fs::exists(extractDir / "linux-amd64" / "lib1.so"));
    EXPECT_TRUE(fs::exists(extractDir / "darwin-arm64" / "lib2.dylib"));
    
    std::ifstream f1(extractDir / "linux-amd64" / "lib1.so");
    std::string c1((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
    EXPECT_EQ(c1, "content1");
    
    std::ifstream f2(extractDir / "darwin-arm64" / "lib2.dylib");
    std::string c2((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
    EXPECT_EQ(c2, "content2");
}

TEST_F(PackageTest, ExtractAll_EmptyPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    
    fs::path extractDir = tempDir / "extracted";
    auto result = pkg->extractAll(extractDir);
    
    EXPECT_TRUE(result.success);
}

TEST_F(PackageTest, ExtractVariant_CaseInsensitive) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");
    
    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");
    
    auto pkg = Package::load(pkgPath);
    pkg->addVariant("Linux-AMD64", file);
    pkg->save(pkgPath);
    
    pkg = Package::load(pkgPath);
    
    fs::path extractDir = tempDir / "extracted";
    
    auto result = pkg->extractVariant("LINUX-AMD64", extractDir);
    EXPECT_TRUE(result.success) << result.error;
    
    EXPECT_TRUE(fs::exists(extractDir / "linux-amd64" / "lib.so"));
}

// =============================================================================
// Mandatory Hashes Tests
// =============================================================================

TEST_F(PackageTest, AddVariant_ComputesHashes) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "library content");

    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    pkg->addVariant("linux-amd64", file);

    // Hashes should be present after addVariant
    EXPECT_FALSE(pkg->getManifest().hashes.empty());
    EXPECT_TRUE(pkg->getManifest().hashes.count("root") > 0);
    EXPECT_TRUE(pkg->getManifest().hashes.count("variants") > 0);
    EXPECT_TRUE(pkg->getManifest().hashes.count("variants/linux-amd64") > 0);
}

TEST_F(PackageTest, AddVariant_HashesUpdateOnReplace) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file1 = tempDir / "old.so";
    createTestFile(file1, "old content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    auto hashesAfterFirst = pkg->getManifest().hashes;

    // Replace with different content
    fs::path file2 = tempDir / "new.so";
    createTestFile(file2, "new different content");
    pkg->addVariant("linux-amd64", file2);
    auto hashesAfterSecond = pkg->getManifest().hashes;

    // Hashes should change when content changes
    EXPECT_NE(hashesAfterFirst["root"], hashesAfterSecond["root"]);
}

TEST_F(PackageTest, RemoveVariant_UpdatesHashes) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.so";
    createTestFile(file1, "content1");
    createTestFile(file2, "content2");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->addVariant("darwin-arm64", file2);
    auto hashesWithTwo = pkg->getManifest().hashes;

    pkg->removeVariant("linux-amd64");
    auto hashesWithOne = pkg->getManifest().hashes;

    // Root hash should change
    EXPECT_NE(hashesWithTwo["root"], hashesWithOne["root"]);
    // Removed variant hash should be gone
    EXPECT_EQ(hashesWithOne.count("variants/linux-amd64"), 0u);
    // Remaining variant hash should still exist
    EXPECT_TRUE(hashesWithOne.count("variants/darwin-arm64") > 0);
}

TEST_F(PackageTest, Hashes_PersistThroughSaveLoad) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    auto originalHashes = pkg->getManifest().hashes;
    pkg->save(pkgPath);

    // Reload and check hashes are preserved
    auto pkg2 = Package::load(pkgPath);
    ASSERT_TRUE(pkg2.has_value());
    EXPECT_EQ(pkg2->getManifest().hashes, originalHashes);
}

TEST_F(PackageTest, Verify_UnsignedPackage_ChecksHashes) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    pkg->save(pkgPath);

    // Verify should pass for unsigned package with valid hashes
    auto result = Package::verify(pkgPath);
    EXPECT_TRUE(result.valid) << "Errors: " <<
        (result.errors.empty() ? "none" : result.errors[0]);
}

TEST_F(PackageTest, ClearSignature_PreservesHashes) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    auto hashesBeforeClear = pkg->getManifest().hashes;

    pkg->clearSignature();

    EXPECT_EQ(pkg->getManifest().hashes, hashesBeforeClear);
    EXPECT_FALSE(pkg->getManifest().hashes.empty());
}

TEST_F(PackageTest, RecomputeHashes_EmptyPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());

    pkg->recomputeHashes();
    // Empty skeleton has no hashable content (just variants/ dir)
    EXPECT_TRUE(pkg->getManifest().hashes.empty());
}

TEST_F(PackageTest, RecomputeHashes_Idempotent) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);
    auto hashes1 = pkg->getManifest().hashes;

    pkg->recomputeHashes();
    auto hashes2 = pkg->getManifest().hashes;

    EXPECT_EQ(hashes1, hashes2);
}

TEST_F(PackageTest, Hashes_MultipleVariants) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.dylib";
    createTestFile(file1, "linux content");
    createTestFile(file2, "darwin content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->addVariant("darwin-arm64", file2);

    auto& hashes = pkg->getManifest().hashes;
    EXPECT_TRUE(hashes.count("root") > 0);
    EXPECT_TRUE(hashes.count("variants") > 0);
    EXPECT_TRUE(hashes.count("variants/linux-amd64") > 0);
    EXPECT_TRUE(hashes.count("variants/darwin-arm64") > 0);

    // Each variant should have a different hash (different content)
    EXPECT_NE(hashes["variants/linux-amd64"], hashes["variants/darwin-arm64"]);
}

// =============================================================================
// Package Signing Tests
// =============================================================================

TEST_F(PackageTest, SignPackage_Basic) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "library content");

    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    pkg->addVariant("linux-amd64", file);

    auto kp = crypto::generateKeypair();
    auto result = pkg->signPackage(kp.secretKey);
    EXPECT_TRUE(result.success) << result.error;
    EXPECT_TRUE(pkg->isSigned());
}

TEST_F(PackageTest, SignPackage_WithMetadata) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);

    auto kp = crypto::generateKeypair();
    auto result = pkg->signPackage(kp.secretKey, "Test Publisher", "https://example.com");
    EXPECT_TRUE(result.success);

    // Verify signature info contains metadata
    auto sigInfo = pkg->verifySignature();
    EXPECT_TRUE(sigInfo.is_signed);
    EXPECT_TRUE(sigInfo.signature_valid);
    EXPECT_TRUE(sigInfo.hashes_valid);
    EXPECT_EQ(sigInfo.signer_name, "Test Publisher");
    EXPECT_EQ(sigInfo.signer_url, "https://example.com");
    EXPECT_EQ(sigInfo.signer_did, crypto::publicKeyToDid(kp.publicKey));
}

TEST_F(PackageTest, SignVerify_RoundTrip) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);

    auto kp = crypto::generateKeypair();
    pkg->signPackage(kp.secretKey);
    pkg->save(pkgPath);

    // Load and verify
    auto pkg2 = Package::load(pkgPath);
    ASSERT_TRUE(pkg2.has_value());
    EXPECT_TRUE(pkg2->isSigned());

    auto sigInfo = pkg2->verifySignature();
    EXPECT_TRUE(sigInfo.is_signed);
    EXPECT_TRUE(sigInfo.signature_valid);
    EXPECT_TRUE(sigInfo.hashes_valid);
    EXPECT_TRUE(sigInfo.error.empty()) << sigInfo.error;
}

TEST_F(PackageTest, VerifySignature_Unsigned) {
    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());

    auto sigInfo = pkg->verifySignature();
    EXPECT_FALSE(sigInfo.is_signed);
    EXPECT_FALSE(sigInfo.signature_valid);
    EXPECT_FALSE(sigInfo.hashes_valid);
}

TEST_F(PackageTest, VerifySignature_Did) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);

    auto kp = crypto::generateKeypair();
    std::string expectedDid = crypto::publicKeyToDid(kp.publicKey);

    pkg->signPackage(kp.secretKey);

    auto sigInfo = pkg->verifySignature();
    EXPECT_EQ(sigInfo.signer_did, expectedDid);
    EXPECT_EQ(sigInfo.signer_did.substr(0, 8), "did:jwk:");
}

TEST_F(PackageTest, AddVariant_ClearsSignature) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.so";
    createTestFile(file1, "content1");
    createTestFile(file2, "content2");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);

    auto kp = crypto::generateKeypair();
    pkg->signPackage(kp.secretKey);
    EXPECT_TRUE(pkg->isSigned());

    // Adding another variant should clear the signature
    pkg->addVariant("darwin-arm64", file2);
    EXPECT_FALSE(pkg->isSigned());

    // But hashes should still be present and updated
    EXPECT_FALSE(pkg->getManifest().hashes.empty());
    EXPECT_TRUE(pkg->getManifest().hashes.count("variants/darwin-arm64") > 0);
}

TEST_F(PackageTest, RemoveVariant_ClearsSignature) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file1 = tempDir / "lib1.so";
    fs::path file2 = tempDir / "lib2.so";
    createTestFile(file1, "content1");
    createTestFile(file2, "content2");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file1);
    pkg->addVariant("darwin-arm64", file2);

    auto kp = crypto::generateKeypair();
    pkg->signPackage(kp.secretKey);
    EXPECT_TRUE(pkg->isSigned());

    // Removing a variant should clear the signature
    pkg->removeVariant("darwin-arm64");
    EXPECT_FALSE(pkg->isSigned());
}

TEST_F(PackageTest, SignPackage_EmptyPackage_Fails) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());

    auto kp = crypto::generateKeypair();
    auto result = pkg->signPackage(kp.secretKey);
    EXPECT_FALSE(result.success);
}

TEST_F(PackageTest, Verify_SignedPackage_ValidHashes) {
    ASSERT_TRUE(crypto::init());

    fs::path pkgPath = tempDir / "test.lgx";
    Package::create(pkgPath, "testpkg");

    fs::path file = tempDir / "lib.so";
    createTestFile(file, "content");

    auto pkg = Package::load(pkgPath);
    pkg->addVariant("linux-amd64", file);

    auto kp = crypto::generateKeypair();
    pkg->signPackage(kp.secretKey, "Publisher", "https://example.com");
    pkg->save(pkgPath);

    // verify() (the static method) should pass
    auto result = Package::verify(pkgPath);
    EXPECT_TRUE(result.valid) << "Errors: " <<
        (result.errors.empty() ? "none" : result.errors[0]);
}
