#include "semver_command.h"
#include "logos/semver.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace lgx {

std::string SemverCommand::usage() const {
    return "lgx semver <subcommand> [args]\n"
           "\n"
           "Subcommands:\n"
           "  compare <a> <b>             Print -1, 0 or 1 by SemVer 2.0.0 precedence\n"
           "  sort [--desc] <v>...        Print the versions in order, one per line\n"
           "  satisfies <version> <range> Exit 0 if the version satisfies the npm-style range\n"
           "  valid <version>             Exit 0 if the version is valid SemVer 2.0.0\n"
           "  valid-range <range>         Exit 0 if the range is well-formed\n"
           "\n"
           "Pre-releases order per spec §11 (1.0.0-rc.2 < 1.0.0-rc.11 < 1.0.0) and build\n"
           "metadata is ignored for precedence. A range never matches a pre-release unless\n"
           "the range itself names one at the same major.minor.patch.\n"
           "\n"
           "Examples:\n"
           "  lgx semver compare 1.0.0-rc.2 1.0.0-rc.11   # -> -1\n"
           "  lgx semver sort --desc 1.0.0 2.0.0-alpha 1.9.0\n"
           "  lgx semver satisfies 2.0.0-alpha '^1.0.0'   # -> exit 1\n";
}

int SemverCommand::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("semver: missing subcommand");
        std::cerr << usage();
        return 2;
    }

    const std::string sub = args[0];

    if (sub == "compare") {
        if (args.size() != 3) {
            printError("semver compare: expected exactly 2 versions");
            return 2;
        }
        std::cout << logos::semver::compare(args[1], args[2]) << "\n";
        return 0;
    }

    if (sub == "sort") {
        bool descending = false;
        std::vector<std::string> versions;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--desc") descending = true;
            else versions.push_back(args[i]);
        }
        if (versions.empty()) {
            printError("semver sort: no versions given");
            return 2;
        }
        logos::semver::sort(versions, descending);
        for (const std::string& v : versions) std::cout << v << "\n";
        return 0;
    }

    if (sub == "satisfies") {
        if (args.size() != 3) {
            printError("semver satisfies: expected <version> <range>");
            return 2;
        }
        return logos::semver::satisfies(args[1], args[2]) ? 0 : 1;
    }

    if (sub == "valid") {
        if (args.size() != 2) {
            printError("semver valid: expected exactly 1 version");
            return 2;
        }
        return logos::semver::valid(args[1]) ? 0 : 1;
    }

    if (sub == "valid-range") {
        if (args.size() != 2) {
            printError("semver valid-range: expected exactly 1 range");
            return 2;
        }
        return logos::semver::valid_range(args[1]) ? 0 : 1;
    }

    printError("semver: unknown subcommand '" + sub + "'");
    std::cerr << usage();
    return 2;
}

} // namespace lgx
