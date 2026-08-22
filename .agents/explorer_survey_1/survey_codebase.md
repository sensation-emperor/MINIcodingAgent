# Codebase Survey & Subsystem Architecture Report
**Project**: MINIcodingAgent (AIOS) Full Developer Suite  
**Date**: 2026-08-22  
**Author**: Explorer 1 (`.agents/explorer_survey_1`)  
**Workspace Root**: `c:\Users\kaush\Downloads\MINIcodingAgent`

---

## 1. Executive Summary & Codebase Architecture Map

MINIcodingAgent (AIOS) is a high-performance C++23 autonomous coding agent platform and AI Operating System designed for local LLMs (LM Studio, Ollama) and cloud providers (OpenAI, Anthropic, OpenRouter). The architecture is structured around a modular kernel and specialized subsystems for multi-agent orchestration, concurrent DAG task scheduling, SIMD-accelerated semantic retrieval, multi-language AST indexing, and an atomic-circuit-breaker model routing layer.

### System Architecture Overview

```
+---------------------------------------------------------------------------------------------------+
|                                  USER / CLI / GUI INTERFACES                                      |
|  - Interactive Terminal REPL & Slash Commands (src/cli/ [Planned])                                |
|  - Desktop Skeuomorphic-Glassmorphic GUI (src/gui/ [Theme, FloatingIslandNavBar, CommandPills])   |
+---------------------------------------------------------------------------------------------------+
                                                  |
+---------------------------------------------------------------------------------------------------+
|                                     AIOS KERNEL & EVENT BUS                                       |
|  - Kernel Lifecycle Coordinator (`src/kernel/Kernel.h`, `Kernel.cpp`)                             |
|  - Thread-Safe EventBus (`src/events/EventBus.h`, `EventBus.cpp`)                                 |
|  - Configuration Manager (`src/config/ConfigManager.h`, `ConfigManager.cpp`)                      |
|  - Central Logging Engine (`src/logging/Logger.h`, `Logger.cpp` via spdlog)                       |
+---------------------------------------------------------------------------------------------------+
       |                                   |                                     |
+--------------------------+  +--------------------------+  +---------------------------------------+
|  MULTI-AGENT SYSTEM      |  |  TASK GRAPH & PLANNING   |  |  REPOSITORY & CONTEXT                 |
|  (`src/agents/`)         |  |  (`src/taskgraph/`,      |  |  (`src/parser/`, `src/repository/`,   |
|  - Orchestrator Pipeline |  |   `src/planner/`)        |  |   `src/context/`)                     |
|  - PlannerAgent          |  |  - TaskGraph (DAG)       |  |  - Multi-Lang AST Parser              |
|  - ResearcherAgent       |  |  - TaskGraphExecutor     |  |  - Incremental Repository Index       |
|  - CoderAgent            |  |    (Atomic In-Degree,    |  |  - BM25 + Reciprocal Rank Fusion      |
|  - TesterAgent           |  |     Signaled Workers)    |  |  - Context Assembly & Token Eviction  |
|  - ReviewerAgent         |  |  - CoT / ToT / Reflect   |  |                                       |
|  - DebuggerAgent         |  |    Planner Engine        |  |                                       |
|  - AgentToolParser       |  |                          |  |                                       |
+--------------------------+  +--------------------------+  +---------------------------------------+
       |                                   |                                     |
+---------------------------------------------------------------------------------------------------+
|                                  INTELLIGENT MODEL ROUTING LAYER                                  |
|  (`src/providers/ModelRouter.h`, `ModelProvider.h`, `src/network/HttpClient.h`)                   |
|  - Multi-Provider Suite: LMStudio (localhost:1234), Ollama (localhost:11434), OpenAI, Anthropic |
|  - Atomic Half-Open Circuit Breaker & Health State Ledger                                         |
|  - Lock-Free Concurrent Metrics Ledger (Latency, Throughput, Tokens)                              |
|  - Universal HTTP/1.1 SSE Streaming Token Engine                                                  |
+---------------------------------------------------------------------------------------------------+
       |                                   |                                     |
+--------------------------+  +--------------------------+  +---------------------------------------+
|  UNIFIED MEMORY SYSTEM   |  |  TOOL & WORKSPACE SUITE  |  |  TESTING & DIAGNOSTICS                |
|  (`src/memory/`,         |  |  (`src/tools/`,          |  |  (`src/testing/` [Planned],           |
|   `src/vector/`,         |  |   `src/workspace/`       |  |   `tests/`)                           |
|   `src/knowledge/`,      |  |   [Planned])             |  |  - Auto Test Generator                |
|   `src/database/`)       |  |  - ToolRegistry (14 cats)|  |  - Compiler Error Diagnostics Engine  |
|  - LRU/LFU Working Mem   |  |  - Sandboxed Git Worktree|  |  - 65 GoogleTest Cases                |
|  - SIMD AVX2 VectorStore |  |  - Branch Isolation      |  |  - Latency/Ops Microbenchmarks        |
|  - Knowledge Graph       |  |  - Execution Sandbox     |  |                                       |
|  - SQLite/JSON DB Engine |  |                          |  |                                       |
+--------------------------+  +--------------------------+  +---------------------------------------+
```

---

## 2. Directory Structure & Layout of All Subsystems

The workspace is organized cleanly into modular C++ directories under `src/`, with unit and benchmark tests under `tests/`, and build artifacts configured under `build/`.

```
MINIcodingAgent/
├── CMakeLists.txt                 # CMake 3.20+ build definition
├── vcpkg.json                     # vcpkg manifest (fmt, spdlog, nlohmann-json, boost, gtest)
├── BUILD.md                       # Build & environment setup guide
├── PROJECT.md                     # Architecture & feature tracking
├── README.md                      # High-level documentation & architecture specifications
├── TEST_INFRA.md                  # Test criteria, coverage targets, and benchmark harness specs
├── FEATURES_DEVELOPMENT_PLAN.md   # Completed and planned feature roadmap
├── ORIGINAL_REQUEST.md            # Authoritative mission & requirements
├── aios_memory.db                 # Default sqlite/json storage state
├── .agents/                       # Agent metadata, plans, handoffs, and progress
├── docs/                          # Guides and getting-started documentation
├── build/                         # MSVC CMake build directory (Release/aios_core.lib, aios_tests.exe)
├── tests/                         # GTest suite and benchmark harness
│   ├── test_main.cpp              # GTest entry point
│   ├── test_agents.cpp            # Tests: Parser, Specialized Agents, Orchestrator, AgentManager
│   ├── test_providers.cpp         # Tests: HttpClient, SSE streaming, ModelRouter, CircuitBreaker
│   ├── test_taskgraph.cpp         # Tests: TaskGraph DAG, Cycle Detection, TaskGraphExecutor
│   ├── test_memory.cpp            # Tests: DatabaseEngine, VectorStore SIMD, KnowledgeGraph, MemoryManager
│   ├── test_repository.cpp        # Tests: ASTParser, Incremental Indexing, BM25 Hybrid Search
│   ├── test_gui.cpp               # Tests: Theme styling, floating island radius, pill buttons
│   └── benchmark_aios.cpp         # Microbenchmarks: 100-node DAG, 10K vector search, SSE parsing
└── src/
    ├── main.cpp                   # Application entry point CLI executable
    ├── kernel/                    # Kernel lifecycle, state coordination, event loops
    ├── taskgraph/                 # DAG data structures, topological sorting, concurrent executor
    ├── planner/                   # CoT, ToT, and Reflection reasoning planning strategies
    ├── scheduler/                 # Priority-based worker queue and task scheduler
    ├── providers/                 # AI Model providers (LM Studio, Ollama, OpenAI, Anthropic) & Router
    ├── network/                   # Universal HTTP client, SSE line scanner, buffer manager
    ├── memory/                    # Unified memory manager (short/long-term/semantic/graph)
    ├── vector/                    # Dense 64-byte aligned SIMD vector store, FNV-1a hashing
    ├── database/                  # JSON/disk persistent database engine
    ├── knowledge/                 # Software domain knowledge graph (entities, relations, BFS)
    ├── parser/                    # Multi-language structural AST parser (C++, Py, TS, RS, Go, Java)
    ├── repository/                # Incremental repository index, BM25 + Reciprocal Rank Fusion
    ├── context/                   # Context engine, file ranking, token budget & LRU eviction
    ├── tools/                     # ToolRegistry singleton, 14 tool categories & permissions
    ├── agents/                    # Multi-agent orchestrator, specialized agents, resilient parser
    ├── events/                    # Thread-safe publisher-subscriber EventBus
    ├── logging/                   # Structured logging (spdlog wrapper)
    ├── config/                    # Configuration manager and JSON setting loader
    ├── gui/                       # Skeuomorphic-Glassmorphic theme and Qt desktop widgets
    │
    │── [Stubs / Extension Points for Full Developer Suite]
    ├── terminal/                  # Legacy stub -> Target: src/cli/ REPL & Slash Command Shell
    ├── git/                       # Legacy stub -> Target: src/workspace/ Git Worktree Isolation
    ├── sandbox/                   # Legacy stub -> Target: src/workspace/ Sandbox Manager
    ├── verification/              # Legacy stub -> Target: src/testing/ Test Generation & Diagnostics
    ├── filesystem/                # Filesystem utilities
    ├── cache/                     # Generic cache stub
    ├── embeddings/                # Embeddings stub
    ├── execution/                 # Execution runtime stub
    ├── lsp/                       # Language server protocol client stub
    ├── metrics/                   # Metrics collector stub
    ├── models/                    # Model definition stubs
    ├── reflection/                # Reflection engine stub
    ├── settings/                  # User settings stub
    └── workflow/                  # Workflow automation stub
```

---

## 3. Build System, Toolchain & Dependencies

### 3.1 Build Tools & Environment
- **Build Generator**: CMake 3.20+ via Visual Studio 2022/MSVC toolchain (`cl.exe` 14.51.36231, x64).
- **C++ Standard**: C++23 (`CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`).
- **Package Manager**: `vcpkg` manifest mode (`vcpkg.json` pinned to `builtin-baseline`: `cb2981c4e03d421fa03b9bb5044cd1986180e7e4`).
- **CMake Toolchain Location**: Visual Studio embedded CMake at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.

### 3.2 Dependencies (`vcpkg.json`)
1. `fmt`: String formatting library (`fmt::fmt`).
2. `spdlog`: High performance structured logging (`spdlog::spdlog`).
3. `nlohmann-json`: JSON serialization and AST representation (`nlohmann_json::nlohmann_json`).
4. `boost-system`: Boost System library (`Boost::system`).
5. `boost-thread`: Boost Concurrency primitives (`Boost::thread`).
6. `gtest`: GoogleTest testing framework (`GTest::gtest`, `GTest::gtest_main`).
7. `ws2_32`: Windows socket networking library linked on `WIN32`.
8. `Qt6` / `Qt5` (Optional, when `BUILD_GUI=ON`): Core, Gui, Widgets for the desktop GUI.

### 3.3 Target Definitions in `CMakeLists.txt`
- `aios_core` (STATIC Library): Contains all core subsystem objects compiled together.
- `mini_coding_agent` (Executable): Main application binary linking `aios_core`.
- `aios_tests` (Executable, when `BUILD_TESTS=ON`): GTest test runner and microbenchmark suite.

---

## 4. Deep-Dive Subsystem Catalog & Integration Contracts

### 4.1 Multi-Agent System & Orchestrator (`src/agents/`)
- **Key Classes**:
  - `MultiAgentOrchestrator`: Hierarchical workflow coordinator. Executes linear and iterative workflows (`Plan -> Research -> Git Snapshot -> Code -> Test -> (Debug Loop) -> Review`). Handles Git checkpointing and automatic rollbacks on unrecoverable failures.
  - `Agent` (Base class): Subclasses `std::enable_shared_from_this<Agent>`. Manages agent state (`Idle`, `Thinking`, `Acting`, `Waiting`, `Completed`, `Failed`, `Cancelled`), configuration, tools, and message turn loops.
  - `AgentToolParser`: Resilient multi-format tool invocation parser. Extracts tool calls from Markdown JSON blocks (` ```json {...} ``` `), XML tags (`<tool_call name="...">...</tool_call>`), ReAct action syntax (`Action: tool(...)`), and detects task completion (`TASK_COMPLETE`, `<task_completed/>`).
  - Specialized Agents:
    - `PlannerAgent`: Generates structured `AgentPlanResult` containing ordered `AgentPlanStep` items.
    - `ResearcherAgent`: Read-only codebase explorer with `filesystem`, `search`, and `context` tools.
    - `CoderAgent`: Code modifications and surgical patch creation.
    - `TesterAgent`: Automated build and test runner with diagnostic extraction.
    - `ReviewerAgent`: Quality scoring (0–100), security auditing, and edge-case evaluation.
    - `DebuggerAgent`: Automated root-cause diagnostics and surgical patch generation.

### 4.2 Task Graph DAG & Concurrent Executor (`src/taskgraph/`)
- **Key Classes**:
  - `TaskNode`: Represents an atomic DAG task unit (`id`, `title`, `description`, `assigned_agent_type`, `dependencies`, `dependents`, `input_params`, `output_data`, `state`, `retry_count`, `execution_time`, `topological_level`).
  - `TaskGraph`: Thread-safe Directed Acyclic Graph container. Provides cycle detection (DFS with recursion stack), topological sorting, parallel execution level grouping, dynamic ready queue queries (`getReadyNodes()`), JSON serialization, and DOT graph export.
  - `TaskGraphExecutor`: High-concurrency thread pool executor.
    - **$O(1)$ Atomic In-Degree Resolution**: Each node tracks an atomic in-degree counter; completing a node decrements dependent counters in $O(1)$ time.
    - **Lock Contention Elimination**: Decouples read-only graph topology from runtime node states (`ExecutionContext`).
    - **Targeted Signaling**: Uses `cv_.notify_one()` to wake up workers without thundering-herd overhead.
    - **Cascading Invalidation**: When a prerequisite fails without retry, downstream dependents are recursively marked `Skipped`.

### 4.3 Planner Engine (`src/planner/`)
- **Key Classes**:
  - `Planner`: Multi-strategy planning engine integrating:
    1. **Chain-of-Thought (CoT)**: Rapid linear DAG task decomposition.
    2. **Tree-of-Thought (ToT)**: Explores multiple candidate plan branch DAGs, computes heuristic viability scores, and selects the highest scoring branch.
    3. **Reflection & Self-Critique**: Drafts an initial plan, analyzes gaps/prerequisites, and outputs a refined execution graph.

### 4.4 Model Provider & Intelligent Router (`src/providers/`, `src/network/`)
- **Key Classes**:
  - `ModelProvider`: Base class for AI backends. Supports synchronous and SSE token streaming (`postStream`). Concrete implementations: `LMStudioProvider` (local OpenAI-compatible), `OllamaProvider` (local Ollama REST API), `OpenAIProvider`, `AnthropicProvider`, `OpenRouterProvider`.
  - `ModelRouter`: Thread-safe intelligent router with:
    - **Role-Based Routing**: Maps `AgentType` to specific model tiers and configs.
    - **Automatic Fallback Chain**: Sequential fallback upon provider failures (`LM Studio -> Ollama -> OpenRouter -> OpenAI -> Anthropic`).
    - **Atomic Half-Open Circuit Breaker**: State transitions (`Healthy`, `Degraded`, `HalfOpen`, `Offline`) with configurable cooldown recovery probes.
    - **Lock-Free Metrics Ledger**: `AtomicMetricsLedger` tracks total requests, successes, failures, prompt/completion tokens, latency (ms), and throughput (tokens/sec).
  - `HttpClient`: Universal HTTP/1.1 client with zero-allocation SSE buffer parsing (`processSseBuffer`).

### 4.5 Memory Architecture & Semantic Store (`src/memory/`, `src/vector/`, `src/database/`, `src/knowledge/`)
- **Key Classes**:
  - `MemoryManager`: Unified memory coordinator.
    - Short-term / Working Memory: In-memory LRU/LFU cache with byte limits (`setMemoryLimit`).
    - Long-term Memory: Disk persistence via `DatabaseEngine`.
    - Semantic Memory: Vector embeddings and similarity search via `VectorStore`.
    - Domain Graph Memory: Entity and relationship tracking via `KnowledgeGraph`.
  - `VectorStore`: High-performance dense vector index.
    - Flat contiguous 64-byte aligned vector matrix.
    - AVX2/FMA SIMD dot products with scalar fallback.
    - Zero-allocation `std::string_view` word and 3-gram feature projection.
    - Bounded $O(N \log K)$ min-heap top-K search.
    - Multi-reader `std::shared_mutex` concurrency.
  - `DatabaseEngine`: Persistent JSON/disk database for conversations, messages, memories, and preferences.
  - `KnowledgeGraph`: Domain-specific graph for symbols, bug fixes, architecture decisions, and coding preferences.

### 4.6 Repository Intelligence & AST Parser (`src/parser/`, `src/repository/`)
- **Key Classes**:
  - `ASTParser`: Multi-language structural AST parser supporting C++, Python, TypeScript, JavaScript, Rust, Go, Java. Extracts classes, structs, methods, docstrings, includes/imports, callees, and FNV-1a checksums.
  - `RepositoryIndex`: Incremental codebase indexer.
    - Checksum-based incremental updates (skips unchanged files).
    - Exact and fuzzy symbol table queries (`findSymbol`, `findCallers`, `findReferences`).
    - Inverted index BM25 lexical token frequency scoring.
    - Reciprocal Rank Fusion (RRF) combining lexical BM25 and symbol table match scores.

### 4.7 Context Engine (`src/context/`)
- **Key Classes**:
  - `ContextEngine`: Assembles and prioritizes context for LLM prompts. Ranks files by relevance, manages token budgets, evicts stale symbols, and tracks conversation state.

### 4.8 Tool Registry & Tool System (`src/tools/`)
- **Key Classes**:
  - `ToolRegistry`: Singleton managing 14 tool categories (`Filesystem`, `Terminal`, `Git`, `Search`, `LSP`, `Network`, `CodeAnalysis`, `Testing`, `Build`, `Database`, `Memory`, `Context`, `Agent`, `System`).
  - Permission enforcement (`Read`, `Write`, `Execute`, `Network`, `Dangerous`).
  - Execution statistics tracking (total calls, successes, failures, latency).

### 4.9 Desktop GUI & Theme System (`src/gui/`)
- **Key Classes**:
  - `Theme`: Generates Mobile Skeuomorphism-Glassmorphism styles with Coral Rose (`#FF6B9D`) to Sunset Orange (`#FF9A56`) gradient, floating island navigation bars (`corner_radius >= 24dp`, margins `12-16dp`), floating pill and circular buttons (`corner_radius >= 20dp`), and deep rounded container cards (`corner_radius >= 14dp`).
  - Qt Widgets: `FloatingIslandNavBar`, `CommandPillWidget`, `ChatView`, `DiffViewer`, `TaskGraphView`, `TerminalWidget`, `MainWindow`.

---

## 5. Test Infrastructure & Baseline Verification Results

### 5.1 Test Suites Overview
The test runner `build/Release/aios_tests.exe` includes **65 test cases across 18 test suites**:

| Test Suite | Test Cases Count | Focus Area | Baseline Status |
|------------|------------------|------------|-----------------|
| `AgentToolParserTest` | 4 | JSON markdown, XML tags, ReAct syntax, task completion | 3 Passed, **1 Failed** |
| `SpecializedAgentsTest` | 3 | PlannerAgent, ReviewerAgent, DebuggerAgent | **3 Passed** |
| `OrchestratorTest` | 2 | Full workflow execution, automated debug/repair loop | **2 Passed** |
| `AgentManagerTest` | 1 | Agent creation and execution | **1 Passed** |
| `HttpClientTest` | 4 | URL parsing, SSE buffer processing, fragmented chunks | **4 Passed** |
| `ModelProviderTest` | 2 | Fast-path token extraction (OpenAI & Anthropic formats) | **2 Passed** |
| `ModelRouterTest` | 8 | Role routing, fallback chain, circuit breaker, atomic metrics | **8 Passed** |
| `TaskGraphTest` | 7 | DAG building, cycle detection, serialization, complex graphs | **7 Passed** |
| `TaskGraphExecutorTest`| 6 | Parallel diamond DAG, skipped nodes, mesh, retries, cancel | **6 Passed** |
| `PlannerTest` | 3 | CoT plan, ToT optimal branch, end-to-end plan graph | **3 Passed** |
| `ThemeTest` | 4 | Floating island radius/margins, pill buttons, gradients, QSS | **4 Passed** |
| `ASTParserTest` | 3 | C++, Python, TypeScript, Rust AST symbol extraction | **3 Passed** |
| `RepositoryIndexTest` | 3 | Incremental indexing, caller lookup, BM25 hybrid search | 1 Passed, **2 Failed** |
| `DatabaseEngineTest` | 1 | Conversation & message storage/retrieval | **1 Passed** |
| `VectorStoreTest` | 7 | Cosine similarity, custom vectors, precision, hashing, stress | **7 Passed** |
| `KnowledgeGraphTest` | 2 | Graph building, BFS neighbor traversal, bug-fix lookup | **2 Passed** |
| `MemoryManagerTest` | 2 | Working memory LRU eviction, semantic search | 1 Passed, **1 Failed** |
| `BenchmarkAIOS` | 3 | 100-node DAG benchmark, 10K vector search, SSE parser | **3 Passed** |
| **Total** | **65** | | **61 Passed, 4 Failed** |

### 5.2 Root-Cause Analysis of 4 Failing Baseline Tests

1. **`AgentToolParserTest.ParsesXmlTagToolCalls` (Failure)**
   - *Symptom*: Expected `calls[0].parameters["operation"] == "grep"`, received `""`.
   - *Root Cause*: In `src/agents/AgentToolParser.cpp` line 8, `parseJsonBlock` expects a top-level `"tool"`, `"name"`, `"action"`, or `"function"` key. When parsing `<tool_call name="search"> { "operation": "grep", "query": "AgentManager" } </tool_call>`, the XML tag holds the tool name while the body is pure parameter JSON without a `"name"` field. `parseJsonBlock` returned `false`, and the subsequent fallback regex failed to extract parameters from JSON.
   - *Fix Needed*: Allow `parseJsonBlock` or `extractXmlTagBlocks` to accept the tool name from the XML attribute and parse the body as parameter JSON.

2. **`RepositoryIndexTest.IndexesFilesIncrementally` (Failure)**
   - *Symptom*: Expected `stats.total_symbols == 3`, received `1`.
   - *Root Cause*: In `src/parser/ASTParser.cpp` line 136, when parsing C++ line `class UserManager { void createUser() {} void deleteUser() {} };`, `class_regex` matches and the parser immediately calls `continue;`. This skips detecting methods declared on the same line or single-line class declarations.
   - *Fix Needed*: In `ASTParser::parseCpp`, do not skip the rest of the line when a class/struct is matched, and allow scanning for method signatures inside single-line or multi-symbol lines.

3. **`RepositoryIndexTest.FindsSymbolsAndCallers` (Failure)**
   - *Symptom*: Expected caller `login` inside parent `AuthController`, received `validateToken` without parent.
   - *Root Cause*: In `src/parser/ASTParser.cpp` line 164, `sym.callees` is populated using `extractCallees(line)`. `line` is only the function signature line (e.g. `void login() {`). The subsequent lines containing the actual function calls (e.g. `validateToken();`) are outside the signature line and thus never scanned for callees.
   - *Fix Needed*: Scan subsequent lines inside the function scope body (until the closing brace) to extract all invoked callees and associate them with the method symbol.

4. **`MemoryManagerTest.StoresAndEvictsWorkingMemory` (Failure)**
   - *Symptom*: Expected `mm.retrieve("k1").has_value() == false` after LRU eviction, received `true`.
   - *Root Cause*: In `src/memory/memory.cpp` line 233, `MemoryManager::retrieve(key)` checks `impl_->entries` first; if evicted from working memory, it automatically queries the persistent database fallback `database_engine_->getMemory(key)`. Since `store("k1", ...)` saved to both working memory and `database_engine_`, `retrieve("k1")` succeeds by pulling from the database.
   - *Fix Needed*: Provide category-specific retrieval or ensure LRU working memory eviction semantics properly distinguish cache retrieval from permanent persistence, or verify `retrieveWorkingMemory(key)` / adjust DB persistence on ephemeral session keys.

5. **Executable Build Error: `src/main.cpp`**
   - *Symptom*: `mini_coding_agent.exe` compilation fails with `error C2039: 'thread': is not a member of 'std'`.
   - *Root Cause*: `src/main.cpp` was missing `#include <thread>` and `#include <chrono>`.
   - *Fix Needed*: Add standard library headers `#include <thread>` and `#include <chrono>` to `src/main.cpp`.

---

## 6. Target Full Developer Suite Requirements & Architectural Gap Analysis

From `ORIGINAL_REQUEST.md`, the authoritative mission is to build and integrate four core modules:

```
+---------------------------------------------------------------------------------------------------+
|                            TARGET FULL DEVELOPER SUITE (MODULES 1-4)                              |
+---------------------------------------------------------------------------------------------------+
|  1. Interactive Terminal REPL & Slash Command Shell (`src/cli/`)                                  |
|     - Rich interactive CLI with line editing, history, and colored ANSI/ASCII banners.            |
|     - Slash commands: /help, /plan, /run, /model, /tools, /context, /memory, /git, /test, /exit.  |
|     - Live token streaming display and progress bars.                                             |
|                                                                                                   |
|  2. Sandboxed Git Worktree & Multi-Branch Workspace Isolation (`src/workspace/`)                  |
|     - Git worktree lifecycle management (create, isolate, switch, clean).                         |
|     - Multi-branch safe execution and atomic rollback checkpoints.                                |
|     - Sandboxed directory isolation preventing accidental destructive mutations.                  |
|                                                                                                   |
|  3. Automated Test Generation & Code Diagnostics Engine (`src/testing/`)                          |
|     - Unit test synthesis for C++, Python, JavaScript/TypeScript from AST definitions.            |
|     - Automated test runner invoking CMake/GTest, pytest, npm test with timeout safeguards.        |
|     - Compiler & Runtime error diagnostics extractor and root-cause classifier.                   |
|                                                                                                   |
|  4. Regression & Multi-Subsystem Verification (100% Pass)                                         |
|     - Resolve all 4 baseline failing tests (`aios_tests`).                                        |
|     - Add comprehensive test suites for CLI, Workspace, and Testing engines.                      |
|     - Validate zero deadlocks, race conditions, and high-concurrency DAG performance.              |
+---------------------------------------------------------------------------------------------------+
```

### Gap Analysis by Subsystem:

1. **`src/cli/` (Interactive Terminal REPL & Slash Commands)**:
   - *Current State*: Empty stub `src/terminal/terminal.h` (15 lines).
   - *Target State*: Full `src/cli/` module with `ReplShell`, `CommandRegistry`, `SlashCommandHandler`, formatted syntax printing, multi-agent session interaction, and live SSE token streaming hooks.

2. **`src/workspace/` (Sandboxed Git Worktree & Branch Isolation)**:
   - *Current State*: Empty stubs `src/git/git.h` and `src/sandbox/sandbox.h`.
   - *Target State*: Full `src/workspace/` module with `WorkspaceManager`, `GitWorktreeManager`, `SandboxIsolation`, directory virtualization, branch snapshotting, and transaction rollback mechanics.

3. **`src/testing/` (Automated Test Generation & Diagnostics Engine)**:
   - *Current State*: Empty stub `src/verification/verification.h`.
   - *Target State*: Full `src/testing/` module with `TestGenerator`, `DiagnosticsEngine`, `CompilerErrorParser`, `TestRunner`, and automatic repair feedback loop integration with `DebuggerAgent` and `MultiAgentOrchestrator`.

4. **Subsystem Regression & Verification**:
   - *Current State*: 61 of 65 tests pass; 4 failing tests in `AgentToolParser`, `ASTParser`, `RepositoryIndex`, and `MemoryManager`.
   - *Target State*: 100% pass across all 65 existing tests plus new test suites for `src/cli/`, `src/workspace/`, and `src/testing/`.

---

## 7. Integration Contracts & Interface Specifications

### 7.1 MultiAgentOrchestrator ↔ WorkspaceManager & TestEngine
```cpp
// Workflow with Workspace Isolation & Test Generation
class MultiAgentOrchestrator {
public:
    void setWorkspaceManager(std::shared_ptr<WorkspaceManager> workspace);
    void setTestEngine(std::shared_ptr<TestEngine> test_engine);
    // Automatic pipeline: Worktree create -> Plan -> Code -> Generate Tests -> Run Tests -> Review -> Merge/Rollback
    WorkflowResult runIsolatedWorkflow(const std::string& task_description, const std::string& branch_name);
};
```

### 7.2 ReplShell ↔ Kernel & Orchestrator
```cpp
class ReplShell {
public:
    explicit ReplShell(Kernel& kernel, std::shared_ptr<MultiAgentOrchestrator> orchestrator);
    void startInteractiveLoop();
    bool executeSlashCommand(const std::string& command_line);
    void registerCommand(const std::string& slash_cmd, std::function<void(const std::vector<std::string>&)> handler);
};
```

### 7.3 TestEngine ↔ ASTParser & Diagnostics
```cpp
class TestEngine {
public:
    GeneratedTestSuite generateUnitTests(const std::string& file_path, const CodeSymbol& symbol);
    TestExecutionResult runTests(const std::string& test_target_or_file);
    DiagnosticReport analyzeFailure(const std::string& compiler_or_test_output);
};
```

---

## 8. Conclusion

The MINIcodingAgent repository possesses a robust, highly modern C++23 foundation with excellent thread-safety models, atomic concurrency primitives, and comprehensive unit tests. With targeted fixes to the 4 baseline test issues and the implementation of the three developer suite modules (`src/cli/`, `src/workspace/`, and `src/testing/`), the platform is primed for full end-to-end integration and 100% test verification.
