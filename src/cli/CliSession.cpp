#include "cli/CliSession.h"
#include "cli/TerminalRenderer.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <nlohmann/json.hpp>

namespace aios {

namespace {
std::string generateSessionId() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1000, 9999);
    std::ostringstream oss;
    oss << "sess_" << now << "_" << dis(gen);
    return oss.str();
}
} // namespace

CliSession::CliSession() {
    reset();
}

void CliSession::reset() {
    state_.session_id = generateSessionId();
    state_.current_workspace_branch = "main";
    state_.current_workspace_path = ".";
    state_.current_provider = "lm_studio";
    state_.current_model = "local-small-llm";
    state_.total_tokens_used = 0;
    state_.start_time = std::chrono::system_clock::now();
    state_.variables.clear();
    command_history_.clear();
}

void CliSession::recordCommand(const std::string& cmd, bool success) {
    if (!cmd.empty()) {
        command_history_.emplace_back(cmd, success);
    }
}

std::string CliSession::getPlainPromptString() const {
    std::ostringstream oss;
    oss << "aios [" << state_.current_workspace_branch << "|"
        << state_.current_provider << "] > ";
    return oss.str();
}

std::string CliSession::getPromptString() const {
    if (!TerminalRenderer::instance().isColorEnabled()) {
        return getPlainPromptString();
    }

    std::ostringstream oss;
    // Coral Rose #FF6B9D for " aios "
    oss << "\033[38;2;255;107;157m aios \033[0m[";
    // Sunset Orange #FF9A56 for branch|provider
    oss << "\033[38;2;255;154;86m" << state_.current_workspace_branch << "|"
        << state_.current_provider << "\033[0m] ";
    // Teal Success #2DD4BF for prompt glyph ❯
    oss << "\033[38;2;45;212;191m❯\033[0m ";
    return oss.str();
}

bool CliSession::saveToFile(const std::string& path) const {
    try {
        nlohmann::json j;
        j["session_id"] = state_.session_id;
        j["current_workspace_branch"] = state_.current_workspace_branch;
        j["current_workspace_path"] = state_.current_workspace_path;
        j["current_provider"] = state_.current_provider;
        j["current_model"] = state_.current_model;
        j["total_tokens_used"] = state_.total_tokens_used;
        j["variables"] = state_.variables;

        nlohmann::json hist = nlohmann::json::array();
        for (const auto& [cmd, succ] : command_history_) {
            hist.push_back({{"command", cmd}, {"success", succ}});
        }
        j["history"] = hist;

        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool CliSession::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        nlohmann::json j;
        file >> j;

        if (j.contains("session_id")) state_.session_id = j["session_id"].get<std::string>();
        if (j.contains("current_workspace_branch")) state_.current_workspace_branch = j["current_workspace_branch"].get<std::string>();
        if (j.contains("current_workspace_path")) state_.current_workspace_path = j["current_workspace_path"].get<std::string>();
        if (j.contains("current_provider")) state_.current_provider = j["current_provider"].get<std::string>();
        if (j.contains("current_model")) state_.current_model = j["current_model"].get<std::string>();
        if (j.contains("total_tokens_used")) state_.total_tokens_used = j["total_tokens_used"].get<size_t>();
        if (j.contains("variables") && j["variables"].is_object()) {
            state_.variables = j["variables"].get<std::unordered_map<std::string, std::string>>();
        }
        if (j.contains("history") && j["history"].is_array()) {
            command_history_.clear();
            for (const auto& item : j["history"]) {
                if (item.contains("command") && item.contains("success")) {
                    command_history_.emplace_back(item["command"].get<std::string>(), item["success"].get<bool>());
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace aios
