#pragma once

#include "command.h"

namespace lgx {

/**
 * Verify command: lgx verify <pkg.lgx>
 * 
 * Validates a package against the LGX specification.
 */
class VerifyCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "verify"; }
    std::string description() const override { 
        return "Verify a package is valid"; 
    }
    std::string usage() const override {
        return "lgx verify <pkg.lgx>\n"
               "\n"
               "Validates a package against the LGX specification:\n"
               "  - tar.gz readable\n"
               "  - Root layout restrictions enforced\n"
               "  - Manifest required fields present\n"
               "  - NFC normalization enforced for all paths\n"
               "  - Completeness constraint for main vs variants\n"
               "  - Each main entry points to existing file\n"
               "  - No forbidden file types\n"
               "\n"
               "Returns 0 on success, non-zero on validation failure.\n"
               "\n"
               "Examples:\n"
               "  lgx verify mymodule.lgx";
    }
};

} // namespace lgx
