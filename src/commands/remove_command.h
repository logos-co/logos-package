#pragma once

#include "command.h"

namespace lgx {

/**
 * Remove command: lgx remove <pkg.lgx> --variant <v> [-y/--yes]
 * 
 * Removes a variant from the package.
 */
class RemoveCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "remove"; }
    std::string description() const override { 
        return "Remove a variant from a package"; 
    }
    std::string usage() const override {
        return "lgx remove <pkg.lgx> --variant <v> [-y/--yes]\n"
               "\n"
               "Removes a variant and its main entry from the package.\n"
               "\n"
               "Options:\n"
               "  --variant, -v <name>   Variant name to remove\n"
               "  --yes, -y              Skip confirmation prompts\n"
               "\n"
               "Examples:\n"
               "  lgx remove mymodule.lgx --variant linux-amd64\n"
               "  lgx remove mymodule.lgx -v web -y";
    }
};

} // namespace lgx
