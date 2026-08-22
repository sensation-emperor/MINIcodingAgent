# AIOS Feature Development Plan & Progress

## Overview
This document outlines the features planned and implemented for the MINIcodingAgent (AIOS) application.

---

## ✅ COMPLETED FEATURES

### 1. Context Engine (`src/context/ContextEngine.h`, `src/context/ContextEngine.cpp`)
- Intelligent file ranking, dependency chains, and symbol caching.
- Token budget management and LRU eviction.
- Conversation state tracking.

---

### 2. Tool Registry (`src/tools/ToolRegistry.h`, `src/tools/ToolRegistry.cpp`)
- 14 tool categories (Filesystem, Terminal, Git, Search, LSP, Network, Testing, Build, etc.).
- Permission enforcement (Read, Write, Execute, Network, Dangerous).
- Execution statistics and singleton registry.

---

### 3. Enhanced Multi-Agent System & Orchestration (`src/agents/`)
- **Resilient Tool Parsing (`AgentToolParser`)**:
  - Extracts tool calls from Markdown JSON blocks, XML `<tool_call>` tags, ReAct action syntax, and raw JSON envelopes.
  - Formats schemas and tool observations for small local LLMs.
- **Specialized Concrete Agents (`SpecializedAgents`)**:
  - `PlannerAgent`: Decomposes tasks into dependency-ordered `AgentPlanStep` graphs.
  - `ResearcherAgent`: Read-only codebase explorer with `filesystem`, `search`, and `context` tools.
  - `CoderAgent`: Code modifications and surgical patch creation.
  - `TesterAgent`: Automated build and test runner with diagnostic extraction.
  - `ReviewerAgent`: Quality scoring (0–100), security auditing, and edge-case evaluation.
  - `DebuggerAgent`: Automated root-cause diagnostics and surgical patch generation.
- **Hierarchical Orchestrator (`Orchestrator`)**:
  - Pipeline: `Plan -> Research -> Git Snapshot -> Code -> Test -> (Debug/Repair Loop) -> Review`.
  - Dual observability: Real-time `EventBus` publishing (`orchestrator.step_progress`) + direct async callbacks.
  - Automatic Git checkpointing and rollback support.

---

### 4. Model Routing & Live AI Providers (`src/providers/`, `src/network/`)
- **Universal HTTP Transport & SSE Streaming (`HttpClient`)**:
  - Cross-platform HTTP/1.1 client with Server-Sent Events (SSE) token delta streaming.
  - Robust zero-allocation SSE line & buffer fragmentation parser.
- **Multi-Provider Suite (`ModelProvider`)**:
  - `LMStudioProvider`: Local OpenAI-compatible endpoint (`localhost:1234/v1`), model discovery, and SSE streaming.
  - `OllamaProvider`: Local Ollama REST API (`localhost:11434`), tag discovery, and ndjson/SSE streaming.
  - `OpenAIProvider`: Official OpenAI API (`api.openai.com/v1`) with Bearer auth and SSE streaming.
  - `AnthropicProvider`: Claude Messages API (`api.anthropic.com/v1`) with `x-api-key` and content block delta streaming.
  - `OpenRouterProvider`: Multi-model aggregator endpoint with custom app headers.
- **Intelligent Model Router (`ModelRouter`)**:
  - Role-based routing table mapping `AgentType` to model tiers.
  - Automatic fallback chain: `LM Studio -> Ollama -> OpenRouter -> OpenAI -> Anthropic`.
  - Circuit breaker health management (`Healthy`, `Degraded`, `Offline`) with recovery probing.
  - Embedded Metrics Ledger tracking prompt tokens, completion tokens, latency (ms), and throughput (tokens/sec).

---

### 5. Advanced Planning & Task Graph DAG Execution Engine (`src/taskgraph/`, `src/planner/`)
- **Task Graph DAG Engine (`TaskGraph`)**:
  - Directed Acyclic Graph (DAG) task container with cycle detection, topological levels calculation, and JSON serialization.
  - Node states: `Pending`, `Ready`, `Running`, `Completed`, `Failed`, `Skipped`, `RolledBack`.
- **High-Performance Concurrent Executor (`TaskGraphExecutor`)**:
  - $O(1)$ atomic in-degree dependency resolution on task completion.
  - Fine-grained locking eliminating global mutex serialization.
  - Targeted condition variable signaling (`notify_one`) preventing thundering-herd on worker threads.
  - Dynamic dependency resolution, retry handling, and cascading downstream invalidation (`Skipped`).
- **Multi-Strategy Reasoning (`Planner`)**:
  - `Chain-of-Thought (CoT)`: Fast linear DAG task decomposition.
  - `Tree-of-Thought (ToT)`: Generates multiple candidate plan branch DAGs, computes heuristic viability scores, and selects optimal branch.
  - `Reflection & Self-Critique`: Drafts initial plan, analyzes gaps/prerequisites, and outputs refined execution graph.

---

### 6. Desktop GUI with Mobile Skeuomorphism-Glassmorphism System (`src/gui/`)
- **Theme & Design System (`Theme`)**:
  - Coral Rose (`#FF6B9D`) to Sunset Orange (`#FF9A56`) brand gradient.
  - Tactile depth with a single virtual light source from top-left.
  - Floating Island Navigation Bars with capsule shape (`border-radius >= 24px`, margins `12-16px`).
  - Floating Pill & Circular Buttons (`border-radius >= 20px`).
  - Container cards with deep rounded corners (`border-radius >= 14px`).
- **Custom Glassmorphic Widgets**:
  - `FloatingIslandNavBar`: Floating capsule top bar with zero-overlap touch targets.
  - `CommandPillWidget`: Bottom command capsule with Mode Toggle (Orchestrator / Single Agent / DAG Planner), Model Selector, and circular CTA button.
  - `ChatView`: Translucent conversational surface with agent avatar pills, thought disclosure boxes, and live token streaming.
  - `DiffViewer`: Syntax-highlighted code diff inspector inside container cards.
  - `TaskGraphView`: Live interactive DAG progress visualizer.
  - `TerminalWidget`: Translucent terminal console for build and shell outputs.
  - `MainWindow`: Orchestrates viewports and connects to backend asynchronously.

---

### 7. Repository Intelligence & AST Parser (`src/parser/`, `src/repository/`)
- **Multi-Language AST Engine (`ASTParser`)**:
  - Structural symbol extraction for C++, Python, JavaScript/TypeScript, Rust, Go, and Java.
  - Extracts classes, functions, methods, structs, interfaces, imports, line numbers, and preceding docstrings.
  - Callee extraction and call graph relationship tracking.
- **Incremental Repository Index (`RepositoryIndex`)**:
  - Checksum-based incremental indexing (skipping unchanged files, re-parsing only modified files).
  - Exact and fuzzy symbol table lookups (`findSymbol`, `findCallers`, `findReferences`).
  - **Hybrid Search Engine**: Combines BM25 lexical token frequency ranking and symbol table matches using **Reciprocal Rank Fusion (RRF)**.

---

### 8. Persistent Long-Term Memory & SQLite Knowledge Engine (`src/database/`, `src/vector/`, `src/knowledge/`, `src/memory/`)
- **Persistent Storage Engine (`DatabaseEngine`)**:
  - Atomic, persistent storage for conversations, messages, structured memories, and user preferences.
  - JSON import/export and disk synchronization.
- **SIMD Dense Vector Store & Semantic Retrieval (`VectorStore`)**:
  - Flat contiguous 64-byte aligned vector matrix for SIMD/vectorized dot products.
  - Zero-allocation `std::string_view` word and 3-gram feature projection.
  - Bounded $O(N \log K)$ min-heap selection.
  - Multi-reader `std::shared_mutex` concurrency.
- **Domain-Specific Software Knowledge Graph (`KnowledgeGraph`)**:
  - Entity types: `Symbol`, `BugFix`, `ArchitectureDecision`, `UserPreference`, `ProjectTask`.
  - Relationship types: `Calls`, `Implements`, `Fixes`, `DependsOn`, `Prefers`, `Violates`.
  - Multi-hop BFS graph traversal, error-to-fix lookup, and user preference extraction for prompt injection.
- **Unified Memory Manager (`MemoryManager`)**:
  - Working memory buffer with LRU/LFU eviction and byte-limit compaction.
  - Unified interface tying together short-term, long-term, vector semantic, and graph memory.

---

### 9. Performance Profiling & Microbenchmark Suite (`tests/benchmark_aios.cpp`)
- Automated benchmark suite measuring operations/sec and latency percentiles (p50, p95, p99) for:
  - 100-node wide concurrent DAG execution under `TaskGraphExecutor`.
  - 1,000+ document semantic search queries under `VectorStore`.
  - Zero-allocation SSE stream chunk extraction under `HttpClient`.
