#include "installed_package.h"

#include "path_normalizer.h"
#include "../crypto/signing.h"

#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace lgx {

namespace fs = std::filesystem;

const std::set<std::string> INSTALL_SIDECAR_FILES = {
    "manifest.json",
    "manifest.sig",
    "variant"
};

namespace {

constexpr const char* kAssetsPrefix = "assets/";

bool readFileBytes(const fs::path& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.bad();
}

// (relative path, hex SHA-256) for every payload file in an installed
// directory. Paths use '/' regardless of platform: they are hashed, and the
// hash was computed over archive paths.
struct Listing {
    std::vector<std::pair<std::string, std::string>> files;
};

// Ok, or why not: NotFromAPackage is a statement about the TREE (nothing the
// extractor produces looks like that), Unreadable one about this process.
enum class WalkResult { Ok, NotFromAPackage, Unreadable };

WalkResult collectListing(const fs::path& dir, Listing& out, std::string& error) {
    std::error_code ec;
    fs::recursive_directory_iterator it(dir, fs::directory_options::none, ec);
    if (ec) {
        error = "Cannot read installed directory: " + dir.string() + " - " + ec.message();
        return WalkResult::Unreadable;
    }

    const fs::recursive_directory_iterator last;
    for (; it != last; ) {
        const fs::path path = it->path();
        std::error_code sec;
        const bool isSymlink = it->is_symlink(sec);
        const bool isDir = !sec && it->is_directory(sec);
        const bool isFile = !sec && it->is_regular_file(sec);

        // Nothing exotic can have come out of a .lgx: the tar reader admits
        // regular files and directories only, and extraction adds no links.
        if (sec || isSymlink || (!isDir && !isFile)) {
            error = "Not a regular file: " + path.string();
            return WalkResult::NotFromAPackage;
        }

        if (isDir) {
            it.increment(ec);
            if (ec) {
                error = "Cannot read installed directory: " + dir.string() + " - " + ec.message();
                return WalkResult::Unreadable;
            }
            continue;
        }

        const std::string rel = path.lexically_relative(dir).generic_string();
        if (rel.empty() || rel.rfind("..", 0) == 0) {
            error = "Path escapes the installed directory: " + path.string();
            return WalkResult::NotFromAPackage;
        }
        if (!INSTALL_SIDECAR_FILES.count(rel)) {
            std::vector<uint8_t> bytes;
            if (!readFileBytes(path, bytes)) {
                error = "Cannot read installed file: " + path.string();
                return WalkResult::Unreadable;
            }
            out.files.emplace_back(rel, crypto::sha256Hex(bytes));
        }

        it.increment(ec);
        if (ec) {
            error = "Cannot read installed directory: " + dir.string() + " - " + ec.message();
            return WalkResult::Unreadable;
        }
    }
    return WalkResult::Ok;
}

// Only worth checking on disk when the path is one the archive would have
// accepted; a rejected path already produced a manifest error.
bool safeRelativePath(const std::string& p) {
    return PathNormalizer::validateArchivePath(p).valid;
}

void setDetail(std::string* detail, std::string value) {
    if (detail) *detail = std::move(value);
}

} // namespace

InstalledIntegrity verifyInstalledTree(const fs::path& dir,
                                       const Manifest& manifest,
                                       const std::string& variant,
                                       std::string* detail) {
    setDetail(detail, "");

    const std::string variantLc = PathNormalizer::toLowercase(variant);
    if (variantLc.empty()) {
        setDetail(detail, "No installed variant given");
        return InstalledIntegrity::BadInput;
    }

    const auto declared = manifest.hashes.find("variants/" + variantLc);
    if (declared == manifest.hashes.end()) {
        setDetail(detail, "Missing 'variants/" + variantLc + "' hash in manifest");
        return InstalledIntegrity::NoHash;
    }

    if (!crypto::init()) {
        setDetail(detail, "Failed to initialize crypto library for hash verification");
        return InstalledIntegrity::Unreadable;
    }

    Listing listing;
    std::string error;
    switch (collectListing(dir, listing, error)) {
        case WalkResult::Ok:
            break;
        case WalkResult::NotFromAPackage:
            setDetail(detail, std::move(error));
            return InstalledIntegrity::Mismatch;
        case WalkResult::Unreadable:
            setDetail(detail, std::move(error));
            return InstalledIntegrity::Unreadable;
    }

    const auto declaredAssets = manifest.hashes.find("assets");
    const bool haveRootAssets = declaredAssets != manifest.hashes.end() &&
                                !declaredAssets->second.empty();

    // Nothing came from the package root, so every file on disk is variant
    // content and there is only one reading.
    if (!haveRootAssets) {
        if (crypto::computeLeafHash(listing.files) == declared->second) {
            return InstalledIntegrity::Ok;
        }
        setDetail(detail, "installed content does not match manifest hashes");
        return InstalledIntegrity::Mismatch;
    }

    // Root assets exist, and extraction merged them into the same directory as
    // the variant's own files. Which of the assets/ files came from the root
    // has to be guessed -- but a guess only counts once hashes["assets"] comes
    // out right, so a root asset that was deleted can no longer read as a
    // package that never had one.
    std::vector<std::pair<std::string, std::string>> underAssets, payload;
    for (const auto& f : listing.files) {
        if (f.first.rfind(kAssetsPrefix, 0) == 0) underAssets.push_back(f);
        else payload.push_back(f);
    }

    // `iconOnly` narrows the guess to the canonical icon -- the only root asset
    // the packaging API can write -- which is what leaves a variant's own
    // assets/ in the payload where its hash expects them. The wider guess
    // covers a hand-built archive that put every asset at the root.
    auto accepts = [&](bool iconOnly) {
        std::vector<std::pair<std::string, std::string>> assets, keptInPayload;
        for (const auto& f : underAssets) {
            if (!iconOnly || f.first == Manifest::ICON_PATH) {
                assets.emplace_back(f.first.substr(strlen(kAssetsPrefix)), f.second);
            } else {
                keptInPayload.push_back(f);
            }
        }
        if (crypto::computeLeafHash(assets) != declaredAssets->second) return false;

        // Either the archive held these paths at the root only, or it held them
        // in the variant too and the variant hash needs them counted as well.
        auto variantReading = payload;
        variantReading.insert(variantReading.end(), keptInPayload.begin(), keptInPayload.end());
        if (crypto::computeLeafHash(variantReading) == declared->second) return true;

        variantReading = payload;
        variantReading.insert(variantReading.end(), underAssets.begin(), underAssets.end());
        return crypto::computeLeafHash(variantReading) == declared->second;
    };

    if (accepts(true) || accepts(false)) {
        return InstalledIntegrity::Ok;
    }

    setDetail(detail, "installed content does not match manifest hashes");
    return InstalledIntegrity::Mismatch;
}

Package::VerifyResult verifyInstalled(const fs::path& dir,
                                      const Manifest& manifest,
                                      const std::string& variant) {
    Package::VerifyResult result = Package::VerifyResult::ok();
    auto fail = [&result](std::string message) {
        result.valid = false;
        result.errors.push_back(std::move(message));
    };

    auto manifestValidation = manifest.validate();
    if (!manifestValidation.valid) {
        for (const auto& err : manifestValidation.errors) {
            fail("Manifest: " + err);
        }
    }

    const std::string variantLc = PathNormalizer::toLowercase(variant);
    if (variantLc.empty()) {
        fail("No installed variant given");
        return result;
    }

    // Read only the canonical location, never the manifest's own `icon` string
    // joined onto dir: nothing validates that string as a path, so joining it
    // would let a crafted manifest point the read outside the package.
    std::vector<uint8_t> iconBytes;
    const bool haveIcon = manifest.icon == Manifest::ICON_PATH &&
                          readFileBytes(dir / "assets" / "icon.png", iconBytes);
    Package::validateIconContract(manifest, haveIcon ? &iconBytes : nullptr, result);

    // The one part of validateCompleteness() that survives: does the INSTALLED
    // variant have a main entry (unless the package is view-only ui_qml)? The
    // other direction — a main entry with no variant directory — is normal
    // here, because an install holds one variant out of many. Seeding the set
    // with every declared variant is what silences it.
    std::set<std::string> existing = manifest.getVariants();
    existing.insert(variantLc);
    auto completeness = manifest.validateCompleteness(existing);
    if (!completeness.valid) {
        for (const auto& err : completeness.errors) fail(err);
    }

    const auto mainPath = manifest.getMain(variantLc);
    if (mainPath && safeRelativePath(*mainPath) &&
        !fs::is_regular_file(dir / *mainPath)) {
        fail("main[" + variantLc + "] points to non-existent file: " + *mainPath);
    }

    if (manifest.type == "ui_qml" && !manifest.view.empty() &&
        safeRelativePath(manifest.view) &&
        !fs::is_regular_file(dir / manifest.view)) {
        fail("view file missing for variant '" + variantLc + "': " + manifest.view);
    }

    std::string detail;
    switch (verifyInstalledTree(dir, manifest, variantLc, &detail)) {
        case InstalledIntegrity::Ok:
            break;
        case InstalledIntegrity::Mismatch:
            fail("Content hash mismatch: " + detail);
            break;
        case InstalledIntegrity::NoHash:
        case InstalledIntegrity::Unreadable:
        case InstalledIntegrity::BadInput:
            fail(detail);
            break;
    }

    return result;
}

} // namespace lgx
