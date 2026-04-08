#include "keygen_command.h"
#include "../crypto/signing.h"
#include "../crypto/keyring.h"

#include <iostream>

namespace lgx {

int KeygenCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);

    std::string keyName = getOption(opts, "name", "n");
    if (keyName.empty()) {
        printError("Missing required option: --name <name>");
        return 1;
    }

    if (!crypto::init()) {
        printError("Failed to initialize crypto library");
        return 1;
    }

    std::string outputDir = getOption(opts, "output-dir", "o");
    std::filesystem::path keysDir;
    if (!outputDir.empty()) {
        keysDir = outputDir;
    } else {
        keysDir = crypto::Keyring::defaultKeysDirectory();
    }
    if (keysDir.empty()) {
        printError("Cannot determine keys directory (HOME not set?)");
        return 1;
    }

    // Check if key already exists
    auto existingPath = keysDir / (keyName + ".jwk");
    if (std::filesystem::exists(existingPath)) {
        printError("Key '" + keyName + "' already exists at: " + existingPath.string());
        return 1;
    }

    // Generate keypair
    auto kp = crypto::generateKeypair();

    // Save
    if (!crypto::Keyring::saveKeypair(keysDir, keyName, kp)) {
        printError("Failed to save keypair: " + crypto::Keyring::getLastError());
        return 1;
    }

    // Print DID to stdout
    std::string did = crypto::publicKeyToDid(kp.publicKey);
    std::cout << did << std::endl;

    printSuccess("Keypair generated: " + keysDir.string() + "/" + keyName + ".{jwk,pub,did}");
    return 0;
}

} // namespace lgx
