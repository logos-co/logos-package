#include "command.h"

#include <algorithm>
#include <sstream>

namespace lgx {

std::map<std::string, std::string> Command::parseArgs(
    const std::vector<std::string>& args,
    std::vector<std::string>& positional
) {
    std::map<std::string, std::string> opts;
    positional.clear();
    
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg.substr(0, 2) == "--") {
            // Long option
            std::string opt = arg.substr(2);
            size_t eqPos = opt.find('=');
            
            if (eqPos != std::string::npos) {
                // --key=value format
                opts[opt.substr(0, eqPos)] = opt.substr(eqPos + 1);
            } else if (i + 1 < args.size() && args[i + 1][0] != '-') {
                // --key value format
                opts[opt] = args[++i];
            } else {
                // Flag (no value)
                opts[opt] = "true";
            }
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
            // Short option
            std::string opt = arg.substr(1);
            
            if (opt.size() == 1 && i + 1 < args.size() && args[i + 1][0] != '-') {
                // -k value format
                opts[opt] = args[++i];
            } else {
                // Flag(s)
                for (char c : opt) {
                    opts[std::string(1, c)] = "true";
                }
            }
        } else {
            positional.push_back(arg);
        }
    }
    
    return opts;
}

bool Command::hasFlag(
    const std::map<std::string, std::string>& opts,
    const std::string& longName,
    const std::string& shortName
) {
    if (opts.find(longName) != opts.end()) {
        return opts.at(longName) == "true";
    }
    if (!shortName.empty() && opts.find(shortName) != opts.end()) {
        return opts.at(shortName) == "true";
    }
    return false;
}

std::string Command::getOption(
    const std::map<std::string, std::string>& opts,
    const std::string& longName,
    const std::string& shortName,
    const std::string& defaultValue
) {
    auto it = opts.find(longName);
    if (it != opts.end() && it->second != "true") {
        return it->second;
    }
    
    if (!shortName.empty()) {
        it = opts.find(shortName);
        if (it != opts.end() && it->second != "true") {
            return it->second;
        }
    }
    
    return defaultValue;
}

bool Command::confirm(const std::string& message, bool defaultNo) {
    std::string prompt = message + (defaultNo ? " [y/N]: " : " [Y/n]: ");
    std::cout << prompt << std::flush;
    
    std::string response;
    if (!std::getline(std::cin, response)) {
        return !defaultNo;  // EOF, use default
    }
    
    // Trim whitespace
    response.erase(0, response.find_first_not_of(" \t\n\r"));
    response.erase(response.find_last_not_of(" \t\n\r") + 1);
    
    if (response.empty()) {
        return !defaultNo;
    }
    
    char c = std::tolower(response[0]);
    return c == 'y';
}

void Command::printError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
}

void Command::printSuccess(const std::string& message) {
    std::cout << message << std::endl;
}

void Command::printInfo(const std::string& message) {
    std::cout << message << std::endl;
}

} // namespace lgx
