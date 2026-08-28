#include <gtest/gtest.h>

#include "core/platform_variant.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace lgx;

namespace {

bool accepts(const std::vector<std::string>& variants, const std::string& v) {
    return std::find(variants.begin(), variants.end(), v) != variants.end();
}

std::string join(const std::vector<std::string>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ", ";
        out += v[i];
    }
    return out + "]";
}

std::string osHalf(const std::string& variant) {
    return variant.substr(0, variant.rfind('-'));
}

} // namespace

// =============================================================================
// Producer/consumer spelling agreement
//
// A TABLE over every target this ecosystem ships, not a probe of the host,
// because the defect being pinned is invisible from any single host: an arm64
// Mac never computes darwin-x86_64 and so never meets it.
//
// Left column is what a host computes for itself (hostVariant); right column
// is a spelling some producer actually writes into a .lgx, so it has to
// resolve on that host. The producers disagree with EACH OTHER --
//
//   nix-bundle-lgx/flake.nix:52                  darwin-amd64  linux-amd64
//                                                darwin-arm64  linux-arm64
//   nix-bundle-logos-module-install/flake.nix:49 darwin-x86_64 linux-x86_64
//                                                darwin-arm64  linux-arm64
//
// -- and the second CONSUMES the first, calling lgpm --platform with its own
// spelling on a package the first named. So both columns must resolve here.
// =============================================================================

namespace {
struct VariantAliasCase {
    const char* host;          // what hostVariant() would return
    const char* alsoAccepted;  // a producer spelling that must still resolve
};

const VariantAliasCase kAliasCases[] = {
    { "linux-x86_64",   "linux-amd64"     },
    { "linux-amd64",    "linux-x86_64"    },
    { "linux-arm64",    "linux-aarch64"   },
    { "linux-aarch64",  "linux-arm64"     },
    { "darwin-x86_64",  "darwin-amd64"    },
    { "darwin-amd64",   "darwin-x86_64"   },
    { "darwin-arm64",   "darwin-aarch64"  },
    { "darwin-aarch64", "darwin-arm64"    },
    { "windows-x86_64", "windows-amd64"   },
    { "windows-amd64",  "windows-x86_64"  },
    { "windows-arm64",  "windows-aarch64" },
    { "windows-aarch64","windows-arm64"   },
};
} // namespace

TEST(PlatformVariantTest, EveryTargetAcceptsBothArchSpellings) {
    for (const auto& c : kAliasCases) {
        auto variants = variantSpellings(c.host);
        EXPECT_TRUE(accepts(variants, c.host))
            << "host " << c.host << " does not accept its own spelling; got " << join(variants);
        EXPECT_TRUE(accepts(variants, c.alsoAccepted))
            << "host " << c.host << " does not accept producer spelling "
            << c.alsoAccepted << "; got " << join(variants);
    }
}

TEST(PlatformVariantTest, TheCallersOwnSpellingLeads) {
    for (const auto& c : kAliasCases) {
        auto variants = variantSpellings(c.host);
        ASSERT_FALSE(variants.empty());
        EXPECT_EQ(variants.front(), c.host) << join(variants);
    }
}

TEST(PlatformVariantTest, NoSpellingIsListedTwice) {
    for (const auto& c : kAliasCases) {
        auto variants = variantSpellings(c.host);
        std::set<std::string> unique(variants.begin(), variants.end());
        EXPECT_EQ(unique.size(), variants.size()) << join(variants);
    }
}

// =============================================================================
// Fail-closed guarantees. Aliasing must WIDEN within one target and never
// across targets: a Windows package on a Mac, or an arm64 package on an x86_64
// host, must still be refused.
// =============================================================================

TEST(PlatformVariantTest, NeverAcceptsAnotherOperatingSystem) {
    const char* const hosts[] = { "darwin-x86_64", "darwin-arm64", "linux-x86_64",
                                  "linux-arm64", "windows-x86_64" };
    const char* const foreign[] = { "darwin-x86_64", "darwin-amd64", "darwin-arm64",
                                    "darwin-aarch64", "linux-x86_64", "linux-amd64",
                                    "linux-arm64", "linux-aarch64", "windows-x86_64",
                                    "windows-amd64", "windows-arm64", "windows-aarch64" };
    for (const char* host : hosts) {
        auto variants = variantSpellings(host);
        for (const char* f : foreign) {
            if (osHalf(f) == osHalf(host)) continue;
            EXPECT_FALSE(accepts(variants, f))
                << "host " << host << " accepted foreign-OS variant " << f
                << "; got " << join(variants);
        }
    }
}

TEST(PlatformVariantTest, NeverAcceptsAnotherArchitecture) {
    struct { const char* host; const char* foreignArch; } cases[] = {
        { "darwin-x86_64",  "darwin-arm64"    },
        { "darwin-x86_64",  "darwin-aarch64"  },
        { "darwin-arm64",   "darwin-x86_64"   },
        { "darwin-arm64",   "darwin-amd64"    },
        { "linux-x86_64",   "linux-arm64"     },
        { "linux-arm64",    "linux-amd64"     },
        { "windows-x86_64", "windows-arm64"   },
    };
    for (const auto& c : cases) {
        auto variants = variantSpellings(c.host);
        EXPECT_FALSE(accepts(variants, c.foreignArch))
            << "host " << c.host << " accepted foreign-arch variant " << c.foreignArch
            << "; got " << join(variants);
    }
}

TEST(PlatformVariantTest, UnknownArchitectureGetsNoAliasesAndStillNamesItself) {
    // A spelling no table row knows must degrade to "itself only" rather than
    // throwing away the caller's own variant.
    auto variants = variantSpellings("linux-riscv64");
    EXPECT_TRUE(accepts(variants, "linux-riscv64")) << join(variants);
    EXPECT_EQ(variants.size(), 1u) << join(variants);
}

TEST(PlatformVariantTest, ANameWithNoSeparatorHasNoArchitectureToAlias) {
    auto variants = variantSpellings("unknown");
    EXPECT_EQ(variants, std::vector<std::string>{ "unknown" }) << join(variants);
}

TEST(PlatformVariantTest, ASuffixedSpellingAliasesNothingAndStillNamesItself) {
    // lgpm appends "-dev" for a non-portable build, which lands INSIDE the
    // architecture component. Splitting on the last '-' is what keeps those
    // names out of the table entirely, so they alias nothing and still name
    // themselves -- the fail-closed degrade, not a silent widening.
    auto variants = variantSpellings("darwin-arm64-dev");
    EXPECT_EQ(variants, std::vector<std::string>{ "darwin-arm64-dev" }) << join(variants);
}

TEST(PlatformVariantTest, EmptyInEmptyOut) {
    EXPECT_TRUE(variantSpellings("").empty());
}

// =============================================================================
// The host's own name
// =============================================================================

TEST(PlatformVariantTest, HostVariantIsAnOsDashArchName) {
    const std::string host = hostVariant();
    ASSERT_FALSE(host.empty());
    EXPECT_NE(host, "unknown") << "no variant rule for this build target";
    EXPECT_NE(host.rfind('-'), std::string::npos) << host;
}

TEST(PlatformVariantTest, TheHostAcceptsItsOwnSpelling) {
    EXPECT_TRUE(accepts(variantSpellings(hostVariant()), hostVariant()));
}
