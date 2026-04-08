#pragma once

#include "command.h"

namespace lgx {

/**
 * Verify command: lgx verify <pkg.lgx>
 *
 * Validates a package against the LGX specification and
 * verifies cryptographic signatures if present.
 */
class VerifyCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "verify"; }
    std::string description() const override {
        return "Verify a package is valid";
    }
    std::string usage() const override {
        return "lgx verify <pkg.lgx> [--keyring-dir <dir>]\n"
               "\n"
               "Validates a package against the LGX specification:\n"
               "  - tar.gz readable\n"
               "  - Root layout restrictions enforced\n"
               "  - Manifest required fields present\n"
               "  - NFC normalization enforced for all paths\n"
               "  - Completeness constraint for main vs variants\n"
               "  - Each main entry points to existing file\n"
               "  - No forbidden file types\n"
               "\n"
               "If the package is signed, also verifies:\n"
               "  - Ed25519 signature over manifest.json\n"
               "  - Content hashes match (Merkle tree)\n"
               "  - Signer key against trusted keyring\n"
               "\n"
               "Options:\n"
               "  --keyring-dir <dir>  Keyring directory for trust lookup (default: ~/.config/logos/trusted-keys/)\n"
               "\n"
               "Returns 0 on success, non-zero on validation failure.\n"
               "\n"
               "Examples:\n"
               "  lgx verify mymodule.lgx\n"
               "  lgx verify mymodule.lgx --keyring-dir /path/to/keyring";
    }
};

} // namespace lgx
