#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
        testDir.string() + " --main dist/index.js -y",
        &output
    );
    
    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.find("Added"), std::string::npos);
    
    // Verify still valid
    exitCode = runLgx("verify " + pkgPath.string());
    EXPECT_EQ(exitCode, 0);
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
