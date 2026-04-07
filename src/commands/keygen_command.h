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
        return "lgx keygen --name <name>\n"
               "\n"
               "Generates an Ed25519 signing keypair.\n"
               "\n"
               "Options:\n"
               "  --name, -n <name>  Name for the keypair (required)\n"
               "\n"
               "Files created:\n"
               "  ~/.config/logos/keys/<name>.jwk  Secret key in JWK format (permissions 0600)\n"
               "  ~/.config/logos/keys/<name>.pub  Public key (SSH format)\n"
               "  ~/.config/logos/keys/<name>.did  DID string (did:jwk:...)\n"
               "\n"
               "The DID (did:jwk:...) is printed to stdout.\n";
    }
};

} // namespace lgx
