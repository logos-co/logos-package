#pragma once

#include "command.h"

namespace lgx {

/**
 * Add command: lgx add <pkg.lgx> --variant <v> --files <path> [--main <relpath>] [-y/--yes]
 * 
 * Adds files to a variant. If the variant exists, it is completely replaced.
 */
class AddCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "add"; }
    std::string description() const override { 
        return "Add files to a package variant"; 
    }
    std::string usage() const override {
        return "lgx add <pkg.lgx> --variant <v> --files <path> [--main <relpath>] [-y/--yes]\n"
               "\n"
               "Adds files to a variant in the package.\n"
               "If the variant already exists, it is COMPLETELY REPLACED (no merge).\n"
               "\n"
               "Options:\n"
               "  --variant, -v <name>   Variant name (e.g., linux-amd64)\n"
               "  --files, -f <path>     Path to file or directory to add\n"
               "  --main, -m <relpath>   Path to main file relative to variant root\n"
               "                         (required if --files is a directory)\n"
               "  --yes, -y              Skip confirmation prompts\n"
               "\n"
               "Examples:\n"
               "  lgx add mymodule.lgx --variant linux-amd64 --files ./libfoo.so\n"
               "  lgx add mymodule.lgx -v web -f ./dist --main dist/index.js\n"
               "  lgx add mymodule.lgx -v darwin-arm64 -f ./build -m build/lib.dylib -y";
    }
};

} // namespace lgx
