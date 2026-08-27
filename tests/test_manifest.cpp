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
  "icon": "icon.png",
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
    EXPECT_EQ(manifest->icon, "icon.png");
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
      "icon": "",
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
      "icon": "",
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
      "icon": "",
      "dependencies": []
    })";
    
    auto manifest = Manifest::fromJson(json);
    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->main.empty());
}

TEST(ManifestTest, FromJson_ViewOnlyUiQmlManifest) {
    const char* json = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "dependencies": [],
      "view": "qml/Main.qml"
    })";

    auto manifest = Manifest::fromJson(json);
    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->main.empty());
    EXPECT_EQ(manifest->view, "qml/Main.qml");
}

TEST(ManifestTest, FromJson_InvalidViewType) {
    const char* json = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "dependencies": [],
      "view": {}
    })";

    auto manifest = Manifest::fromJson(json);
    EXPECT_FALSE(manifest.has_value());
    EXPECT_FALSE(Manifest::getLastError().empty());
}

TEST(ManifestTest, FromJson_InvalidJson) {
    const char* json = "{ not valid json }";
    
    auto manifest = Manifest::fromJson(json);
    EXPECT_FALSE(manifest.has_value());
    EXPECT_FALSE(Manifest::getLastError().empty());
}

TEST(ManifestTest, FromJson_MissingIcon) {
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
    EXPECT_FALSE(manifest.has_value());
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
      "icon": "",
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
    EXPECT_EQ(parsed->icon, original->icon);
    EXPECT_EQ(parsed->main, original->main);
}

TEST(ManifestTest, ToJson_RoundtripPreservesViewWithoutMain) {
    const char* json = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "dependencies": [],
      "view": "qml/Main.qml"
    })";

    auto original = Manifest::fromJson(json);
    ASSERT_TRUE(original.has_value());

    std::string serialized = original->toJson();
    auto parsed = Manifest::fromJson(serialized);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->main.empty());
    EXPECT_EQ(parsed->view, "qml/Main.qml");
}

TEST(ManifestTest, ToJson_RoundtripPreservesDisplayName) {
    const char* json = R"({
      "manifestVersion": "0.3.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "core",
      "category": "",
      "icon": "",
      "dependencies": [],
      "display_name": "Friendly Label"
    })";

    auto original = Manifest::fromJson(json);
    ASSERT_TRUE(original.has_value());
    EXPECT_EQ(original->displayName, "Friendly Label");

    std::string serialized = original->toJson();
    auto parsed = Manifest::fromJson(serialized);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->displayName, "Friendly Label");
}

TEST(ManifestTest, Provides_AcceptsBothAuthorAndWireForms) {
    // The author's metadata.json uses objects (because `uses` needs room for
    // cardinality); a hand-written manifest is likely to use bare strings.
    // Both parse, and anything without an "intent" string is skipped rather
    // than failing the package.
    std::string json = R"({
      "manifestVersion": "0.5.0",
      "name": "chat_ui",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "view": "Main.qml",
      "dependencies": [],
      "provides": [{"intent": "chat.group.open"}, "wallet.sign", {"noIntentKey": 1}]
    })";

    auto m = Manifest::fromJson(json);
    ASSERT_TRUE(m.has_value());
    ASSERT_EQ(m->provides.size(), 2u);
    EXPECT_EQ(m->provides[0].intent, "chat.group.open");
    EXPECT_EQ(m->provides[1].intent, "wallet.sign");
}

TEST(ManifestTest, Provides_EmitsTheObjectFormOnWrite) {
    std::string json = R"({
      "manifestVersion": "0.5.0",
      "name": "chat_ui",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "view": "Main.qml",
      "dependencies": [],
      "provides": [{"intent": "chat.group.open"}]
    })";

    auto m = Manifest::fromJson(json);
    ASSERT_TRUE(m.has_value());

    // Objects on the wire, so the parameter shape survives. One shape, not two:
    // a provider with no params is still emitted as an object.
    const std::string out = m->toJson();
    EXPECT_NE(out.find("\"chat.group.open\""), std::string::npos);
    EXPECT_NE(out.find("intent"), std::string::npos);

    auto round = Manifest::fromJson(out);
    ASSERT_TRUE(round.has_value());
    ASSERT_EQ(round->provides.size(), m->provides.size());
    EXPECT_EQ(round->provides[0].intent, m->provides[0].intent);
}

TEST(ManifestTest, Provides_SurvivesAFieldByFieldCopy) {
    // merge_command copies the reference manifest field by field rather than
    // assigning the whole struct, so every new field has to be added there by
    // hand. `provides` was missed on the first pass: merging per-platform
    // packages into a multi-variant one silently un-declared its intents.
    //
    // This pins the property that matters — capabilities belong to the package,
    // not to a platform build, so they must survive the merge.
    std::string json = R"({
      "manifestVersion": "0.5.0",
      "name": "chat_ui",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "view": "Main.qml",
      "dependencies": [],
      "provides": ["chat.group.open"]
    })";

    auto ref = Manifest::fromJson(json);
    ASSERT_TRUE(ref.has_value());

    Manifest merged;
    merged.manifestVersion = ref->manifestVersion;
    merged.name = ref->name;
    merged.version = ref->version;
    merged.type = ref->type;
    merged.view = ref->view;
    merged.dependencies = ref->dependencies;
    merged.provides = ref->provides;

    ASSERT_EQ(merged.provides.size(), ref->provides.size());
    EXPECT_EQ(merged.provides[0].intent, ref->provides[0].intent);
    EXPECT_NE(merged.toJson().find("chat.group.open"), std::string::npos);
}

TEST(ManifestTest, Provides_KeepsOnlyTheNameNotTheParamShape) {
    // The author's metadata.json may describe an intent's payload; the manifest
    // deliberately does not carry that description. The shell enforces params
    // against the INSTALLED metadata.json, and a second copy here would be a
    // bundle-time snapshot that nothing reads and that can drift from the
    // original. The catalog question this copy exists to answer — "which
    // installable package provides X?" — needs the name and nothing else.
    std::string json = R"({
      "manifestVersion": "0.5.0",
      "name": "wallet_ui",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "view": "Main.qml",
      "dependencies": [],
      "provides": [{
        "intent": "wallet.send",
        "params": [{"name": "to", "type": "string", "required": true}]
      }]
    })";

    auto m = Manifest::fromJson(json);
    ASSERT_TRUE(m.has_value());
    ASSERT_EQ(m->provides.size(), 1u);
    EXPECT_EQ(m->provides[0].intent, "wallet.send");

    // Accepted on the way in, dropped on the way out.
    const std::string out = m->toJson();
    EXPECT_NE(out.find("wallet.send"), std::string::npos);
    EXPECT_EQ(out.find("\"params\""), std::string::npos);
}

TEST(ManifestTest, ToJson_OmitsProvidesWhenUnset) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    ASSERT_TRUE(manifest->provides.empty());

    // Older packages must round-trip byte-identically.
    EXPECT_EQ(manifest->toJson().find("provides"), std::string::npos);
}

TEST(ManifestTest, ToJson_OmitsDisplayNameWhenUnset) {
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    ASSERT_TRUE(manifest->displayName.empty());

    // Should not emit "display_name" in the serialized form so older
    // packages round-trip byte-identically.
    std::string json = manifest->toJson();
    EXPECT_EQ(json.find("display_name"), std::string::npos);
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

// The package `version` must be a full SemVer 2.0.0 version. A non-conforming
// version is unparseable to the comparators, so it sorts below every valid
// version and only orders against other junk by byte comparison — rejecting it
// here fails `lgx verify` loudly instead of shipping a package that sorts wrong.
TEST(ManifestTest, Validate_NonSemverVersionIsRejected) {
    for (const char* bad : {"0.1.2.3",   // four sections — the reported case
                            "1.0",       // partial
                            "v1.0.0",    // leading v
                            "1.0.0-",    // empty pre-release
                            "01.0.0",    // leading zero
                            "banana"}) {
        Manifest m;
        m.manifestVersion = "0.1.0";
        m.name = "test";
        m.version = bad;
        auto result = m.validate();
        EXPECT_FALSE(result.valid) << "should reject version '" << bad << "'";
    }
}

TEST(ManifestTest, Validate_SemverVersionsAreAccepted) {
    for (const char* good : {"1.0.0", "0.1.3", "10.20.30",
                             "1.0.0-rc.1", "1.0.0-alpha.1+build.5"}) {
        Manifest m;
        m.manifestVersion = "0.1.0";
        m.name = "test";
        m.version = good;
        auto result = m.validate();
        EXPECT_TRUE(result.valid) << "should accept version '" << good
                                  << "': " << (result.errors.empty() ? "" : result.errors[0]);
    }
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

TEST(ManifestTest, Validate_InvalidViewPath) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "1.0.0";
    m.type = "ui_qml";
    m.view = "../qml/Main.qml";

    auto result = m.validate();
    EXPECT_FALSE(result.valid);
}

TEST(ManifestTest, Validate_UiQmlMissingViewIsInvalid) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "1.0.0";
    m.type = "ui_qml";
    // view intentionally left empty

    auto result = m.validate();
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("view"), std::string::npos);
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

TEST(ManifestTest, ValidateCompleteness_ViewOnlyUiQmlWithoutMainIsValid) {
    Manifest m;
    m.manifestVersion = "0.1.0";
    m.name = "test";
    m.version = "1.0.0";
    m.type = "ui_qml";
    m.view = "qml/Main.qml";

    std::set<std::string> variants = {"linux-amd64", "darwin-arm64"};

    auto result = m.validateCompleteness(variants);
    EXPECT_TRUE(result.valid);
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

// =============================================================================
// CompareMetadata Tests
// =============================================================================

TEST(ManifestTest, CompareMetadata_IdenticalManifests) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    auto result = a->compareMetadata(*b);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
}

TEST(ManifestTest, CompareMetadata_DifferentMainOnly) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    // Change the main field — should still match
    b->main.clear();
    b->setMain("web", "index.js");

    auto result = a->compareMetadata(*b);
    EXPECT_TRUE(result.valid);
}

TEST(ManifestTest, CompareMetadata_DifferentName) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    b->name = "otherpkg";

    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("name"), std::string::npos);
}

TEST(ManifestTest, CompareMetadata_DifferentVersion) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    b->version = "2.0.0";

    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("version"), std::string::npos);
}

TEST(ManifestTest, CompareMetadata_DifferentDisplayName) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    b->displayName = "Other Label";

    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("display_name"), std::string::npos);
}

TEST(ManifestTest, CompareMetadata_DifferentDependencies) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    b->dependencies.push_back("extra-dep");

    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("dependencies"), std::string::npos);
}

TEST(ManifestTest, CompareMetadata_DifferentView) {
    const char* viewJsonA = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "dependencies": [],
      "view": "qml/Main.qml"
    })";
    const char* viewJsonB = R"({
      "manifestVersion": "0.1.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "ui_qml",
      "category": "",
      "icon": "",
      "dependencies": [],
      "view": "qml/Other.qml"
    })";

    auto a = Manifest::fromJson(viewJsonA);
    auto b = Manifest::fromJson(viewJsonB);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("view"), std::string::npos);
}

TEST(ManifestTest, CompareMetadata_MultipleDifferences) {
    auto a = Manifest::fromJson(VALID_MANIFEST_JSON);
    auto b = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    b->name = "otherpkg";
    b->version = "2.0.0";
    b->author = "Other Author";

    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    EXPECT_GE(result.errors.size(), 3);
}
