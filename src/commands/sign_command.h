#pragma once

#include "command.h"

namespace lgx {

/**
 * Sign command: lgx sign <pkg.lgx> --key <name>
 *
 * Signs a package with an Ed25519 keypair.
 * Computes Merkle tree hashes and creates manifest.sig.
 */
class SignCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "sign"; }
    std::string description() const override {
        return "Sign a package with an Ed25519 key";
    }
    std::string usage() const override {
        return "lgx sign <pkg.lgx> --key <name-or-path> [--keys-dir <dir>] [--name \"Display Name\"] [--url \"https://...\"]\n"
               "\n"
               "Signs a package by computing content hashes and creating\n"
               "an Ed25519 signature over the manifest.\n"
               "\n"
               "Options:\n"
               "  --key, -k <name-or-path>  Signing key (required): a key name resolved\n"
               "                            as <keys-dir>/<name>.jwk, or — when the value\n"
               "                            contains a path separator — a key file loaded\n"
               "                            by content (filename/extension irrelevant).\n"
               "                            Use ./file.jwk for a file in the current dir.\n"
               "  --keys-dir, -d <dir>      Directory containing key files, used with a\n"
               "                            key name (default: ~/.config/logos/keys/).\n"
               "                            Not allowed when --key is a file path.\n"
               "  --name <display-name>     Signer display name (self-asserted metadata)\n"
               "  --url <url>               Signer URL (self-asserted metadata)\n"
               "\n"
               "Use 'lgx keygen' to generate a keypair first.\n"
               "\n"
               "The signature covers the exact bytes of manifest.json after\n"
               "hashes have been computed. The signed package includes:\n"
               "  - manifest.json with 'hashes' field (Merkle tree)\n"
               "  - manifest.sig with DID, Ed25519 signature, and signer metadata\n";
    }
};

} // namespace lgx
