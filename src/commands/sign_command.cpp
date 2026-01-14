#include "sign_command.h"

namespace lgx {

int SignCommand::execute(const std::vector<std::string>& args) {
    (void)args;  // Unused
    
    printError("Sign command not implemented in v0.1");
    return 1;
}

} // namespace lgx
