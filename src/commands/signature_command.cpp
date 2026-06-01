#include "signature_command.h"
#include "core/package.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace lgx {

namespace {

// Locate the raw `manifest.sig` bytes inside the loaded package. Mirrors
// the `findRawSignatureBytes` helper in manifest_command.cpp — duplicated
// here on purpose so the two commands stay independent (manifest_command's
// helper sits in an anonymous namespace and isn't part of the Package API
// proper; promoting it to a public header just for code-sharing would be
// scope creep for this command). Returns nullopt when the package is
// unsigned.
std::optional<std::string> findRawSignatureBytes(const Package& pkg) {
    for (const auto& entry : pkg.getEntries()) {
        if (entry.path == "manifest.sig" && !entry.isDirectory) {
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

    auto rawSig = findRawSignatureBytes(*pkg);
    if (!rawSig.has_value()) {
        // Unsigned package: deliberately no stdout output, no stderr
        // noise, exit 0. Callers (e.g. the out-of-CI index builder)
        // distinguish "unsigned" from "error" via the exit status, not
        // the stream length — matching the contract documented in the
        // command's help text.
        return 0;
    }

    // Raw bytes verbatim, no trailing newline — byte-identical to the
    // file embedded in the .lgx. Mirrors `lgx manifest --json` for the
    // manifest side.
    std::cout.write(rawSig->data(),
                    static_cast<std::streamsize>(rawSig->size()));
    return 0;
}

} // namespace lgx
