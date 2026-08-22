# Project: MINIcodingAgent (AIOS) Full Developer Suite

## Architecture
The MINIcodingAgent (AIOS) Full Developer Suite extends the high-performance C++23 autonomous agent operating system with four core pillars:
1. **Interactive Terminal REPL & Slash Command Shell (`src/cli/`)**: Cross-platform ANSI/VT100 interactive shell, TrueColor styling, dynamic prompt engine, history persistence, 18 slash commands, streaming token rendering, and non-interactive/pipe runners.
2. **Sandboxed Git Worktree & Multi-Branch Workspace Isolation (`src/workspace/`)**: Native Git worktree lifecycle management, ephemeral branch sandboxes, canonical path containment security, shadow snapshotting, and rollback capabilities.
3. **Automated Test Generation & Code Diagnostics Engine (`src/testing/`)**: Polyglot AST-driven test synthesis (C++, Python, JS/TS, Rust, Go), edge case/mock generation, static diagnostics & compiler/linter integration, sandboxed test execution, coverage calculation, and closed-loop failure diagnosis.
4. **Core Subsystem Integration & Verification (`src/main.cpp`, `src/agents/`, `src/taskgraph/`, `src/memory/`, `src/providers/`, `tests/`)**: Deep bidirectional integration with Orchestrator, TaskGraphExecutor, ModelRouter, MemoryManager, ToolRegistry, and 100% pass rate on `aios_tests`.

```
                                 +-------------------------------------+
                                 |         src/main.cpp (Entry)        |
                                 +-------------------------------------+
                                                    |
                                                    v
                              +-------------------------------------------+
                              |         Interactive Terminal REPL         |
                              |                 src/cli/                  |
                              +-------------------------------------------+
                                   |              |              |
           +-----------------------+              |              +-----------------------+
           |                                      v                                      |
           v                        +---------------------------+                        v
+---------------------+             |   MultiAgentOrchestrator  |             +---------------------+
| Sandboxed Workspace |             |     src/agents/           |             | Automated Testing   |
| src/workspace/      | <---------> |  - TaskGraphExecutor      | <---------> | & Diagnostics       |
| - GitWorktree       |             |  - ModelRouter            |             | src/testing/        |
| - BranchSandbox     |             |  - MemoryManager          |             | - TestGenerator     |
| - PathContainment   |             |  - ToolRegistry           |             | - DiagnosticsEngine |
| - SnapshotManager   |             +---------------------------+             | - TestRunner        |
+---------------------+                                                       | - CoverageAnalyzer  |
                                                                              +---------------------+
```

---

## Feature Inventory
Every feature identified during the Survey phase is enumerated below with its assigned milestone.

| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | VT100 / ANSI Terminal Init | Cross-platform raw mode, Windows VT console mode, TTY detection & fallback | M1 | survey_cli_spec |
| 2 | LineReader & Dynamic Prompt | Single-line cursor navigation, multiline buffer handling, dynamic brand prompt | M1 | survey_cli_spec |
| 3 | History & Autocompletion | Persistent `~/.aios_history`, deduplication, reverse search `Ctrl+R`, tab completion | M1 | survey_cli_spec |
| 4 | TerminalRenderer & Brand Theme | Coral Rose (`#FF6B9D`), Sunset Orange (`#FF9A56`), cards, tables, spinners, markdown | M1 | survey_cli_spec |
| 5 | Slash Command Engine | Extensible CommandRegistry and argument parser for 18 slash commands | M1 | survey_cli_spec |
| 6 | Built-in Slash Commands | `/help`, `/run`, `/task`, `/model`, `/memory`, `/workspace`, `/test`, `/status`, `/checkpoint`, `/rollback`, `/diff`, `/history`, `/config`, `/tools`, `/clear`, `/multiline`, `/session`, `/exit` | M1 | survey_cli_spec |
| 7 | Non-Interactive & Pipe Runner | Command-line execution flags (`-e`, `-f`), CI/CD headless JSON streaming pipe mode | M1 | survey_cli_spec |
| 8 | Signal Handling & Interruption | Two-tier `SIGINT` / `Ctrl+C` handling (cancelling in-flight task without killing shell) | M1 | survey_cli_spec |
| 9 | Git Worktree Lifecycle | Creation, listing, locking, pruning, and removal under `.aios/worktrees/wt_<task_id>` | M2 | survey_workspace_testing_spec |
| 10 | Ephemeral Branch Sandbox | Ephemeral branch creation (`aios/ephemeral/<task_id>`), staging, merge, squash, rebase | M2 | survey_workspace_testing_spec |
| 11 | Conflict Detection & Diff | Pre-flight 3-way merge conflict detection and conflict marker extraction | M2 | survey_workspace_testing_spec |
| 12 | Path Containment Security | Canonical path resolution, prefix containment, symlink escape checks, protected file protection | M2 | survey_workspace_testing_spec |
| 13 | Snapshot & Rollback Engine | In-memory shadow snapshotting, Myers unified diff calculation, single-file/full rollback | M2 | survey_workspace_testing_spec |
| 14 | Workspace Tools & ToolRegistry | Unified `WorkspaceManager` and tool handlers registered in `ToolRegistry` | M2 | survey_workspace_testing_spec |
| 15 | Polyglot Test Synthesis | AST-driven test generation for C++ (GTest/Catch2), Python (pytest), JS/TS, Rust, Go | M3 | survey_workspace_testing_spec |
| 16 | Boundary Value & Mock Gen | Boundary edge-case inputs (nulls, NaNs, max/min, large payloads), Google Mock / pytest fixtures | M3 | survey_workspace_testing_spec |
| 17 | Static Code Diagnostics | Multi-tier AST parsing, syntax validation, compiler error parsing (MSVC/GCC/Clang), linter integration | M3 | survey_workspace_testing_spec |
| 18 | Code Smell & Flaw Scanners | Static detectors for resource leaks, concurrency data races, and security risks | M3 | survey_workspace_testing_spec |
| 19 | Sandboxed Test Execution | Isolated test process execution, timeouts, process tree termination, output normalization | M3 | survey_workspace_testing_spec |
| 20 | Code Coverage Engine | Line, function, and branch coverage parsing (LCOV/Cobertura/JSON), uncovered span calculation | M3 | survey_workspace_testing_spec |
| 21 | Closed-Loop Failure Diagnosis | Error-to-AST localization, root-cause diagnosis, surgical patch synthesis for DebuggerAgent | M3 | survey_workspace_testing_spec |
| 22 | Testing Tools & ToolRegistry | Unified `TestingManager` and tool handlers registered in `ToolRegistry` | M3 | survey_workspace_testing_spec |
| 23 | Fix ToolParser XML Attributes | Fix `AgentToolParser::parseJsonBlock` handling for XML tag attributes | M4 | survey_codebase |
| 24 | Fix ASTParser Method Extraction | Fix `ASTParser::parseCpp` to extract multiple methods per class line and body callees | M4 | survey_codebase |
| 25 | Fix MemoryManager Cache Eviction | Fix `MemoryManager::retrieve` to respect working memory eviction without fallback pollution | M4 | survey_codebase |
| 26 | Fix main.cpp Missing Includes | Add missing `<thread>` and `<chrono>` headers in `src/main.cpp` and link CLI REPL | M4 | survey_codebase |
| 27 | Multi-Subsystem Integration | Wire CLI, Workspace, and Testing engines into Orchestrator, TaskGraph, ModelRouter, Memory | M4 | survey_codebase |
| 28 | 100% Pass on aios_tests | Ensure all 65+ unit/integration test cases across all 18+ test suites pass cleanly | M4 | survey_codebase |
| 29 | Opaque-Box E2E Test Suite (T1-4) | Comprehensive requirement-driven test suite spanning all features and edge cases | M5 / E2E Track | ORIGINAL_REQUEST |
| 30 | Adversarial Coverage Hardening (T5) | White-box edge-case and mutation testing to harden coverage | M5 / E2E Track | ORIGINAL_REQUEST |

---

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Interactive Terminal REPL & Slash Shell (`src/cli/`) | Features 1–8: `Repl`, `LineReader`, `TerminalRenderer`, `SlashCommand`, `CommandRegistry`, `NonInteractiveRunner`, CLI entry | none | PLANNED |
| M2 | Sandboxed Git Worktree & Workspace Isolation (`src/workspace/`) | Features 9–14: `GitWorktree`, `BranchSandbox`, `PathContainment`, `SnapshotManager`, `WorkspaceManager`, `WorkspaceTools` | none | PLANNED |
| M3 | Automated Test Generation & Code Diagnostics (`src/testing/`) | Features 15–22: `TestGenerator`, `DiagnosticsEngine`, `TestRunner`, `CoverageAnalyzer`, `TestingManager`, `TestingTools` | none | PLANNED |
| M4 | Subsystem Integration & 100% Test Pass (`aios_tests`) | Features 23–28: Fix baseline failures in Parser/AST/Memory, update `main.cpp`, integrate core subsystems, 100% `aios_tests` pass | M1, M2, M3 | PLANNED |
| M5 | Final Acceptance & Adversarial Hardening (Tiers 1–5) | Features 29–30: Pass 100% E2E test suite (Tiers 1-4) and complete Tier 5 adversarial hardening | M4, E2E Track | PLANNED |

---

## Interface Contracts

### 1. `src/cli/` ↔ `src/agents/Orchestrator.h`
```cpp
namespace aios::cli {
    class Repl {
    public:
        explicit Repl(std::shared_ptr<aios::agents::MultiAgentOrchestrator> orchestrator,
                      std::shared_ptr<aios::providers::ModelRouter> model_router,
                      std::shared_ptr<aios::workspace::WorkspaceManager> workspace_mgr,
                      std::shared_ptr<aios::testing::TestingManager> testing_mgr);
        int run();
        void stop();
        void handleSignal(int sig);
    };
}
```

### 2. `src/workspace/` ↔ `src/agents/Orchestrator.h` & `src/tools/ToolRegistry.h`
```cpp
namespace aios::workspace {
    struct WorktreeInfo {
        std::string task_id;
        std::filesystem::path path;
        std::string branch;
        bool locked{false};
    };

    struct Snapshot {
        std::string snapshot_id;
        std::chrono::system_clock::time_point timestamp;
        std::unordered_map<std::string, std::string> file_contents;
    };

    class WorkspaceManager {
    public:
        virtual ~WorkspaceManager() = default;
        virtual std::expected<WorktreeInfo, std::string> createWorktree(const std::string& task_id, const std::string& base_branch = "HEAD") = 0;
        virtual std::expected<void, std::string> removeWorktree(const std::string& task_id, bool force = false) = 0;
        virtual std::expected<std::string, std::string> createSnapshot(const std::filesystem::path& root_path) = 0;
        virtual std::expected<void, std::string> rollback(const std::string& snapshot_id, const std::filesystem::path& root_path) = 0;
        virtual std::expected<std::string, std::string> computeDiff(const std::string& snapshot_id, const std::filesystem::path& root_path) = 0;
        virtual bool isPathContained(const std::filesystem::path& target, const std::filesystem::path& base) = 0;
    };
}
```

### 3. `src/testing/` ↔ `src/agents/Orchestrator.h` & `src/tools/ToolRegistry.h`
```cpp
namespace aios::testing {
    struct DiagnosticIssue {
        enum class Severity { Info, Warning, Error, Critical };
        Severity severity;
        std::string file;
        int line;
        int column;
        std::string message;
        std::string rule_id;
    };

    struct TestSuiteResult {
        bool success{false};
        int passed{0};
        int failed{0};
        int skipped{0};
        double duration_seconds{0.0};
        std::vector<std::string> failure_details;
    };

    class TestingManager {
    public:
        virtual ~TestingManager() = default;
        virtual std::vector<DiagnosticIssue> runDiagnostics(const std::filesystem::path& path) = 0;
        virtual std::expected<std::string, std::string> generateTests(const std::filesystem::path& source_file, const std::string& language) = 0;
        virtual std::expected<TestSuiteResult, std::string> runTests(const std::string& test_command, std::chrono::milliseconds timeout = std::chrono::seconds(60)) = 0;
        virtual std::expected<double, std::string> analyzeCoverage(const std::filesystem::path& coverage_file) = 0;
    };
}
```

---

## Code Layout
```
c:\Users\kaush\Downloads\MINIcodingAgent\
├── CMakeLists.txt
├── vcpkg.json
├── src/
│   ├── main.cpp
│   ├── cli/
│   │   ├── Repl.h / Repl.cpp
│   │   ├── LineReader.h / LineReader.cpp
│   │   ├── TerminalRenderer.h / TerminalRenderer.cpp
│   │   ├── SlashCommand.h / SlashCommand.cpp
│   │   ├── CommandRegistry.h / CommandRegistry.cpp
│   │   └── NonInteractiveRunner.h / NonInteractiveRunner.cpp
│   ├── workspace/
│   │   ├── GitWorktree.h / GitWorktree.cpp
│   │   ├── BranchSandbox.h / BranchSandbox.cpp
│   │   ├── PathContainment.h / PathContainment.cpp
│   │   ├── SnapshotManager.h / SnapshotManager.cpp
│   │   ├── WorkspaceManager.h / WorkspaceManager.cpp
│   │   └── WorkspaceTools.h / WorkspaceTools.cpp
│   ├── testing/
│   │   ├── TestGenerator.h / TestGenerator.cpp
│   │   ├── DiagnosticsEngine.h / DiagnosticsEngine.cpp
│   │   ├── TestRunner.h / TestRunner.cpp
│   │   ├── CoverageAnalyzer.h / CoverageAnalyzer.cpp
│   │   ├── TestingManager.h / TestingManager.cpp
│   │   └── TestingTools.h / TestingTools.cpp
│   ├── kernel/
│   ├── agents/
│   ├── taskgraph/
│   ├── memory/
│   ├── providers/
│   ├── parser/
│   ├── repository/
│   ├── tools/
│   └── gui/
└── tests/
    ├── test_main.cpp
    ├── test_cli.cpp
    ├── test_workspace.cpp
    ├── test_testing_engine.cpp
    ├── test_agents.cpp
    ├── test_repository.cpp
    ├── test_memory.cpp
    └── e2e/
        ├── test_e2e_harness.cpp
        ├── test_e2e_cli.cpp
        ├── test_e2e_workspace.cpp
        ├── test_e2e_testing_engine.cpp
        └── test_e2e_full_workflow.cpp
```
