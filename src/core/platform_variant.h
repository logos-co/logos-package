#pragma once

#include <string>
#include <vector>

namespace lgx {

// Variant NAMES: which spellings of "<os>-<architecture>" name the same thing,
// and which one this build is.
//
// A variant name is not free text — it is a key inside the signed hash tree
// (hashes["variants/<name>"]), so this library is where the vocabulary belongs
// and every consumer reads it from here rather than keeping its own.

/**
 * The variant this build of the library targets, e.g. "darwin-arm64".
 *
 * Compile-time, from the predefined macros: it describes the machine the
 * binary runs on and reads nothing. "unknown" for a target with no rule here.
 */
std::string hostVariant();

/**
 * `variant` first, then every other live spelling of its ARCHITECTURE half:
 * the list a consumer walks when looking for its own variant in a package.
 *
 * Empty in, empty out. An architecture no row knows degrades to the input
 * alone, which is the pre-alias behaviour and still fail-closed.
 */
std::vector<std::string> variantSpellings(const std::string& variant);

} // namespace lgx
