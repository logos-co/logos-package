// Conformance tests for the shared semver implementation.
//
// The precedence cases are taken from the SemVer 2.0.0 spec itself
// (https://semver.org/spec/v2.0.0.html, §10-§11). The range cases pin the npm
// dialect the manifests use — ranges are not part of the spec, so they need
// their own contract.
//
// Several cases below are regressions for bugs the five previous hand-rolled
// copies shipped; they are called out inline.

#include <gtest/gtest.h>

#include "logos/semver.hpp"

#include <string>
#include <vector>

using namespace logos::semver;

namespace {

// Assert a strictly ascending precedence chain, and that it holds in both
// directions (so an asymmetric comparator can't sneak through).
void ExpectAscending(const std::vector<std::string>& chain) {
    for (size_t i = 0; i + 1 < chain.size(); ++i) {
        const std::string& lo = chain[i];
        const std::string& hi = chain[i + 1];
        EXPECT_EQ(compare(lo, hi), -1) << lo << " should precede " << hi;
        EXPECT_EQ(compare(hi, lo), 1) << hi << " should follow " << lo;
        EXPECT_EQ(compare(lo, lo), 0) << lo << " should equal itself";
    }
}

}  // namespace

// ─────────────────────────────── precedence ───────────────────────────────

// Spec §11: "1.0.0 < 2.0.0 < 2.1.0 < 2.1.1".
TEST(Semver, SpecMainPrecedenceChain) {
    ExpectAscending({"1.0.0", "2.0.0", "2.1.0", "2.1.1"});
}

// Spec §11: "1.0.0-alpha < 1.0.0".
TEST(Semver, PreReleasePrecedesItsRelease) {
    ExpectAscending({"1.0.0-alpha", "1.0.0"});
}

// Spec §11, verbatim: the full pre-release ordering example.
TEST(Semver, SpecPreReleasePrecedenceChain) {
    ExpectAscending({
        "1.0.0-alpha",
        "1.0.0-alpha.1",
        "1.0.0-alpha.beta",
        "1.0.0-beta",
        "1.0.0-beta.2",
        "1.0.0-beta.11",
        "1.0.0-rc.1",
        "1.0.0",
    });
}

// REGRESSION. Every previous implementation got this wrong: the downloader
// compared the whole pre-release tag as one ASCII string (so "rc.2" > "rc.11"),
// while lgpm and the UI dropped the tag entirely (so "1.0.0-rc.1" == "1.0.0").
// Spec §11: numeric identifiers are compared numerically.
TEST(Semver, NumericPreReleaseIdentifiersCompareNumerically) {
    EXPECT_EQ(compare("1.0.0-rc.2", "1.0.0-rc.11"), -1);
    EXPECT_EQ(compare("1.0.0-beta.2", "1.0.0-beta.11"), -1);
    EXPECT_EQ(compare("1.0.0-alpha.9", "1.0.0-alpha.10"), -1);
}

// Spec §11: "Numeric identifiers always have lower precedence than
// alphanumeric identifiers."
TEST(Semver, NumericSortsBelowAlphanumeric) {
    EXPECT_EQ(compare("1.0.0-1", "1.0.0-alpha"), -1);
    EXPECT_EQ(compare("1.0.0-alpha.1", "1.0.0-alpha.beta"), -1);
}

// Spec §11: "A larger set of pre-release fields has a higher precedence than a
// smaller set, if all of the preceding identifiers are equal."
TEST(Semver, LargerPreReleaseFieldSetWins) {
    EXPECT_EQ(compare("1.0.0-alpha", "1.0.0-alpha.1"), -1);
    EXPECT_EQ(compare("1.0.0-beta", "1.0.0-beta.0"), -1);
}

// Spec §10: "Build metadata MUST be ignored when determining version
// precedence."
TEST(Semver, BuildMetadataIgnoredForPrecedence) {
    EXPECT_EQ(compare("1.0.0+build.1", "1.0.0+build.2"), 0);
    EXPECT_EQ(compare("1.0.0+anything", "1.0.0"), 0);
    EXPECT_EQ(compare("1.0.0-rc.1+a", "1.0.0-rc.1+b"), 0);
}

// ─────────────────────────────── validation ───────────────────────────────

TEST(Semver, AcceptsValidVersions) {
    EXPECT_TRUE(valid("0.0.0"));
    EXPECT_TRUE(valid("1.2.3"));
    EXPECT_TRUE(valid("1.0.0-alpha.beta.1"));
    EXPECT_TRUE(valid("1.0.0-rc.1+build.99"));
    EXPECT_TRUE(valid("10.20.30"));
}

TEST(Semver, RejectsInvalidVersions) {
    EXPECT_FALSE(valid(""));
    EXPECT_FALSE(valid("1"));
    EXPECT_FALSE(valid("1.0"));        // partial is not a version
    EXPECT_FALSE(valid("01.0.0"));     // §2: no leading zeroes
    EXPECT_FALSE(valid("1.0.0-"));     // empty pre-release
    EXPECT_FALSE(valid("1.0.0-01"));   // §9: no leading zeroes in numeric ids
    EXPECT_FALSE(valid("v1.0.0"));     // the `v` prefix is not part of a version
    EXPECT_FALSE(valid("banana"));
}

// An unparseable version must never win "latest" — a catalog carrying a junk
// version string should sort it to the bottom, not the top.
TEST(Semver, InvalidVersionsSortBelowValidOnes) {
    EXPECT_EQ(compare("banana", "0.0.1"), -1);
    EXPECT_EQ(compare("0.0.1", "banana"), 1);
    EXPECT_EQ(compare("banana", "banana"), 0);
    EXPECT_EQ(compare("", "0.0.1"), -1);
}

// Validation is strict, comparison is lenient — deliberately.
//
// `valid()` holds a manifest to the spec. But comparison also has to cope with
// the partial versions that real manifests and catalogs already carry (every
// comparator this header replaced accepted them). If `1.0` were "invalid" at
// compare time it would sort below every real version and silently reorder
// existing catalogs, which is a much worse outcome than accepting a
// sloppy-but-unambiguous string.
TEST(Semver, ComparisonIsLenientAboutPartialVersions) {
    EXPECT_FALSE(valid("1.0"));                   // ...but not a *valid* version
    EXPECT_EQ(compare("1.0", "1.0.0"), 0);        // ...yet it still compares as 1.0.0
    EXPECT_EQ(compare("2.0", "1.0.0"), 1);
    EXPECT_EQ(compare("v1.2.3", "1.2.3"), 0);     // a `v` prefix is tolerated too
    EXPECT_EQ(compare("1", "1.0.0"), 0);
}

// ──────────────────────────────── sorting ─────────────────────────────────

// Descending is the order the catalog stores `versions[]` in, and `versions[0]`
// is what every client shows as "latest".
TEST(Semver, SortDescendingPutsHighestFirst) {
    std::vector<std::string> v = {"1.0.0", "1.0.0-rc.11", "1.0.0-rc.2", "2.0.0-alpha", "1.9.0"};
    sort(v, /*descending=*/true);
    EXPECT_EQ(v, (std::vector<std::string>{"2.0.0-alpha", "1.9.0", "1.0.0", "1.0.0-rc.11", "1.0.0-rc.2"}));
}

TEST(Semver, SortAscending) {
    std::vector<std::string> v = {"1.0.10", "1.0.9", "1.0.0-rc.1"};
    sort(v);
    EXPECT_EQ(v, (std::vector<std::string>{"1.0.0-rc.1", "1.0.9", "1.0.10"}));
}

// ───────────────────────────────── ranges ─────────────────────────────────

TEST(Semver, CaretRanges) {
    EXPECT_TRUE(satisfies("1.2.3", "^1.2.3"));
    EXPECT_TRUE(satisfies("1.9.9", "^1.2.3"));
    EXPECT_FALSE(satisfies("2.0.0", "^1.2.3"));
    EXPECT_FALSE(satisfies("1.2.2", "^1.2.3"));

    // Below 1.0.0 the caret tightens to the leftmost non-zero component.
    EXPECT_TRUE(satisfies("0.2.9", "^0.2.3"));
    EXPECT_FALSE(satisfies("0.3.0", "^0.2.3"));
    EXPECT_TRUE(satisfies("0.0.3", "^0.0.3"));
    EXPECT_FALSE(satisfies("0.0.4", "^0.0.3"));

    // Partial bodies.
    EXPECT_TRUE(satisfies("1.9.9", "^1.2"));
    EXPECT_FALSE(satisfies("2.0.0", "^1.2"));
    EXPECT_TRUE(satisfies("1.9.9", "^1"));
    EXPECT_FALSE(satisfies("2.0.0", "^1"));
}

TEST(Semver, TildeRanges) {
    EXPECT_TRUE(satisfies("1.2.9", "~1.2.3"));
    EXPECT_FALSE(satisfies("1.3.0", "~1.2.3"));
    EXPECT_TRUE(satisfies("1.2.0", "~1.2"));
    EXPECT_FALSE(satisfies("1.3.0", "~1.2"));
    EXPECT_TRUE(satisfies("1.9.0", "~1"));
    EXPECT_FALSE(satisfies("2.0.0", "~1"));
}

TEST(Semver, WildcardAndComparatorRanges) {
    EXPECT_TRUE(satisfies("1.2.9", "1.2.x"));
    EXPECT_FALSE(satisfies("1.3.0", "1.2.x"));
    EXPECT_TRUE(satisfies("9.9.9", "*"));
    EXPECT_TRUE(satisfies("9.9.9", "latest"));
    EXPECT_TRUE(satisfies("1.2.3", "=1.2.3"));
    EXPECT_FALSE(satisfies("1.2.4", "=1.2.3"));

    // Partial inequalities round the bound outward, per npm: `>1.2` means
    // ">= 1.3.0", not "> 1.2.0".
    EXPECT_TRUE(satisfies("1.3.0", ">1.2"));
    EXPECT_FALSE(satisfies("1.2.9", ">1.2"));
    EXPECT_TRUE(satisfies("1.2.0", ">=1.2"));
}

TEST(Semver, ConjunctionAndDisjunction) {
    EXPECT_TRUE(satisfies("1.5.0", ">=1.0.0 <2.0.0"));
    EXPECT_FALSE(satisfies("2.0.0", ">=1.0.0 <2.0.0"));
    EXPECT_TRUE(satisfies("3.0.0", "^1.0.0 || ^3.0.0"));
    EXPECT_TRUE(satisfies("1.2.0", "^1.0.0 || ^3.0.0"));
    EXPECT_FALSE(satisfies("2.0.0", "^1.0.0 || ^3.0.0"));
}

// REGRESSION, and the sharpest edge of the whole change. The npm rule: a range
// never matches a pre-release unless the range itself names one at the same
// major.minor.patch.
//
// The old matcher had no such rule, so `^1.0.0` matched `2.0.0-alpha` — an
// unreleased alpha of the *next major* silently satisfying a caret range on 1.x
// and getting installed as a dependency.
TEST(Semver, RangesDoNotMatchUnrequestedPreReleases) {
    EXPECT_FALSE(satisfies("2.0.0-alpha", "^1.0.0"));
    EXPECT_FALSE(satisfies("1.5.0-beta.1", "^1.0.0"));
    EXPECT_FALSE(satisfies("1.0.0-rc.1", ">=1.0.0"));
    EXPECT_FALSE(satisfies("1.0.0-alpha", "*"));
    EXPECT_FALSE(satisfies("2.0.0-alpha", ">=1.0.0 <3.0.0"));
}

// ...but a range that explicitly opts in still gets them.
TEST(Semver, RangesMatchPreReleasesWhenExplicitlyRequested) {
    EXPECT_TRUE(satisfies("1.0.0-rc.2", "^1.0.0-rc.1"));
    EXPECT_TRUE(satisfies("1.0.0-rc.1", "=1.0.0-rc.1"));
    EXPECT_TRUE(satisfies("1.0.0-rc.1", ">=1.0.0-rc.1 <2.0.0"));
    EXPECT_TRUE(satisfies("1.0.0", "^1.0.0-rc.1"));  // the real release still matches

    // Opt-in is scoped to that exact major.minor.patch: asking for 1.0.0
    // pre-releases does not let 1.5.0's pre-releases through.
    EXPECT_FALSE(satisfies("1.5.0-beta.1", "^1.0.0-rc.1"));
}

TEST(Semver, RangeSyntaxValidation) {
    EXPECT_TRUE(valid_range("^1.2.3"));
    EXPECT_TRUE(valid_range("1.2.x"));
    EXPECT_TRUE(valid_range(">=1.0.0 <2.0.0"));
    EXPECT_TRUE(valid_range("^1.0.0 || ^2.0.0"));
    EXPECT_TRUE(valid_range("*"));

    EXPECT_FALSE(valid_range(""));
    EXPECT_FALSE(valid_range("garbage!!"));
    EXPECT_FALSE(valid_range("^1.0.0 ||"));
    EXPECT_FALSE(valid_range("1.2.3.4"));
    // Hyphen ranges are npm syntax we deliberately do not support. Reject them
    // rather than silently misreading the `-` as part of a version.
    EXPECT_FALSE(valid_range("1.2.3 - 2.3.4"));

    // A wildcard (or empty) component must not be followed by a concrete one.
    // These previously slipped through and silently widened: `1.x.3` -> `1.x`,
    // `1..2` -> `1`, `x.1` -> `*`. A range must not claim more than it means.
    EXPECT_FALSE(valid_range("1.x.3"));
    EXPECT_FALSE(valid_range("1..2"));
    EXPECT_FALSE(valid_range("x.1"));
    EXPECT_FALSE(valid_range(".1.2"));
    // ...but a trailing wildcard is the normal, valid form.
    EXPECT_TRUE(valid_range("1.2.x"));
    EXPECT_TRUE(valid_range("1.x"));
    EXPECT_TRUE(valid_range("^1.x"));
}

// An absent `version` field means "no constraint", which is distinct from an
// explicit `*` (a real wildcard, and therefore still subject to the
// pre-release rule above).
TEST(Semver, EmptyRangeIsUnconstrained) {
    EXPECT_TRUE(satisfies("1.2.3", ""));
    EXPECT_TRUE(satisfies("1.0.0-alpha", ""));
}

TEST(Semver, SatisfiesRejectsInvalidInput) {
    EXPECT_FALSE(satisfies("banana", "^1.0.0"));
    EXPECT_FALSE(satisfies("1.0.0", "garbage!!"));
}
