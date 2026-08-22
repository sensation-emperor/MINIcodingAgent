#include "cli/ReplClient.h"
#include "providers/ModelRouter.h"
#include "memory/memory.h"
#include "agents/Orchestrator.h"
#include "logging/Logger.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace aios {

namespace {
    // ANSI color codes
    constexpr int ANSI_RESET   = 0;
    constexpr int ANSI_BOLD    = 1;
    constexpr int ANSI_DIM     = 2;
    constexpr int ANSI_RED     = 31;
    constexpr int ANSI_GREEN   = 32;
    constexpr int ANSI_YELLOW  = 33;
    constexpr int ANSI_BLUE    = 34;
    constexpr int ANSI_MAGENTA = 35;
    constexpr int ANSI_CYAN    = 36;
    constexpr int ANSI_WHITE   = 37;

    const char* SPINNER_FRAMES[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    constexpr int SPINNER_FRAME_COUNT = 10;

    static const std::unordered_map<std::string, SlashCommand> SLASH_MAP = {
        {"/plan",      SlashCommand::Plan},
        {"/research",  SlashCommand::Research},
        {"/code",      SlashCommand::Code},
        {"/test",      SlashCommand::Test},
        {"/review",    SlashCommand::Review},
        {"/diff",      SlashCommand::Diff},
        {"/rollback",  SlashCommand::Rollback},
        {"/memory",    SlashCommand::Memory},
        {"/benchmark", SlashCommand::Benchmark},
        {"/exit",      SlashCommand::Exit},
    };

    bool supports_ansi() {
        return ISATTY(FILENO(stdout)) != 0;
    }
}

ReplClient::ReplClient(ReplConfig config) : config_(config) {}

void ReplClient::setModelRouter(std::shared_ptr<ModelRouter> router) {
    router_ = router;
}

void ReplClient::setMemoryManager(std::shared_ptr<MemoryManager> memory) {
    memory_ = memory;
}

void ReplClient::setOrchestrator(std::shared_ptr<Orchestrator> orchestrator) {
    orchestrator_ = orchestrator;
}

// ─────────────────────────────────────────────────────────────
// Static formatting helpers
// ─────────────────────────────────────────────────────────────

std::string ReplClient::colorize(std::string_view text, int ansi_code) {
    if (!supports_ansi()) return std::string(text);
    return "\033[" + std::to_string(ansi_code) + "m" + std::string(text) + "\033[0m";
}

std::string ReplClient::agentTag(std::string_view role) {
    if (!supports_ansi()) return "[" + std::string(role) + "] ";
    // Different colors per agent role
    int color = ANSI_CYAN;
    std::string r(role);
    if (r == "Planner")    color = ANSI_BLUE;
    else if (r == "Coder") color = ANSI_GREEN;
    else if (r == "Tester") color = ANSI_YELLOW;
    else if (r == "Debugger") color = ANSI_RED;
    else if (r == "Reviewer") color = ANSI_MAGENTA;
    return "\033[" + std::to_string(color) + ";1m[" + r + "]\033[0m ";
}

std::string ReplClient::dimText(std::string_view text) {
    if (!supports_ansi()) return std::string(text);
    return "\033[2m" + std::string(text) + "\033[0m";
}

void ReplClient::printDivider(int width) {
    if (supports_ansi()) std::cout << "\033[2m";
    std::cout << std::string(width, '─');
    if (supports_ansi()) std::cout << "\033[0m";
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────

void ReplClient::printSpinnerFrame(int frame) const {
    if (!config_.show_spinner || !supports_ansi()) return;
    std::cout << "\r" << colorize(SPINNER_FRAMES[frame % SPINNER_FRAME_COUNT], ANSI_CYAN)
              << " " << dimText("Thinking...") << std::flush;
}

void ReplClient::clearSpinnerLine() const {
    if (!config_.show_spinner || !supports_ansi()) return;
    std::cout << "\r\033[K" << std::flush;
}

void ReplClient::printStreamToken(std::string_view token) const {
    std::cout << token << std::flush;
}

void ReplClient::printAgentMessage(std::string_view role, std::string_view message) const {
    std::cout << "\n" << agentTag(role) << "\n";
    printDivider(60);
    std::cout << message << "\n";
    printDivider(60);
    std::cout << "\n";
}

void ReplClient::printError(std::string_view error) const {
    std::cout << colorize("✗ Error: ", ANSI_RED) << error << "\n";
}

void ReplClient::printHelp() const {
    printDivider(72);
    std::cout << colorize("  AIOS Slash Commands\n", ANSI_BOLD);
    printDivider(72);
    const std::vector<std::pair<std::string, std::string>> cmds = {
        {"/plan <goal>",         "Decompose a goal into a DAG task graph"},
        {"/research <query>",    "Research the codebase for context"},
        {"/code <instruction>",  "Write or modify code"},
        {"/test [target]",       "Run or generate tests"},
        {"/review [target]",     "Code review and quality scoring"},
        {"/diff [branch]",       "Show diff against branch (default: HEAD)"},
        {"/rollback [id]",       "Roll back to last checkpoint"},
        {"/memory <query>",      "Query the memory store"},
        {"/benchmark",           "Run performance benchmark suite"},
        {"/exit",                "Exit the REPL"},
    };
    for (const auto& [cmd, desc] : cmds) {
        std::cout << "  " << colorize(cmd, ANSI_CYAN)
                  << dimText("  —  " + desc) << "\n";
    }
    printDivider(72);
    std::cout << "\n";
}

std::string ReplClient::readLine(std::string_view prompt) const {
    std::cout << colorize(std::string(prompt), ANSI_GREEN) << " " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {
        return "/exit";
    }
    return line;
}

SlashCommand ReplClient::parseSlashCommand(std::string_view input, std::string& args_out) const {
    if (input.empty() || input[0] != '/') {
        args_out = std::string(input);
        return SlashCommand::Unknown;
    }

    size_t space_pos = input.find(' ');
    std::string cmd_part = std::string(input.substr(0, space_pos));
    args_out = (space_pos != std::string_view::npos && space_pos + 1 < input.size())
               ? std::string(input.substr(space_pos + 1))
               : "";

    // Lowercase
    std::transform(cmd_part.begin(), cmd_part.end(), cmd_part.begin(), ::tolower);

    auto it = SLASH_MAP.find(cmd_part);
    if (it != SLASH_MAP.end()) return it->second;
    return SlashCommand::Unknown;
}

// ─────────────────────────────────────────────────────────────
// Slash command handlers
// ─────────────────────────────────────────────────────────────

void ReplClient::handlePlan(const std::string& goal) {
    if (goal.empty()) { printError("Usage: /plan <goal description>"); return; }
    std::cout << colorize("◆ Planning: ", ANSI_BLUE) << goal << "\n\n";
    // TODO: Wire into Planner → TaskGraph via orchestrator
    std::cout << dimText("  → Planner agent would decompose this goal into a TaskGraph DAG.\n");
    std::cout << dimText("  (Orchestrator integration pending provider setup.)\n\n");
}

void ReplClient::handleResearch(const std::string& query) {
    if (query.empty()) { printError("Usage: /research <query>"); return; }
    std::cout << colorize("◆ Researching: ", ANSI_CYAN) << query << "\n\n";
    if (memory_) {
        std::cout << dimText("  → Querying MemoryManager...\n");
        auto results = memory_->search(query, 5);
        if (results.empty()) {
            std::cout << dimText("  No memory results found.\n\n");
        } else {
            for (const auto& r : results) {
                std::cout << "  • " << r << "\n";
            }
            std::cout << "\n";
        }
    } else {
        std::cout << dimText("  (MemoryManager not connected.)\n\n");
    }
}

void ReplClient::handleCode(const std::string& instruction) {
    if (instruction.empty()) { printError("Usage: /code <instruction>"); return; }
    std::cout << colorize("◆ Code instruction: ", ANSI_GREEN) << instruction << "\n\n";
    std::cout << dimText("  → CoderAgent would process this instruction via ModelRouter.\n\n");
}

void ReplClient::handleTest(const std::string& target) {
    std::cout << colorize("◆ Test target: ", ANSI_YELLOW);
    std::cout << (target.empty() ? "all" : target) << "\n\n";
    std::cout << dimText("  → TesterAgent would run/generate tests for this target.\n\n");
}

void ReplClient::handleReview(const std::string& target) {
    std::cout << colorize("◆ Reviewing: ", ANSI_MAGENTA);
    std::cout << (target.empty() ? "staged changes" : target) << "\n\n";
    std::cout << dimText("  → ReviewerAgent would perform a quality and security audit.\n\n");
}

void ReplClient::handleDiff(const std::string& args) {
    std::string branch = args.empty() ? "HEAD" : args;
    std::cout << colorize("◆ Diff vs: ", ANSI_CYAN) << branch << "\n\n";
    std::cout << dimText("  → WorkspaceManager would show a diff for the sandbox branch.\n\n");
}

void ReplClient::handleRollback(const std::string& checkpoint_id) {
    std::cout << colorize("◆ Rollback checkpoint: ", ANSI_RED);
    std::cout << (checkpoint_id.empty() ? "latest" : checkpoint_id) << "\n\n";
    if (config_.require_tool_confirmation) {
        DangerousAction action;
        action.tool_name = "git_rollback";
        action.description = "Revert all uncommitted changes to checkpoint " + 
                             (checkpoint_id.empty() ? "latest" : checkpoint_id);
        action.is_destructive = true;
        if (!confirmDangerousAction(action)) {
            std::cout << colorize("  Rollback cancelled.\n", ANSI_YELLOW);
            return;
        }
    }
    std::cout << dimText("  → WorkspaceManager would execute atomic rollback.\n\n");
}

void ReplClient::handleMemory(const std::string& args) {
    if (args.empty()) { printError("Usage: /memory <query>"); return; }
    handleResearch(args);
}

void ReplClient::handleBenchmark(const std::string& /*args*/) {
    std::cout << colorize("◆ Running AIOS benchmarks...\n", ANSI_CYAN);
    std::cout << dimText("  → benchmark_aios test suite would be executed.\n\n");
}

// ─────────────────────────────────────────────────────────────
// Slash command dispatcher
// ─────────────────────────────────────────────────────────────

void ReplClient::dispatchSlashCommand(SlashCommand cmd, const std::string& args) {
    switch (cmd) {
        case SlashCommand::Plan:      handlePlan(args);      break;
        case SlashCommand::Research:  handleResearch(args);  break;
        case SlashCommand::Code:      handleCode(args);      break;
        case SlashCommand::Test:      handleTest(args);      break;
        case SlashCommand::Review:    handleReview(args);    break;
        case SlashCommand::Diff:      handleDiff(args);      break;
        case SlashCommand::Rollback:  handleRollback(args);  break;
        case SlashCommand::Memory:    handleMemory(args);    break;
        case SlashCommand::Benchmark: handleBenchmark(args); break;
        case SlashCommand::Exit:      stop();                break;
        default:
            printError("Unknown slash command. Type /help for available commands.");
            break;
    }
}

// ─────────────────────────────────────────────────────────────
// Danger Gate
// ─────────────────────────────────────────────────────────────

bool ReplClient::confirmDangerousAction(const DangerousAction& action) {
    std::cout << "\n";
    printDivider(60);
    std::cout << colorize("  ⚠ Dangerous Action Confirmation\n", ANSI_YELLOW);
    printDivider(60);
    std::cout << "  Tool:    " << colorize(action.tool_name, ANSI_RED) << "\n";
    std::cout << "  Action:  " << action.description << "\n";
    if (!action.affected_paths.empty()) {
        std::cout << "  Affects:\n";
        for (const auto& p : action.affected_paths) {
            std::cout << "    • " << p << "\n";
        }
    }
    if (action.is_destructive) {
        std::cout << "  " << colorize("⚠ This action is IRREVERSIBLE.", ANSI_RED) << "\n";
    }
    printDivider(60);
    std::cout << "  Proceed? " << colorize("[y/N]", ANSI_YELLOW) << " ";

    std::string resp;
    std::getline(std::cin, resp);
    std::transform(resp.begin(), resp.end(), resp.begin(), ::tolower);
    return (resp == "y" || resp == "yes");
}

// ─────────────────────────────────────────────────────────────
// Main REPL Loop
// ─────────────────────────────────────────────────────────────

void ReplClient::run() {
    running_ = true;

    // Startup banner
    if (supports_ansi()) std::cout << "\033[H\033[2J";
    std::cout << colorize("\n  ╔════════════════════════════════════════╗\n", ANSI_CYAN);
    std::cout << colorize("  ║   MINIcodingAgent — AIOS REPL v1.0    ║\n", ANSI_CYAN);
    std::cout << colorize("  ╚════════════════════════════════════════╝\n\n", ANSI_CYAN);
    std::cout << dimText("  Type /help for slash commands, or ask a question to begin.\n\n");

    // Load context into conversation history
    std::vector<Message> messages;

    while (running_) {
        std::string input = readLine(config_.prompt_symbol);

        // Trim
        size_t s = input.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        input = input.substr(s);
        if (input.empty()) continue;

        // Show help shortcut
        if (input == "/help" || input == "?") {
            printHelp();
            continue;
        }

        // Parse slash command or treat as chat message
        std::string args;
        SlashCommand cmd = parseSlashCommand(input, args);

        if (cmd == SlashCommand::Exit) {
            std::cout << colorize("\n  Goodbye!\n\n", ANSI_CYAN);
            break;
        }

        if (cmd != SlashCommand::Unknown) {
            dispatchSlashCommand(cmd, args);
            continue;
        }

        // Regular chat — route to ModelRouter with streaming
        if (router_) {
            history_.push_back({"user", input});

            // Build message list
            messages.clear();
            for (auto& [role, content] : history_) {
                messages.push_back({role, content});
            }

            std::cout << "\n" << agentTag("AIOS") << "\n";
            printDivider(60);

            // Spinner thread while waiting for first token
            std::atomic<bool> got_first_token{false};
            std::thread spinner_thread([&]() {
                int frame = 0;
                while (!got_first_token.load(std::memory_order_relaxed)) {
                    printSpinnerFrame(frame++);
                    std::this_thread::sleep_for(std::chrono::milliseconds(80));
                }
            });

            std::string full_response;
            auto on_token = [&](const std::string& token) {
                if (!got_first_token.exchange(true, std::memory_order_relaxed)) {
                    clearSpinnerLine();
                }
                printStreamToken(token);
                full_response += token;
            };

            auto resp = router_->route(AgentType::Generic, messages, on_token);
            got_first_token = true;
            if (spinner_thread.joinable()) spinner_thread.join();

            if (!resp.success) {
                clearSpinnerLine();
                printError(resp.error);
            } else {
                history_.push_back({"assistant", full_response});
                std::cout << "\n";
            }

            printDivider(60);
            std::cout << "\n";
        } else {
            printError("ModelRouter not connected. Set a provider to enable chat.");
        }
    }

    running_ = false;
}

void ReplClient::stop() {
    running_ = false;
}

} // namespace aios
