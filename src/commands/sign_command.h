#pragma once

#include "command.h"

namespace lgx {

/**
 * Sign command: lgx sign <pkg.lgx>
 * 
 * TODO: Not implemented in v0.1.
 * Exits with non-zero status.
 */
class SignCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "sign"; }
    std::string description() const override { 
        return "Sign a package (not implemented)"; 
    }
    std::string usage() const override {
        return "lgx sign <pkg.lgx>\n"
               "\n"
               "Signs a package and writes manifest.cose.\n"
               "\n"
               "NOTE: Not implemented in v0.1. Always exits with non-zero status.";
    }
};

} // namespace lgx
