#include <gtest/gtest.h>
#include "lgx.h"
#include <filesystem>
#include <fstream>
#include <cstring>

class LibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        test_dir_ = std::filesystem::temp_directory_path() / "lgx_test_lib";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir_);
    }
    
    std::filesystem::path test_dir_;
};

TEST_F(LibraryTest, VersionTest) {
    const char* version = lgx_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STREQ(version, "0.1.0");
}

TEST_F(LibraryTest, CreatePackage) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    lgx_result_t result = lgx_create(output_path.c_str(), "testpkg");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error, nullptr);
    
    // Verify file was created
    EXPECT_TRUE(std::filesystem::exists(output_path));
}

TEST_F(LibraryTest, CreatePackageInvalidArgs) {
    lgx_result_t result = lgx_create(nullptr, "testpkg");
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error, nullptr);
    
    result = lgx_create("test.lgx", nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error, nullptr);
}

TEST_F(LibraryTest, LoadPackage) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    // Create package first
    lgx_result_t result = lgx_create(output_path.c_str(), "testpkg");
    ASSERT_TRUE(result.success);
    
    // Load it
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Cleanup
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, LoadPackageInvalidPath) {
    lgx_package_t pkg = lgx_load("/nonexistent/path.lgx");
    EXPECT_EQ(pkg, nullptr);
    
    const char* error = lgx_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(strlen(error), 0);
}

TEST_F(LibraryTest, LoadPackageNullArg) {
    lgx_package_t pkg = lgx_load(nullptr);
    EXPECT_EQ(pkg, nullptr);
    
    const char* error = lgx_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(LibraryTest, GetPackageMetadata) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    // Create and load package
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Get name
    const char* name = lgx_get_name(pkg);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "testpkg");
    
    // Get version
    const char* version = lgx_get_version(pkg);
    ASSERT_NE(version, nullptr);
    EXPECT_STREQ(version, "0.0.1");
    
    // Get description (should be empty initially)
    const char* desc = lgx_get_description(pkg);
    ASSERT_NE(desc, nullptr);

    // Get icon (should be empty initially)
    const char* icon = lgx_get_icon(pkg);
    ASSERT_NE(icon, nullptr);
    EXPECT_STREQ(icon, "");

    lgx_free_package(pkg);
}

TEST_F(LibraryTest, SetPackageMetadata) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    // Create and load package
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Set version
    lgx_result_t result = lgx_set_version(pkg, "1.2.3");
    EXPECT_TRUE(result.success);
    
    const char* version = lgx_get_version(pkg);
    EXPECT_STREQ(version, "1.2.3");
    
    // Set description
    lgx_set_description(pkg, "Test package");
    const char* desc = lgx_get_description(pkg);
    EXPECT_STREQ(desc, "Test package");

    // Set icon
    lgx_set_icon(pkg, "icon.png");
    const char* icon = lgx_get_icon(pkg);
    EXPECT_STREQ(icon, "icon.png");

    lgx_free_package(pkg);
}

TEST_F(LibraryTest, SavePackage) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto output_path2 = (test_dir_ / "test2.lgx").string();
    
    // Create and load package
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Modify it
    lgx_set_version(pkg, "1.0.0");
    lgx_set_description(pkg, "Modified package");
    
    // Save to new location
    lgx_result_t result = lgx_save(pkg, output_path2.c_str());
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(output_path2));
    
    lgx_free_package(pkg);
    
    // Verify changes persisted
    lgx_package_t pkg2 = lgx_load(output_path2.c_str());
    ASSERT_NE(pkg2, nullptr);
    
    EXPECT_STREQ(lgx_get_version(pkg2), "1.0.0");
    EXPECT_STREQ(lgx_get_description(pkg2), "Modified package");
    
    lgx_free_package(pkg2);
}

TEST_F(LibraryTest, VerifyPackage) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    // Create package
    lgx_create(output_path.c_str(), "testpkg");
    
    // Verify it
    lgx_verify_result_t result = lgx_verify(output_path.c_str());
    EXPECT_TRUE(result.valid);
    
    // Should have no errors
    if (result.errors) {
        for (int i = 0; result.errors[i]; i++) {
            // Print any errors for debugging
            printf("Error: %s\n", result.errors[i]);
        }
    }
    
    lgx_free_verify_result(result);
}

TEST_F(LibraryTest, VerifyInvalidPackage) {
    lgx_verify_result_t result = lgx_verify("/nonexistent/path.lgx");
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.errors, nullptr);
    
    // Should have at least one error
    EXPECT_NE(result.errors[0], nullptr);
    
    lgx_free_verify_result(result);
}

TEST_F(LibraryTest, AddVariantSingleFile) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto file_path = (test_dir_ / "test.txt").string();
    
    // Create a test file
    std::ofstream(file_path) << "test content";
    
    // Create and load package
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Add variant with single file
    lgx_result_t result = lgx_add_variant(pkg, "test-variant", file_path.c_str(), "test.txt");
    EXPECT_TRUE(result.success) << (result.error ? result.error : "");
    
    // Check variant exists
    EXPECT_TRUE(lgx_has_variant(pkg, "test-variant"));
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, HasVariant) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Should not have any variants initially
    EXPECT_FALSE(lgx_has_variant(pkg, "nonexistent"));
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, GetVariants) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto file_path = (test_dir_ / "test.txt").string();
    
    // Create a test file
    std::ofstream(file_path) << "test content";
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Initially no variants
    const char** variants = lgx_get_variants(pkg);
    ASSERT_NE(variants, nullptr);
    EXPECT_EQ(variants[0], nullptr); // Empty array
    lgx_free_string_array(variants);
    
    // Add a variant
    lgx_add_variant(pkg, "test-variant", file_path.c_str(), "test.txt");
    
    // Now should have one variant
    variants = lgx_get_variants(pkg);
    ASSERT_NE(variants, nullptr);
    ASSERT_NE(variants[0], nullptr);
    EXPECT_STREQ(variants[0], "test-variant");
    EXPECT_EQ(variants[1], nullptr); // NULL-terminated
    lgx_free_string_array(variants);
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, RemoveVariant) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto file_path = (test_dir_ / "test.txt").string();
    
    // Create a test file
    std::ofstream(file_path) << "test content";
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Add variant
    lgx_add_variant(pkg, "test-variant", file_path.c_str(), "test.txt");
    EXPECT_TRUE(lgx_has_variant(pkg, "test-variant"));
    
    // Remove variant
    lgx_result_t result = lgx_remove_variant(pkg, "test-variant");
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(lgx_has_variant(pkg, "test-variant"));
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, RemoveNonexistentVariant) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    // Try to remove non-existent variant
    lgx_result_t result = lgx_remove_variant(pkg, "nonexistent");
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error, nullptr);
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, NullPackageHandles) {
    // All functions should handle NULL package gracefully
    EXPECT_FALSE(lgx_save(nullptr, "test.lgx").success);
    EXPECT_FALSE(lgx_add_variant(nullptr, "variant", "path", nullptr).success);
    EXPECT_FALSE(lgx_remove_variant(nullptr, "variant").success);
    EXPECT_FALSE(lgx_has_variant(nullptr, "variant"));
    EXPECT_EQ(lgx_get_variants(nullptr), nullptr);
    EXPECT_EQ(lgx_get_name(nullptr), nullptr);
    EXPECT_EQ(lgx_get_version(nullptr), nullptr);
    EXPECT_EQ(lgx_get_description(nullptr), nullptr);
    EXPECT_EQ(lgx_get_icon(nullptr), nullptr);

    // These should not crash with NULL
    lgx_free_package(nullptr);
}

TEST_F(LibraryTest, FreeStringArray) {
    // Test that freeing NULL is safe
    lgx_free_string_array(nullptr);
    
    // Test freeing an empty array
    const char** empty = static_cast<const char**>(malloc(sizeof(char*)));
    empty[0] = nullptr;
    lgx_free_string_array(empty);
}

TEST_F(LibraryTest, FreeVerifyResult) {
    // Test that freeing empty result is safe
    lgx_verify_result_t result = {true, nullptr, nullptr};
    lgx_free_verify_result(result);
}

// =============================================================================
// Extract Tests
// =============================================================================

TEST_F(LibraryTest, ExtractVariant) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto file_path = (test_dir_ / "test.txt").string();
    auto extract_dir = (test_dir_ / "extracted").string();
    
    std::ofstream(file_path) << "test content";
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    lgx_result_t result = lgx_add_variant(pkg, "test-variant", file_path.c_str(), "test.txt");
    ASSERT_TRUE(result.success);
    
    lgx_save(pkg, output_path.c_str());
    
    result = lgx_extract(pkg, "test-variant", extract_dir.c_str());
    EXPECT_TRUE(result.success) << (result.error ? result.error : "");
    
    auto extracted_file = std::filesystem::path(extract_dir) / "test-variant" / "test.txt";
    EXPECT_TRUE(std::filesystem::exists(extracted_file)) << "Expected: " << extracted_file.string();
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, ExtractAllVariants) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto file_path = (test_dir_ / "test.txt").string();
    auto extract_dir = (test_dir_ / "extracted").string();
    
    std::ofstream(file_path) << "test content";
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    lgx_add_variant(pkg, "variant1", file_path.c_str(), "test.txt");
    lgx_add_variant(pkg, "variant2", file_path.c_str(), "test.txt");
    lgx_save(pkg, output_path.c_str());
    
    lgx_result_t result = lgx_extract(pkg, nullptr, extract_dir.c_str());
    EXPECT_TRUE(result.success) << (result.error ? result.error : "");
    
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(extract_dir) / "variant1" / "test.txt"));
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(extract_dir) / "variant2" / "test.txt"));
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, ExtractNonexistentVariant) {
    auto output_path = (test_dir_ / "test.lgx").string();
    auto extract_dir = (test_dir_ / "extracted").string();
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    lgx_result_t result = lgx_extract(pkg, "nonexistent", extract_dir.c_str());
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error, nullptr);
    
    lgx_free_package(pkg);
}

TEST_F(LibraryTest, ExtractNullArgs) {
    auto output_path = (test_dir_ / "test.lgx").string();
    
    lgx_create(output_path.c_str(), "testpkg");
    lgx_package_t pkg = lgx_load(output_path.c_str());
    ASSERT_NE(pkg, nullptr);
    
    lgx_result_t result = lgx_extract(nullptr, "variant", "/tmp");
    EXPECT_FALSE(result.success);
    
    result = lgx_extract(pkg, "variant", nullptr);
    EXPECT_FALSE(result.success);
    
    lgx_free_package(pkg);
}
