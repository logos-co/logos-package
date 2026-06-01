#include "signature_command.h"
#include "core/package.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace lgx {

namespace {

// Locate the raw bytes of a given top-level archive entry. Mirrors the
// `findRawManifestBytes` / `findRawSignatureBytes` helpers in
// manifest_command.cpp — replicated here so the two commands stay
// independent (those helpers live in an anonymous namespace and aren't
// part of the Package API proper; promoting them to a public header
// just for code-sharing would be scope creep for this command).
// Returns nullopt when the entry is absent.
std::optional<std::string> findRawEntryBytes(const Package& pkg, const std::string& path) {
    for (const auto& entry : pkg.getEntries()) {
        if (entry.path == path && !entry.isDirectory) {
            return std::string(entry.data.begin(), entry.data.end());
        }
    }
    return std::nullopt;
}

} // namespace

int SignatureCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    parseArgs(args, positional);

    if (positional.empty()) {
        printError("Missing package path");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }

    const std::string pkgPath = positional[0];
    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }

    auto pkg = Package::load(pkgPath);
    if (!pkg) {
        printError("Failed to load package: " + Package::getLastError());
        return 1;
    }

    // Malformed-vs-unsigned guard. `Package::load` succeeds for any
    // openable+decompressable .lgx — including pathological archives
    // with no `manifest.json` at all. Without this check, such a
    // package would slip past as "unsigned" (exit 0, empty stdout) and
    // a caller scripting against the exit-status contract ("0 means
    // I can stop worrying about this package") would silently treat
    // a broken archive as fine. Real packages always carry a manifest;
    // its absence means the file isn't a usable .lgx, which is an
    // error, not an "unsigned" state.
    if (!findRawEntryBytes(*pkg, "manifest.json").has_value()) {
        printError("Package does not contain a manifest.json entry "
                   "(malformed .lgx — not the same as an unsigned package)");
        return 1;
    }

    auto rawSig = findRawEntryBytes(*pkg, "manifest.sig");
    if (!rawSig.has_value()) {
        // Genuinely unsigned package: no stdout output, no stderr noise,
        // exit 0. Callers (e.g. the out-of-CI index builder) distinguish
        // "unsigned" from "error" via the exit status, not the stream
        // length — matching the contract in the command's help text.
        return 0;
    }

    // Byte-identical output to the file embedded in the .lgx. On
    // Windows, stdout opens in text mode by default — `\n` in the
    // payload would be translated to `\r\n` on the wire, breaking the
    // byte-identical contract. Switch to binary mode before writing.
    // POSIX stdout is always binary, so the #ifdef is the whole guard.
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::cout.write(rawSig->data(),
                    static_cast<std::streamsize>(rawSig->size()));
    return 0;
}

} // namespace lgx
