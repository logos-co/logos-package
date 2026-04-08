#include "verify_command.h"
#include "core/package.h"
#include "../crypto/signing.h"
#include "../crypto/keyring.h"

#include <filesystem>
#include <iostream>

namespace lgx {

int VerifyCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);

    // Check for package path
    if (positional.empty()) {
        printError("Missing package path");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }

    std::string pkgPath = positional[0];

    // Check if package exists
    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }

    // Structural verification
    auto result = Package::verify(pkgPath);

    // Print structural warnings
    if (!result.warnings.empty()) {
        for (const auto& warning : result.warnings) {
            std::cout << "Warning: " << warning << std::endl;
        }
    }

    if (!result.valid) {
        printError("Package validation failed:");
        for (const auto& error : result.errors) {
            std::cerr << "  - " << error << std::endl;
        }
        return 1;
    }

    printSuccess("Package structure is valid: " + pkgPath);

    // Signature verification
    if (!crypto::init()) {
        printError("Failed to initialize crypto library");
        return 1;
    }

    auto pkg = Package::load(pkgPath);
    if (!pkg) {
        printError("Failed to load package for signature verification");
        return 1;
    }

    auto sigInfo = pkg->verifySignature();

    if (!sigInfo.is_signed) {
        printInfo("Package is unsigned");
        return 0;
    }

    // Package is signed — check signature
    if (!sigInfo.signature_valid) {
        printError("Signature verification FAILED: " + sigInfo.error);
        return 1;
    }

    if (!sigInfo.package_valid) {
        printError("Package validation FAILED: " + sigInfo.error);
        return 1;
    }

    printSuccess("Signature is valid");
    printInfo("Signer DID: " + sigInfo.signer_did);
    if (!sigInfo.signer_name.empty()) {
        printInfo("Signer name: " + sigInfo.signer_name);
    }
    if (!sigInfo.signer_url.empty()) {
        printInfo("Signer URL: " + sigInfo.signer_url);
    }

    // Check keyring
    std::string keyringDirOpt = getOption(opts, "keyring-dir", "");
    std::filesystem::path keyringDir;
    if (!keyringDirOpt.empty()) {
        keyringDir = keyringDirOpt;
    } else {
        keyringDir = crypto::Keyring::defaultDirectory();
    }
    if (!keyringDir.empty() && std::filesystem::exists(keyringDir)) {
        crypto::Keyring keyring(keyringDir);
        auto trusted = keyring.findByDid(sigInfo.signer_did);
        if (trusted) {
            printSuccess("Signer is trusted: " + trusted->name);
        } else {
            printInfo("Signer DID is NOT in trusted keyring");
        }
    }

    return 0;
}

} // namespace lgx
