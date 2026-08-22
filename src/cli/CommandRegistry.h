#pragma once

#include "cli/SlashCommand.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace aios {

class CommandRegistry {
public:
    static CommandRegistry& instance();

    void registerCommand(std::shared_ptr<SlashCommand> cmd);
    std::shared_ptr<SlashCommand> getCommand(const std::string& name) const;
    std::vector<std::shared_ptr<SlashCommand>> getAllCommands() const;
    std::vector<std::shared_ptr<SlashCommand>> getCommandsByCategory(const std::string& category) const;

    CommandResult dispatch(const std::string& line, CommandContext& base_ctx);
    std::vector<std::string> getCompletions(const std::string& prefix) const;

    void registerBuiltinCommands();

    // Helper tokenizers
    static std::vector<std::string> tokenize(const std::string& line);
    static void parseFlags(const std::vector<std::string>& tokens,
                           std::vector<std::string>& positional_args,
                           std::unordered_map<std::string, std::string>& flags);

private:
    CommandRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<SlashCommand>> commands_;
    std::unordered_map<std::string, std::string> alias_map_;
};

} // namespace aios
