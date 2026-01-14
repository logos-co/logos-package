#pragma once

#include "command.h"

namespace lgx {

/**
 * Publish command: lgx publish <pkg.lgx>
 * 
 * No-op in v0.1, exits with status 0.
 */
class PublishCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "publish"; }
    std::string description() const override { 
        return "Publish a package (no-op)"; 
    }
    std::string usage() const override {
        return "lgx publish <pkg.lgx>\n"
               "\n"
               "Publishes a package to the registry.\n"
               "\n"
               "NOTE: No-op in v0.1. Always exits with status 0.";
    }
};

} // namespace lgx
