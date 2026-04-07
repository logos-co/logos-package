#include "sign_command.h"
#include "../core/package.h"
#include "../crypto/signing.h"
#include "../crypto/keyring.h"

#include <iostream>

namespace lgx {

int SignCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);

    if (positional.empty()) {
        printError("Missing package path");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }

    std::string keyName = getOption(opts, "key", "k");
    if (keyName.empty()) {
        printError("Missing required option: --key <name>");
        return 1;
    }

    std::string signerName = getOption(opts, "name", "");
    std::string signerUrl = getOption(opts, "url", "");

    std::string pkgPath = positional[0];

    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }

    if (!crypto::init()) {
        printError("Failed to initialize crypto library");
        return 1;
    }

    // Load secret key
    auto keysDir = crypto::Keyring::defaultKeysDirectory();
    if (keysDir.empty()) {
        printError("Cannot determine keys directory (HOME not set?)");
        return 1;
    }

    auto sk = crypto::Keyring::loadSecretKey(keysDir, keyName);
    if (!sk) {
        printError("Failed to load secret key '" + keyName + "': " +
                  crypto::Keyring::getLastError());
        return 1;
    }

    // Load package
    auto pkg = Package::load(pkgPath);
    if (!pkg) {
        printError("Failed to load package: " + pkgPath);
        return 1;
    }

    // Sign
    auto signResult = pkg->signPackage(*sk, signerName, signerUrl);
    if (!signResult.success) {
        printError("Failed to sign package: " + signResult.error);
        return 1;
    }

    // Save
    auto saveResult = pkg->save(pkgPath);
    if (!saveResult.success) {
        printError("Failed to save signed package: " + saveResult.error);
        return 1;
    }

    auto did = crypto::publicKeyToDid(crypto::extractPublicKey(*sk));
    printSuccess("Package signed: " + pkgPath);
    printInfo("Signer DID: " + did);

    return 0;
}

} // namespace lgx
