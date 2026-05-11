#pragma once

#include "command.h"

namespace lgx {

/**
 * Manifest command: lgx manifest <pkg.lgx> [--json]
 *
 * Prints the contents of manifest.json inside an LGX package.
 * Default output is human-readable; --json prints the raw manifest JSON
 * bytes as they appear inside the package (byte-identical to the embedded
 * file), which is what tooling such as logos-modules-release-action
 * consumes.
 */
class ManifestCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "manifest"; }
    std::string description() const override {
        return "Print the manifest of a package";
    }
    std::string usage() const override {
        return "lgx manifest <pkg.lgx> [--json]\n"
               "\n"
               "Prints the contents of manifest.json from inside the package.\n"
               "\n"
               "By default, output is a human-readable summary including\n"
               "name, version, type, category, author, root hash, variants,\n"
               "dependencies, and signer DID (when the package is signed).\n"
               "\n"
               "With --json, the raw manifest.json bytes are written to\n"
               "stdout verbatim. This is intended for tooling and is\n"
               "byte-identical to the file inside the .lgx.\n"
               "\n"
               "Options:\n"
               "  --json  Output raw manifest.json bytes\n"
               "\n"
               "Returns 0 on success, non-zero if the package or its manifest\n"
               "is missing or malformed.\n"
               "\n"
               "Examples:\n"
               "  lgx manifest mymodule.lgx\n"
               "  lgx manifest mymodule.lgx --json > manifest.json";
    }
};

} // namespace lgx
