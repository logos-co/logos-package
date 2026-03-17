#pragma once

#include "command.h"

namespace lgx {

/**
 * Merge command: lgx merge <pkg1.lgx> <pkg2.lgx> ... [-o <output.lgx>] [-y/--yes]
 *
 * Merges variants from multiple .lgx packages into one multi-variant package.
 * Validates that manifests are identical (ignoring variant-specific fields like main),
 * and by default fails on duplicate variants (or skips them when --skip-duplicates is used).
 */
class MergeCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "merge"; }
    std::string description() const override {
        return "Merge multiple .lgx packages into one";
    }
    std::string usage() const override {
        return "lgx merge <pkg1.lgx> <pkg2.lgx> ... [-o <output.lgx>] [-y/--yes]\n"
               "\n"
               "Merges multiple .lgx packages into a single multi-variant package.\n"
               "All input packages must have identical manifests (except for the\n"
               "variant-specific 'main' field). Fails if any variant appears in\n"
               "more than one input package.\n"
               "\n"
               "Options:\n"
               "  --output, -o <path>    Output .lgx path (default: <name>.lgx)\n"
               "  --skip-duplicates      Warn and skip duplicate variants instead of failing\n"
               "  --yes, -y              Skip confirmation prompts\n"
               "\n"
               "Examples:\n"
               "  lgx merge linux.lgx darwin.lgx\n"
               "  lgx merge linux.lgx darwin.lgx -o mymodule-all.lgx\n"
               "  lgx merge pkg-linux-amd64.lgx pkg-darwin-arm64.lgx -y";
    }
};

} // namespace lgx
