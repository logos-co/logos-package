#pragma once

#include "command.h"

namespace lgx {

/**
 * Keygen command: lgx keygen --name <name>
 *
 * Generates an Ed25519 signing keypair.
 */
class KeygenCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "keygen"; }
    std::string description() const override {
        return "Generate a signing keypair";
    }
    std::string usage() const override {
        return "lgx keygen --name <name> [--output-dir <dir>]\n"
               "\n"
               "Generates an Ed25519 signing keypair.\n"
               "\n"
               "Options:\n"
               "  --name, -n <name>       Name for the keypair (required)\n"
               "  --output-dir, -o <dir>  Directory to write key files (default: ~/.config/logos/keys/)\n"
               "\n"
               "Files created:\n"
               "  <dir>/<name>.jwk  Secret key in JWK format (permissions 0600)\n"
               "  <dir>/<name>.pub  Public key (SSH format)\n"
               "  <dir>/<name>.did  DID string (did:jwk:...)\n"
               "\n"
               "The DID (did:jwk:...) is printed to stdout.\n";
    }
};

} // namespace lgx
