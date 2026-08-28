#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace lgx {

/**
 * Outcome of resolving a manifest's `main` against an installed directory.
 *
 * FileMissing is deliberately not folded into NoVariantMatch: a manifest that
 * names a main this install does not contain is the variant-mismatch case, and
 * a caller wants to say so rather than report "no main declared". Collapsing
 * every one of these into an empty string is the fail-open pattern that keeps
 * hiding broken installs.
 */
enum class MainResolution {
    Resolved,        ///< the manifest named a main and that file is in the directory
    NotDeclared,     ///< the manifest declares no `main`
    MalformedEntry,  ///< `main` is neither object nor string, or names nothing usable
    NoVariantMatch,  ///< `main` is a variant map and no candidate variant is a key
    FileMissing,     ///< `main` named a file that is not in the directory
    BadInput,        ///< no directory, or manifest bytes that are not a JSON object
};

/**
 * The package's main file, as the manifest names it.
 */
struct MainFile {
    MainResolution state = MainResolution::BadInput;
    /// Absolute, native separators. Non-empty only when Resolved, so an
    /// unresolved main can never be mistaken for a usable one.
    std::string path;
    std::string declaredPath;  ///< the relative path the manifest named
    std::string variant;       ///< the `main` key that selected it (map form only)
    std::string error;         ///< detail for the non-Resolved states

    bool isResolved() const { return state == MainResolution::Resolved; }
};

/**
 * Resolve `main` for the first of `variants` the manifest names.
 *
 * THIS TOUCHES THE DISK, and has to: "does the named path stay inside the
 * package" and "is it a file rather than a directory" cannot be answered from
 * the manifest text. `dir` is not required to exist — a directory that is not
 * there simply contains no main.
 *
 * The rules, in order:
 *   - the first candidate variant that is a key of `main` wins OUTRIGHT and
 *     does NOT fall through to a later one when its named file is missing;
 *   - a key whose value is empty or not a string DOES fall through, and is
 *     reported as MalformedEntry only if nothing later matches;
 *   - a path that resolves outside `dir` is MalformedEntry and is never
 *     opened — the directory IS the package, so a path leaving it names
 *     nothing of the package's, whatever it names;
 *   - a path naming a directory is MalformedEntry, not Resolved: a caller
 *     handed it would fail to load it.
 *
 * Variant keys are matched VERBATIM. Manifest::normalizeVariantKeys() is the
 * write path's business; a directory on disk is read as it was written.
 *
 * @param manifestBytes manifest.json exactly as read. Parsed here rather than
 *   taken as a Manifest because `main` also has a plain-string form, which
 *   Manifest::fromJson rejects outright — this has to resolve packages that
 *   type will not load.
 * @param variants Candidate variant spellings, most preferred first. Empty
 *   means no candidate, so a variant-map `main` yields NoVariantMatch.
 */
MainFile resolveMain(const std::filesystem::path& dir,
                     const std::string& manifestBytes,
                     const std::vector<std::string>& variants);

} // namespace lgx
