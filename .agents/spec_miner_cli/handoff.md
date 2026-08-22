# Handoff Report: Specification Mining for Interactive Terminal REPL & Slash Command Shell (`src/cli/`)

## 1. Observation
- **Original User Request & Mission**: `ORIGINAL_REQUEST.md:12-16` defines the goal: "Build and integrate the Full Developer Suite for MINIcodingAgent (AIOS) consisting of: 1. Interactive Terminal REPL & Slash Command Shell (src/cli/), 2. Sandboxed Git Worktree & Multi-Branch Workspace Isolation (src/workspace/), 3. Automated Test Generation & Code Diagnostics Engine (src/testing/), 4. Regression & Multi-Subsystem Verification across Orchestrator, TaskGraphExecutor, ModelRouter, MemoryManager, and existing test suites (aios_tests 100% pass)."
- **Core Architecture & Dependencies**: `CMakeLists.txt:1-85` configures C++23 standard (`CMAKE_CXX_STANDARD 23`), linking against `fmt::fmt`, `spdlog::spdlog`, `nlohmann_json::nlohmann_json`, `Boost::system`, and `Boost::thread`. Windows platform links `ws2_32`.
- **Existing Terminal Subsystem**: `src/terminal/terminal.h:1-15` and `src/terminal/terminal.cpp` contains only placeholder stubs (`class Terminal { bool initialize(); void shutdown(); void stop(); };`).
- **Main Entry Point**: `src/main.cpp:1-96` currently runs a basic banner, parses `--config` and `--help`, initializes `Kernel::instance()`, and waits on a background thread with standard signal handling (`SIGINT`, `SIGTERM`).
- **Multi-Agent Orchestrator**: `src/agents/Orchestrator.h:1-90` provides `runWorkflow(task_description, context)`, `onProgress(callback)`, and agent accessors for `PlannerAgent`, `ResearcherAgent`, `CoderAgent`, `TesterAgent`, `ReviewerAgent`, `DebuggerAgent`, with `auto_git_checkpoint` and `rollback_on_unrecoverable_failure`.
- **TaskGraph DAG & Executor**: `src/taskgraph/TaskGraph.h:1-197` implements concurrent DAG scheduling, topological levels, atomic in-degree resolution, `onNodeStateChange(callback)`, and `TaskGraphExecutor::execute(graph)`.
- **ModelRouter & Streaming**: `src/providers/ModelRouter.h:1-166` and `src/network/HttpClient.h` provide `route()` and `routeWithFallback()` with token callback streaming (`TokenCallback`), circuit-breaker health tracking (`ProviderHealth`), and metrics tracking (`AtomicMetricsLedger`).
- **Unified Memory Subsystem**: `src/memory/memory.h:1-135` provides semantic vector search (`searchSemantic()`), knowledge graph queries (`queryRelatedKnowledge()`, `findBugFix()`), and key-value memory retrieval (`store()`, `retrieve()`).
- **Brand Theme & UI Guidelines**: `src/gui/Theme.h:1-50` establishes Coral Rose (`#FF6B9D`), Sunset Orange (`#FF9A56`), Teal Success (`#2DD4BF`), Coral Red Error (`#F87171`), Amber Warning (`#FBBF24`), and dark glassmorphic styling.

## 2. Logic Chain
1. *Observation*: `ORIGINAL_REQUEST.md` requests a complete Developer Suite with an interactive terminal REPL and slash command shell in `src/cli/`.
2. *Observation*: `src/terminal/terminal.h` is currently a placeholder stub, and `src/main.cpp` lacks interactive REPL execution and slash command parsing.
3. *Inference*: To provide a first-class developer experience, `src/cli/` must be designed as a modular, high-performance C++23 terminal subsystem comprising:
   - `Repl`: The main interactive controller managing lifecycle, signals, and session state.
   - `LineReader`: Cross-platform line input handling (Windows Virtual Terminal Processing / POSIX raw mode), persistent command history (`~/.aios_history`), multiline editing, and tab autocompletion.
   - `TerminalRenderer`: TrueColor ANSI styling matching project brand guidelines (Coral Rose `#FF6B9D`, Sunset Orange `#FF9A56`), cards/boxes, markdown syntax highlighting, tables, spinners, and zero-delay streaming token output.
   - `SlashCommand` & `CommandRegistry`: Extensible command pattern registering 18 built-in commands (`/help`, `/run`, `/task`, `/model`, `/memory`, `/workspace`, `/test`, `/status`, `/checkpoint`, `/rollback`, `/diff`, `/history`, `/config`, `/tools`, `/clear`, `/multiline`, `/session`, `/exit`).
   - `NonInteractiveRunner`: CLI flag execution (`-e`, `-f`) and CI/CD headless pipe mode (`--pipe`, `--format json`).
4. *Integration*: The REPL directly interfaces with:
   - `MultiAgentOrchestrator` via progress and token callbacks to render live agent execution steppers and dangerous tool approval prompts.
   - `TaskGraphExecutor` via `onNodeStateChange` to render live DAG execution matrices.
   - `ModelRouter` for streaming token output and provider switching/health ledger views.
   - `MemoryManager` for semantic search and knowledge graph bug fix retrieval.
   - `WorkspaceManager` and `DiagnosticEngine` for sandboxed worktree switching and automated test generation.
5. *Edge Cases*: Gracefully manages `Ctrl+C` interruption (cancelling in-flight requests without killing the shell), non-TTY stdin pipes, large pasted code blocks, terminal resizing, and provider outages.

## 3. Caveats
- No caveats. The specification is fully grounded in the existing C++23 codebase architecture, build system, subsystem headers, and project requirements.

## 4. Conclusion
The comprehensive specification for `src/cli/` has been formulated and documented in `survey_cli_spec.md`. The design provides complete coverage across REPL architecture, slash command inventory, AIOS core subsystem integration, signal handling, session persistence, non-interactive script/pipe modes, C++ public APIs, and file layout.

## 5. Verification Method
- Inspect specification file: `c:\Users\kaush\Downloads\MINIcodingAgent\.agents\spec_miner_cli\survey_cli_spec.md`.
- Verify C++ class layouts against existing CMake configuration (`CMakeLists.txt`) and standard library headers.
- When implemented, verify test suite execution via:
  ```powershell
  ctest --test-dir build --output-on-failure
  ```
