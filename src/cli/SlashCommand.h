#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>

namespace aios {

class Kernel;
class CliSession;
class TerminalRenderer;

struct CommandContext {
    std::string raw_line;
    std::string command_name;
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> flags;
    std::shared_ptr<CliSession> session;
    Kernel& kernel;
    TerminalRenderer& renderer;
    bool is_interactive = true;
};

struct CommandResult {
    bool success = true;
    std::string output;
    std::string error_message;
    int exit_code = 0;
    bool should_exit = false;

    static CommandResult ok(const std::string& out = "") {
        return CommandResult{true, out, "", 0, false};
    }
    static CommandResult error(const std::string& err, int code = 1) {
        return CommandResult{false, "", err, code, false};
    }
    static CommandResult exit() {
        CommandResult r;
        r.success = true;
        r.should_exit = true;
        return r;
    }
};

class SlashCommand {
public:
    virtual ~SlashCommand() = default;

    virtual std::string getName() const = 0;
    virtual std::vector<std::string> getAliases() const { return {}; }
    virtual std::string getCategory() const = 0; // "Core", "Workflow", "Model", "Workspace", "Testing", "System"
    virtual std::string getDescription() const = 0;
    virtual std::string getUsage() const = 0;
    virtual std::vector<std::string> getExamples() const { return {}; }

    virtual CommandResult execute(CommandContext& ctx) = 0;
    virtual std::vector<std::string> getCompletions(const std::vector<std::string>& args, 
                                                    size_t arg_index) const {
        return {};
    }
};

} // namespace aios
