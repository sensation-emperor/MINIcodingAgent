#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace aios {

struct SessionState {
    std::string session_id;
    std::string current_workspace_branch = "main";
    std::string current_workspace_path = ".";
    std::string current_provider = "lm_studio";
    std::string current_model = "local-small-llm";
    size_t total_tokens_used = 0;
    std::chrono::system_clock::time_point start_time;
    std::unordered_map<std::string, std::string> variables;
};

class CliSession {
public:
    CliSession();
    ~CliSession() = default;

    SessionState& getState() { return state_; }
    const SessionState& getState() const { return state_; }

    void recordCommand(const std::string& cmd, bool success);
    std::string getPromptString() const;
    std::string getPlainPromptString() const;

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    void reset();

    const std::vector<std::pair<std::string, bool>>& getCommandHistory() const {
        return command_history_;
    }

private:
    SessionState state_;
    std::vector<std::pair<std::string, bool>> command_history_;
};

} // namespace aios
