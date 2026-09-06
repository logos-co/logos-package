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

// =============================================================================
// Object-form Dependency Tests
//
// A dependency entry is either a plain string (the legacy 0.2.x form) or an
// object {name, version?, signer?}. Before these tests the object form had no
// coverage at all: parsing, the type-error branches, the isSimple() 0.2.0
// byte-compat serialisation branch, and every validate() rule on `version` /
// `signer` / `name` were exercised only by the source itself.
// =============================================================================

// A syntactically well-formed did:jwk. The payload is the base64url JWK from
// docs/spec.md; nothing here verifies the key, only the DID shape.
static const char* VALID_DID =
    "did:jwk:eyJjcnYiOiJFZDI1NTE5Iiwia3R5IjoiT0tQIiwieCI6IjExcVlBWUt4Q3JmVlNfN"
    "1R5V1FIT2c3aGN2UGFwaU1scndJYWFQY0hVUm8ifQ";

// Build a manifest whose only interesting content is its dependencies array.
static std::string manifestWithDeps(const std::string& depsArrayJson) {
    return std::string(R"({
      "manifestVersion": "0.3.0",
      "name": "test",
      "version": "1.0.0",
      "description": "",
      "author": "",
      "type": "core",
      "category": "",
      "icon": "",
      "dependencies": )") + depsArrayJson + R"(,
      "main": {}
    })";
}

// --- parsing: the three accepted shapes -------------------------------------

TEST(ManifestDependencyTest, FromJson_MixedStringAndObjectForms) {
    auto m = Manifest::fromJson(manifestWithDeps(
        R"([
          "plain_dep",
          {"name": "ranged_dep", "version": "^1.2.0"},
          {"name": "pinned_dep", "version": ">=0.5.0", "signer": ")" + std::string(VALID_DID) + R"("}
        ])"));

    ASSERT_TRUE(m.has_value()) << Manifest::getLastError();
    ASSERT_EQ(m->dependencies.size(), 3u);

    EXPECT_EQ(m->dependencies[0].name, "plain_dep");
    EXPECT_FALSE(m->dependencies[0].version.has_value());
    EXPECT_FALSE(m->dependencies[0].signer.has_value());
    EXPECT_TRUE(m->dependencies[0].isSimple());

    EXPECT_EQ(m->dependencies[1].name, "ranged_dep");
    ASSERT_TRUE(m->dependencies[1].version.has_value());
    EXPECT_EQ(*m->dependencies[1].version, "^1.2.0");
    EXPECT_FALSE(m->dependencies[1].signer.has_value());
    EXPECT_FALSE(m->dependencies[1].isSimple());

    EXPECT_EQ(m->dependencies[2].name, "pinned_dep");
    ASSERT_TRUE(m->dependencies[2].version.has_value());
    EXPECT_EQ(*m->dependencies[2].version, ">=0.5.0");
    ASSERT_TRUE(m->dependencies[2].signer.has_value());
    EXPECT_EQ(*m->dependencies[2].signer, VALID_DID);
    EXPECT_FALSE(m->dependencies[2].isSimple());
}

// {"name": x} with no other key is semantically identical to the bare string.
TEST(ManifestDependencyTest, FromJson_NameOnlyObjectEqualsPlainString) {
    auto obj = Manifest::fromJson(manifestWithDeps(R"([{"name": "dep"}])"));
    auto str = Manifest::fromJson(manifestWithDeps(R"(["dep"])"));
    ASSERT_TRUE(obj.has_value());
    ASSERT_TRUE(str.has_value());
    EXPECT_EQ(obj->dependencies, str->dependencies);
    EXPECT_TRUE(obj->dependencies[0].isSimple());
}

TEST(ManifestDependencyTest, FromJson_SignerWithoutVersion) {
    auto m = Manifest::fromJson(manifestWithDeps(
        R"([{"name": "dep", "signer": ")" + std::string(VALID_DID) + R"("}])"));
    ASSERT_TRUE(m.has_value()) << Manifest::getLastError();
    ASSERT_EQ(m->dependencies.size(), 1u);
    EXPECT_FALSE(m->dependencies[0].version.has_value());
    ASSERT_TRUE(m->dependencies[0].signer.has_value());
    EXPECT_FALSE(m->dependencies[0].isSimple());
}

// --- parsing: every type-error branch ---------------------------------------

TEST(ManifestDependencyTest, FromJson_RejectsNonStringNonObjectEntry) {
    for (const char* bad : {"[42]", "[[\"dep\"]]", "[null]", "[true]"}) {
        auto m = Manifest::fromJson(manifestWithDeps(bad));
        EXPECT_FALSE(m.has_value()) << "should reject dependencies " << bad;
        EXPECT_NE(Manifest::getLastError().find("must be a string or object"),
                  std::string::npos) << Manifest::getLastError();
    }
}

TEST(ManifestDependencyTest, FromJson_RejectsObjectWithoutName) {
    for (const char* bad : {R"([{"version": "^1.0.0"}])",
                            R"([{"name": 42}])",
                            R"([{"name": null}])",
                            R"([{}])"}) {
        auto m = Manifest::fromJson(manifestWithDeps(bad));
        EXPECT_FALSE(m.has_value()) << "should reject dependencies " << bad;
        EXPECT_NE(Manifest::getLastError().find("missing required 'name'"),
                  std::string::npos) << Manifest::getLastError();
    }
}

TEST(ManifestDependencyTest, FromJson_RejectsNonStringVersion) {
    auto m = Manifest::fromJson(manifestWithDeps(R"([{"name": "dep", "version": 123}])"));
    EXPECT_FALSE(m.has_value());
    EXPECT_NE(Manifest::getLastError().find("has non-string 'version'"),
              std::string::npos) << Manifest::getLastError();
    // The name is interpolated into the message so the offender is identifiable.
    EXPECT_NE(Manifest::getLastError().find("dep"), std::string::npos);
}

TEST(ManifestDependencyTest, FromJson_RejectsNonStringSigner) {
    auto m = Manifest::fromJson(manifestWithDeps(R"([{"name": "dep", "signer": true}])"));
    EXPECT_FALSE(m.has_value());
    EXPECT_NE(Manifest::getLastError().find("has non-string 'signer'"),
              std::string::npos) << Manifest::getLastError();
}

// Unknown keys are ignored rather than rejected, so a manifest written by a
// newer tool still parses on an older one.
TEST(ManifestDependencyTest, FromJson_IgnoresUnknownDependencyKeys) {
    auto m = Manifest::fromJson(
        manifestWithDeps(R"([{"name": "dep", "version": "^1.0.0", "future_field": {"a": 1}}])"));
    ASSERT_TRUE(m.has_value()) << Manifest::getLastError();
    ASSERT_EQ(m->dependencies.size(), 1u);
    EXPECT_EQ(m->dependencies[0].name, "dep");
    EXPECT_EQ(*m->dependencies[0].version, "^1.0.0");
}

// --- serialisation: the isSimple() 0.2.0 byte-compat branch ------------------

// A name-only dependency must serialise back as a bare string so a 0.2.0
// manifest that never used ranges round-trips byte-identically.
TEST(ManifestDependencyTest, ToJson_SimpleDependenciesEmitAsPlainStrings) {
    auto m = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(m.has_value());

    const std::string json = m->toJson();
    EXPECT_NE(json.find(R"("dep1")"), std::string::npos) << json;
    // No object form anywhere in the dependencies array.
    EXPECT_EQ(json.find(R"("name": "dep1")"), std::string::npos) << json;
}

// ...and the object form survives as an object, keeping both optional fields.
TEST(ManifestDependencyTest, ToJson_RoundtripPreservesVersionAndSigner) {
    const std::string src = manifestWithDeps(
        R"([
          "plain_dep",
          {"name": "ranged_dep", "version": "^1.2.0"},
          {"name": "pinned_dep", "version": ">=0.5.0", "signer": ")" + std::string(VALID_DID) + R"("}
        ])");

    auto original = Manifest::fromJson(src);
    ASSERT_TRUE(original.has_value()) << Manifest::getLastError();

    auto parsed = Manifest::fromJson(original->toJson());
    ASSERT_TRUE(parsed.has_value()) << Manifest::getLastError();

    EXPECT_EQ(parsed->dependencies, original->dependencies);
}

// Serialisation is idempotent: a second pass changes nothing. This is what
// makes `lgx add` on a package with object-form deps hash-stable.
TEST(ManifestDependencyTest, ToJson_ObjectFormIsIdempotent) {
    auto m = Manifest::fromJson(manifestWithDeps(
        R"([{"name": "dep", "version": "^1.2.0", "signer": ")" + std::string(VALID_DID) + R"("}])"));
    ASSERT_TRUE(m.has_value());

    const std::string once = m->toJson();
    auto reparsed = Manifest::fromJson(once);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->toJson(), once);
}

// A name-only OBJECT normalises down to a string — the round-trip preserves
// meaning, not bytes. Pinned so the narrowing stays deliberate.
TEST(ManifestDependencyTest, ToJson_NameOnlyObjectNarrowsToString) {
    auto m = Manifest::fromJson(manifestWithDeps(R"([{"name": "dep"}])"));
    ASSERT_TRUE(m.has_value());

    const std::string json = m->toJson();

    // Look only inside the dependencies array — the manifest itself has a
    // top-level "name" field that would otherwise match.
    const size_t open = json.find("\"dependencies\"");
    ASSERT_NE(open, std::string::npos) << json;
    const size_t lo = json.find('[', open);
    const size_t hi = json.find(']', lo);
    ASSERT_NE(lo, std::string::npos);
    ASSERT_NE(hi, std::string::npos);
    const std::string depsArr = json.substr(lo, hi - lo + 1);

    EXPECT_NE(depsArr.find(R"("dep")"), std::string::npos) << depsArr;
    EXPECT_EQ(depsArr.find(R"("name")"), std::string::npos) << depsArr;
}

// A dependency carrying only a signer must NOT lose it on the way out — the
// isSimple() check keys off both optionals, not just `version`.
TEST(ManifestDependencyTest, ToJson_SignerOnlyDependencySurvives) {
    auto m = Manifest::fromJson(manifestWithDeps(
        R"([{"name": "dep", "signer": ")" + std::string(VALID_DID) + R"("}])"));
    ASSERT_TRUE(m.has_value());

    auto parsed = Manifest::fromJson(m->toJson());
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->dependencies.size(), 1u);
    ASSERT_TRUE(parsed->dependencies[0].signer.has_value());
    EXPECT_EQ(*parsed->dependencies[0].signer, VALID_DID);
    EXPECT_FALSE(parsed->dependencies[0].version.has_value());
}

// --- validate(): the semver range rule --------------------------------------

TEST(ManifestDependencyTest, Validate_AcceptsWellFormedRanges) {
    for (const char* good : {"^1.2.0", "~1.2.3", ">=1.2 <2.0", "1.2.x", "*",
                             "1.x", "^1.0.0 || ^2.0.0", "latest", "=1.0.0",
                             ">=1.0.0-rc.1"}) {
        Manifest m;
        m.manifestVersion = "0.3.0";
        m.name = "test";
        m.version = "1.0.0";
        m.dependencies.push_back(Dependency("dep"));
        m.dependencies[0].version = good;

        auto result = m.validate();
        EXPECT_TRUE(result.valid) << "should accept range '" << good << "': "
                                  << (result.errors.empty() ? "" : result.errors[0]);
    }
}

TEST(ManifestDependencyTest, Validate_RejectsMalformedRanges) {
    // Note "1.2.3 - 2.3.4": npm hyphen ranges are deliberately NOT supported.
    // They are rejected outright rather than misread — see the matching case in
    // test_semver.cpp. The rest previously slipped through a looser regex.
    for (const char* bad : {"garbage!!", "", "1.2.3 - 2.3.4", "^1.0.0 ||",
                            "1.2.3.4", "1.x.3", "x.1", "1..2"}) {
        Manifest m;
        m.manifestVersion = "0.3.0";
        m.name = "test";
        m.version = "1.0.0";
        m.dependencies.push_back(Dependency("dep"));
        m.dependencies[0].version = bad;

        auto result = m.validate();
        EXPECT_FALSE(result.valid) << "should reject range '" << bad << "'";
        ASSERT_FALSE(result.errors.empty());
        EXPECT_NE(result.errors[0].find("invalid semver range"), std::string::npos)
            << result.errors[0];
        // The offending dependency and the offending value are both named.
        EXPECT_NE(result.errors[0].find("dep"), std::string::npos);
    }
}

// An absent `version` means "any version" and must not be validated as if it
// were an empty range (an empty range is invalid; an absent one is not).
TEST(ManifestDependencyTest, Validate_AbsentVersionIsNotAnEmptyRange) {
    Manifest m;
    m.manifestVersion = "0.3.0";
    m.name = "test";
    m.version = "1.0.0";
    m.dependencies.push_back(Dependency("dep"));   // version stays nullopt

    auto result = m.validate();
    EXPECT_TRUE(result.valid) << (result.errors.empty() ? "" : result.errors[0]);
}

// --- validate(): the signer DID rule ----------------------------------------

TEST(ManifestDependencyTest, Validate_AcceptsWellFormedDid) {
    Manifest m;
    m.manifestVersion = "0.3.0";
    m.name = "test";
    m.version = "1.0.0";
    m.dependencies.push_back(Dependency("dep"));
    m.dependencies[0].signer = VALID_DID;

    auto result = m.validate();
    EXPECT_TRUE(result.valid) << (result.errors.empty() ? "" : result.errors[0]);
}

TEST(ManifestDependencyTest, Validate_RejectsMalformedDid) {
    for (const char* bad : {"not-a-did",
                            "",
                            "did:pkh:eip155:1:0xabc",   // wrong DID method
                            "did:jwk:",                 // empty payload
                            "did:jwk:has spaces",
                            "did:jwk:has/slash",        // not base64url
                            "DID:JWK:abc"}) {           // scheme is case-sensitive
        Manifest m;
        m.manifestVersion = "0.3.0";
        m.name = "test";
        m.version = "1.0.0";
        m.dependencies.push_back(Dependency("dep"));
        m.dependencies[0].signer = bad;

        auto result = m.validate();
        EXPECT_FALSE(result.valid) << "should reject signer '" << bad << "'";
        ASSERT_FALSE(result.errors.empty());
        EXPECT_NE(result.errors[0].find("invalid signer DID"), std::string::npos)
            << result.errors[0];
    }
}

// --- validate(): the name rules ---------------------------------------------

TEST(ManifestDependencyTest, Validate_RejectsEmptyDependencyName) {
    Manifest m;
    m.manifestVersion = "0.3.0";
    m.name = "test";
    m.version = "1.0.0";
    m.dependencies.push_back(Dependency(""));

    auto result = m.validate();
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("empty name"), std::string::npos)
        << result.errors[0];
}

// An empty name short-circuits with `continue`, so a bad range on the SAME
// entry must not also be reported — one error per broken entry.
TEST(ManifestDependencyTest, Validate_EmptyNameSuppressesOtherErrorsOnSameEntry) {
    Manifest m;
    m.manifestVersion = "0.3.0";
    m.name = "test";
    m.version = "1.0.0";
    m.dependencies.push_back(Dependency(""));
    m.dependencies[0].version = "garbage!!";

    auto result = m.validate();
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("empty name"), std::string::npos);
}

TEST(ManifestDependencyTest, Validate_RejectsNonLowercaseDependencyName) {
    Manifest m;
    m.manifestVersion = "0.3.0";
    m.name = "test";
    m.version = "1.0.0";
    m.dependencies.push_back(Dependency("NotLowerCase"));

    auto result = m.validate();
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("is not lowercase"), std::string::npos)
        << result.errors[0];
}

// Each broken dependency contributes its own error; they do not mask each other.
TEST(ManifestDependencyTest, Validate_ReportsEveryBrokenDependency) {
    Manifest m;
    m.manifestVersion = "0.3.0";
    m.name = "test";
    m.version = "1.0.0";

    m.dependencies.push_back(Dependency("bad_range"));
    m.dependencies[0].version = "garbage!!";
    m.dependencies.push_back(Dependency("bad_signer"));
    m.dependencies[1].signer = "nope";
    m.dependencies.push_back(Dependency("Uppercase"));
    m.dependencies.push_back(Dependency("fine"));      // must NOT produce an error

    auto result = m.validate();
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errors.size(), 3u);
}

// --- Dependency value semantics ---------------------------------------------

TEST(ManifestDependencyTest, Equality_DistinguishesVersionAndSigner) {
    Dependency base("dep");

    Dependency sameName("dep");
    EXPECT_EQ(base, sameName);

    Dependency ranged("dep");
    ranged.version = "^1.0.0";
    EXPECT_NE(base, ranged);

    Dependency otherRange("dep");
    otherRange.version = "^2.0.0";
    EXPECT_NE(ranged, otherRange);

    Dependency pinned("dep");
    pinned.version = "^1.0.0";
    pinned.signer = VALID_DID;
    EXPECT_NE(ranged, pinned);

    Dependency pinnedCopy("dep");
    pinnedCopy.version = "^1.0.0";
    pinnedCopy.signer = VALID_DID;
    EXPECT_EQ(pinned, pinnedCopy);
}

TEST(ManifestDependencyTest, ToString_RendersConstraints) {
    Dependency plain("dep");
    EXPECT_EQ(plain.toString(), "dep");

    Dependency ranged("dep");
    ranged.version = "^1.2.0";
    EXPECT_EQ(ranged.toString(), "dep ^1.2.0");

    Dependency pinned("dep");
    pinned.version = "^1.2.0";
    pinned.signer = "did:jwk:abc";
    EXPECT_EQ(pinned.toString(), "dep ^1.2.0 [signer=did:jwk:abc]");

    Dependency signerOnly("dep");
    signerOnly.signer = "did:jwk:abc";
    EXPECT_EQ(signerOnly.toString(), "dep [signer=did:jwk:abc]");
}

// compareMetadata() must treat a changed CONSTRAINT as a difference, not just a
// changed set of names — otherwise two variants of a package could be merged
// while disagreeing about which version of a dependency they need.
TEST(ManifestDependencyTest, CompareMetadata_DetectsDifferingConstraints) {
    const std::string src = manifestWithDeps(R"([{"name": "dep", "version": "^1.0.0"}])");

    auto a = Manifest::fromJson(src);
    auto b = Manifest::fromJson(src);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(a->compareMetadata(*b).valid);

    // Same name, different range.
    b->dependencies[0].version = "^2.0.0";
    auto result = a->compareMetadata(*b);
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("dependencies"), std::string::npos);

    // Same name and range, different signer pin.
    b->dependencies[0].version = "^1.0.0";
    b->dependencies[0].signer = VALID_DID;
    EXPECT_FALSE(a->compareMetadata(*b).valid);
}

// A bare string and a name-only object are the SAME dependency, so a manifest
// that merely rewrote its notation must not read as a metadata mismatch.
TEST(ManifestDependencyTest, CompareMetadata_StringAndNameOnlyObjectMatch) {
    auto a = Manifest::fromJson(manifestWithDeps(R"(["dep"])"));
    auto b = Manifest::fromJson(manifestWithDeps(R"([{"name": "dep"}])"));
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    EXPECT_TRUE(a->compareMetadata(*b).valid);
}

// =============================================================================
// optional_dependencies / interface_dependencies (manifestVersion 0.6.0)
//
// The two dependency kinds that are NOT the required closure. Both are absent
// from every 0.5.0-and-earlier package, so the parse must treat "missing" as
// "none declared" rather than as a validation failure.
// =============================================================================

static const char* MANIFEST_WITH_OPTIONAL_DEPS = R"({
  "manifestVersion": "0.6.0",
  "name": "consumer",
  "version": "1.0.0",
  "description": "Consumer",
  "author": "Test",
  "type": "library",
  "category": "test",
  "icon": "icon.png",
  "dependencies": ["hard_dep"],
  "optional_dependencies": ["opt_plain", {"name": "opt_ranged", "version": "^1.2.0"}],
  "interface_dependencies": ["storage", "telemetry"],
  "main": { "linux-amd64": "lib/test.so" }
})";

TEST(ManifestOptionalDepsTest, ParsesBothNewArrays) {
    auto manifest = Manifest::fromJson(MANIFEST_WITH_OPTIONAL_DEPS);
    ASSERT_TRUE(manifest.has_value());

    // Required stays exactly what it was — the split is the whole point.
    ASSERT_EQ(manifest->dependencies.size(), 1u);
    EXPECT_EQ(manifest->dependencies[0].name, "hard_dep");

    // Optional accepts both on-disk forms, constraints included.
    ASSERT_EQ(manifest->optionalDependencies.size(), 2u);
    EXPECT_EQ(manifest->optionalDependencies[0].name, "opt_plain");
    EXPECT_TRUE(manifest->optionalDependencies[0].isSimple());
    EXPECT_EQ(manifest->optionalDependencies[1].name, "opt_ranged");
    ASSERT_TRUE(manifest->optionalDependencies[1].version.has_value());
    EXPECT_EQ(*manifest->optionalDependencies[1].version, "^1.2.0");

    EXPECT_EQ(manifest->interfaceDependencies,
              (std::vector<std::string>{"storage", "telemetry"}));
}

TEST(ManifestOptionalDepsTest, OlderManifestWithoutTheKeysStaysValid) {
    // VALID_MANIFEST_JSON is a 0.1.0 document declaring neither key.
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->optionalDependencies.empty());
    EXPECT_TRUE(manifest->interfaceDependencies.empty());
    EXPECT_TRUE(manifest->validate().valid);
}

TEST(ManifestOptionalDepsTest, RoundTripsThroughJson) {
    auto manifest = Manifest::fromJson(MANIFEST_WITH_OPTIONAL_DEPS);
    ASSERT_TRUE(manifest.has_value());

    auto reparsed = Manifest::fromJson(manifest->toJson());
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->optionalDependencies, manifest->optionalDependencies);
    EXPECT_EQ(reparsed->interfaceDependencies, manifest->interfaceDependencies);
}

TEST(ManifestOptionalDepsTest, AbsentKeysAreNotEmitted) {
    // A package declaring neither must serialize exactly as earlier tooling did,
    // so existing packages do not churn when rebuilt.
    auto manifest = Manifest::fromJson(VALID_MANIFEST_JSON);
    ASSERT_TRUE(manifest.has_value());
    const std::string out = manifest->toJson();
    EXPECT_EQ(out.find("optional_dependencies"), std::string::npos);
    EXPECT_EQ(out.find("interface_dependencies"), std::string::npos);
}

TEST(ManifestOptionalDepsTest, NonArrayIsRejected) {
    const char* json = R"({
      "manifestVersion": "0.6.0", "name": "x", "version": "1.0.0",
      "description": "", "author": "", "type": "library", "category": "test",
      "icon": "i.png", "dependencies": [], "optional_dependencies": "nope",
      "main": {}
    })";
    EXPECT_FALSE(Manifest::fromJson(json).has_value());
}

TEST(ManifestOptionalDepsTest, InterfaceEntryMustBeAName) {
    // The build-time object form ({name, file, impl_class}) is deliberately not
    // accepted here: those paths mean nothing in a built package, and silently
    // dropping them would make the manifest look like it carried information
    // it did not.
    const char* json = R"({
      "manifestVersion": "0.6.0", "name": "x", "version": "1.0.0",
      "description": "", "author": "", "type": "library", "category": "test",
      "icon": "i.png", "dependencies": [],
      "interface_dependencies": [{"name": "storage", "file": "storage.lidl"}],
      "main": {}
    })";
    EXPECT_FALSE(Manifest::fromJson(json).has_value());
}

TEST(ManifestOptionalDepsTest, CompareMetadataCatchesAMismatch) {
    // Non-variant fields: two builds of one package that disagree about what it
    // can call are not two variants of the same package.
    auto a = Manifest::fromJson(MANIFEST_WITH_OPTIONAL_DEPS);
    auto b = Manifest::fromJson(MANIFEST_WITH_OPTIONAL_DEPS);
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_TRUE(a->compareMetadata(*b).valid);

    b->optionalDependencies.pop_back();
    EXPECT_FALSE(a->compareMetadata(*b).valid);

    auto c = Manifest::fromJson(MANIFEST_WITH_OPTIONAL_DEPS);
    ASSERT_TRUE(c.has_value());
    c->interfaceDependencies.clear();
    EXPECT_FALSE(a->compareMetadata(*c).valid);
}
