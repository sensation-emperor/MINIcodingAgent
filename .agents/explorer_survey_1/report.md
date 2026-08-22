# AIOS Performance & Architecture Survey Report

**Prepared by:** Explorer 1  
**Date:** 2026-08-22  
**Target Project:** MINIcodingAgent (AIOS)  
**Corpus Root:** `c:\Users\kaush\Downloads\MINIcodingAgent`  

---

## 1. Executive Summary

This survey establishes the complete technical baseline for the **AIOS Performance & Concurrency Optimization** project. The project is an agentic operating system written in **C++23** utilizing modern CMake and vcpkg. The core codebase contains 18 modular subsystems including parallel DAG task execution, dense semantic vector retrieval, multi-provider model routing with circuit breaker semantics, repository AST intelligence, and a glassmorphic GUI framework.

The three primary subsystems targeted for optimization are:
1. **TaskGraphExecutor (`src/taskgraph/`)**: Thread pool DAG execution engine with dynamic ready-queue scheduling.
2. **VectorStore (`src/vector/`)**: Dense vector storage, deterministic n-gram feature hashing, and cosine similarity search.
3. **ModelRouter & HttpClient (`src/providers/`, `src/network/`)**: Multi-provider request dispatcher, SSE stream chunker, and provider health tracker.

---

## 2. Toolchain, Build System & Environment

### 2.1 Compiler and Toolchain Details
On the user's Windows environment:
- **IDE / Toolchain Suite**: Microsoft Visual Studio Community 2026 (Version `18.7.11911.148`, Dev18).
- **C++ Compiler**: MSVC `cl.exe` (Version `14.51.36231` for `x64`).
  - Path: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe`
- **CMake**: CMake bundled with Visual Studio.
  - Path: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- **Build Generator**: Ninja or MSVC MSBuild / Visual Studio 18 2026 generator.
  - Ninja Path: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
- **Package Manager**: vcpkg (Version `2026-04-08`).
  - Path: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg\vcpkg.exe`
  - Toolchain file: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake`
- **Developer Environment Activation**:
  - `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`

### 2.2 Dependencies (`vcpkg.json` / `CMakeLists.txt`)
| Dependency | Purpose | vcpkg Target | Required / Optional |
|---|---|---|---|
| `fmt` | High-performance formatting | `fmt::fmt` | Required |
| `spdlog` | Fast structured logging | `spdlog::spdlog` | Required |
| `nlohmann_json` | JSON serialization/parsing | `nlohmann_json::nlohmann_json` | Required |
| `Boost` (`system`, `thread`) | Threading & systems utilities | `Boost::system`, `Boost::thread` | Required |
| `GTest` (`gtest`) | Unit testing framework | `GTest::gtest_main`, `GTest::gtest` | Required (when `BUILD_TESTS=ON`) |
| `CPR` | C++ Requests HTTP library | `cpr::cpr` | Optional (`HAS_CPR` fallback to native WinSock2) |
| `Qt6` / `Qt5` | Skeuomorphic GUI | `Qt6::Core`, `Widgets` | Optional (`BUILD_GUI=OFF` by default) |

### 2.3 Build Targets
1. **`aios_core` (STATIC Library)**:
   - Includes 21 core subsystem translation units (`src/kernel/`, `src/taskgraph/`, `src/providers/`, `src/vector/`, `src/memory/`, etc.).
   - On Windows, links `ws2_32.lib` for socket operations.
2. **`mini_coding_agent` (Executable)**:
   - Main entry point from `src/main.cpp`.
   - Links `aios_core`.
3. **`aios_tests` (Test Executable)**:
   - Built when `BUILD_TESTS=ON` (default).
   - Links `aios_core`, `GTest::gtest_main`, `GTest::gtest`.
   - Registered with CTest under test name `AIOS_Tests`.

### 2.4 Toolchain & Manifest Baseline Notice
In `vcpkg.json`, line 12 defines `"builtin-baseline": "latest"`. In current vcpkg versions, `"latest"` is not accepted as a valid commit SHA and produces an error during cmake generation. The recommended 40-character baseline SHA for this installation is `"cb2981c4e03d421fa03b9bb5044cd1986180e7e4"`.

---

## 3. Codebase Directory Layout

```
c:\Users\kaush\Downloads\MINIcodingAgent\
├── CMakeLists.txt              # Root build configuration (C++23 standard)
├── vcpkg.json                  # Manifest listing dependencies (fmt, spdlog, boost, gtest, etc.)
├── BUILD.md                    # Build instructions documentation
├── FEATURES_DEVELOPMENT_PLAN.md# Feature status and implementation roadmap
├── README.md                   # Full architectural documentation
├── docs\
│   └── GETTING_STARTED.md      # Getting started guide
├── src\
│   ├── main.cpp                # CLI entry point
│   ├── agents\                 # Orchestrator, SpecializedAgents, AgentToolParser, AgentManager
│   ├── cache\                  # Caching layer
│   ├── config\                 # ConfigManager
│   ├── context\                # ContextEngine & token budgeting
│   ├── database\               # DatabaseEngine (persistent JSON / disk storage)
│   ├── embeddings\             # Embedding interfaces
│   ├── events\                 # EventBus publish/subscribe system
│   ├── execution\              # Tool execution primitives
│   ├── filesystem\             # Filesystem tool implementations
│   ├── git\                    # Git checkpointing and diff operations
│   ├── gui\                    # Skeuomorphic Theme, FloatingIslandNavBar, CommandPillWidget, ChatView, etc.
│   ├── kernel\                 # Kernel initialization, lifecycle management
│   ├── knowledge\              # KnowledgeGraph & BFS graph traversal
│   ├── logging\                # Logger (spdlog wrapper)
│   ├── lsp\                    # Language Server Protocol client interface
│   ├── memory\                 # Unified MemoryManager (short-term, long-term, semantic)
│   ├── metrics\                # Metrics recording stubs
│   ├── models\                 # Model domain definitions
│   ├── network\                # HttpClient (native WinSock socket & SSE streaming chunker)
│   ├── parser\                 # ASTParser (multi-language symbol extractor)
│   ├── planner\                # Planner (CoT, ToT, reflection strategies)
│   ├── providers\              # ModelProvider (LMStudio, Ollama, OpenAI, Anthropic) & ModelRouter
│   ├── reflection\             # Reflection utilities
│   ├── repository\             # RepositoryIndex (incremental indexing, BM25/RRF search)
│   ├── sandbox\                # Process sandbox
│   ├── scheduler\              # Priority task scheduler thread pool
│   ├── settings\               # Application settings
│   ├── taskgraph\              # TaskGraph DAG & concurrent TaskGraphExecutor
│   ├── terminal\               # Terminal execution engine
│   ├── tools\                  # ToolRegistry & permission management
│   ├── vector\                 # VectorStore (dense feature hashing, cosine similarity)
│   ├── verification\           # Self-verification utilities
│   └── workflow\               # High-level workflow state machine
└── tests\
    ├── test_main.cpp           # GoogleTest main runner and Logger init
    ├── test_agents.cpp         # AgentToolParser, SpecializedAgents, Orchestrator, AgentManager tests
    ├── test_gui.cpp            # Theme, border radii, brand gradient tests
    ├── test_memory.cpp         # DatabaseEngine, VectorStore, KnowledgeGraph, MemoryManager tests
    ├── test_providers.cpp      # HttpClient SSE parsing, ModelRouter fallback, circuit breaker tests
    ├── test_repository.cpp     # ASTParser, incremental RepositoryIndex, BM25 hybrid search tests
    └── test_taskgraph.cpp      # DAG cycles, topological sort, TaskGraphExecutor parallel run, Planner tests
```

---

## 4. Test Suites and Coverage Inventory

All unit tests are implemented using **GoogleTest (GTest)** and located in `tests/`:

| Test File | Test Fixtures / Groups | Test Cases Count | Scope Covered |
|---|---|---|---|
| `test_taskgraph.cpp` | `TaskGraphTest`, `TaskGraphExecutorTest`, `PlannerTest` | 7 | DAG cycle detection, topological levels, JSON serialization, parallel diamond DAG execution, downstream failure invalidation, CoT & ToT planning. |
| `test_providers.cpp` | `HttpClientTest`, `ModelRouterTest` | 6 | URL parsing, SSE buffer streaming, fragmented chunk handling, role routing, fallback on failure, circuit breaker offline transitions, metrics ledger. |
| `test_memory.cpp` | `DatabaseEngineTest`, `VectorStoreTest`, `KnowledgeGraphTest`, `MemoryManagerTest` | 6 | Conversation/message persistence, cosine similarity search, custom embeddings, knowledge graph BFS/bug-fixes, LRU memory eviction, semantic memory retrieval. |
| `test_agents.cpp` | `AgentToolParserTest`, `SpecializedAgentsTest`, `OrchestratorTest`, `AgentManagerTest` | 8 | JSON/XML/ReAct tool parsing, planner/reviewer/debugger agent execution, full multi-agent workflow, repair cycles, agent manager. |
| `test_gui.cpp` | `ThemeTest` | 4 | Floating island navigation bar radius (>=24px) & margins (12-16px), pill & circular button radii (>=20px), container card radius (>=14px), Coral Rose to Sunset Orange gradient, global stylesheet generation. |
| `test_repository.cpp` | `ASTParserTest`, `RepositoryIndexTest` | 6 | C++, Python, TypeScript, Rust AST parsing, incremental indexing cache, exact/fuzzy symbol lookup, caller search, BM25 Reciprocal Rank Fusion hybrid search. |

**Total Unit Test Cases**: 37 tests.

---

## 5. Build and Execution Instructions

### 5.1 Using CMake with MSVC on Windows PowerShell
```powershell
# 1. Open PowerShell and navigate to workspace
cd c:\Users\kaush\Downloads\MINIcodingAgent

# 2. Configure build using Visual Studio CMake and vcpkg toolchain
$CMAKE_BIN = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$VCPKG_TOOLCHAIN = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"

& $CMAKE_BIN -B build -S . -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" -DBUILD_TESTS=ON

# 3. Compile all targets
& $CMAKE_BIN --build build --config Release --parallel

# 4. Run tests
& $CMAKE_BIN --build build --target RUN_TESTS
# OR direct invocation
.\build\Release\aios_tests.exe
```

---

## 6. Optimization Target Analysis & Architectural Bottlenecks

### 6.1 Subsystem 1: TaskGraphExecutor (`src/taskgraph/taskgraph.cpp`)
- **Current Architecture**:
  - `TaskGraphExecutor::execute` spawns $N$ worker threads in a `workerLoop`.
  - Workers synchronize on `queue_mutex_` and `cv_` waiting for `ready_queue_`.
  - Coarse locking: `TaskGraph` methods (`getNodeRef`, `getReadyNodes`, `areDependenciesCompleted`) each acquire `TaskGraph::mutex_`.
- **Bottlenecks Identified**:
  1. *Lock Contention*: As concurrency scales (100+ tasks), worker threads repeatedly lock both `queue_mutex_` and `TaskGraph::mutex_` when checking dependencies and advancing node states.
  2. *Excessive Wakeups*: Frequent `cv_.notify_all()` wakes all waiting threads when only one task node is enqueued.
  3. *Dynamic Topological State*: `areDependenciesCompleted` scans prerequisites sequentially under lock. An atomic in-degree counter per node would allow lock-free dependency resolution upon predecessor completion.

### 6.2 Subsystem 2: VectorStore (`src/vector/VectorStore.cpp`)
- **Current Architecture**:
  - Embeddings are 128-dimensional `std::vector<float>` (`DEFAULT_EMBEDDING_DIM = 128`).
  - `computeDeterministicEmbedding` tokenizes strings using `std::stringstream`, computes 64-bit FNV-1a hashes for whole words and 3-gram character substrings, and projects into the dense vector.
  - `searchByVector` iterates through all documents in `documents_` unordered map under `mutex_`, calculating cosine similarity sequentially.
- **Bottlenecks Identified**:
  1. *Memory Allocation*: `std::stringstream`, `std::string` allocations, and subword slicing on every token create severe allocation overhead when indexing 10,000+ documents.
  2. *Sequential Search*: Cosine similarity computes dot product and L2 norms in a naive scalar loop without SIMD/AVX2 vectorization.
  3. *Unnormalized Stored Vectors*: Vectors are re-normalized or dot-products calculate `sqrt(norm_a * norm_b)` repeatedly instead of pre-normalizing documents at insertion time so similarity reduces to a fast dot product $\vec{a} \cdot \vec{b}$.
  4. *Flat Map Scan*: $O(N)$ linear iteration across `documents_` unordered map causes CPU cache misses. Contiguous memory layout (flat matrix / contiguous float array) would maximize cache line utilization and vectorization.

### 6.3 Subsystem 3: ModelRouter & HttpClient (`src/providers/`, `src/network/`)
- **Current Architecture**:
  - `ModelRouter` manages provider selection, fallback chains, circuit breakers, and metrics ledgers.
  - `HttpClient::processSseBuffer` parses incoming HTTP stream chunks by splitting on newlines via `std::istringstream` and searching for `data: ` prefixes.
- **Bottlenecks Identified**:
  1. *Stream Fragmentation & Allocations*: String copies and `std::istringstream` allocations per buffer chunk during SSE streaming degrade throughput under high token rates. Zero-copy string view slicing (`std::string_view`) can eliminate fragmentation.
  2. *Locking Scope*: `ModelRouter::routeWithFallback` holds `mutex_` intermittently during dispatch and ledger updates, but metrics ledger maps undergo repeated lookups. Atomic metrics or fine-grained per-provider locks will reduce routing evaluation latency.

---

## 7. Next Steps & Recommendations

1. **Microbenchmark Suite**: Implement a dedicated microbenchmark suite (or executable benchmark harness) to quantitatively measure baseline p50, p95, p99 latency and throughput for:
   - TaskGraph DAG execution under 100+ tasks.
   - VectorStore 10,000+ document indexing and cosine search.
   - HttpClient SSE buffer parsing and ModelRouter routing latency.
2. **Lock-Free / In-Degree Scheduling**: Refactor `TaskGraphExecutor` to utilize atomic dependency countdowns, reducing lock hold times and contention.
3. **Contiguous Vector SIMD**: Structure vector embeddings as flat contiguous arrays and implement vectorized SIMD (AVX2/NEON/auto-vectorized) dot-product kernels with pre-normalized vectors.
4. **Zero-Copy SSE Processing**: Refactor `HttpClient::processSseBuffer` to use `std::string_view` zero-copy chunk parsing.
