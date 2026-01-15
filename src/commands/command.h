#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>

namespace lgx {

/**
 * Base class for CLI commands.
 */
class Command {
public:
    virtual ~Command() = default;
    
    /**
     * Execute the command.
     * 
     * @param args Command arguments (excluding the command name)
     * @return Exit code (0 = success)
     */
    virtual int execute(const std::vector<std::string>& args) = 0;
    
    /**
     * Get command name.
     */
    virtual std::string name() const = 0;
    
    /**
     * Get command description.
     */
    virtual std::string description() const = 0;
    
    /**
     * Get usage string.
     */
    virtual std::string usage() const = 0;

protected:
    /**
     * Parse command-line arguments into options map.
     * Supports --key value, --key=value, and -k value formats.
     */
    static std::map<std::string, std::string> parseArgs(
        const std::vector<std::string>& args,
        std::vector<std::string>& positional
    );
    
    /**
     * Check if a flag is present (--flag or -f).
     */
    static bool hasFlag(
        const std::map<std::string, std::string>& opts,
        const std::string& longName,
        const std::string& shortName = ""
    );
    
    /**
     * Get option value.
     */
    static std::string getOption(
        const std::map<std::string, std::string>& opts,
        const std::string& longName,
        const std::string& shortName = "",
        const std::string& defaultValue = ""
    );
    
    /**
     * Prompt for confirmation.
     * @param message Prompt message
     * @param defaultNo If true, default answer is No
     * @return true if user confirms
     */
    static bool confirm(const std::string& message, bool defaultNo = true);
    
    /**
     * Print error message to stderr.
     */
    static void printError(const std::string& message);
    
    /**
     * Print success message to stdout.
     */
    static void printSuccess(const std::string& message);
    
    /**
     * Print info message to stdout.
     */
    static void printInfo(const std::string& message);
};

} // namespace lgx
