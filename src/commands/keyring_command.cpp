#include "keyring_command.h"
#include "../crypto/keyring.h"
#include "../crypto/signing.h"

#include <iostream>

namespace lgx {

int KeyringCommand::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Missing subcommand. Use: lgx keyring add|remove|list [--dir <dir>]");
        return 1;
    }

    if (!crypto::init()) {
        printError("Failed to initialize crypto library");
        return 1;
    }

    // Extract --dir/-d from args before subcommand dispatch
    std::vector<std::string> filteredArgs;
    std::string customDir;
    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "--dir" || args[i] == "-d") && i + 1 < args.size()) {
            customDir = args[++i];
        } else {
            filteredArgs.push_back(args[i]);
        }
    }

    std::filesystem::path keyringDir;
    if (!customDir.empty()) {
        keyringDir = customDir;
    } else {
        keyringDir = crypto::Keyring::defaultDirectory();
    }
    if (keyringDir.empty()) {
        printError("Cannot determine keyring directory (HOME not set?)");
        return 1;
    }

    crypto::Keyring keyring(keyringDir);

    if (filteredArgs.empty()) {
        printError("Missing subcommand. Use: lgx keyring add|remove|list [--dir <dir>]");
        return 1;
    }

    std::string subcommand = filteredArgs[0];

    if (subcommand == "add") {
        if (filteredArgs.size() < 3) {
            printError("Usage: lgx keyring add <name> <did:jwk:...> [--display-name \"...\"] [--url \"...\"] [--dir <dir>]");
            return 1;
        }

        std::string keyName = filteredArgs[1];
        std::string did = filteredArgs[2];

        // Parse optional flags from remaining args
        std::vector<std::string> remaining(filteredArgs.begin() + 3, filteredArgs.end());
        std::vector<std::string> positional;
        auto opts = parseArgs(remaining, positional);

        std::string displayName = getOption(opts, "display-name", "");
        std::string url = getOption(opts, "url", "");

        // Validate DID format
        auto pk = crypto::didToPublicKey(did);
        if (!pk) {
            printError("Invalid DID string: " + did);
            return 1;
        }

        if (!keyring.addKey(keyName, did, displayName, url)) {
            printError("Failed to add key: " + crypto::Keyring::getLastError());
            return 1;
        }

        printSuccess("Added trusted key: " + keyName);
        printInfo("DID: " + did);
        return 0;
    }

    if (subcommand == "remove") {
        if (filteredArgs.size() < 2) {
            printError("Usage: lgx keyring remove <name> [--dir <dir>]");
            return 1;
        }

        std::string keyName = filteredArgs[1];
        if (!keyring.removeKey(keyName)) {
            printError("Failed to remove key: " + crypto::Keyring::getLastError());
            return 1;
        }

        printSuccess("Removed trusted key: " + keyName);
        return 0;
    }

    if (subcommand == "list") {
        auto keys = keyring.listKeys();
        if (keys.empty()) {
            printInfo("No trusted keys");
            return 0;
        }

        for (const auto& key : keys) {
            // Truncate DID for display
            std::string didDisplay = key.did;
            if (didDisplay.size() > 60) {
                didDisplay = didDisplay.substr(0, 57) + "...";
            }
            std::cout << key.name;
            if (!key.displayName.empty()) {
                std::cout << " (" << key.displayName << ")";
            }
            std::cout << std::endl;
            std::cout << "  DID: " << didDisplay << std::endl;
            if (!key.url.empty()) {
                std::cout << "  URL: " << key.url << std::endl;
            }
            if (!key.addedAt.empty()) {
                std::cout << "  Added: " << key.addedAt << std::endl;
            }
        }
        return 0;
    }

    printError("Unknown subcommand: " + subcommand + ". Use: add, remove, or list");
    return 1;
}

} // namespace lgx
