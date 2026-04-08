#pragma once

#include "command.h"

namespace lgx {

/**
 * Keyring command: lgx keyring add|remove|list
 *
 * Manages trusted public keys for package verification.
 */
class KeyringCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "keyring"; }
    std::string description() const override {
        return "Manage trusted signing keys";
    }
    std::string usage() const override {
        return "lgx keyring <subcommand> [options]\n"
               "\n"
               "Subcommands:\n"
               "  add <name> <did:jwk:...> [--display-name \"...\"] [--url \"...\"]\n"
               "                             Add a trusted key by DID\n"
               "  remove <name>              Remove a trusted key\n"
               "  list                       List all trusted keys\n"
               "\n"
               "Options:\n"
               "  --dir, -d <dir>  Keyring directory (default: ~/.config/logos/trusted-keys/)\n"
               "\n"
               "Keys are stored as .json files in the keyring directory.\n";
    }
};

} // namespace lgx
