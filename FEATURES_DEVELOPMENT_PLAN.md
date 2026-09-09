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

---

## 🚧 PLANNED FEATURES: LM Studio Bionic-like Enhancements

### 10. Enhanced Local Model Experience (LM Studio First-Class Integration)
- **Model Discovery & Auto-Configuration**:
  - Automatic detection of running LM Studio instances via local network scan.
  - One-click model download/install from Hugging Face through LM Studio integration.
  - Model capability detection (context window, token limits, supported features).
- **Adaptive Prompt Optimization**:
  - Dynamic prompt templating based on detected model family (Llama, Mistral, Phi, Gemma).
  - Token-aware context compression with sliding window for small-context models.
  - Smart system prompt injection for coding-specific tasks.

---

### 11. Interactive Chat Interface Enhancements (Bionic-inspired UX)
- **Rich Conversation Management**:
  - Multi-thread conversation support with branch/fork capabilities.
  - Conversation state persistence with quick resume functionality.
  - Visual conversation tree explorer showing agent decision paths.
- **Inline Code Actions**:
  - Click-to-apply code suggestions directly from chat output.
  - Inline diff preview before accepting changes.
  - One-click revert for applied modifications.
- **Agent Thought Transparency**:
  - Collapsible reasoning traces showing step-by-step agent thinking.
  - Confidence indicators for suggested actions.
  - Alternative solution branching with comparison view.

---

### 12. Advanced Autocomplete & Inline Completion
- **Context-Aware Code Completion**:
  - Real-time streaming inline completions powered by local LLM.
  - Multi-line completion support with intelligent cursor placement.
  - Ghost text rendering with tab-to-accept interaction.
- **Semantic Code Understanding**:
  - Symbol-aware completions using AST context.
  - Import/include auto-completion based on usage patterns.
  - Function signature help with parameter hints from repository knowledge.

---

### 13. Enhanced Model Router Intelligence
- **Workload-Aware Model Selection**:
  - Automatic routing: simple queries → small models, complex reasoning → large models.
  - Cost-performance optimization with user-configurable preferences.
  - Fallback chain with graceful degradation and user notification.
- **Model Health Monitoring Dashboard**:
  - Real-time latency, throughput, and error rate visualization.
  - Historical performance tracking per model/provider.
  - Predictive health alerts before failures occur.

---

### 14. Developer Workflow Automation
- **Smart Task Templates**:
  - Pre-built workflows for common tasks (refactor, debug, add feature, write tests).
  - Customizable workflow builder with drag-and-drop task graph editor.
  - Workflow sharing and community template marketplace.
- **Git Integration Enhancements**:
  - Intelligent commit message generation from code changes.
  - PR/MR description auto-generation with change summary.
  - Conflict resolution assistance with AI-powered merge suggestions.

---

### 15. Knowledge & Memory Augmentation
- **Long-Term Project Memory**:
  - Persistent project-specific knowledge graphs across sessions.
  - User preference learning and automatic prompt customization.
  - Architectural decision record (ADR) extraction and storage.
- **Cross-Project Intelligence**:
  - Pattern recognition across multiple repositories.
  - Reusable component identification and suggestion.
  - Best practice recommendations based on successful patterns.

---

### 16. Observability & Debugging Tools
- **Agent Execution Tracing**:
  - Detailed execution timeline with tool call inspection.
  - Token usage breakdown per agent/task/conversation.
  - Bottleneck identification with optimization suggestions.
- **Error Diagnosis & Recovery**:
  - Automatic root cause analysis for failed agent runs.
  - Suggested recovery actions with one-click retry.
  - Failure pattern detection and prevention recommendations.

---

### 17. Security & Compliance Features
- **Enhanced Permission System**:
  - Granular permission controls per tool/action type.
  - Audit logging with tamper-proof event records.
  - Compliance reporting for enterprise deployments.
- **Code Security Scanning**:
  - Integrated vulnerability detection in generated code.
  - License compliance checking for suggested dependencies.
  - Secret detection and redaction in outputs.

---

## 📋 IMPLEMENTATION PRIORITIES

| Priority | Feature | Effort | Impact | Dependencies |
|----------|---------|--------|--------|--------------|
| P0 | Enhanced Local Model Experience | Medium | High | Existing ModelProvider infrastructure |
| P0 | Interactive Chat Interface | High | High | GUI framework, EventBus |
| P1 | Advanced Autocomplete | High | High | LSP integration, AST parser |
| P1 | Model Router Intelligence | Medium | Medium | Metrics system, HttpClient |
| P2 | Developer Workflow Automation | Medium | Medium | TaskGraph, Workspace Manager |
| P2 | Knowledge & Memory Augmentation | High | Medium | VectorStore, KnowledgeGraph |
| P3 | Observability & Debugging Tools | Medium | Medium | Logging, Metrics |
| P3 | Security & Compliance Features | Low | Low | ToolRegistry, Permission system |

---

## 🎯 SUCCESS METRICS

- **Model Integration**: Support 5+ local model providers with seamless switching
- **User Experience**: <100ms perceived latency for inline completions
- **Code Quality**: >90% acceptance rate for AI-suggested changes
- **Reliability**: 99.9% uptime for local model connections
- **Performance**: Handle 1000+ file repositories with <5s context building
- **Memory Efficiency**: <500MB RAM footprint for typical development sessions
