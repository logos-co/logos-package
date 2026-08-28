#include <gtest/gtest.h>

#include "core/main_resolution.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace lgx;
namespace fs = std::filesystem;

namespace {

const char* const kManifestWithMap = R"({
  "name": "accounts_module",
  "version": "1.0.1",
  "type": "core",
  "main": {
    "darwin-arm64": "accounts_module_plugin.dylib",
    "linux-amd64": "accounts_module_plugin.so"
  }
})";

} // namespace

// Resolving a manifest's `main` against a directory on disk. Every case here
// used to be answered twice — by the package manager, which collapsed all of
// them to one empty string, and by the module library, which did not.
class MainResolutionTest : public ::testing::Test {
protected:
    fs::path tempDir;
    fs::path moduleDir;

    void SetUp() override {
        tempDir = fs::temp_directory_path() /
                  ("lgx_main_" + std::to_string(::rand()));
        moduleDir = tempDir / "my_module";
        fs::create_directories(moduleDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }

    void writeFile(const fs::path& path, const std::string& content) {
        fs::create_directories(path.parent_path());
        std::ofstream f(path, std::ios::binary);
        f << content;
    }
};

// =============================================================================
// Variant map: which key wins
// =============================================================================

TEST_F(MainResolutionTest, VariantMapResolvesAgainstCallerSuppliedVariants) {
    writeFile(moduleDir / "accounts_module_plugin.dylib", "MACHO");

    MainFile main = resolveMain(moduleDir, kManifestWithMap, {"darwin-arm64"});

    ASSERT_TRUE(main.isResolved());
    EXPECT_EQ(main.variant, "darwin-arm64");
    EXPECT_EQ(main.declaredPath, "accounts_module_plugin.dylib");
    EXPECT_EQ(main.path, (moduleDir / "accounts_module_plugin.dylib").string());
    EXPECT_TRUE(fs::path(main.path).is_absolute());
}

TEST_F(MainResolutionTest, FirstMatchingVariantWinsAndDoesNotFallThrough) {
    // The rule both consumers already had, and the one most at risk from a
    // rewrite: a matched variant whose file is missing loses OUTRIGHT rather
    // than handing the resolve to the next candidate.
    writeFile(moduleDir / "accounts_module_plugin.so", "ELF");

    MainFile main = resolveMain(moduleDir, kManifestWithMap,
                                {"darwin-arm64", "linux-amd64"});

    EXPECT_EQ(main.state, MainResolution::FileMissing);
    EXPECT_EQ(main.variant, "darwin-arm64");
    EXPECT_TRUE(main.path.empty());
}

TEST_F(MainResolutionTest, NamedFileMissingIsFileMissingNotNoVariantMatch) {
    MainFile main = resolveMain(moduleDir, kManifestWithMap, {"darwin-arm64"});

    EXPECT_EQ(main.state, MainResolution::FileMissing);
    EXPECT_EQ(main.declaredPath, "accounts_module_plugin.dylib");
    EXPECT_TRUE(main.path.empty());
    EXPECT_FALSE(main.error.empty());
}

TEST_F(MainResolutionTest, NoCandidateVariantIsAKey) {
    MainFile main = resolveMain(moduleDir, kManifestWithMap, {"windows-x86_64"});

    EXPECT_EQ(main.state, MainResolution::NoVariantMatch);
    EXPECT_TRUE(main.declaredPath.empty());
    EXPECT_NE(main.error.find("windows-x86_64"), std::string::npos) << main.error;
}

TEST_F(MainResolutionTest, AnEmptyCandidateListIsNoVariantMatch) {
    MainFile main = resolveMain(moduleDir, kManifestWithMap, {});

    EXPECT_EQ(main.state, MainResolution::NoVariantMatch);
}

TEST_F(MainResolutionTest, VariantKeysAreMatchedVerbatim) {
    // The write path lowercases variant keys; a directory on disk is read as
    // it was written, and case-folding here would invent a match the hash tree
    // does not have.
    writeFile(moduleDir / "accounts_module_plugin.dylib", "MACHO");

    MainFile main = resolveMain(moduleDir, kManifestWithMap, {"Darwin-ARM64"});

    EXPECT_EQ(main.state, MainResolution::NoVariantMatch);
}

// =============================================================================
// Unusable entries: fall through, but say so
// =============================================================================

TEST_F(MainResolutionTest, AnEmptyMapValueFallsThroughAndIsMalformedEntry) {
    MainFile main = resolveMain(moduleDir, R"({"main":{"darwin-arm64":""}})",
                                {"darwin-arm64"});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_EQ(main.variant, "darwin-arm64");
}

TEST_F(MainResolutionTest, ANonStringMapValueIsMalformedEntryAndDoesNotThrow) {
    // The package manager called .get<std::string>() on whatever was under a
    // matched key, so this shape threw out of a scan of the modules dir.
    MainFile main;
    ASSERT_NO_THROW(main = resolveMain(moduleDir, R"({"main":{"darwin-arm64":42}})",
                                       {"darwin-arm64"}));

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_EQ(main.variant, "darwin-arm64");
}

TEST_F(MainResolutionTest, AnUnusableValueStillLetsALaterVariantWin) {
    writeFile(moduleDir / "p.so", "ELF");

    MainFile main = resolveMain(
        moduleDir, R"({"main":{"darwin-arm64":"","linux-amd64":"p.so"}})",
        {"darwin-arm64", "linux-amd64"});

    ASSERT_TRUE(main.isResolved());
    EXPECT_EQ(main.variant, "linux-amd64");
}

// =============================================================================
// The plain-string form, which the manifest parser rejects outright
// =============================================================================

TEST_F(MainResolutionTest, PlainStringForm) {
    writeFile(moduleDir / "my_module_plugin.so", "ELF");

    MainFile main = resolveMain(moduleDir, R"({"main":"my_module_plugin.so"})", {});

    ASSERT_TRUE(main.isResolved());
    EXPECT_EQ(main.declaredPath, "my_module_plugin.so");
    EXPECT_TRUE(main.variant.empty());
}

TEST_F(MainResolutionTest, AnEmptyStringIsMalformedEntry) {
    MainFile main = resolveMain(moduleDir, R"({"main":""})", {});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
}

TEST_F(MainResolutionTest, NeitherMapNorStringIsMalformedEntry) {
    MainFile main = resolveMain(moduleDir, R"({"main":42})", {});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_FALSE(main.error.empty());
}

TEST_F(MainResolutionTest, NotDeclared) {
    MainFile main = resolveMain(moduleDir, R"({"name":"my_module"})", {});

    EXPECT_EQ(main.state, MainResolution::NotDeclared);
    EXPECT_TRUE(main.error.empty());
}

// =============================================================================
// What is on disk: containment, and file-ness
// =============================================================================

TEST_F(MainResolutionTest, EscapingTheDirectoryIsRefused) {
    // The directory IS the package. The escaping path is made to EXIST, so a
    // plain fs::exists() check would resolve it and hand back a file the
    // package does not own.
    writeFile(tempDir / "outside_plugin.so", "ELF");

    MainFile main = resolveMain(moduleDir, R"({"main":"../outside_plugin.so"})", {});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_TRUE(main.path.empty());
}

TEST_F(MainResolutionTest, AnAbsolutePathIsRefused) {
    writeFile(tempDir / "outside_plugin.so", "ELF");
    const std::string manifest =
        R"({"main":")" + (tempDir / "outside_plugin.so").generic_string() + R"("})";

    MainFile main = resolveMain(moduleDir, manifest, {});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_TRUE(main.path.empty());
}

TEST_F(MainResolutionTest, ADotSegmentThatStaysInsideStillResolves) {
    // Containment is lexical, so it must not refuse a path that merely LOOKS
    // like traversal while landing back inside the package.
    writeFile(moduleDir / "lib" / "p.so", "ELF");

    MainFile main = resolveMain(moduleDir, R"({"main":"lib/../lib/p.so"})", {});

    ASSERT_TRUE(main.isResolved());
    EXPECT_EQ(main.path, (moduleDir / "lib" / "p.so").string());
}

TEST_F(MainResolutionTest, NonNormalComponentsResolveAfterNormalising) {
    // Containment is checked LEXICALLY, so "./sub/../plugin.so" normalises to
    // "plugin.so" and resolves even though `sub` does not exist. lgpm's old
    // fs::exists() on the raw path failed here. Deliberate: a lexical guard
    // also refuses a `..` that would traverse a SYMLINKED intermediate out of
    // the package, which the old check would have handed back for dlopen.
    writeFile(moduleDir / "plugin.so", "ELF");

    MainFile main = resolveMain(moduleDir, R"({"main":"./sub/../plugin.so"})", {});

    EXPECT_EQ(main.state, MainResolution::Resolved);
    EXPECT_FALSE(main.path.empty());
}

TEST_F(MainResolutionTest, NonNormalComponentsCannotEscape) {
    // The same normalisation must not become a way out of the directory.
    writeFile(tempDir / "outside_plugin.so", "ELF");

    MainFile main = resolveMain(moduleDir, R"({"main":"./sub/../../outside_plugin.so"})", {});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_TRUE(main.path.empty());
}

TEST_F(MainResolutionTest, NamingADirectoryIsNotResolved) {
    // exists() is true for a directory, which would hand the caller a path it
    // can never load.
    fs::create_directories(moduleDir / "not_a_plugin");

    MainFile main = resolveMain(moduleDir, R"({"main":"not_a_plugin"})", {});

    EXPECT_EQ(main.state, MainResolution::MalformedEntry);
    EXPECT_TRUE(main.path.empty());
}

TEST_F(MainResolutionTest, ASymlinkToAFileStillResolves) {
    writeFile(moduleDir / "real_plugin.so", "ELF");
    std::error_code ec;
    fs::create_symlink(moduleDir / "real_plugin.so", moduleDir / "link_plugin.so", ec);
    if (ec) GTEST_SKIP() << "symlinks unavailable: " << ec.message();

    MainFile main = resolveMain(moduleDir, R"({"main":"link_plugin.so"})", {});

    EXPECT_EQ(main.state, MainResolution::Resolved);
}

TEST_F(MainResolutionTest, AMissingDirectoryHasNoMain) {
    MainFile main = resolveMain(tempDir / "nope", kManifestWithMap, {"darwin-arm64"});

    EXPECT_EQ(main.state, MainResolution::FileMissing);
}

// =============================================================================
// Caller input this cannot answer at all
// =============================================================================

TEST_F(MainResolutionTest, AnUnparseableManifestIsBadInput) {
    MainFile main = resolveMain(moduleDir, "{ not json", {});

    EXPECT_EQ(main.state, MainResolution::BadInput);
    EXPECT_FALSE(main.error.empty());
}

TEST_F(MainResolutionTest, AJsonArrayIsBadInputNotNotDeclared) {
    MainFile main = resolveMain(moduleDir, R"(["main"])", {});

    EXPECT_EQ(main.state, MainResolution::BadInput);
}

TEST_F(MainResolutionTest, NoDirectoryIsBadInput) {
    MainFile main = resolveMain(fs::path(), R"({"main":"p.so"})", {});

    EXPECT_EQ(main.state, MainResolution::BadInput);
}
