#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <optional>

namespace aios {

class ModelRouter;
class MemoryManager;
class Orchestrator;
class TaskGraph;

enum class SlashCommand {
    Plan,
    Research,
    Code,
    Test,
    Review,
    Diff,
    Rollback,
    Memory,
    Benchmark,
    Exit,
    Unknown
};

struct ReplConfig {
    bool color_output = true;
    bool show_spinner = true;
    bool require_tool_confirmation = true;
    bool stream_tokens = true;
    std::string prompt_symbol = "❯ ";
    std::string agent_tag_prefix = "[AIOS] ";
};

struct DangerousAction {
    std::string tool_name;
    std::string description;
    std::vector<std::string> affected_paths;
    bool is_destructive = false;
};

/**
 * @brief Rich interactive CLI/REPL for MINIcodingAgent (AIOS)
 *
 * Provides:
 * - Multi-turn conversation loop with live token streaming
 * - Slash command dispatch (/plan, /code, /test, /review, etc.)
 * - Dangerous action confirmation prompts
 * - Color-coded agent role tags & spinner animations
 */
class ReplClient {
public:
    explicit ReplClient(ReplConfig config = {});
    ~ReplClient() = default;

    void setModelRouter(std::shared_ptr<ModelRouter> router);
    void setMemoryManager(std::shared_ptr<MemoryManager> memory);
    void setOrchestrator(std::shared_ptr<Orchestrator> orchestrator);

    // Start the interactive REPL loop (blocks until /exit)
    void run();
    void stop();

    // Danger gate — called by tool executor before applying modifications
    bool confirmDangerousAction(const DangerousAction& action);

    // Output formatting helpers (public for testing)
    static std::string colorize(std::string_view text, int ansi_code);
    static std::string agentTag(std::string_view role);
    static std::string dimText(std::string_view text);
    static void printDivider(int width = 72);

private:
    SlashCommand parseSlashCommand(std::string_view input, std::string& args_out) const;
    void dispatchSlashCommand(SlashCommand cmd, const std::string& args);

    void handlePlan(const std::string& goal);
    void handleResearch(const std::string& query);
    void handleCode(const std::string& instruction);
    void handleTest(const std::string& target);
    void handleReview(const std::string& target);
    void handleDiff(const std::string& args);
    void handleRollback(const std::string& checkpoint_id);
    void handleMemory(const std::string& args);
    void handleBenchmark(const std::string& args);

    void printSpinnerFrame(int frame) const;
    void clearSpinnerLine() const;
    void printStreamToken(std::string_view token) const;
    void printAgentMessage(std::string_view role, std::string_view message) const;
    void printError(std::string_view error) const;
    void printHelp() const;
    std::string readLine(std::string_view prompt) const;

    ReplConfig config_;
    std::shared_ptr<ModelRouter> router_;
    std::shared_ptr<MemoryManager> memory_;
    std::shared_ptr<Orchestrator> orchestrator_;
    std::atomic<bool> running_{false};
    std::vector<std::pair<std::string, std::string>> history_; // {role, content}
    int spinner_frame_ = 0;
};

} // namespace aios
