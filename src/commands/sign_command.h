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
        return "lgx sign <pkg.lgx> --key <name> [--keys-dir <dir>] [--name \"Display Name\"] [--url \"https://...\"]\n"
               "\n"
               "Signs a package by computing content hashes and creating\n"
               "an Ed25519 signature over the manifest.\n"
               "\n"
               "Options:\n"
               "  --key, -k <name>       Name of the signing key (required)\n"
               "  --keys-dir, -d <dir>   Directory containing key files (default: ~/.config/logos/keys/)\n"
               "  --name <display-name>  Signer display name (self-asserted metadata)\n"
               "  --url <url>            Signer URL (self-asserted metadata)\n"
               "\n"
               "The secret key is loaded from <keys-dir>/<name>.jwk.\n"
               "Use 'lgx keygen' to generate a keypair first.\n"
               "\n"
               "The signature covers the exact bytes of manifest.json after\n"
               "hashes have been computed. The signed package includes:\n"
               "  - manifest.json with 'hashes' field (Merkle tree)\n"
               "  - manifest.sig with DID, Ed25519 signature, and signer metadata\n";
    }
};

} // namespace lgx
