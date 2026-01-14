#include "verify_command.h"
#include "core/package.h"

#include <filesystem>

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
    
    // Verify the package
    auto result = Package::verify(pkgPath);
    
    // Print results
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
    
    printSuccess("Package is valid: " + pkgPath);
    return 0;
}

} // namespace lgx
