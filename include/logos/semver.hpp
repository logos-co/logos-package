// The one semver implementation for the Logos packaging stack.
//
// Everything that parses, compares, sorts or range-matches a version — lgx,
// lgpm, lgpd, the package-manager UI, and (via `lgx semver`) the catalog
// builder — goes through this header. It replaced five divergent hand-rolled
// copies that disagreed with each other and with the spec.
//
// Two layers, split the way the specs themselves split:
//
//   Precedence  is defined by SemVer 2.0.0 (§10-§11) and is delegated wholesale
//               to z4kn4fein/cpp-semver: numeric identifiers compare
//               numerically (so `rc.11` > `rc.2`), numeric sorts below
//               alphanumeric, a longer identifier set wins ties, and build
//               metadata is ignored. We do not reimplement any of it.
//
//   Ranges      (`^ ~ x * || >= <= > < =`) are NOT in the semver spec at all.
//               They are an npm convention that the manifests already use
//               ("npm/Cargo-style semver range" — see Manifest::Dependency).
//               No library can be "the spec" here, so that layer lives below,
//               built on the comparator above.
//
// The npm pre-release rule is enforced: a range never matches a pre-release
// version unless the range itself names a pre-release at the same
// major.minor.patch. Without it `^1.0.0` matches `2.0.0-alpha` — an unreleased
// alpha of the next major silently satisfying a caret range on 1.x.
//
// Header-only and dependency-free on purpose: the package-manager UI is a QML
// plugin with no native link deps, and must be able to include this without
// dragging in zlib/ICU/libsodium.

#ifndef LOGOS_SEMVER_HPP
#define LOGOS_SEMVER_HPP

// <cstdint> MUST come before <semver/semver.hpp>.
//
// cpp-semver 0.4.0 uses uint64_t throughout but never includes <cstdint>
// itself (upstream bug). libc++ happens to drag it in transitively via
// <string>/<regex>, so this builds on macOS — but libstdc++ stopped doing that
// in GCC 13, and on Linux the header then fails to parse with "'uint64_t' does
// not name a type", which collapses semver::version and surfaces as a wall of
// bogus "has no member named 'major'" errors in THIS file.
#include <cstdint>

#include <semver/semver.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace logos {
namespace semver {

using version = ::semver::version;

// ─────────────────────────── parse / validate ───────────────────────────

// Strict parse: the official semver.org regex. Rejects `1.0`, `01.0.0`,
// `1.0.0-`, and anything else the spec disallows. This is what `valid()`
// answers with — validation should hold manifests to the spec.
inline std::optional<version> parse(const std::string& s)
{
    try {
        return version::parse(s, /*strict=*/true);
    } catch (const ::semver::semver_exception&) {
        return std::nullopt;
    }
}

inline bool valid(const std::string& s)
{
    return parse(s).has_value();
}

// Lenient parse: additionally accepts a `v` prefix and partial versions,
// zero-filling the missing components (`1.2` → 1.2.0, `v1` → 1.0.0).
//
// Comparison uses this, validation does not. Real manifests and catalogs in the
// wild carry partial versions, and the comparators that came before this header
// all accepted them. Rejecting `1.0` outright at *compare* time would make it
// sort below every valid version, silently reordering existing catalogs — a far
// worse failure than accepting a sloppy-but-unambiguous version string. Truly
// unparseable input (`banana`, ``) is still rejected.
inline std::optional<version> parse_lenient(const std::string& s)
{
    try {
        return version::parse(s, /*strict=*/false);
    } catch (const ::semver::semver_exception&) {
        return std::nullopt;
    }
}

// ───────────────────────────── precedence ───────────────────────────────

// cpp-semver keeps version::compare private and exposes only the relational
// operators, so recover the three-way result from those.
inline int compare(const version& a, const version& b)
{
    if (a < b) return -1;
    if (b < a) return 1;
    return 0;
}

// Three-way compare over raw strings, as a total order, so it can back
// std::sort directly.
//
// Versions compare by SemVer precedence (leniently parsed — see
// parse_lenient). An unparseable version sorts BELOW every parseable one — a
// catalog carrying a junk version string must never let it win "latest" — and
// two unparseable strings fall back to a byte compare so the relation stays a
// strict weak ordering.
inline int compare(const std::string& a, const std::string& b)
{
    const auto va = parse_lenient(a);
    const auto vb = parse_lenient(b);
    if (va && vb) return compare(*va, *vb);
    if (va) return 1;
    if (vb) return -1;
    if (a == b) return 0;
    return a < b ? -1 : 1;
}

inline bool equal(const std::string& a, const std::string& b) { return compare(a, b) == 0; }
inline bool less(const std::string& a, const std::string& b) { return compare(a, b) < 0; }
inline bool greater_or_equal(const std::string& a, const std::string& b) { return compare(a, b) >= 0; }

// Stable sort in place. `descending` = newest first, which is the order the
// catalog stores `versions[]` in.
inline void sort(std::vector<std::string>& versions, bool descending = false)
{
    std::stable_sort(versions.begin(), versions.end(),
                     [descending](const std::string& a, const std::string& b) {
                         const int c = compare(a, b);
                         return descending ? c > 0 : c < 0;
                     });
}

// ─────────────────────────────── ranges ─────────────────────────────────

namespace detail {

enum class op { eq, lt, lte, gt, gte };

// A range desugars to a disjunction of conjunctions of these.
struct primitive {
    op       kind;
    version  bound;
};

using conjunction = std::vector<primitive>;

inline std::string trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

inline std::vector<std::string> split(const std::string& s, const std::string& sep)
{
    std::vector<std::string> out;
    size_t pos = 0;
    for (;;) {
        const size_t next = s.find(sep, pos);
        if (next == std::string::npos) { out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos, next - pos));
        pos = next + sep.size();
    }
    return out;
}

inline bool is_wildcard(const std::string& p)
{
    return p == "*" || p == "x" || p == "X";
}

// A comparator's body is a *partial* version: `1`, `1.2`, `1.2.3`, `1.2.x`,
// with an optional pre-release/build suffix. `specified` counts how many of
// major/minor/patch were actually pinned (a wildcard stops the count), which
// is what decides how `^`, `~` and the partial `<`/`>` forms desugar.
struct partial {
    version base{0, 0, 0};
    int     specified = 0;
};

inline std::optional<partial> parse_partial(const std::string& body)
{
    if (body.empty()) return std::nullopt;
    if (is_wildcard(body)) return partial{version(0, 0, 0), 0};

    // Peel the pre-release / build suffix off before the dotted numeric split,
    // so the '.' inside `-rc.1` isn't mistaken for a component separator.
    std::string core = body;
    std::string suffix;
    const size_t cut = core.find_first_of("-+");
    if (cut != std::string::npos) {
        suffix = core.substr(cut);
        core = core.substr(0, cut);
    }

    const std::vector<std::string> parts = split(core, ".");
    if (parts.empty() || parts.size() > 3) return std::nullopt;

    int specified = 0;
    std::string norm[3] = {"0", "0", "0"};
    for (size_t i = 0; i < parts.size(); ++i) {
        if (is_wildcard(parts[i]) || parts[i].empty()) break;  // wildcard ends the pin
        norm[i] = parts[i];
        ++specified;
    }
    if (specified == 0 && !suffix.empty()) return std::nullopt;  // `x-rc.1` is nonsense

    // Round-trip through the strict parser so the body is held to exactly the
    // same grammar as a version (rejects `01`, `1.2.3.4`, `1.0.0-`, ...).
    const std::string canonical = norm[0] + "." + norm[1] + "." + norm[2] + suffix;
    const auto v = parse(canonical);
    if (!v) return std::nullopt;
    return partial{*v, specified};
}

inline version bump_major(const version& v) { return version(v.major() + 1, 0, 0); }
inline version bump_minor(const version& v) { return version(v.major(), v.minor() + 1, 0); }
inline version bump_patch(const version& v) { return version(v.major(), v.minor(), v.patch() + 1); }

// Desugar one comparator token (e.g. `^1.2`, `>=1.2.3-rc.1`, `1.x`) into
// primitives. Returns false if the token isn't valid range syntax.
inline bool desugar(const std::string& token, conjunction& out)
{
    const std::string c = trim(token);
    if (c.empty()) return false;

    // `latest` is accepted as an alias for `*` — the old hand-rolled matcher
    // took it, and manifests in the wild use it.
    if (is_wildcard(c) || c == "latest") {
        out.push_back({op::gte, version(0, 0, 0)});
        return true;
    }

    std::string o;
    size_t i = 0;
    if (c.rfind(">=", 0) == 0 || c.rfind("<=", 0) == 0) { o = c.substr(0, 2); i = 2; }
    else if (c[0] == '>' || c[0] == '<' || c[0] == '=' || c[0] == '^' || c[0] == '~') { o = c.substr(0, 1); i = 1; }
    else { o = "="; }

    const auto p = parse_partial(trim(c.substr(i)));
    if (!p) return false;
    const version& b = p->base;
    const int spec = p->specified;

    if (o == "^") {
        // Caret: compatible up to the next change of the leftmost non-zero
        // component that was actually pinned.
        //   ^1.2.3 → >=1.2.3 <2.0.0     ^0.2.3 → >=0.2.3 <0.3.0
        //   ^0.0.3 → >=0.0.3 <0.0.4     ^0.0.x → >=0.0.0 <0.1.0
        //   ^1.x   → >=1.0.0 <2.0.0     ^0.x   → >=0.0.0 <1.0.0
        version hi(0, 0, 0);
        if (b.major() != 0)                    hi = bump_major(b);
        else if (spec >= 2 && b.minor() != 0)  hi = bump_minor(b);
        else if (spec == 3)                    hi = bump_patch(b);
        else if (spec == 2)                    hi = bump_minor(b);
        else                                   hi = bump_major(b);
        out.push_back({op::gte, b});
        out.push_back({op::lt, hi});
        return true;
    }
    if (o == "~") {
        // Tilde: patch-level drift when the minor is pinned, else minor-level.
        //   ~1.2.3 → >=1.2.3 <1.3.0     ~1.2 → >=1.2.0 <1.3.0
        //   ~1     → >=1.0.0 <2.0.0
        const version hi = (spec >= 2) ? bump_minor(b) : bump_major(b);
        out.push_back({op::gte, b});
        out.push_back({op::lt, hi});
        return true;
    }
    if (o == "=") {
        if (spec == 0) { out.push_back({op::gte, version(0, 0, 0)}); return true; }  // `x.y.z` all-wild
        if (spec == 3) { out.push_back({op::eq, b}); return true; }
        // A partial equality is a range: `1.2` means "any 1.2.z", `1` means "any 1.y.z".
        out.push_back({op::gte, b});
        out.push_back({op::lt, spec == 2 ? bump_minor(b) : bump_major(b)});
        return true;
    }
    // Inequalities against a partial body follow npm: the unpinned components
    // round the bound outward, so `>1.2` means ">=1.3.0", not ">1.2.0".
    const version rounded_up = (spec == 2) ? bump_minor(b) : bump_major(b);

    if (o == ">") {
        if (spec == 3) out.push_back({op::gt, b});
        else           out.push_back({op::gte, rounded_up});
        return true;
    }
    if (o == "<=") {
        if (spec == 3) out.push_back({op::lte, b});
        else           out.push_back({op::lt, rounded_up});
        return true;
    }
    if (o == ">=") { out.push_back({op::gte, b}); return true; }
    if (o == "<")  { out.push_back({op::lt, b});  return true; }
    return false;
}

inline bool satisfies_primitive(const version& v, const primitive& p)
{
    const int c = ::logos::semver::compare(v, p.bound);
    switch (p.kind) {
        case op::eq:  return c == 0;
        case op::lt:  return c < 0;
        case op::lte: return c <= 0;
        case op::gt:  return c > 0;
        case op::gte: return c >= 0;
    }
    return false;
}

// Parse a full range into a disjunction of conjunctions. Empty on failure.
inline std::optional<std::vector<conjunction>> parse_range(const std::string& range)
{
    std::vector<conjunction> alternatives;
    for (const std::string& alt : split(range, "||")) {
        const std::string a = trim(alt);
        if (a.empty()) return std::nullopt;  // `1.0.0 ||` / `|| 1.0.0` are malformed

        conjunction conj;
        std::istringstream iss(a);
        std::string tok;
        bool saw = false;
        while (iss >> tok) {
            saw = true;
            if (!desugar(tok, conj)) return std::nullopt;
        }
        if (!saw) return std::nullopt;
        alternatives.push_back(std::move(conj));
    }
    if (alternatives.empty()) return std::nullopt;
    return alternatives;
}

}  // namespace detail

// Is this a well-formed range? (Syntax only — says nothing about what matches.)
inline bool valid_range(const std::string& range)
{
    if (range.empty()) return false;
    return detail::parse_range(range).has_value();
}

// Does `version` satisfy `range`?
//
// An empty range means "no constraint was declared" (the manifest's `version`
// field is optional) and matches anything. Note this differs from an explicit
// `*`, which is a real npm wildcard and therefore — like every range — does
// NOT match a pre-release.
inline bool satisfies(const std::string& version_str, const std::string& range)
{
    if (range.empty()) return true;

    const auto v = parse_lenient(version_str);
    if (!v) return false;

    const auto alternatives = detail::parse_range(range);
    if (!alternatives) return false;

    for (const detail::conjunction& conj : *alternatives) {
        bool all = true;
        for (const detail::primitive& p : conj) {
            if (!detail::satisfies_primitive(*v, p)) { all = false; break; }
        }
        if (!all) continue;

        // The npm pre-release rule. A pre-release is only ever a candidate for
        // a range that explicitly opted into pre-releases at that exact
        // major.minor.patch. So `^1.0.0` rejects `2.0.0-alpha` (and `1.5.0-beta`),
        // while `^1.0.0-rc.1` still accepts `1.0.0-rc.2`.
        if (v->is_prerelease()) {
            bool opted_in = false;
            for (const detail::primitive& p : conj) {
                if (p.bound.is_prerelease()
                    && p.bound.major() == v->major()
                    && p.bound.minor() == v->minor()
                    && p.bound.patch() == v->patch()) {
                    opted_in = true;
                    break;
                }
            }
            if (!opted_in) continue;
        }
        return true;
    }
    return false;
}

}  // namespace semver
}  // namespace logos

#endif  // LOGOS_SEMVER_HPP
