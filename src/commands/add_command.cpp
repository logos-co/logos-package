#include "add_command.h"
#include "core/package.h"
#include "core/path_normalizer.h"

#include <filesystem>

namespace lgx {

int AddCommand::execute(const std::vector<std::string>& args) {
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
    
    std::string filesPath = getOption(opts, "files", "f");
    if (filesPath.empty()) {
        printError("Missing --files option");
        return 1;
    }
    
    std::string mainPath = getOption(opts, "main", "m");
    bool autoYes = hasFlag(opts, "yes", "y");
    
    // Check if package exists
    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }
    
    // Check if files path exists
    if (!std::filesystem::exists(filesPath)) {
        printError("Path not found: " + filesPath);
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
    
    // Check if variant exists (replacement warning)
    bool variantExists = pkg.hasVariant(variantLc);
    
    // Determine the effective main path
    bool isDir = std::filesystem::is_directory(filesPath);
    std::string effectiveMain;
    
    if (isDir) {
        if (mainPath.empty()) {
            printError("--main is required when --files is a directory");
            return 1;
        }
        effectiveMain = mainPath;
    } else {
        // Single file: use basename or provided main
        effectiveMain = mainPath.empty() 
            ? std::filesystem::path(filesPath).filename().string()
            : mainPath;
    }
    
    // Check if main would change
    bool mainWouldChange = pkg.wouldMainChange(variantLc, effectiveMain);
    
    // Prompt for confirmation if needed
    if (!autoYes) {
        bool needsConfirm = false;
        std::string confirmMsg;
        
        if (variantExists && mainWouldChange) {
            confirmMsg = "Variant '" + variantLc + "' exists and main would change. Replace?";
            needsConfirm = true;
        } else if (variantExists) {
            confirmMsg = "Variant '" + variantLc + "' exists and will be replaced. Continue?";
            needsConfirm = true;
        } else if (mainWouldChange) {
            confirmMsg = "main[" + variantLc + "] would change. Continue?";
            needsConfirm = true;
        }
        
        if (needsConfirm) {
            if (!confirm(confirmMsg, true)) {
                printInfo("Aborted.");
                return 1;
            }
        }
    }
    
    // Add the variant
    std::optional<std::string> mainOpt = mainPath.empty() ? std::nullopt : std::make_optional(mainPath);
    auto result = pkg.addVariant(variantLc, filesPath, mainOpt);
    
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
    
    if (variantExists) {
        printSuccess("Replaced variant '" + variantLc + "' in " + pkgPath);
    } else {
        printSuccess("Added variant '" + variantLc + "' to " + pkgPath);
    }
    
    return 0;
}

} // namespace lgx
