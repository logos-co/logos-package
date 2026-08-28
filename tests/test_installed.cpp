#include <gtest/gtest.h>

#include "core/installed_package.h"
#include "core/manifest.h"
#include "core/package.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "test_png.h"

using namespace lgx;
namespace fs = std::filesystem;

// Checks over a package that has been INSTALLED: one variant extracted into a
// directory, with the sidecars an installer writes beside it. Every fixture
// here goes through the real extractVariant() path rather than a hand-built
// directory, because the layout under test is the one extraction produces.
class InstalledTest : public ::testing::Test {
protected:
    fs::path tempDir;

    void SetUp() override {
        tempDir = fs::temp_directory_path() /
                  ("lgx_installed_" + std::to_string(::rand()));
        fs::create_directories(tempDir);
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

    // Extract `variant` and write the sidecars, exactly as lgpm does: the full
    // root manifest.json, plus a `variant` file naming what was installed.
    fs::path install(const Package& pkg, const std::string& variant,
                     const std::string& outName = "installed") {
        fs::path out = tempDir / outName;
        EXPECT_TRUE(pkg.extractVariant(variant, out).success);
        fs::path dir = out / variant;
        writeFile(dir / "manifest.json", pkg.getManifest().toJson());
        writeFile(dir / "variant", variant);
        return dir;
    }

    // A core module with a nested payload, saved and reloaded so the fixture
    // holds the same manifest an installer would read out of the file.
    Package makeCorePackage(const std::string& variant = "linux-amd64") {
        fs::path pkgPath = tempDir / "test.lgx";
        EXPECT_TRUE(Package::create(pkgPath, "testpkg").success);
        auto pkg = Package::load(pkgPath);
        EXPECT_TRUE(pkg.has_value());

        fs::path payload = tempDir / "payload";
        writeFile(payload / "mod.so", "plugin bytes");
        writeFile(payload / "lib" / "libdep.so", "dependency bytes");
        writeFile(payload / "sub" / "manifest.json", "{\"not\":\"a sidecar\"}");
        EXPECT_TRUE(pkg->addVariant(variant, payload, "mod.so").success);
        EXPECT_TRUE(pkg->save(pkgPath).success);

        auto reloaded = Package::load(pkgPath);
        EXPECT_TRUE(reloaded.has_value());
        return std::move(*reloaded);
    }

    // A ui_qml plugin: QML view, no backend `main`, and the canonical icon at
    // the package root — the shape every installed UI plugin has.
    Package makeUiQmlPackage(const std::string& variant = "linux-amd64",
                             uint32_t iconSize = 256) {
        fs::path pkgPath = tempDir / "ui.lgx";
        EXPECT_TRUE(Package::create(pkgPath, "testui").success);
        auto pkg = Package::load(pkgPath);
        EXPECT_TRUE(pkg.has_value());
        pkg->getManifest().type = "ui_qml";
        pkg->getManifest().view = "qml/Main.qml";
        EXPECT_TRUE(pkg->setIcon(lgx_test::makePng(iconSize, iconSize)).success);

        fs::path payload = tempDir / "uipayload";
        writeFile(payload / "qml" / "Main.qml", "import QtQuick\nItem {}");
        EXPECT_TRUE(pkg->addVariant(variant, payload).success);
        EXPECT_TRUE(pkg->save(pkgPath).success);

        auto reloaded = Package::load(pkgPath);
        EXPECT_TRUE(reloaded.has_value());
        return std::move(*reloaded);
    }

    static bool hasError(const Package::VerifyResult& r, const std::string& needle) {
        for (const auto& e : r.errors) {
            if (e.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    static std::string joinErrors(const Package::VerifyResult& r) {
        std::string out;
        for (const auto& e : r.errors) out += "\n  " + e;
        return out;
    }
};

// --- integrity ------------------------------------------------------------

TEST_F(InstalledTest, AcceptsAnUntouchedInstall) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Ok);

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_TRUE(result.valid) << joinErrors(result);
}

// The sidecars are written after the hash was computed, so counting them would
// make every correct install fail.
TEST_F(InstalledTest, InstallerSidecarsAreNotHashed) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    writeFile(dir / "manifest.sig", "{\"version\":\"0.1.0\"}");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Ok);
}

// ...but only at the top level. A payload file that happens to be called
// manifest.json is content.
TEST_F(InstalledTest, ANestedManifestJsonIsHashed) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    writeFile(dir / "sub" / "manifest.json", "{\"not\":\"the packaged bytes\"}");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Mismatch);
}

TEST_F(InstalledTest, AnAlteredPayloadFileIsRejected) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    writeFile(dir / "mod.so", "plugin bytes!");

    std::string detail;
    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64", &detail),
              InstalledIntegrity::Mismatch);

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result, "Content hash mismatch")) << joinErrors(result);
}

TEST_F(InstalledTest, AnExtraFileIsRejected) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    writeFile(dir / "lib" / "evil.so", "smuggled");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Mismatch);
}

TEST_F(InstalledTest, ADeletedFileIsRejected) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    fs::remove(dir / "lib" / "libdep.so");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Mismatch);
}

// Extraction never produces a link, so following one would let a correct-looking
// hash be assembled out of files that are not in the package.
TEST_F(InstalledTest, ASymlinkedPayloadFileIsRejected) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");

    fs::path elsewhere = tempDir / "elsewhere.so";
    writeFile(elsewhere, "plugin bytes");
    fs::remove(dir / "mod.so");
    std::error_code ec;
    fs::create_symlink(elsewhere, dir / "mod.so", ec);
    ASSERT_FALSE(ec) << ec.message();

    std::string detail;
    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64", &detail),
              InstalledIntegrity::Mismatch);
    EXPECT_NE(detail.find("Not a regular file"), std::string::npos) << detail;
}

// A variant the package never declared is unanswerable, not clean: reporting
// Ok here would let any directory pass under a name nothing hashes.
TEST_F(InstalledTest, AnUndeclaredVariantIsNoHash) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");

    std::string detail;
    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "freebsd-x86", &detail),
              InstalledIntegrity::NoHash);
    EXPECT_NE(detail.find("variants/freebsd-x86"), std::string::npos) << detail;
}

TEST_F(InstalledTest, ADeclaredButUninstalledVariantIsAMismatch) {
    fs::path pkgPath = tempDir / "two.lgx";
    ASSERT_TRUE(Package::create(pkgPath, "testpkg").success);
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());

    fs::path a = tempDir / "a"; writeFile(a / "mod.so", "linux");
    fs::path b = tempDir / "b"; writeFile(b / "mod.dylib", "darwin");
    ASSERT_TRUE(pkg->addVariant("linux-amd64", a, "mod.so").success);
    ASSERT_TRUE(pkg->addVariant("darwin-arm64", b, "mod.dylib").success);

    fs::path dir = install(*pkg, "linux-amd64");
    EXPECT_EQ(verifyInstalledTree(dir, pkg->getManifest(), "darwin-arm64"),
              InstalledIntegrity::Mismatch);
}

TEST_F(InstalledTest, NoVariantIsBadInput) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), ""),
              InstalledIntegrity::BadInput);
}

TEST_F(InstalledTest, AMissingDirectoryIsUnreadable) {
    auto pkg = makeCorePackage();
    install(pkg, "linux-amd64");

    std::string detail;
    EXPECT_EQ(verifyInstalledTree(tempDir / "nope", pkg.getManifest(),
                                  "linux-amd64", &detail),
              InstalledIntegrity::Unreadable);
    EXPECT_NE(detail.find("Cannot read installed directory"), std::string::npos)
        << detail;
}

TEST_F(InstalledTest, TheVariantNameIsCaseInsensitive) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "Linux-AMD64"),
              InstalledIntegrity::Ok);
}

// --- root assets ----------------------------------------------------------

// Root `assets/` lands in the same directory as the variant payload but is
// hashed separately, so an install carrying one only verifies if both readings
// are tried.
TEST_F(InstalledTest, AnInstallWithRootAssetsIsAccepted) {
    auto pkg = makeUiQmlPackage();
    ASSERT_TRUE(pkg.getManifest().hashes.count("assets"))
        << "fixture must exercise the root-assets slot";
    fs::path dir = install(pkg, "linux-amd64");
    ASSERT_TRUE(fs::exists(dir / "assets" / "icon.png"));

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Ok);
}

// The reading that excludes assets/ must not leave them unchecked.
TEST_F(InstalledTest, ATamperedRootAssetIsRejected) {
    auto pkg = makeUiQmlPackage();
    fs::path dir = install(pkg, "linux-amd64");
    writeFile(dir / "assets" / "icon.png", "not the packaged icon");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Mismatch);
}

TEST_F(InstalledTest, RootAssetsDoNotExcuseAnAlteredPayload) {
    auto pkg = makeUiQmlPackage();
    fs::path dir = install(pkg, "linux-amd64");
    writeFile(dir / "qml" / "Main.qml", "import QtQuick\nItem { property int x: 1 }");

    EXPECT_EQ(verifyInstalledTree(dir, pkg.getManifest(), "linux-amd64"),
              InstalledIntegrity::Mismatch);
}

// Real packages ship the icon at both assets/icon.png and
// variants/<v>/assets/icon.png. Then the variant hash needs the file counted,
// which is the other reading.
TEST_F(InstalledTest, AnInstallWithBothRootAndVariantAssetsIsAccepted) {
    fs::path pkgPath = tempDir / "both.lgx";
    ASSERT_TRUE(Package::create(pkgPath, "testui").success);
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());
    pkg->getManifest().type = "ui_qml";
    pkg->getManifest().view = "qml/Main.qml";
    ASSERT_TRUE(pkg->setIcon(lgx_test::makePng()).success);

    fs::path payload = tempDir / "bothpayload";
    writeFile(payload / "qml" / "Main.qml", "import QtQuick\nItem {}");
    {
        auto png = lgx_test::makePng();
        fs::create_directories(payload / "assets");
        std::ofstream f(payload / "assets" / "icon.png", std::ios::binary);
        f.write(reinterpret_cast<const char*>(png.data()), png.size());
    }
    ASSERT_TRUE(pkg->addVariant("linux-amd64", payload).success);

    fs::path dir = install(*pkg, "linux-amd64");
    EXPECT_EQ(verifyInstalledTree(dir, pkg->getManifest(), "linux-amd64"),
              InstalledIntegrity::Ok);
}

// --- manifest rules, main, view, icon -------------------------------------

TEST_F(InstalledTest, ManifestRulesAreReportedFromTheInstalledDirectory) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");

    Manifest broken = pkg.getManifest();
    broken.version = "1.0";
    auto result = verifyInstalled(dir, broken, "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result, "Manifest: 'version' is not a valid SemVer"))
        << joinErrors(result);
}

TEST_F(InstalledTest, AMissingMainIsReported) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    fs::remove(dir / "mod.so");

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result,
        "main[linux-amd64] points to non-existent file: mod.so"))
        << joinErrors(result);
}

TEST_F(InstalledTest, AMissingViewIsReported) {
    auto pkg = makeUiQmlPackage();
    fs::path dir = install(pkg, "linux-amd64");
    fs::remove(dir / "qml" / "Main.qml");

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result,
        "view file missing for variant 'linux-amd64': qml/Main.qml"))
        << joinErrors(result);
}

// The carve-out a naive reuse breaks: a QML-only plugin legitimately declares
// no `main` at all, and must not be told it is missing one.
TEST_F(InstalledTest, AViewOnlyUiQmlPluginNeedsNoMain) {
    auto pkg = makeUiQmlPackage();
    ASSERT_TRUE(pkg.getManifest().main.empty()) << "fixture must have no main";
    fs::path dir = install(pkg, "linux-amd64");

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_TRUE(result.valid) << joinErrors(result);
    EXPECT_FALSE(hasError(result, "has no main entry"));
}

TEST_F(InstalledTest, AnInstalledVariantWithoutAMainEntryIsRejected) {
    auto pkg = makeCorePackage();
    fs::path dir = install(pkg, "linux-amd64");
    pkg.getManifest().removeMain("linux-amd64");

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result, "Variant 'linux-amd64' has no main entry"))
        << joinErrors(result);
}

// An install holds one variant out of many. The archive rule "every main entry
// has a variant directory" would condemn every correct multi-variant install.
TEST_F(InstalledTest, VariantsThatWereNotInstalledAreNotReportedMissing) {
    fs::path pkgPath = tempDir / "two.lgx";
    ASSERT_TRUE(Package::create(pkgPath, "testpkg").success);
    auto pkg = Package::load(pkgPath);
    ASSERT_TRUE(pkg.has_value());

    fs::path a = tempDir / "a"; writeFile(a / "mod.so", "linux");
    fs::path b = tempDir / "b"; writeFile(b / "mod.dylib", "darwin");
    ASSERT_TRUE(pkg->addVariant("linux-amd64", a, "mod.so").success);
    ASSERT_TRUE(pkg->addVariant("darwin-arm64", b, "mod.dylib").success);

    fs::path dir = install(*pkg, "linux-amd64");
    auto result = verifyInstalled(dir, pkg->getManifest(), "linux-amd64");
    EXPECT_TRUE(result.valid) << joinErrors(result);
    EXPECT_FALSE(hasError(result, "darwin-arm64"));
}

TEST_F(InstalledTest, TheIconContractIsEnforcedAgainstTheInstalledFile) {
    auto pkg = makeUiQmlPackage("linux-amd64", 128);
    fs::path dir = install(pkg, "linux-amd64");

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result, "expected PNG, exactly 256x256; actual: PNG, 128x128"))
        << joinErrors(result);
}

TEST_F(InstalledTest, AnIconMissingFromTheInstallIsReported) {
    auto pkg = makeUiQmlPackage();
    fs::path dir = install(pkg, "linux-amd64");
    fs::remove(dir / "assets" / "icon.png");

    auto result = verifyInstalled(dir, pkg.getManifest(), "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result, "which is not present in the package"))
        << joinErrors(result);
}

// A crafted `icon` must never steer a read out of the package directory.
TEST_F(InstalledTest, AnIconPathOutsideThePackageIsNotRead) {
    auto pkg = makeUiQmlPackage();
    fs::path dir = install(pkg, "linux-amd64");

    Manifest crafted = pkg.getManifest();
    crafted.icon = "../../assets/icon.png";
    auto result = verifyInstalled(dir, crafted, "linux-amd64");
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(hasError(result, "must be 'assets/icon.png'")) << joinErrors(result);
}
