#include "commands/command.h"
#include "commands/create_command.h"
#include "commands/add_command.h"
#include "commands/remove_command.h"
#include "commands/extract_command.h"
#include "commands/verify_command.h"
#include "commands/sign_command.h"
#include "commands/publish_command.h"

#include <iostream>
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace {

void printVersion() {
    std::cout << "lgx version 0.1.0" << std::endl;
}

void printUsage(const std::map<std::string, std::unique_ptr<lgx::Command>>& commands) {
    std::cout << "lgx - LGX Package Manager\n"
              << "\n"
              << "Usage: lgx <command> [options]\n"
              << "\n"
              << "Commands:\n";
    
    for (const auto& [name, cmd] : commands) {
        std::cout << "  " << name;
        // Pad to 12 characters
        for (size_t i = name.length(); i < 12; ++i) {
            std::cout << ' ';
        }
        std::cout << cmd->description() << "\n";
    }
    
    std::cout << "\n"
              << "Options:\n"
              << "  --help, -h     Show help for a command\n"
              << "  --version, -V  Show version information\n"
              << "\n"
              << "Examples:\n"
              << "  lgx create mymodule\n"
              << "  lgx add mymodule.lgx --variant linux-amd64 --files ./libfoo.so\n"
              << "  lgx verify mymodule.lgx\n"
              << "\n"
              << "Run 'lgx <command> --help' for more information on a command.\n";
}

void printCommandHelp(const lgx::Command& cmd) {
    std::cout << cmd.usage() << std::endl;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Register commands
    std::map<std::string, std::unique_ptr<lgx::Command>> commands;
    commands["create"] = std::make_unique<lgx::CreateCommand>();
    commands["add"] = std::make_unique<lgx::AddCommand>();
    commands["remove"] = std::make_unique<lgx::RemoveCommand>();
    commands["extract"] = std::make_unique<lgx::ExtractCommand>();
    commands["verify"] = std::make_unique<lgx::VerifyCommand>();
    commands["sign"] = std::make_unique<lgx::SignCommand>();
    commands["publish"] = std::make_unique<lgx::PublishCommand>();
    
    // Parse arguments
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    // Handle no arguments
    if (args.empty()) {
        printUsage(commands);
        return 0;
    }
    
    // Handle global flags
    std::string firstArg = args[0];
    
    if (firstArg == "--version" || firstArg == "-V") {
        printVersion();
        return 0;
    }
    
    if (firstArg == "--help" || firstArg == "-h") {
        printUsage(commands);
        return 0;
    }
    
    // Check for command
    auto it = commands.find(firstArg);
    if (it == commands.end()) {
        std::cerr << "Error: Unknown command '" << firstArg << "'\n"
                  << "\nRun 'lgx --help' to see available commands.\n";
        return 1;
    }
    
    // Get command arguments (skip command name)
    std::vector<std::string> cmdArgs(args.begin() + 1, args.end());
    
    // Check for help flag on command
    for (const auto& arg : cmdArgs) {
        if (arg == "--help" || arg == "-h") {
            printCommandHelp(*it->second);
            return 0;
        }
    }
    
    // Execute command
    return it->second->execute(cmdArgs);
}
