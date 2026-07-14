#pragma once

#include "command.h"

namespace lgx {

/**
 * `lgx semver` — expose the shared semver implementation to non-C++ callers.
 *
 * This exists for the catalog builder (logos-modules-release-tool's index.py),
 * which orders each package's `versions[]` and must agree with the C++ clients
 * exactly. index.py is deliberately stdlib-only and already requires `lgx` on
 * PATH for the two commands that sort (`build` / `add`), so shelling out here
 * keeps a single implementation across both languages instead of adding a
 * second one in Python that could drift.
 */
class SemverCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string name() const override { return "semver"; }
    std::string description() const override { return "Compare, sort and range-match versions"; }
    std::string usage() const override;
};

} // namespace lgx
