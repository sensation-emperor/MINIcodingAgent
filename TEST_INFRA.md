# MINIcodingAgent (AIOS) Full Developer Suite: E2E Test Infrastructure

## 1. Test Philosophy & Architecture
The AIOS E2E Test Framework provides an opaque-box, requirement-driven, and multi-tier verification harness for autonomous multi-agent developer workflows.

```
+========================================================================================================+
|                                        E2E Test Architecture                                           |
+========================================================================================================+
                                                     |
         +---------------------------+---------------+---------------------------+
         |                           |                                           |
         v                           v                                           v
+---------------------+     +---------------------+                     +---------------------+
|      Tier 1 & 2     |     |       Tier 3        |                     |      Tier 4 & 5     |
|   Unit & Boundary   |     |    Cross-Feature    |                     |   E2E Multi-Agent   |
|   Features (1-30)   |     |     Combinations    |                     |  Workflow & Chaos   |
+---------------------+     +---------------------+                     +---------------------+
         |                           |                                           |
         +---------------------------+-------------------------------------------+
                                     |
                                     v
                  +-------------------------------------+
                  |   GoogleTest Runner (aios_tests)    |
                  |  - ctest / aios_tests execution     |
                  |  - High-resolution timing metrics   |
                  |  - Process sandboxing & containment |
                  +-------------------------------------+
```

### Core Principles
1. **Opaque-Box Verification**: Tests interact exclusively with public subsystem interfaces (`Repl`, `SlashCommand`, `WorkspaceManager`, `TestingManager`, `MultiAgentOrchestrator`, `TaskGraphExecutor`).
2. **Progressive Testability**: Self-contained test cases that spin up ephemeral sandboxes in temporary filesystem roots and tear them down cleanly post-execution.
3. **Adversarial & Chaos Resilience**: Verifies system stability against malformed tool calls, syntax breakage, directory traversal attempts, circular DAG dependencies, and model provider outages.

---

## 2. 4-Tier Test Methodology & Feature Inventory

| Tier | Focus | Description | Coverage Target |
|---|---|---|---|
| **Tier 1** | Primary Feature Coverage | Positive path execution for all 30 features across CLI, Workspace, Testing, and Agents. | $\ge 5$ test cases per feature |
| **Tier 2** | Boundary & Corner Cases | Extreme values, nulls, NaNs, empty inputs, large payloads (500KB+), unicode, malformed inputs. | $\ge 5$ test cases per feature |
| **Tier 3** | Cross-Feature Combinations | Pairwise integration between CLI, Workspace worktrees, Test Generator, Diagnostics, ModelRouter, Memory, and TaskGraph. | Cross-module matrix |
| **Tier 4** | Real-World Multi-Agent Workflows | Complete end-to-end user journeys: User prompt $\rightarrow$ Worktree sandbox $\rightarrow$ Code gen $\rightarrow$ Diagnostics $\rightarrow$ Test synthesis $\rightarrow$ Test run $\rightarrow$ Rollback/Commit. | Full lifecycle scenarios |
| **Tier 5** | Adversarial Hardening | Rapid cancellation (`SIGINT`), path breakout exploits, circular dependencies, provider circuit breaker recovery. | Chaos & security stress |

---

## 3. Test Suites Directory Structure

```
tests/
├── test_main.cpp                   # GoogleTest main runner with spdlog logger init
├── test_agents.cpp                 # Unit tests for SpecializedAgents & AgentToolParser
├── test_providers.cpp              # Unit tests for ModelProviders & ModelRouter
├── test_taskgraph.cpp              # Unit tests for TaskGraph & TaskGraphExecutor
├── test_gui.cpp                    # Unit tests for Theme & UI components
├── test_repository.cpp             # Unit tests for ASTParser & RepositoryIndex
├── test_memory.cpp                 # Unit tests for DatabaseEngine, VectorStore, MemoryManager
├── test_workspace.cpp              # Unit tests for GitWorktree, PathContainment, SnapshotManager
├── test_testing_engine.cpp         # Unit tests for DiagnosticsEngine, TestGenerator, TestRunner
├── test_cli.cpp                    # Unit tests for LineReader, CommandRegistry, TerminalRenderer
├── benchmark_aios.cpp              # Microbenchmarks for TaskGraph, VectorStore, SSE streaming
└── e2e/
    ├── test_e2e_harness.h          # E2ETestHarness test fixture & OpaqueBoxValidator
    ├── test_e2e_harness.cpp        # Sandbox setup, mock routing, and validation logic
    ├── test_e2e_cli.cpp            # E2E test suite covering Features 1-8 across Tiers 1-4
    ├── test_e2e_workspace.cpp      # E2E test suite covering Features 9-14 across Tiers 1-4
    ├── test_e2e_testing_engine.cpp # E2E test suite covering Features 15-22 across Tiers 1-4
    └── test_e2e_full_workflow.cpp  # E2E test suite covering Features 23-30 across Tiers 1-5
```

---

## 4. How to Build and Run the Test Suite

### 4.1 Prerequisites
- C++23 compatible compiler (MSVC 2022 v17.8+, GCC 13+, Clang 16+)
- CMake 3.20+
- vcpkg packages: `fmt`, `spdlog`, `nlohmann-json`, `boost-system`, `boost-thread`, `gtest`

### 4.2 Build Command
```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DBUILD_TESTS=ON
cmake --build . --config Release -j
```

### 4.3 Test Execution Command
```powershell
# Run all tests via CTest
ctest --output-on-failure -C Release

# Or run test executable directly with GoogleTest filter
.\build\Release\aios_tests.exe --gtest_color=yes

# Run specific E2E test suites
.\build\Release\aios_tests.exe --gtest_filter=E2E*
```
