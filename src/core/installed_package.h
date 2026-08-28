#pragma once

#include "manifest.h"
#include "package.h"

#include <filesystem>
#include <set>
#include <string>

namespace lgx {

// Checks for a package that has been INSTALLED: one variant of a .lgx,
// extracted into a directory, with the manifest written beside it.
//
// Most of Package::validatePackage() describes ARCHIVE SHAPE — variants/, the
// root-entry whitelist, per-tar-entry path rules — and none of it survives
// extraction. What survives is what the manifest says about the package plus
// whether the files are still the ones it covers, and that is what these run,
// on the same code the archive path runs.

// Files an installer writes beside the payload. They were never in the hashed
// listing, so the tree walk skips them. Top level only: a payload's own
// `sub/manifest.json` is content.
extern const std::set<std::string> INSTALL_SIDECAR_FILES;

// Typed rather than a message: this is the verdict a host branches on before
// loading code out of the directory.
enum class InstalledIntegrity {
    Ok = 0,
    // Definitive: content was added, removed or altered.
    Mismatch = 1,
    // No hash declared for this variant. Nothing proved, nothing disproved.
    NoHash = 2,
    // The check could not run — unreadable directory, or crypto init failed.
    Unreadable = 3,
    // The CALLER's input is unusable. Distinct from NoHash so a typo'd variant
    // cannot read as "this package simply has no hash" and be waved through.
    BadInput = 4
};

/**
 * Recompute hashes["variants/<variant>"] over an installed directory. Content
 * paths were hashed relative to `variants/<v>/`, which is exactly the
 * flattened installed layout, so no re-prefixing is involved.
 *
 * Both readings of `assets/` are tried, because extraction puts root-level
 * assets (covered by hashes["assets"]) in the same directory as variant files
 * (covered by the variant leaf) and nothing then tells them apart by path. The
 * second reading only counts if what it excluded matches hashes["assets"], so
 * every file still answers to exactly one declared hash.
 *
 * @param detail Optional; receives a human-readable reason on a non-Ok result
 */
InstalledIntegrity verifyInstalledTree(const std::filesystem::path& dir,
                                       const Manifest& manifest,
                                       const std::string& variant,
                                       std::string* detail = nullptr);

/**
 * Every check that survives installation: the manifest's own rules, tree
 * integrity, `main`/`view` resolution and the icon contract. Errors read the
 * same as `lgx verify`'s.
 *
 * @param variant The installed variant, e.g. from the `variant` sidecar.
 *                Required: only its files are on disk, so it is the only one
 *                whose hash, main and view can be checked.
 */
Package::VerifyResult verifyInstalled(const std::filesystem::path& dir,
                                      const Manifest& manifest,
                                      const std::string& variant);

} // namespace lgx
