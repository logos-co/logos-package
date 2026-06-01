#pragma once

#include "command.h"

namespace lgx {

/**
 * Signature command: lgx signature <pkg.lgx>
 *
 * Prints the raw `manifest.sig` bytes from inside the package to stdout,
 * verbatim (byte-identical to the file embedded in the .lgx). This is
 * the signature counterpart of `lgx manifest --json` — intended for
 * tooling that needs to capture the exact signature blob, e.g. an
 * out-of-CI index builder reproducing what
 * `logos-modules-release-action`'s `release.yml` records under
 * `sidecar.json#signature`.
 *
 * Unsigned packages print nothing to stdout and exit 0 — callers
 * distinguish "no signature" from "error" by checking the exit status,
 * not the stream length.
 */
class SignatureCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "signature"; }
    std::string description() const override {
        return "Print the raw manifest.sig bytes of a package";
    }
    std::string usage() const override {
        return "lgx signature <pkg.lgx>\n"
               "\n"
               "Writes the package's `manifest.sig` contents to stdout\n"
               "verbatim (byte-identical to the file embedded in the .lgx).\n"
               "\n"
               "Unsigned packages produce no output and exit 0. A bad or\n"
               "unreadable package exits non-zero with the error on stderr.\n"
               "\n"
               "Pair with `lgx manifest <pkg> --json` to capture both halves\n"
               "of a package's signed envelope in machine-readable form\n"
               "(e.g. when building an index.json outside CI).\n"
               "\n"
               "Returns 0 on success (including the unsigned case), non-zero\n"
               "if the package is missing or malformed.\n"
               "\n"
               "Examples:\n"
               "  lgx signature mymodule.lgx           # to terminal\n"
               "  lgx signature mymodule.lgx > sig     # capture to file\n"
               "  lgx signature mymodule.lgx | jq .did # extract signer DID";
    }
};

} // namespace lgx
