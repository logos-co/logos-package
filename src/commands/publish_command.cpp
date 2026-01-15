#include "publish_command.h"

namespace lgx {

int PublishCommand::execute(const std::vector<std::string>& args) {
    (void)args;  // Unused
    
    printInfo("Publish: no-op in v0.1");
    return 0;
}

} // namespace lgx
