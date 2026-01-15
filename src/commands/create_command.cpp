#include "create_command.h"
#include "core/package.h"
#include "core/path_normalizer.h"

#include <filesystem>

namespace lgx {

int CreateCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);
    
    if (positional.empty()) {
        printError("Missing package name");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }
    
    std::string name = positional[0];
    std::string nameLower = PathNormalizer::toLowercase(name);
    
    // Determine output filename
    std::string filename = nameLower + ".lgx";
    
    // Check if file already exists
    if (std::filesystem::exists(filename)) {
        printError("File already exists: " + filename);
        return 1;
    }
    
    // Create the package
    auto result = Package::create(filename, nameLower);
    
    if (!result.success) {
        printError(result.error);
        return 1;
    }
    
    printSuccess("Created package: " + filename);
    return 0;
}

} // namespace lgx
