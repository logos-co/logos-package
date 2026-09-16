#pragma once

#include "command.h"

namespace lgx {

/**
 * Extract command: lgx extract <pkg.lgx> [--variant <v> | --assets-only] [--output <dir>]
 * 
 * Extracts variant contents from a package to a directory.
 * If no variant is specified, extracts all variants.
 * With --assets-only, extracts only the root assets/ tree.
 */
class ExtractCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "extract"; }
    std::string description() const override { 
        return "Extract variant contents or root assets from a package";
    }
    std::string usage() const override {
        return "lgx extract <pkg.lgx> [--variant <v> | --assets-only] [--output <dir>]\n"
               "\n"
               "Extracts variant contents from a package to a directory.\n"
               "If no variant is specified, all variants are extracted.\n"
               "With --assets-only, only the root assets/ tree is extracted and no\n"
               "variant is unpacked; a package without assets extracts nothing.\n"
               "\n"
               "Options:\n"
               "  --variant, -v <name>   Variant to extract (extracts all if omitted)\n"
               "  --assets-only          Extract only root assets/ (not with --variant)\n"
               "  --output, -o <dir>     Output directory (defaults to current directory)\n"
               "\n"
               "Output Structure:\n"
               "  <output>/<variant-name>/   Contents of each variant, plus root assets/\n"
               "  <output>/assets/           Root assets, with --assets-only\n"
               "\n"
               "Examples:\n"
               "  lgx extract mymodule.lgx\n"
               "  lgx extract mymodule.lgx --variant linux-amd64\n"
               "  lgx extract mymodule.lgx -v web -o ./extracted\n"
               "  lgx extract mymodule.lgx --output /tmp/pkg\n"
               "  lgx extract mymodule.lgx --assets-only -o ./contracts";
    }
};

} // namespace lgx
