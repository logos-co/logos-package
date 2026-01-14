#include <gtest/gtest.h>
#include "core/manifest.h"

using namespace lgx;

// Valid manifest JSON for reuse
static const char* VALID_MANIFEST_JSON = R"({
  "manifestVersion": "0.1.0",
  "name": "testpkg",
  "version": "1.0.0",
  "description": "Test package",
  "author": "Test Author",
  "type": "library",
  "category": "test",
  "dependencies": ["dep1", "dep2"],
  "main": {
    "linux-amd64": "lib/test.so",
    "darwin-arm64": "lib/test.dylib"
  }
})";

// =============================================================================
// Parsing Tests
// =============================================================================

TEST(ManifestTest, FromJson_ValidManifest) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    
    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->manifestVersion, "0.1.0");
    EXPECT_EQ(manifest->name, "testpkg");
    EXPECT_EQ(manifest->version, "1.0.0");
    EXPECT_EQ(manifest->description, "Test package");
    EXPECT_EQ(manifest->author, "Test Author");
    EXPECT_EQ(manifest->type, "library");
    EXPECT_EQ(manifest->category, "test");
    EXPECT_EQ(manifest->dependencies.size(), 2);
    EXPECT_EQ(manifest->main.size(), 2);
}

TEST(ManifestTest, FromJson_MissingManifestVersion) {
    const char* json = R"({
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "",
      "category": "",
      "dependencies": [],
      "main": {}
    })";
    
    auto manifest = Manifest::fromJson(json);
    EXPECT_FALSE(manifest.has_value());
}

TEST(ManifestTest, FromJson_MissingName) {
    const char* json = R"({
      "manifestVersion": "0.1.0",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "",
      "category": "",
      "dependencies": [],
      "main": {}
    })";
    
    auto manifest = Manifest::fromJson(json);
    EXPECT_FALSE(manifest.has_value());
}

TEST(ManifestTest, FromJson_MissingMain) {
    const char* json = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "",
      "category": "",
      "dependencies": []
    })";
    
    auto manifest = Manifest::fromJson(json);
    EXPECT_FALSE(manifest.has_value());
}

TEST(ManifestTest, FromJson_InvalidJson) {
    const char* json = "{ not valid json }";
    
    auto manifest = Manifest::fromJson(json);
    EXPECT_FALSE(manifest.has_value());
    EXPECT_FALSE(Manifest::getLastError().empty());
}

TEST(ManifestTest, FromJson_EmptyDependencies) {
    const char* json = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "",
      "category": "",
      "dependencies": [],
      "main": {}
    })";
    
    auto manifest = Manifest::fromJson(json);
    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->dependencies.empty());
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST(ManifestTest, ToJson_Roundtrip) {
    auto original = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(original.has_value());
    
    std::string json = original->toJson();
    auto parsed = Manifest::fromJson(json);
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->manifestVersion, original->manifestVersion);
    EXPECT_EQ(parsed->name, original->name);
    EXPECT_EQ(parsed->version, original->version);
    EXPECT_EQ(parsed->main, original->main);
}

TEST(ManifestTest, ToJson_Deterministic) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    
    std::string json1 = manifest->toJson();
    std::string json2 = manifest->toJson();
    
    EXPECT_EQ(json1, json2);
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST(ManifestTest, Validate_ValidManifest) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    
    auto result = manifest->validate();
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
}

TEST(ManifestTest, Validate_UnsupportedVersion) {
    Manifest m;
    m.manifestVersion = "2.0.0";  // Major version 2 not supported
    m.name = "test";
    m.version = "1.0.0";
    
    auto result = m.validate();
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.errors.empty());
}

TEST(ManifestTest, Validate_EmptyName) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "";
    m.version = "1.0.0";
    
    auto result = m.validate();
    EXPECT_FALSE(result.valid);
}

TEST(ManifestTest, Validate_EmptyVersion) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "";
    
    auto result = m.validate();
    EXPECT_FALSE(result.valid);
}

TEST(ManifestTest, Validate_InvalidMainPath) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "1.0.0";
    m.main["linux"] = "/absolute/path";  // Invalid - absolute path
    
    auto result = m.validate();
    EXPECT_FALSE(result.valid);
}

// =============================================================================
// Completeness Constraint Tests
// =============================================================================

TEST(ManifestTest, ValidateCompleteness_Valid) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    
    std::set<std::string> variants = {"linux-amd64", "darwin-arm64"};
    
    auto result = manifest->validateCompleteness(variants);
    EXPECT_TRUE(result.valid);
}

TEST(ManifestTest, ValidateCompleteness_MissingVariantDir) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    
    // Only one variant directory exists, but main has two
    std::set<std::string> variants = {"linux-amd64"};
    
    auto result = manifest->validateCompleteness(variants);
    EXPECT_FALSE(result.valid);
}

TEST(ManifestTest, ValidateCompleteness_MissingMainEntry) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "1.0.0";
    m.main["linux-amd64"] = "lib.so";
    
    // Two variant directories exist, but main only has one
    std::set<std::string> variants = {"linux-amd64", "darwin-arm64"};
    
    auto result = m.validateCompleteness(variants);
    EXPECT_FALSE(result.valid);
}

TEST(ManifestTest, ValidateCompleteness_CaseInsensitive) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "1.0.0";
    m.main["linux-amd64"] = "lib.so";
    
    // Variant directory with different case
    std::set<std::string> variants = {"Linux-AMD64"};
    
    auto result = m.validateCompleteness(variants);
    EXPECT_TRUE(result.valid);
}

// =============================================================================
// Variant Key Normalization Tests
// =============================================================================

TEST(ManifestTest, SetMain_NormalizesKey) {
    Manifest m;
    m.setMain("Linux-AMD64", "lib.so");
    
    EXPECT_TRUE(m.main.find("linux-amd64") != m.main.end());
    EXPECT_TRUE(m.main.find("Linux-AMD64") == m.main.end());
}

TEST(ManifestTest, GetMain_CaseInsensitive) {
    Manifest m;
    m.setMain("linux-amd64", "lib.so");
    
    auto result = m.getMain("Linux-AMD64");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "lib.so");
}

TEST(ManifestTest, RemoveMain) {
    Manifest m;
    m.setMain("linux-amd64", "lib.so");
    m.setMain("darwin-arm64", "lib.dylib");
    
    EXPECT_EQ(m.main.size(), 2);
    
    m.removeMain("linux-amd64");
    
    EXPECT_EQ(m.main.size(), 1);
    EXPECT_FALSE(m.getMain("linux-amd64").has_value());
    EXPECT_TRUE(m.getMain("darwin-arm64").has_value());
}

TEST(ManifestTest, GetVariants) {
    Manifest m;
    m.setMain("linux-amd64", "lib.so");
    m.setMain("darwin-arm64", "lib.dylib");
    m.setMain("web", "index.js");
    
    auto variants = m.getVariants();
    
    EXPECT_EQ(variants.size(), 3);
    EXPECT_TRUE(variants.count("linux-amd64") > 0);
    EXPECT_TRUE(variants.count("darwin-arm64") > 0);
    EXPECT_TRUE(variants.count("web") > 0);
}

// =============================================================================
// Name Normalization Tests
// =============================================================================

TEST(ManifestTest, NormalizeName) {
    Manifest m;
    m.name = "MyPackage";
    
    m.normalizeName();
    
    EXPECT_EQ(m.name, "mypackage");
}

TEST(ManifestTest, NormalizeVariantKeys) {
    Manifest m;
    m.main["Linux-AMD64"] = "lib.so";
    m.main["Darwin-ARM64"] = "lib.dylib";
    
    m.normalizeVariantKeys();
    
    EXPECT_EQ(m.main.size(), 2);
    EXPECT_TRUE(m.main.find("linux-amd64") != m.main.end());
    EXPECT_TRUE(m.main.find("darwin-arm64") != m.main.end());
}

// =============================================================================
// Version Support Tests
// =============================================================================

TEST(ManifestTest, IsVersionSupported_Major0) {
    EXPECT_TRUE(Manifest::isVersionSupported("0.1.0"));
    EXPECT_TRUE(Manifest::isVersionSupported("0.2.0"));
    EXPECT_TRUE(Manifest::isVersionSupported("0.99.99"));
}

TEST(ManifestTest, IsVersionSupported_Major1Plus) {
    EXPECT_FALSE(Manifest::isVersionSupported("1.0.0"));
    EXPECT_FALSE(Manifest::isVersionSupported("2.0.0"));
}

TEST(ManifestTest, IsVersionSupported_Invalid) {
    EXPECT_FALSE(Manifest::isVersionSupported("invalid"));
    EXPECT_FALSE(Manifest::isVersionSupported(""));
}

// =============================================================================
// Default Constructor Test
// =============================================================================

TEST(ManifestTest, DefaultConstructor) {
    Manifest m;
    
    EXPECT_EQ(m.manifestVersion, Manifest::CURRENT_VERSION);
    EXPECT_TRUE(m.name.empty());
    EXPECT_TRUE(m.version.empty());
    EXPECT_TRUE(m.main.empty());
}
