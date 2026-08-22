# AIOS Full Developer Suite: Interactive Terminal REPL & Slash Command Shell Specification

## Executive Summary
This document provides the authoritative functional and technical specification for the **Interactive Terminal REPL & Slash Command Shell** (`src/cli/`) subsystem in the MINIcodingAgent (AIOS) Full Developer Suite.

The CLI subsystem provides an intelligent, developer-centric terminal operating environment featuring:
1. A robust **Read-Eval-Print Loop (REPL)** with cross-platform ANSI/VT100 support, multiline editing, history persistence, reverse search, and context-aware tab autocompletion.
2. An extensible **Slash Command System** with 18 built-in commands covering task DAG orchestration, model routing, workspace worktrees, automated testing, diagnostics, memory inspection, and session management.
3. Tight bidirectional integration with AIOS core subsystems (`MultiAgentOrchestrator`, `TaskGraphExecutor`, `ModelRouter`, `MemoryManager`, `WorkspaceManager`, `DiagnosticEngine`, `EventBus`).
4. Real-time token streaming with live Markdown formatting and interactive approval gating for high-risk operations.
5. Non-interactive script execution and CI/CD pipe modes with clean machine-readable I/O.

---

## 1. REPL Loop Architecture

### 1.1 Architecture Overview
The REPL loop is driven by the `Repl` and `LineReader` classes, decoupled from the underlying OS terminal via the `TerminalRenderer` abstraction.

```
+-----------------------------------------------------------------------------------+
|                                  Terminal REPL                                    |
|                                                                                   |
|  +--------------------+     +--------------------+     +-----------------------+  |
|  |     LineReader     | --> |    Repl Session    | --> | TerminalRenderer      |  |
|  |  - Raw / VT100 In  |     |  - Env & Context   |     |  - Brand ANSI Styling |  |
|  |  - Autocompletion  |     |  - History Tracker |     |  - Markdown & Boxes   |  |
|  |  - Multiline Buffer|     |  - Signal Handler  |     |  - Spinners & Tables  |  |
|  +--------------------+     +--------------------+     +-----------------------+  |
|            |                          |                                           |
|            v                          v                                           |
|  +--------------------+     +--------------------+                                |
|  |  CommandRegistry   |     | Subsystem Router   |                                |
|  |  - Slash Commands  |     |  - Orchestrator    |                                |
|  |  - Arg Tokenizer   |     |  - ModelRouter     |                                |
|  |  - Help & Schemas  |     |  - TaskGraph / Mem |                                |
|  +--------------------+     +--------------------+                                |
+-----------------------------------------------------------------------------------+
```

### 1.2 Cross-Platform Terminal Initialization & Capabilities
- **Windows**: Initializes `ENABLE_VIRTUAL_TERMINAL_PROCESSING` and `ENABLE_PROCESSED_INPUT` on `stdout` and `stdin` console handles via `SetConsoleMode()`. Configures UTF-8 code page via `SetConsoleCP(CP_UTF8)` and `SetConsoleOutputCP(CP_UTF8)`.
- **POSIX / Linux / macOS**: Configures `termios` for raw input processing during line editing, supporting standard VT100 / Xterm 256-color and 24-bit TrueColor escape codes.
- **TTY Detection & Fallback**: Detects `isatty()` / `_isatty()`. In non-interactive pipelines (pipes, redirected files, dumb terminals), disables ANSI escape sequences and interactive line editing, falling back to clean line-by-line streaming.

### 1.3 Prompt Engine
- Dynamic prompt string reflecting current system state:
  ```text
  aios [workspace:branch|provider:model] > 
  ```
  Example rendered with TrueColor ANSI (Coral Rose `#FF6B9D` and Sunset Orange `#FF9A56`):
  `\033[38;2;255;107;157m aios \033[0m[\033[38;2;255;154;86mmain:lm_studio\033[0m] \033[38;2;45;212;191m❯\033[0m `
- Multiline continuation prompt:
  `   ... \033[38;2;160;165;184m│\033[0m `

### 1.4 Input Handling, Multiline & History
1. **Single-Line Input**: Standard editing with Left/Right cursor navigation, Home (`Ctrl+A`), End (`Ctrl+E`), Delete (`Ctrl+D`), Backspace, Clear line (`Ctrl+U`), and Word skip (`Alt+Left` / `Alt+Right` / `Ctrl+Left` / `Ctrl+Right`).
2. **Multiline Input Modes**:
   - **Explicit Toggle**: `/multiline` command toggles multiline mode; input terminates on a trailing single dot `.` line or `Ctrl+D`.
   - **Backslash Continuation**: Ending a line with trailing backslash `\` continues to next line.
   - **Bracketed Code Blocks**: Pasting code blocks enclosed in triple backticks (`` ``` ``) automatically enters multiline buffer collection.
3. **History Persistence**:
   - File location: `~/.aios_history` (or configurable via `AIOS_HISTORY_FILE` / `.aios/history`).
   - Maximum capacity: Default 10,000 entries with automatic deduplication of consecutive identical commands.
   - Navigation: Up/Down arrow keys navigate historical commands.
   - Reverse Search: `Ctrl+R` triggers incremental fuzzy substring search over historical inputs.
4. **Context-Aware Autocompletion (Tab)**:
   - Command completion: `/m` + `Tab` -> `/model`, `/memory`, `/multiline`.
   - Subcommand completion: `/model ` + `Tab` -> `list`, `switch`, `role`, `health`, `metrics`.
   - File path completion: `/test diag src/` + `Tab` -> scans filesystem paths.
   - Git branch/worktree completion: `/workspace switch ` + `Tab` -> lists available branches and worktrees.
   - Dynamic hints: Displayed as inline dimmed suggestions (`\033[90m`).

### 1.5 Terminal Styling & Skeuomorphic-Glassmorphic ANSI Palette
In strict adherence to project UI/UX brand guidelines:
| Token | Hex | RGB | ANSI 24-bit Escape Code | Usage |
|---|---|---|---|---|
| Coral Rose (Primary Brand) | `#FF6B9D` | (255, 107, 157) | `\033[38;2;255;107;157m` | Headers, brand banners, primary accents |
| Sunset Orange (Secondary Brand) | `#FF9A56` | (255, 154, 86) | `\033[38;2;255;154;86m` | Active branch/model indicators, warnings |
| Teal Success | `#2DD4BF` | (45, 212, 191) | `\033[38;2;45;212;191m` | Success checks, prompt glyphs `❯`, passes |
| Coral Red Error | `#F87171` | (248, 113, 113) | `\033[38;2;248;113;113m` | Failures, circuit-breaker trips, critical errors |
| Amber Warning | `#FBBF24` | (251, 191, 36) | `\033[38;2;251;191;36m` | Retries, degraded status, dangerous approvals |
| Text Primary | `#FFFFFF` | (255, 255, 255) | `\033[38;2;255;255;255m` | Primary output, command text |
| Text Muted | `#A0A5B8` | (160, 165, 184) | `\033[38;2;160;165;184m` | Timestamps, borders, inactive metadata |
| Surface Dark | `#12131A` | (18, 19, 26) | `\033[48;2;18;19;26m` | Background card fills |

Rich UI Components:
- **Rounded Box / Card Containers**: Unicode rounded border characters (`╭`, `╮`, `╰`, `╯`, `─`, `│`).
- **Data Tables**: Formatted columns with alignment, headers, and colored borders.
- **Spinners**: Smooth Braille / Unicode animation spinners (`⠋`, `⠙`, `⠹`, `⠸`, `⠼`, `⠴`, `⠦`, `⠧`, `⠇`, `⠏`) for asynchronous agent thinking and task graph execution.
- **Markdown Highlighting**: Syntax highlighting for inline code, bold, italic, and fenced code blocks (`cpp`, `json`, `python`, `diff`).

---

## 2. Slash Command System & Complete Inventory

### 2.1 Command Interface & Parsing Rules
Commands follow the standard syntax:
```text
/<command> [subcommand] [arguments...] [--flags]
```
- Commands starting with `/` are intercepted and dispatched to the `CommandRegistry`.
- Any raw text not starting with `/` is treated as a natural language development task and passed to `/run <task>`.
- Quoted arguments (both single `'...'` and double `"..."`) preserve whitespace and special characters.

### 2.2 Complete Slash Command Inventory Table

| # | Command | Syntax & Arguments | Description | Integration Subsystem |
|---|---|---|---|---|
| 1 | `/help` | `/help [command_name]` | Displays the categorized inventory of all slash commands, or in-depth parameter docs and examples for a specified command. | `CommandRegistry` |
| 2 | `/run` | `/run <task_description>` | Dispatches a natural language task to `MultiAgentOrchestrator` for automated research, planning, coding, testing, and reviewing. | `MultiAgentOrchestrator` |
| 3 | `/task` | `/task <run\|graph\|list\|cancel\|retry> [args]` | Manages task execution and DAG workflows: `graph <goal>` decomposes into a DAG and executes via `TaskGraphExecutor`; `list` shows tasks; `cancel` stops execution; `retry <node_id>` re-runs node. | `TaskGraphExecutor`, `PlannerAgent` |
| 4 | `/model` | `/model <list\|switch\|role\|health\|metrics> [args]` | Model & Provider management: `list` shows providers; `switch <prov> [model]` sets default; `role <role> <prov> <model>` configures per-agent routing; `health` shows circuit breakers; `metrics` prints latency/tokens. | `ModelRouter` |
| 5 | `/memory` | `/memory <search\|store\|stats\|clear\|kg> [args]` | Unified memory operations: `search <query>` performs semantic vector search; `store <key> <val>` stores kv; `stats` prints memory stats; `clear [category]` evicts; `kg <node>` inspects knowledge graph and bug fixes. | `MemoryManager`, `VectorStore`, `KnowledgeGraph` |
| 6 | `/workspace`| `/workspace <list\|create\|switch\|clean\|diff> [args]` | Sandboxed Git worktree management: `list` shows worktrees; `create <branch>` makes isolated sandbox; `switch <branch>` switches worktree; `clean` prunes stale trees; `diff` shows sandbox changes. | `WorkspaceManager`, `GitManager` |
| 7 | `/test` | `/test <run\|gen\|diag> [args]` | Automated test & diagnostics engine: `run [filter]` executes test suites; `gen <file>` synthesizes test cases; `diag <file>` runs AST analysis and diagnostics. | `TestingTools`, `DiagnosticEngine` |
| 8 | `/status` | `/status` | Renders a high-density dashboard card displaying Kernel state, active workspace, loaded tool count, memory usage, and model provider health. | `Kernel`, `ModelRouter`, `MemoryManager` |
| 9 | `/checkpoint`| `/checkpoint [description]` | Manually snapshots the current workspace state to a Git checkpoint tag for instant rollback capability. | `MultiAgentOrchestrator`, `GitManager` |
| 10 | `/rollback` | `/rollback [checkpoint_id]` | Reverts workspace to a specified checkpoint ID (or latest checkpoint if omitted) and cleans up uncommitted artifacts. | `MultiAgentOrchestrator`, `GitManager` |
| 11 | `/diff` | `/diff [file_path]` | Renders a color-coded unified diff (`+` green, `-` red, header cyan) of all modified files or a specific file. | `GitManager`, `TerminalRenderer` |
| 12 | `/history` | `/history [limit]` | Displays the most recent command history entries with execution timestamps and indices. | `LineReader`, `CliSession` |
| 13 | `/config` | `/config <get\|set\|reload\|list> [key] [val]` | Dynamic runtime configuration manager: inspects or updates parameters (timeouts, concurrency, model temperatures) without restart. | `ConfigManager` |
| 14 | `/tools` | `/tools <list\|info\|stats> [name]` | Tool introspection: `list` displays all registered tools by category; `info <name>` shows JSON schema parameters; `stats` displays call count and latency. | `ToolRegistry` |
| 15 | `/clear` | `/clear` | Clears terminal screen buffer and redraws the brand header and prompt. | `TerminalRenderer` |
| 16 | `/multiline` | `/multiline` | Toggles multiline buffer input mode on or off. | `LineReader` |
| 17 | `/session` | `/session <save\|load\|export\|new> [path]` | Interactive session state management: serializes conversation context, active variables, and history to JSON or restores a prior session. | `DatabaseEngine`, `CliSession` |
| 18 | `/exit` | `/exit` or `/quit` | Gracefully cancels active tasks, persists history and session state, and cleanly terminates the process. | `Kernel`, `Repl` |

---

## 3. Integration with AIOS Core Subsystems

### 3.1 `MultiAgentOrchestrator` Integration
- When `/run <task>` is invoked:
  1. `Repl` creates a `ProgressCallback` passed to `orchestrator.onProgress()`.
  2. As each specialized agent (`PlannerAgent`, `ResearcherAgent`, `CoderAgent`, `TesterAgent`, `ReviewerAgent`, `DebuggerAgent`) activates, the CLI renders an animated progress stepper:
     ```text
     ╭─ [Task: Refactor HttpClient SSE Parser] ──────────────────────────╮
     │  ● [Planner]    Plan generated with 3 execution nodes      [✓]   │
     │  ● [Researcher] Located src/network/HttpClient.cpp:55      [✓]   │
     │  ⠋ [Coder]      Applying zero-copy string_view scan...     [...] │
     │  ○ [Tester]     Pending execution                                │
     │  ○ [Reviewer]   Pending execution                                │
     ╰──────────────────────────────────────────────────────────────────╯
     ```
  3. When `CoderAgent` or `ToolRegistry` encounters an operation marked with `ToolPermission::Dangerous` (e.g., executing arbitrary bash commands or deleting directories), the REPL prompts for human approval:
     `\033[38;2;251;191;36m[DANGEROUS OPERATION]\033[0m Agent wants to execute: 'rm -rf build'. Approve? [y/N]: `
  4. The final `WorkflowResult` (including review score, suggestions, diffs) is rendered in a styled summary card.

### 3.2 `TaskGraphExecutor` Integration
- When `/task graph <goal>` is executed:
  1. The task graph is parsed or synthesized from `PlannerAgent`.
  2. `TaskGraphExecutor::onNodeStateChange()` emits real-time callbacks to `Repl`.
  3. `Repl` renders a live-updating DAG matrix:
     ```text
     Level 0: [node_1: ParseAST (Completed 4ms)] [node_2: FetchConfig (Completed 2ms)]
     Level 1: ⠋ [node_3: GenerateTests (Running...)]
     Level 2: ○ [node_4: RunVerification (Ready)]
     ```
  4. If a node fails, the failure reason, stack trace, and debugger diagnosis are printed with error color coding.

### 3.3 `ModelRouter` Streaming Pipeline Integration
- Direct chat prompts or streaming agent outputs utilize the `TokenCallback` handler:
  ```cpp
  router.route(AgentType::Coder, messages, [](const std::string& token) {
      TerminalRenderer::instance().writeToken(token);
  });
  ```
- `TerminalRenderer::writeToken` performs real-time stream rendering:
  - Detects code fence boundaries (`` ```cpp ``) to dynamically enable syntax highlighting.
  - Flushes stdout immediately without buffering delays.
  - Displays token generation speed (`tok/s`) upon completion.

### 3.4 `MemoryManager` & `DatabaseEngine` Integration
- Interactive conversation turns are automatically recorded to `DatabaseEngine` (`DbConversation`, `DbMessage`).
- Context injection: User queries in the REPL automatically query `MemoryManager::searchSemantic(input, top_k=3)` to retrieve relevant user preferences, prior bug fixes, and architectural decisions (ADRs) to inject into the agent context.

### 3.5 `WorkspaceManager` & `GitManager` Integration
- Active workspace worktree path is displayed in the prompt.
- Changing branches or creating worktrees instantly updates the REPL's current working directory context.

---

## 4. Error Handling, Signal Management & Operating Modes

### 4.1 Signal Handling & Cancellation (Ctrl+C / SIGINT)
- The CLI implements a two-tier signal interceptor:
  - **Tier 1 (Execution Interruption)**: When a task, model stream, or task graph is actively running, pressing `Ctrl+C` (`SIGINT`) does **not** terminate the shell. Instead:
    1. Sets `cancelled_ = true` on `MultiAgentOrchestrator` or `TaskGraphExecutor`.
    2. Cancels active HTTP stream sockets in `HttpClient`.
    3. Prints `\n\033[38;2;251;191;36m[Interrupted by user. Active operation cancelled.]\033[0m\n`.
    4. Resets the prompt immediately for new input.
  - **Tier 2 (Shell Exit)**: When the REPL is idle at the prompt:
    - Pressing `Ctrl+C` prints `(To exit AIOS, press Ctrl+D or type /exit)`.
    - Pressing `Ctrl+D` (EOF) or typing `/exit` / `/quit` triggers clean shutdown.

### 4.2 Non-Interactive Script & Pipe Mode
AIOS CLI supports three distinct operating modes:
1. **Interactive REPL Mode** (`mini_coding_agent`):
   - Full TTY capabilities, ANSI TrueColor, cursor navigation, interactive approvals.
2. **One-Shot Command Mode** (`mini_coding_agent -e "<command>"` / `mini_coding_agent --command "<command>"`):
   - Executes the specified slash command or natural language task.
   - Streams output to stdout.
   - Exits with return code `0` on success, `1` on failure.
3. **Batch Script Mode** (`mini_coding_agent -f <script_path>` / `mini_coding_agent --file <script_path>`):
   - Reads a `.aios` or `.txt` script file containing sequential slash commands.
   - Stops on first error unless `--continue-on-error` is specified.
4. **CI/CD Pipe Mode** (`cat input.txt | mini_coding_agent --pipe`):
   - Disables ANSI escape codes (`--no-color`).
   - Reads prompts from `stdin` until EOF.
   - Output can be formatted as JSON (`--format json`) for automated ingestion into CI/CD pipelines.

---

## 5. Exact Public APIs, Classes & File Layout for `src/cli/`

### 5.1 File Layout
```
src/cli/
├── Repl.h                     # Main REPL controller and loop lifecycle
├── Repl.cpp
├── LineReader.h               # Cross-platform raw input, keybindings, history, autocomplete
├── LineReader.cpp
├── TerminalRenderer.h         # Rich ANSI styling, cards, tables, spinners, token streaming
├── TerminalRenderer.cpp
├── SlashCommand.h             # Base interface, command context, execution results
├── SlashCommand.cpp
├── CommandRegistry.h          # Command registration, tokenization, dispatch, help generator
├── CommandRegistry.cpp
├── commands/
│   ├── BuiltinCommands.h      # Complete implementations of all 18 slash commands
│   └── BuiltinCommands.cpp
├── CliSession.h               # Session state, conversation history persistence
├── CliSession.cpp
├── NonInteractiveRunner.h     # One-shot, script file, and CI/CD pipe execution
└── NonInteractiveRunner.cpp
```

### 5.2 C++ Header Specifications

#### 5.2.1 `src/cli/SlashCommand.h`
```cpp
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
```

#### 5.2.2 `src/cli/CommandRegistry.h`
```cpp
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
```

#### 5.2.3 `src/cli/TerminalRenderer.h`
```cpp
#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <chrono>

namespace aios {

enum class TextStyle {
    Normal,
    Bold,
    Dim,
    Italic,
    Underline
};

enum class ColorRole {
    BrandPrimary,    // Coral Rose #FF6B9D
    BrandSecondary,  // Sunset Orange #FF9A56
    Success,         // Teal #2DD4BF
    Error,           // Coral Red #F87171
    Warning,         // Amber #FBBF24
    TextPrimary,     // White #FFFFFF
    TextMuted,       // Grey #A0A5B8
    BackgroundDark   // Dark #12131A
};

class TerminalRenderer {
public:
    static TerminalRenderer& instance();

    void initialize(bool enable_color = true);
    bool isColorEnabled() const { return color_enabled_; }
    void setColorEnabled(bool enabled) { color_enabled_ = enabled; }

    // Color string formatting
    std::string colorize(const std::string& text, ColorRole role, TextStyle style = TextStyle::Normal) const;
    std::string hexColor(const std::string& text, const std::string& hex_code) const;

    // Component Renderers
    void printBanner() const;
    void printHeader(const std::string& title) const;
    void printCard(const std::string& title, const std::string& body, ColorRole border_color = ColorRole::BrandPrimary) const;
    void printTable(const std::vector<std::string>& headers, 
                    const std::vector<std::vector<std::string>>& rows) const;
    void printDiff(const std::string& diff_content) const;
    void printMarkdown(const std::string& markdown) const;

    // Live Streaming & Progress
    void writeToken(const std::string& token);
    void endTokenStream();
    void renderSpinner(const std::string& message, int frame_index);
    void clearLine();

    // Prompts
    bool promptApproval(const std::string& prompt_text);
    std::string promptInput(const std::string& prompt_text);

private:
    TerminalRenderer() = default;
    bool color_enabled_ = true;
    mutable std::mutex mutex_;
    bool in_stream_ = false;
};

} // namespace aios
```

#### 5.2.4 `src/cli/LineReader.h`
```cpp
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace aios {

class CommandRegistry;

class LineReader {
public:
    explicit LineReader(const std::string& history_file_path = "");
    ~LineReader();

    std::string readLine(const std::string& prompt);
    std::string readMultiline(const std::string& prompt);

    void setCompletionHandler(std::function<std::vector<std::string>(const std::string&)> handler);
    
    // History
    void addHistory(const std::string& line);
    void loadHistory();
    void saveHistory();
    std::vector<std::string> getHistory(size_t limit = 100) const;
    void clearHistory();

    void setMultiline(bool enable) { multiline_mode_ = enable; }
    bool isMultiline() const { return multiline_mode_; }

private:
    std::string history_file_;
    std::vector<std::string> history_;
    bool multiline_mode_ = false;
    std::function<std::vector<std::string>(const std::string&)> completion_handler_;

    void setupTerminalRawMode(bool enable);
};

} // namespace aios
```

#### 5.2.5 `src/cli/CliSession.h`
```cpp
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

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    void reset();

private:
    SessionState state_;
    std::vector<std::pair<std::string, bool>> command_history_;
};

} // namespace aios
```

#### 5.2.6 `src/cli/Repl.h`
```cpp
#pragma once

#include "cli/LineReader.h"
#include "cli/TerminalRenderer.h"
#include "cli/CommandRegistry.h"
#include "cli/CliSession.h"
#include <memory>
#include <atomic>

namespace aios {

class Kernel;

class Repl {
public:
    explicit Repl(Kernel& kernel);
    ~Repl();

    bool initialize();
    int run();
    void stop();
    void handleInterrupt(); // SIGINT handler

    bool isRunning() const { return running_; }

private:
    Kernel& kernel_;
    std::unique_ptr<LineReader> line_reader_;
    std::shared_ptr<CliSession> session_;
    std::atomic<bool> running_{false};
    std::atomic<bool> in_execution_{false};
};

} // namespace aios
```

#### 5.2.7 `src/cli/NonInteractiveRunner.h`
```cpp
#pragma once

#include <string>
#include <vector>

namespace aios {

class Kernel;

enum class OutputFormat {
    Text,
    Json
};

class NonInteractiveRunner {
public:
    explicit NonInteractiveRunner(Kernel& kernel);

    int executeCommand(const std::string& command_line, OutputFormat format = OutputFormat::Text);
    int executeScriptFile(const std::string& script_path, bool continue_on_error = false);
    int runPipeMode(OutputFormat format = OutputFormat::Text);

private:
    Kernel& kernel_;
};

} // namespace aios
```

---

## 6. Features Discovered & Probed Inventory

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|---|---|---|---|---|---|---|
| 1 | REPL Core | VT100 / ANSI Raw Terminal Driver | Configures terminal for raw input & 24-bit TrueColor escape sequences on Windows / POSIX | Console handles, mode flags | Raw keystrokes, ANSI render stream | Falls back to std::getline if not a TTY | `src/terminal/` probe & Windows Console API |
| 2 | REPL Core | Dynamic State-Aware Prompt | Shows `aios [branch:model] > ` in Coral Rose and Sunset Orange | Session state, active worktree, model | Color formatted prompt string | Displays default `aios > ` if uninitialized | UI Brand Guidelines & Theme.h |
| 3 | REPL Core | Bracketed & Backslash Multiline Buffer | Supports multi-line input via `/multiline`, trailing `\`, or pasted `` ``` `` blocks | Stream of keystrokes / lines | Consolidated multiline string | Resets buffer on Ctrl+C | REPL requirements |
| 4 | REPL Core | Deduplicated History & Reverse Search | Persists input history to `~/.aios_history` with `Ctrl+R` fuzzy lookup | Keystrokes, file path | Selected historical command | Gracefully handles missing file | LineReader specification |
| 5 | REPL Core | Context-Sensitive Tab Completion | Autocompletes slash commands, subcommands, file paths, and branch names | Partial input string, cursor pos | List of matching completion candidates | Returns empty list if no match | CommandRegistry inspection |
| 6 | Slash System | `/help` Deep Documentation Generator | Generates organized command tables and detailed per-command help | `[command_name]` | Formatted help card with parameters & examples | `Unknown command: <name>` error card | CommandRegistry architecture |
| 7 | Slash System | `/run` Multi-Agent Autonomous Workflow | Dispatches prompt to `MultiAgentOrchestrator` with live progress stepper | `<task_description>` | Multi-agent execution summary & review score | Displays error trace and repair cycles used | `src/agents/Orchestrator.h` |
| 8 | Slash System | `/task` DAG Decomposer & Executor | Decomposes goals into DAGs and runs via `TaskGraphExecutor` with concurrency | `<run\|graph\|list\|cancel\|retry>` | Live DAG level matrix & node state updates | Highlights failed node and downstream skips | `src/taskgraph/TaskGraph.h` |
| 9 | Slash System | `/model` Provider Switcher & Health Ledger | Configures fallback chains, inspects circuit breaker states, switches models | `<list\|switch\|role\|health\|metrics>` | Health table, token/sec metrics, active routes | `Provider not found` / `All routes offline` | `src/providers/ModelRouter.h` |
| 10 | Slash System | `/memory` Vector & Knowledge Graph CLI | Semantic search, KV store, and bug-fix traversal from terminal | `<search\|store\|stats\|clear\|kg>` | Top-K similarity matches, KG entity graph | `No matches found` / `Key not found` | `src/memory/memory.h` |
| 11 | Slash System | `/workspace` Sandboxed Worktree Manager | Lists, creates, switches, and diffs isolated Git worktrees | `<list\|create\|switch\|clean\|diff>` | Active worktree directory, Git status table | `Worktree creation failed` error message | `src/workspace/` requirements |
| 12 | Slash System | `/test` Automated Test & Diagnostics CLI | Synthesizes test suites and runs AST diagnostics on source files | `<run\|gen\|diag> [file_path]` | Test pass/fail summary, compiler diagnostics | `Tests failed: N/M` with failed asserts | `src/tools/ToolRegistry.h` |
| 13 | Slash System | `/status` Global Subsystem Dashboard | Prints unified health card across Kernel, Memory, Tasks, and ModelRouter | None | High-density status table card | Displays degraded subsystem warnings | `src/kernel/Kernel.h` |
| 14 | Slash System | `/checkpoint` & `/rollback` Safety Vault | Creates instant Git checkpoints and restores prior clean workspace states | `[checkpoint_id\|description]` | Checkpoint SHA / confirmation card | `Invalid checkpoint ID` | `MultiAgentOrchestrator` Git hooks |
| 15 | Slash System | `/diff` Color-Coded Unified Diff Viewer | Renders git diffs with green additions, red deletions, cyan headers | `[file_path]` | ANSI-highlighted unified diff | `No changes detected` | `src/gui/DiffViewer.h` parity |
| 16 | Slash System | `/config` Live Configuration Mutator | Gets, sets, and reloads kernel/orchestrator configuration properties | `<get\|set\|reload\|list> [key] [val]`| Property value or updated confirmation | `Key not found` / `Invalid type conversion`| `src/config/ConfigManager.h` |
| 17 | Slash System | `/tools` Schema & Usage Inspector | Displays tool inventory, required permissions, and execution latency | `<list\|info\|stats> [name]` | Tool JSON schema, parameter table, call stats | `Tool not found` | `src/tools/ToolRegistry.h` |
| 18 | Slash System | `/session` Context Serializer & Restorer | Saves and restores interactive conversation history and variables | `<save\|load\|export\|new> [path]` | JSON session artifact / restored state | `Failed to read/write session file` | `src/database/DatabaseEngine.h` |
| 19 | Execution | Dangerous Tool Approval Interceptor | Prompts user before executing high-risk commands (`rm`, shell execution) | `[y/N]` from stdin | Grants or denies tool execution | Aborts tool execution if user denies | `ToolPermission::Dangerous` |
| 20 | Execution | Non-Interactive Script & Pipe Runner | Executes commands from CLI flags (`-e`), scripts (`-f`), or stdin pipes | Command string, script file, stdin stream | Stdout stream / JSON output, exit code | Returns non-zero exit code on failure | `src/main.cpp` CLI args |

---

## 7. Edge Cases & Handling Strategy

| # | Feature | Input / Edge Case Condition | Observed / Specified Behavior |
|---|---|---|---|
| 1 | Signal Handling | User hits `Ctrl+C` while `MultiAgentOrchestrator` is generating code | Shell intercepts `SIGINT`, triggers `orchestrator.cancel()`, leaves REPL running, and resets prompt without crashing. |
| 2 | Model Streaming | Network disconnects or provider returns SSE HTTP 500 mid-stream | Streaming loop halts, catches exception, triggers `ModelRouter` fallback to backup provider, and notifies user. |
| 3 | Input Buffer | User pastes 5,000-line code snippet with internal newlines | LineReader detects bracketed paste / multiline stream, consumes entire buffer into unified input string without executing partial lines. |
| 4 | Terminal Sizing | Terminal resized to narrow width (e.g. 40 columns) | `TerminalRenderer` word-wraps table columns and cards dynamically without breaking ANSI escape codes. |
| 5 | Autocompletion | User hits `Tab` on empty line or unknown slash command `/xyz` | Empty candidate list returns cleanly without error; terminal emits non-disruptive audio/visual bell. |
| 6 | Quoted Arguments | Input has mismatched quotes `/run "Fix bug in 'main.cpp'` | Command tokenizer reports `SyntaxError: Unterminated quote` and preserves input in line reader for correction. |
| 7 | Non-TTY Pipe | Input redirected via `cat task.txt | mini_coding_agent --pipe` | Automatically disables ANSI colors, bypasses line reader raw mode, reads to EOF, and writes plain text output. |
| 8 | Missing Subsystem | User invokes `/model switch` when no providers are configured | Displays clear actionable warning with instructions on how to configure `config.json` or start LM Studio / Ollama. |
| 9 | Dangerous Tool | User declines approval (`n`) for dangerous shell execution | Tool returns `ToolResult::error("Operation aborted by user")`, and agent reflects on rejection to formulate alternate plan. |
| 10 | Rapid Interrupts | User hits `Ctrl+C` multiple times rapidly during idle prompt | First `Ctrl+C` clears current input line; successive `Ctrl+C` prints exit hint `(Press Ctrl+D or type /exit to quit)`. |

---

## 8. Verification & Test Plan

1. **Unit Tests (`tests/test_cli.cpp`)**:
   - `CommandRegistryTest`: Test registration, alias resolution, parameter tokenization, and flag parsing.
   - `LineReaderTest`: Test history loading, deduplication, max capacity pruning, and multiline parsing.
   - `TerminalRendererTest`: Verify ANSI color codes, hex conversion, box/card borders, and Markdown syntax highlighting.
   - `BuiltinCommandsTest`: Test execution of all 18 slash commands with mock Kernel, Orchestrator, and ModelRouter.
   - `NonInteractiveRunnerTest`: Verify `-e` command execution, script file execution, and JSON output formatting.
2. **Integration Tests**:
   - Verify `/run` end-to-end with `MockModelProvider` and live progress callbacks.
   - Verify `/task graph` end-to-end with `TaskGraphExecutor` dynamic state updates.
   - Verify `/workspace` integration with Git worktree sandbox creation and deletion.
   - Verify `/test gen` and `/test diag` integration with `DiagnosticEngine`.
3. **Interactive Manual Sanity Checklist**:
   - Run `./mini_coding_agent` in Windows Terminal (PowerShell / cmd.exe) and verify TrueColor rendering.
   - Test `Ctrl+C` during active generation to verify clean task cancellation.
   - Test tab completion on `/help`, `/model`, `/workspace`.
