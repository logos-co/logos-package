#include <gtest/gtest.h>
#include "core/package.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <utility>
#include <vector>

#include "test_png.h"
#include <sstream>

namespace fs = std::filesystem;

class CLITest : public ::testing::Test {
protected:
    fs::path tempDir;
    fs::path lgxBinary;
    
    void SetUp() override {
        tempDir = fs::temp_directory_path() / ("lgx_cli_test_" + std::to_string(rand()));
        fs::create_directories(tempDir);
        
        // Check for LGX_BINARY environment variable first
        const char* envBinary = std::getenv("LGX_BINARY");
        if (envBinary && fs::exists(envBinary)) {
            lgxBinary = envBinary;
            return;
        }
        
        // Find the lgx binary - try multiple locations
        std::vector<fs::path> searchPaths = {
            fs::current_path().parent_path() / "lgx",        // Running from tests/ subdir
            fs::current_path() / "lgx",                       // Running from build root
            fs::current_path() / "build" / "lgx",            // Running from project root
            fs::current_path().parent_path() / "build" / "lgx" // Running from build/tests
        };
        
        for (const auto& path : searchPaths) {
            if (fs::exists(path)) {
                lgxBinary = path;
                return;
            }
        }
        
        // Binary not found, skip tests
        GTEST_SKIP() << "lgx binary not found. Set LGX_BINARY env var or tried: " 
                     << searchPaths[0] << ", " << searchPaths[1] << ", " 
                     << searchPaths[2] << ", " << searchPaths[3];
    }
    
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }
    
    // Helper to run lgx command
    int runLgx(const std::string& args, std::string* output = nullptr) {
        std::string cmd = lgxBinary.string() + " " + args;
        if (output) {
            cmd += " 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return -1;
            
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                *output += buffer;
            }
            int status = pclose(pipe);
            return WEXITSTATUS(status);
        } else {
            int status = system(cmd.c_str());
            return WEXITSTATUS(status);
        }
    }
};

// Test: lgx create <name>
// Verifies that the CLI can create a new skeleton package
TEST_F(CLITest, CreateCommand) {
    fs::path pkgPath = tempDir / "test.lgx";
    
    std::string output;
    int exitCode = runLgx("create " + (tempDir / "test").string(), &output);
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(fs::exists(pkgPath));
    EXPECT_NE(output.find("Created package"), std::string::npos);
}

// Test: lgx verify <valid-package>
// Verifies that the CLI correctly validates a well-formed package
// Commands: lgx create, lgx verify
TEST_F(CLITest, VerifyCommand_ValidPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    runLgx("create " + (tempDir / "test").string());
    
    std::string output;
    int exitCode = runLgx("verify " + pkgPath.string(), &output);
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("valid"), std::string::npos);
}

// Test: lgx verify <invalid-package>
// Verifies that the CLI correctly rejects an invalid package file
// Commands: lgx verify
TEST_F(CLITest, VerifyCommand_InvalidPackage) {
    fs::path invalidPkg = tempDir / "invalid.lgx";
    std::ofstream(invalidPkg) << "not a valid package";
    
    std::string output;
    int exitCode = runLgx("verify " + invalidPkg.string(), &output);
    
    EXPECT_NE(exitCode, 0);  // Should fail
    EXPECT_FALSE(output.empty());  // Should have error message
}

// Test: lgx add <pkg> --variant <v> --files <single-file> -y
// Verifies adding a single file to a variant and that package remains valid
// Commands: lgx create, lgx add, lgx verify
TEST_F(CLITest, AddCommand_SingleFile) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testFile = tempDir / "lib.so";
    
    // Create package and test file
    runLgx("create " + (tempDir / "test").string());
    std::ofstream(testFile) << "test content";
    
    // Add variant
    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v linux-amd64 -f " + 
        testFile.string() + " -y",
        &output
    );
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Added"), std::string::npos);
    
    // Verify still valid
    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);
}

// Test: lgx add <pkg> --variant <v> --files <directory> --main <path> -y
// Verifies adding a directory to a variant with explicit main entry
// Commands: lgx create, lgx add, lgx verify
TEST_F(CLITest, AddCommand_Directory) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testDir = tempDir / "dist";
    
    // Create package and test directory
    runLgx("create " + (tempDir / "test").string());
    fs::create_directories(testDir);
    std::ofstream(testDir / "index.js") << "console.log('hello')";
    std::ofstream(testDir / "lib.js") << "export {}";
    
    // Add variant with directory
    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v web -f " + 
        testDir.string() + " --main index.js -y",
        &output
    );
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Added"), std::string::npos);
    
    // Verify still valid
    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);
}

// Test: lgx add ... --assets <directory>
// Verifies that generic assets are written once at package root rather than
// copied under the platform variant.
TEST_F(CLITest, AddCommand_PlatformIndependentAssets) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testFile = tempDir / "lib.so";
    fs::path assetsDir = tempDir / "assets";

    runLgx("create " + (tempDir / "test").string());
    std::ofstream(testFile) << "module";
    fs::create_directories(assetsDir / "lidl");
    std::ofstream(assetsDir / "lidl/test.lidl")
        << "module test {\n  depends []\n}\n";

    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v linux-amd64 -f " +
        testFile.string() + " --assets " + assetsDir.string() + " -y",
        &output
    );

    ASSERT_EQ(exitCode, 0) << output;
    auto pkg = lgx::Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());

    size_t rootAssetCount = 0;
    for (const auto& entry : pkg->getEntries()) {
        if (entry.path == "assets/lidl/test.lidl" && !entry.isDirectory)
            ++rootAssetCount;
        EXPECT_EQ(entry.path.find("linux-amd64/assets/"), std::string::npos);
    }
    EXPECT_EQ(rootAssetCount, 1u);
}

// Test: lgx add <pkg> --variant <existing-v> --files <new-file> -y
// Verifies variant replacement (no merge) - old content should be replaced
// Commands: lgx create, lgx add (twice), lgx verify
TEST_F(CLITest, AddCommand_ReplacesVariant) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path file1 = tempDir / "old.so";
    fs::path file2 = tempDir / "new.so";
    
    // Create package
    runLgx("create " + (tempDir / "test").string());
    
    // Add initial variant
    std::ofstream(file1) << "old content";
    runLgx("add " + pkgPath.string() + " -v linux-amd64 -f " + 
           file1.string() + " -y");
    
    // Replace with new file
    std::ofstream(file2) << "new content";
    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v linux-amd64 -f " + 
        file2.string() + " -y",
        &output
    );
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Replaced"), std::string::npos);
    
    // Verify package is still valid
    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);
}

// Test: lgx remove <pkg> --variant <v> -y
// Verifies removing a variant from a package
// Commands: lgx create, lgx add, lgx remove, lgx verify
TEST_F(CLITest, RemoveCommand) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testFile = tempDir / "lib.so";
    
    // Setup: create package and add variant
    runLgx("create " + (tempDir / "test").string());
    std::ofstream(testFile) << "test content";
    runLgx("add " + pkgPath.string() + " -v linux-amd64 -f " + 
           testFile.string() + " -y");
    
    // Remove variant
    std::string output;
    int exitCode = runLgx(
        "remove " + pkgPath.string() + " -v linux-amd64 -y",
        &output
    );
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Removed"), std::string::npos);
    
    // Verify package is still valid (empty but valid)
    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);
}

// Test: lgx remove <pkg> --variant <nonexistent>
// Verifies error handling when removing non-existent variant
// Commands: lgx create, lgx remove
TEST_F(CLITest, RemoveCommand_NonExistent) {
    fs::path pkgPath = tempDir / "test.lgx";
    
    // Create empty package
    runLgx("create " + (tempDir / "test").string());
    
    // Try to remove non-existent variant
    std::string output;
    int exitCode = runLgx(
        "remove " + pkgPath.string() + " -v nonexistent -y",
        &output
    );
    
    EXPECT_NE(exitCode, 0);  // Should fail
    EXPECT_FALSE(output.empty());  // Should have error message
}

// Test: lgx --help
// Verifies that help text is displayed correctly
// Commands: lgx --help
TEST_F(CLITest, HelpCommand) {
    std::string output;
    int exitCode = runLgx("--help", &output);
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Usage"), std::string::npos);
    EXPECT_NE(output.find("create"), std::string::npos);
    EXPECT_NE(output.find("add"), std::string::npos);
    EXPECT_NE(output.find("remove"), std::string::npos);
    EXPECT_NE(output.find("verify"), std::string::npos);
}

// Test: lgx --version
// Verifies that version information is displayed correctly
// Commands: lgx --version
TEST_F(CLITest, VersionCommand) {
    std::string output;
    int exitCode = runLgx("--version", &output);
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("0.1.0"), std::string::npos);
}

// Test: lgx create <name> (when file already exists)
// Verifies error handling when trying to create over existing file
// Commands: lgx create (twice)
TEST_F(CLITest, CreateCommand_FileExists) {
    fs::path pkgPath = tempDir / "test.lgx";
    
    // Create first time
    runLgx("create " + (tempDir / "test").string());
    
    // Try to create again
    std::string output;
    int exitCode = runLgx("create " + (tempDir / "test").string(), &output);
    
    EXPECT_NE(exitCode, 0);  // Should fail
    EXPECT_NE(output.find("exists"), std::string::npos);
}

// Test: lgx add <pkg> --variant <v> --files <directory> (without --main)
// Verifies error handling when adding directory without required --main flag
// Commands: lgx create, lgx add
TEST_F(CLITest, AddCommand_DirectoryWithoutMain) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testDir = tempDir / "dist";
    
    // Create package and test directory
    runLgx("create " + (tempDir / "test").string());
    fs::create_directories(testDir);
    std::ofstream(testDir / "file.txt") << "content";
    
    // Try to add directory without --main
    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v web -f " + testDir.string() + " -y",
        &output
    );
    
    EXPECT_NE(exitCode, 0);  // Should fail
    EXPECT_NE(output.find("required"), std::string::npos);
}

TEST_F(CLITest, AddCommand_UiQmlDirectoryWithoutMain) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testDir = tempDir / "dist";

    runLgx("create " + (tempDir / "test").string());
    fs::create_directories(testDir / "qml");
    std::ofstream(testDir / "qml" / "Main.qml") << "import QtQuick 2.15\nItem {}";

    auto pkgOpt = lgx::Package::load(pkgPath);
    ASSERT_TRUE(pkgOpt.has_value());
    pkgOpt->getManifest().type = "ui_qml";
    pkgOpt->getManifest().view = "qml/Main.qml";
    auto saveResult = pkgOpt->save(pkgPath);
    ASSERT_TRUE(saveResult.success);

    // 0.4.0 requires ui_qml packages to carry a conforming icon.
    const std::string iconPath =
        lgx_test::writePng((tempDir / "icon.png").string());

    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v darwin-arm64 -f " + testDir.string() +
        " --icon " + iconPath + " -y",
        &output
    );

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Added"), std::string::npos);

    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);
}

TEST_F(CLITest, AddCommand_UiQmlDirectoryWithViewFlag) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testDir = tempDir / "dist";

    runLgx("create " + (tempDir / "test").string());
    fs::create_directories(testDir / "qml");
    std::ofstream(testDir / "qml" / "Main.qml") << "import QtQuick 2.15\nItem {}";

    auto pkgOpt = lgx::Package::load(pkgPath);
    ASSERT_TRUE(pkgOpt.has_value());
    pkgOpt->getManifest().type = "ui_qml";
    auto saveResult = pkgOpt->save(pkgPath);
    ASSERT_TRUE(saveResult.success);

    // 0.4.0 requires ui_qml packages to carry a conforming icon.
    const std::string iconPath =
        lgx_test::writePng((tempDir / "icon.png").string());

    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v darwin-arm64 -f " + testDir.string()
            + " --view qml/Main.qml --icon " + iconPath + " -y",
        &output
    );

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Added"), std::string::npos);

    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);

    auto reloaded = lgx::Package::load(pkgPath);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->getManifest().view, "qml/Main.qml");
}

TEST_F(CLITest, AddCommand_UiQmlDirectoryWithoutView) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path testDir = tempDir / "dist";

    runLgx("create " + (tempDir / "test").string());
    fs::create_directories(testDir);
    std::ofstream(testDir / "file.txt") << "content";

    auto pkgOpt = lgx::Package::load(pkgPath);
    ASSERT_TRUE(pkgOpt.has_value());
    pkgOpt->getManifest().type = "ui_qml";
    auto saveResult = pkgOpt->save(pkgPath);
    ASSERT_TRUE(saveResult.success);

    // 0.4.0 requires ui_qml packages to carry a conforming icon.
    const std::string iconPath =
        lgx_test::writePng((tempDir / "icon.png").string());

    std::string output;
    int exitCode = runLgx(
        "add " + pkgPath.string() + " -v darwin-arm64 -f " + testDir.string() +
        " --icon " + iconPath + " -y",
        &output
    );

    EXPECT_NE(exitCode, 0);
    EXPECT_NE(output.find("view"), std::string::npos);
}

// =============================================================================
// Extract Command Tests
// =============================================================================

class ExtractCLITest : public CLITest {
protected:
    // test.lgx with a linux-amd64 variant and, optionally, root assets/lidl/a.lidl
    // and assets/icon.png.
    fs::path makePackage(bool withAssets) {
        fs::path pkgPath = tempDir / "test.lgx";
        fs::path lib = tempDir / "lib.so";
        std::ofstream(lib) << "binary";
        EXPECT_EQ(runLgx("create " + (tempDir / "test").string()), 0);

        std::string add = "add " + pkgPath.string() + " -v linux-amd64 -f " +
                          lib.string() + " -y";
        if (withAssets) {
            fs::path assets = tempDir / "assets-source";
            fs::create_directories(assets / "lidl");
            std::ofstream(assets / "lidl" / "a.lidl") << "module a {\n  depends []\n}\n";
            add += " --assets " + assets.string() + " --icon " +
                   lgx_test::writePng((tempDir / "icon.png").string());
        }
        std::string output;
        EXPECT_EQ(runLgx(add, &output), 0) << output;
        return pkgPath;
    }

    // Every path under `root`, relative and '/'-separated; directories end in '/'.
    static std::set<std::string> listTree(const fs::path& root) {
        std::set<std::string> tree;
        for (const auto& item : fs::recursive_directory_iterator(root)) {
            const std::string rel = fs::relative(item.path(), root).generic_string();
            tree.insert(item.is_directory() ? rel + "/" : rel);
        }
        return tree;
    }
};

// Test: lgx extract <pkg> --assets-only --output <dir>
// Only root assets land in <dir>/assets/; no variant is unpacked.
TEST_F(ExtractCLITest, AssetsOnly_WritesOnlyRootAssets) {
    fs::path pkgPath = makePackage(true);
    fs::path out = tempDir / "out";

    std::string output;
    int exitCode = runLgx("extract " + pkgPath.string() +
                          " --assets-only --output " + out.string(), &output);

    ASSERT_EQ(exitCode, 0) << output;
    EXPECT_NE(output.find("Extracted assets to"), std::string::npos) << output;
    EXPECT_EQ(listTree(out), (std::set<std::string>{
        "assets/", "assets/icon.png", "assets/lidl/", "assets/lidl/a.lidl"}));
}

// The flag takes no value, so it may also precede the package path.
TEST_F(ExtractCLITest, AssetsOnly_FlagBeforePackagePath) {
    fs::path pkgPath = makePackage(true);
    fs::path out = tempDir / "out";

    std::string output;
    int exitCode = runLgx("extract --assets-only " + pkgPath.string() +
                          " -o " + out.string(), &output);

    ASSERT_EQ(exitCode, 0) << output;
    EXPECT_TRUE(fs::exists(out / "assets" / "lidl" / "a.lidl"));
    EXPECT_FALSE(fs::exists(out / "linux-amd64"));
}

TEST_F(ExtractCLITest, AssetsOnly_PackageWithoutAssets) {
    fs::path pkgPath = makePackage(false);
    fs::path out = tempDir / "out";

    std::string output;
    int exitCode = runLgx("extract " + pkgPath.string() +
                          " --assets-only -o " + out.string(), &output);

    EXPECT_EQ(exitCode, 0) << output;
    EXPECT_NE(output.find("No assets to extract"), std::string::npos) << output;
    EXPECT_FALSE(fs::exists(out));
}

// Root assets are the same for every variant, and the flag is bare.
TEST_F(ExtractCLITest, AssetsOnly_UsageErrors) {
    fs::path pkgPath = makePackage(true);
    fs::path out = tempDir / "out";

    const std::vector<std::pair<std::string, std::string>> cases = {
        {"--assets-only --variant linux-amd64", "cannot be combined with --variant"},
        {"-v linux-amd64 --assets-only", "cannot be combined with --variant"},
        {"--assets-only=true", "takes no value"},
    };
    for (const auto& [flags, expected] : cases) {
        std::string output;
        int exitCode = runLgx("extract " + pkgPath.string() + " " + flags +
                              " -o " + out.string(), &output);

        EXPECT_NE(exitCode, 0) << flags;
        EXPECT_NE(output.find(expected), std::string::npos) << flags << ": " << output;
        EXPECT_FALSE(fs::exists(out)) << flags;
    }
}

// Test: lgx extract <pkg> --variant <v> --output <dir>
// Unchanged: the variant goes to <dir>/<v>/ with the root assets beside its files.
TEST_F(ExtractCLITest, Variant_KeepsRootAssetsInTheVariantDirectory) {
    fs::path pkgPath = makePackage(true);
    fs::path out = tempDir / "out";

    std::string output;
    int exitCode = runLgx("extract " + pkgPath.string() +
                          " --variant linux-amd64 --output " + out.string(), &output);

    ASSERT_EQ(exitCode, 0) << output;
    EXPECT_NE(output.find("Extracted variant 'linux-amd64'"), std::string::npos) << output;
    EXPECT_EQ(listTree(out), (std::set<std::string>{
        "linux-amd64/", "linux-amd64/lib.so",
        "linux-amd64/assets/", "linux-amd64/assets/icon.png",
        "linux-amd64/assets/lidl/", "linux-amd64/assets/lidl/a.lidl"}));
}

// =============================================================================
// Merge Command Tests
// =============================================================================

// Helper to create a single-variant .lgx package with metadata
void createSingleVariantPackage(
    const std::string& lgxBinary,
    const fs::path& pkgPath,
    const std::string& name,
    const std::string& variant,
    const std::string& fileContent
) {
    fs::path dir = pkgPath.parent_path();
    fs::path tmpFile = dir / (variant + "_file.so");

    // Create the package using the logical name (so manifest.name matches across packages)
    fs::path createdPath = dir / (name + ".lgx");
    std::string cmd = lgxBinary + " create " + (dir / name).string() + " 2>&1";
    system(cmd.c_str());
    if (createdPath != pkgPath) {
        fs::rename(createdPath, pkgPath);
    }

    // Create a test file and add as variant
    std::ofstream(tmpFile) << fileContent;
    cmd = lgxBinary + " add " + pkgPath.string() +
          " -v " + variant + " -f " + tmpFile.string() + " -y 2>&1";
    system(cmd.c_str());
}

// Test: lgx merge <pkg1.lgx> <pkg2.lgx>
// Verifies basic merge of two single-variant packages
TEST_F(CLITest, MergeCommand_BasicMerge) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path merged = tempDir / "merged.lgx";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "linux lib");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "test", "darwin-arm64", "darwin lib");

    std::string output;
    int exitCode = runLgx(
        "merge " + pkg1.string() + " " + pkg2.string() +
        " -o " + merged.string() + " -y",
        &output
    );

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Merged"), std::string::npos);
    EXPECT_TRUE(fs::exists(merged));

    // Verify the merged package is valid
    exitCode = runLgx("verify " + merged.string(), &output);
    EXPECT_EQ(exitCode, 0);
}

TEST_F(CLITest, MergeCommand_DeduplicatesIdenticalRootAssets) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path merged = tempDir / "merged.lgx";
    fs::path assets = tempDir / "assets";
    fs::create_directories(assets / "lidl");
    std::ofstream(assets / "lidl/test.lidl")
        << "module test {\n  depends []\n}\n";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "linux");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "test", "darwin-arm64", "darwin");
    ASSERT_EQ(runLgx("add " + pkg1.string() + " -v linux-amd64 -f " +
                     (tempDir / "linux-amd64_file.so").string() +
                     " --assets " + assets.string() + " -y"), 0);
    ASSERT_EQ(runLgx("add " + pkg2.string() + " -v darwin-arm64 -f " +
                     (tempDir / "darwin-arm64_file.so").string() +
                     " --assets " + assets.string() + " -y"), 0);

    std::string output;
    ASSERT_EQ(runLgx("merge " + pkg1.string() + " " + pkg2.string() +
                     " -o " + merged.string() + " -y", &output), 0) << output;

    auto pkg = lgx::Package::load(merged);
    ASSERT_TRUE(pkg.has_value());
    size_t count = 0;
    for (const auto& entry : pkg->getEntries())
        if (entry.path == "assets/lidl/test.lidl" && !entry.isDirectory)
            ++count;
    EXPECT_EQ(count, 1u);
}

TEST_F(CLITest, MergeCommand_RejectsConflictingRootAssets) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path merged = tempDir / "merged.lgx";
    fs::path assets1 = tempDir / "assets1";
    fs::path assets2 = tempDir / "assets2";
    fs::create_directories(assets1 / "lidl");
    fs::create_directories(assets2 / "lidl");
    std::ofstream(assets1 / "lidl/test.lidl") << "first";
    std::ofstream(assets2 / "lidl/test.lidl") << "second";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "linux");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "test", "darwin-arm64", "darwin");
    ASSERT_EQ(runLgx("add " + pkg1.string() + " -v linux-amd64 -f " +
                     (tempDir / "linux-amd64_file.so").string() +
                     " --assets " + assets1.string() + " -y"), 0);
    ASSERT_EQ(runLgx("add " + pkg2.string() + " -v darwin-arm64 -f " +
                     (tempDir / "darwin-arm64_file.so").string() +
                     " --assets " + assets2.string() + " -y"), 0);

    std::string output;
    int exitCode = runLgx(
        "merge " + pkg1.string() + " " + pkg2.string() +
        " -o " + merged.string() + " -y",
        &output
    );

    EXPECT_NE(exitCode, 0);
    EXPECT_NE(output.find("Asset conflict"), std::string::npos);
}

// Test: lgx merge with duplicate variants (should fail)
TEST_F(CLITest, MergeCommand_DuplicateVariantsFails) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path merged = tempDir / "merged.lgx";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "lib v1");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "test", "linux-amd64", "lib v2");

    std::string output;
    int exitCode = runLgx(
        "merge " + pkg1.string() + " " + pkg2.string() +
        " -o " + merged.string() + " -y",
        &output
    );

    EXPECT_NE(exitCode, 0);
    EXPECT_NE(output.find("Duplicate variant"), std::string::npos);
}

// Test: lgx merge with --skip-duplicates
TEST_F(CLITest, MergeCommand_SkipDuplicates) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path merged = tempDir / "merged.lgx";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "lib v1");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "test", "linux-amd64", "lib v2");

    std::string output;
    int exitCode = runLgx(
        "merge " + pkg1.string() + " " + pkg2.string() +
        " -o " + merged.string() + " --skip-duplicates -y",
        &output
    );

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("skipping"), std::string::npos);
    EXPECT_TRUE(fs::exists(merged));
}

// Test: lgx merge with manifest mismatch (should fail)
TEST_F(CLITest, MergeCommand_ManifestMismatch) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path merged = tempDir / "merged.lgx";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "pkg1", "linux-amd64", "lib1");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "pkg2", "darwin-arm64", "lib2");

    std::string output;
    int exitCode = runLgx(
        "merge " + pkg1.string() + " " + pkg2.string() +
        " -o " + merged.string() + " -y",
        &output
    );

    EXPECT_NE(exitCode, 0);
    EXPECT_NE(output.find("mismatch"), std::string::npos);
}

// Test: lgx merge with too few arguments
TEST_F(CLITest, MergeCommand_TooFewArgs) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "lib");

    std::string output;
    int exitCode = runLgx("merge " + pkg1.string(), &output);

    EXPECT_NE(exitCode, 0);
    EXPECT_NE(output.find("At least two"), std::string::npos);
}

// Test: lgx merge three packages
TEST_F(CLITest, MergeCommand_ThreePackages) {
    fs::path pkg1 = tempDir / "pkg1.lgx";
    fs::path pkg2 = tempDir / "pkg2.lgx";
    fs::path pkg3 = tempDir / "pkg3.lgx";
    fs::path merged = tempDir / "merged.lgx";

    createSingleVariantPackage(lgxBinary.string(), pkg1, "test", "linux-amd64", "linux amd64");
    createSingleVariantPackage(lgxBinary.string(), pkg2, "test", "linux-arm64", "linux arm64");
    createSingleVariantPackage(lgxBinary.string(), pkg3, "test", "darwin-arm64", "darwin arm64");

    std::string output;
    int exitCode = runLgx(
        "merge " + pkg1.string() + " " + pkg2.string() + " " + pkg3.string() +
        " -o " + merged.string() + " -y",
        &output
    );

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(fs::exists(merged));

    // Verify the merged package is valid
    exitCode = runLgx("verify " + merged.string(), &output);
    EXPECT_EQ(exitCode, 0);
}

// =============================================================================
// Multi-variant package workflow
// =============================================================================

// Test: Multi-variant package workflow
// Verifies creating a package with multiple variants
// Commands: lgx create, lgx add (multiple), lgx verify
TEST_F(CLITest, MultiVariantWorkflow) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path linuxLib = tempDir / "lib_linux.so";
    fs::path darwinLib = tempDir / "lib_darwin.dylib";
    
    // Create package
    runLgx("create " + (tempDir / "test").string());
    
    // Add Linux variant
    std::ofstream(linuxLib) << "linux library";
    runLgx("add " + pkgPath.string() + " -v linux-amd64 -f " + 
           linuxLib.string() + " -y");
    
    // Add Darwin variant
    std::ofstream(darwinLib) << "darwin library";
    runLgx("add " + pkgPath.string() + " -v darwin-arm64 -f " + 
           darwinLib.string() + " -y");
    
    // Verify package with multiple variants
    std::string output;
    int exitCode = runLgx("verify " + pkgPath.string(), &output);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("valid"), std::string::npos);
}

// ── lgx signature ────────────────────────────────────────────────────────
//
// Contract pinned by these tests:
//   * unsigned package  → stdout empty, exit 0
//   * signed package    → stdout = manifest.sig JSON, exit 0
//   * missing path      → stderr message, exit non-zero
// Tooling (the out-of-CI index builder over in logos-modules-release-base)
// relies on the exit-status-not-stream-length convention to distinguish
// "no signature" from "error" — regressing that contract would silently
// flip every unsigned package in a self-hosted catalog into a failure
// during index build, hence the explicit tests below.

// Test: lgx signature on an unsigned package
// Expected: empty stdout, exit 0 (the "I'm fine, just unsigned" path).
TEST_F(CLITest, SignatureCommand_UnsignedPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path lib = tempDir / "lib.so";

    runLgx("create " + (tempDir / "test").string());
    std::ofstream(lib) << "payload";
    runLgx("add " + pkgPath.string() + " -v linux-amd64 -f " +
           lib.string() + " -y");

    std::string output;
    int exitCode = runLgx("signature " + pkgPath.string(), &output);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(output.empty())
        << "unsigned package must produce no stdout output; got: " << output;
}

// Test: lgx signature on a signed package
// Expected: stdout carries the raw manifest.sig JSON (must contain the
// signer's DID), exit 0. The content match is intentionally loose —
// looking for "did" / "signature" substrings is enough to confirm the
// blob came out, without coupling the test to the exact JSON layout.
TEST_F(CLITest, SignatureCommand_SignedPackage) {
    fs::path pkgPath = tempDir / "test.lgx";
    fs::path lib = tempDir / "lib.so";
    fs::path keysDir = tempDir / "keys";

    runLgx("create " + (tempDir / "test").string());
    std::ofstream(lib) << "payload";
    runLgx("add " + pkgPath.string() + " -v linux-amd64 -f " +
           lib.string() + " -y");
    runLgx("keygen --name testkey --output-dir " + keysDir.string());
    int signExit = runLgx("sign " + pkgPath.string() +
                          " --key testkey --keys-dir " + keysDir.string());
    ASSERT_EQ(signExit, 0) << "preflight: signing must succeed for this test";

    std::string output;
    int exitCode = runLgx("signature " + pkgPath.string(), &output);

    EXPECT_EQ(exitCode, 0);
    EXPECT_FALSE(output.empty()) << "signed package must produce stdout";
    EXPECT_NE(output.find("\"did\""), std::string::npos)
        << "signature JSON should include a `did` field; got: " << output;
    EXPECT_NE(output.find("\"signature\""), std::string::npos)
        << "signature JSON should include a `signature` field; got: " << output;
}

// Test: lgx signature with a missing path
// Expected: non-zero exit (the "real error, not unsigned" path).
TEST_F(CLITest, SignatureCommand_MissingPackage) {
    std::string output;
    int exitCode = runLgx(
        "signature " + (tempDir / "does-not-exist.lgx").string(),
        &output);

    EXPECT_NE(exitCode, 0);
    EXPECT_FALSE(output.empty()) << "missing package should print a message";
}
