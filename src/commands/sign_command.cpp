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

    std::string key = getOption(opts, "key", "k");
    if (key.empty()) {
        printError("Missing required option: --key <name-or-path>");
        return 1;
    }

    std::string signerName = getOption(opts, "name", "");
    std::string signerUrl  = getOption(opts, "url", "");
    std::string keysDirOpt = getOption(opts, "keys-dir", "d");

    std::string pkgPath = positional[0];

    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }

    if (!crypto::init()) {
        printError("Failed to initialize crypto library");
        return 1;
    }

    // A value containing a path separator is a key FILE, loaded by
    // content; anything else is a key NAME resolved as
    // <keys-dir>/<name>.jwk. Backward compatible: names with
    // separators were always rejected by validateKeyName().
    const bool isPath =
        key.find('/') != std::string::npos ||
        key.find('\\') != std::string::npos;

    std::optional<crypto::SecretKey> sk;
    if (isPath) {
        if (!keysDirOpt.empty()) {
            printError("--keys-dir has no effect when --key is a file path");
            return 1;
        }
        sk = crypto::Keyring::loadSecretKeyFile(key);
        if (!sk) {
            printError("Failed to load key file '" + key + "': " +
                       crypto::Keyring::getLastError());
            return 1;
        }
    } else {
        std::filesystem::path keysDir;
        if (!keysDirOpt.empty()) {
            keysDir = keysDirOpt;
        } else {
            keysDir = crypto::Keyring::defaultKeysDirectory();
        }
        if (keysDir.empty()) {
            printError("Cannot determine keys directory (HOME not set?)");
            return 1;
        }
        sk = crypto::Keyring::loadSecretKey(keysDir, key);
        if (!sk) {
            printError("Failed to load secret key '" + key + "': " +
                       crypto::Keyring::getLastError());
            return 1;
        }
    }

    auto pkg = Package::load(pkgPath);
    if (!pkg) {
        printError("Failed to load package: " + pkgPath);
        return 1;
    }

    auto signResult = pkg->signPackage(*sk, signerName, signerUrl);
    if (!signResult.success) {
        printError("Failed to sign package: " + signResult.error);
        return 1;
    }

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
