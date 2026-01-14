#include "remove_command.h"
#include "core/package.h"
#include "core/path_normalizer.h"

#include <filesystem>

namespace lgx {

int RemoveCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);
    
    // Check for package path
    if (positional.empty()) {
        printError("Missing package path");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }
    
    std::string pkgPath = positional[0];
    
    // Check for required options
    std::string variant = getOption(opts, "variant", "v");
    if (variant.empty()) {
        printError("Missing --variant option");
        return 1;
    }
    
    bool autoYes = hasFlag(opts, "yes", "y");
    
    // Check if package exists
    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }
    
    // Load the package
    auto pkgOpt = Package::load(pkgPath);
    if (!pkgOpt) {
        printError("Failed to load package: " + Package::getLastError());
        return 1;
    }
    
    Package& pkg = *pkgOpt;
    std::string variantLc = PathNormalizer::toLowercase(variant);
    
    // Check if variant exists
    if (!pkg.hasVariant(variantLc)) {
        printError("Variant not found: " + variantLc);
        return 1;
    }
    
    // Prompt for confirmation
    if (!autoYes) {
        if (!confirm("Remove variant '" + variantLc + "'?", true)) {
            printInfo("Aborted.");
            return 1;
        }
    }
    
    // Remove the variant
    auto result = pkg.removeVariant(variantLc);
    if (!result.success) {
        printError(result.error);
        return 1;
    }
    
    // Save the package
    result = pkg.save(pkgPath);
    if (!result.success) {
        printError("Failed to save package: " + result.error);
        return 1;
    }
    
    printSuccess("Removed variant '" + variantLc + "' from " + pkgPath);
    return 0;
}

} // namespace lgx
