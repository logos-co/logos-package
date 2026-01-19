#include "extract_command.h"
#include "core/package.h"
#include "core/path_normalizer.h"

#include <filesystem>

namespace lgx {

int ExtractCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);
    
    // Check for package path
    if (positional.empty()) {
        printError("Missing package path");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }
    
    std::string pkgPath = positional[0];
    
    std::string variant = getOption(opts, "variant", "v");
    std::string outputDir = getOption(opts, "output", "o", ".");
    
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
    
    Package::Result result;
    if (variant.empty()) {
        result = pkg.extractAll(outputDir);
        if (result.success) {
            auto variants = pkg.getVariants();
            if (variants.empty()) {
                printInfo("No variants to extract");
            } else {
                printSuccess("Extracted " + std::to_string(variants.size()) + 
                           " variant(s) to " + outputDir);
            }
        }
    } else {
        std::string variantLc = PathNormalizer::toLowercase(variant);
        
        if (!pkg.hasVariant(variantLc)) {
            printError("Variant not found: " + variant);
            return 1;
        }
        
        result = pkg.extractVariant(variantLc, outputDir);
        if (result.success) {
            printSuccess("Extracted variant '" + variantLc + "' to " + outputDir);
        }
    }
    
    if (!result.success) {
        printError(result.error);
        return 1;
    }
    
    return 0;
}

} // namespace lgx
