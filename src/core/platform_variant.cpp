#include "platform_variant.h"

#include <algorithm>

namespace lgx {

namespace {

// The one place a variant-name SPELLING is enumerated.
//
// A variant name is "<os>-<architecture>", and both halves have more than one
// live spelling in this ecosystem. Canonical vocabulary is the one
// logos-module-builder's lib/resolvePlatforms.nix pins -- os in
// {linux, darwin, windows}, architecture in {x86_64, aarch64} -- so the first
// entry of every row below is the canonical spelling and the rest are legacy
// ones that some producer actually writes:
//
//   nix-bundle-lgx/flake.nix:52                   darwin-amd64  linux-amd64
//                                                 darwin-arm64  linux-arm64
//   nix-bundle-logos-module-install/flake.nix:49  darwin-x86_64 linux-x86_64
//                                                 darwin-arm64  linux-arm64
//
// Those two producers disagree with each other while the SECOND CONSUMES THE
// FIRST -- it bundles a .lgx the first named and then calls the package
// manager with --platform spelled its own way -- so a CONSUMER is where the
// disagreement has to be absorbed, and this table is the one every consumer
// absorbs it from.
//
// Only the ARCHITECTURE is aliased. The OS half is matched verbatim on
// purpose: aliasing it would weaken the fail-closed check that stops a Windows
// package being installed as a macOS one, and "macos" (logos-release-set's
// release-ASSET naming) is a different namespace that is translated to
// "darwin" before it ever reaches an .lgx.
//
// Adding a spelling here is safe -- it only ever widens what an EXISTING
// package resolves to, and rows are per-architecture so a new OS inherits
// every alias without a new code path. Removing one is not: variant names sit
// inside the signed hash tree (this library writes hashes["variants/<name>"]),
// so a published package can never be renamed on disk. Alias on read.
const std::vector<std::vector<std::string>>& architectureSpellings()
{
    static const std::vector<std::vector<std::string>> kSpellings = {
        { "x86_64",  "amd64" },
        { "aarch64", "arm64" },
    };
    return kSpellings;
}

} // namespace

std::string hostVariant()
{
#if defined(__APPLE__)
    #if defined(__aarch64__)
        return "darwin-arm64";
    #else
        return "darwin-x86_64";
    #endif
#elif defined(__linux__)
    #if defined(__x86_64__)
        return "linux-x86_64";
    #elif defined(__aarch64__)
        return "linux-arm64";
    #else
        return "linux-x86";
    #endif
#elif defined(_WIN32)
    #if defined(_M_X64) || defined(__x86_64__)
        return "windows-x86_64";
    #else
        return "windows-x86";
    #endif
#else
    return "unknown";
#endif
}

std::vector<std::string> variantSpellings(const std::string& variant)
{
    std::vector<std::string> variants;
    if (variant.empty()) {
        return variants;
    }

    // The caller's own spelling always leads: it is what diagnostics print as
    // "this machine", and what an error names as the platform that was wanted.
    variants.push_back(variant);

    // Split on the LAST '-' so the architecture is the trailing component
    // whatever the OS half turns out to contain.
    const std::string::size_type sep = variant.rfind('-');
    if (sep != std::string::npos) {
        const std::string os = variant.substr(0, sep);
        const std::string arch = variant.substr(sep + 1);
        for (const auto& row : architectureSpellings()) {
            if (std::find(row.begin(), row.end(), arch) == row.end())
                continue;
            for (const auto& spelling : row) {
                if (spelling != arch)
                    variants.push_back(os + "-" + spelling);
            }
            break;
        }
    }

    return variants;
}

} // namespace lgx
