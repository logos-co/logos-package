#pragma once

#include "command.h"

namespace lgx {

/**
 * Create command: lgx create <name>
 * 
 * Creates a skeleton package with the given name.
 */
class CreateCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "create"; }
    std::string description() const override { 
        return "Create a new skeleton package"; 
    }
    std::string usage() const override {
        return "lgx create <name>\n"
               "\n"
               "Creates a new .lgx package file with the given name.\n"
               "The name will be automatically lowercased.\n"
               "\n"
               "Examples:\n"
               "  lgx create mymodule       # Creates mymodule.lgx\n"
               "  lgx create MyModule       # Creates mymodule.lgx (lowercase)";
    }
};

} // namespace lgx
