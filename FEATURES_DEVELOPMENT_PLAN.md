# AIOS Feature Development Plan & Progress

## Overview
This document outlines the new features planned and implemented for the MINIcodingAgent (AIOS) application.

---

## ✅ COMPLETED FEATURES

### 1. Context Engine (`src/context/ContextEngine.h`, `src/context/ContextEngine.cpp`)

**Purpose:** Advanced context management for LLM interactions with intelligent file ranking and conversation state tracking.

**Key Features Implemented:**
- **Conversation Management**
  - Start/end conversations with unique IDs
  - Track conversation state including goals, objectives, TODO lists
  - Store and retrieve conversation history
  - Manage user preferences per conversation

- **Context Item Management**
  - Multiple context types: Conversation, File, Symbol, Dependency, GitHistory, TerminalOutput, Error, Documentation, UserPreference
  - Automatic relevance scoring based on type and usage
  - Pin/unpin important context items
  - Token counting and budget management
  - Automatic eviction of low-relevance items when over token budget

- **File Ranking System**
  - Rank files by relevance to queries
  - Track recently modified files
  - Build dependency chains between files
  - Score files based on importance, recency, and relationships

- **Symbol Context**
  - Cache symbol definitions and usages
  - Track caller/callee relationships
  - Find symbols across files

- **Context Building**
  - Build optimized prompts from context
  - Select relevant context items based on task
  - Truncate content to fit token limits
  - Event subscription for context changes

**Data Structures:**
```cpp
struct ContextItem { id, type, content, source, relevance_score, tokens, tags, metadata, pinned }
struct ConversationState { goal, tasks, todo_list, important_files, recent_edits, confidence }
struct FileRanking { file_path, score, reasons, edit_distance, reference_count }
struct SymbolContext { name, type, file_path, definition, usages, callers, callees }
```

---

### 2. Tool Registry (`src/tools/ToolRegistry.h`, `src/tools/ToolRegistry.cpp`)

**Purpose:** Comprehensive tool system enabling agents to interact with filesystem, terminal, git, LSP, and more.

**Key Features Implemented:**

#### Tool Categories (14 categories):
1. **Filesystem Tools** - read, write, delete, list, mkdir, exists, size, copy, move, search
2. **Terminal Tools** - run commands, execute scripts
3. **Git Tools** - status, diff, commit, push, pull, checkout, branch, merge, log, stash
4. **Search Tools** - grep, regex search, find files, find symbols
5. **LSP Tools** - go to definition, find references, hover, completion, diagnostics, rename, format
6. **Network Tools** - HTTP GET/POST, download files
7. **Code Analysis Tools** - parse files, get AST, symbols, call graph, dependencies, complexity analysis
8. **Testing Tools** - run tests, run test files, get coverage
9. **Build Tools** - compile, clean, install dependencies
10. **Memory Tools** - store, retrieve, search, delete memories
11. **Context Tools** - add/get/remove context, rank files

#### Tool System Features:
- **Tool Definition System**
  - Name, description, category
  - Required permissions (Read, Write, Execute, Network, Dangerous)
  - Parameter validation with required/optional flags
  - Default parameter values
  - Usage examples
  - Timeout configuration

- **Permission System**
  - Check permissions before tool execution
  - Mark dangerous tools requiring explicit approval
  - Support for permission grants per session

- **Statistics Tracking**
  - Total/successful/failed calls per tool
  - Execution time statistics (avg, min, max)
  - Registry-wide statistics

- **Tool Registry**
  - Singleton pattern for global access
  - Register/unregister tools dynamically
  - List tools by name or category
  - Execute tools with parameter validation
  - Dependency injection for tool creation

**Architecture:**
```cpp
class Tool { virtual execute(), getDefinition(), validateParams() }
class FilesystemTools : public Tool { ... }
class TerminalTools : public Tool { ... }
class GitTools : public Tool { ... }
// ... etc for each category

class ToolRegistry {
    registerTool(name, tool)
    executeTool(name, params)
    executeWithPermission(name, params, granted_permissions)
    listTools(), listToolsByCategory(category)
}
```

---

## 📋 PLANNED FUTURE FEATURES

### 3. Enhanced Agent System (Extension)
- **Specialized Agent Types**
  - Planner Agent - Break down complex tasks
  - Researcher Agent - Search documentation and codebase
  - Coder Agent - Write and modify code
  - Tester Agent - Run and analyze tests
  - Reviewer Agent - Code review and security audit
  - Debugger Agent - Find and fix bugs
  
- **Multi-Agent Orchestration**
  - Agent collaboration protocols
  - Task distribution among agents
  - Result aggregation from multiple agents
  - Conflict resolution

### 4. Advanced Planning System
- **Task Graph Execution**
  - Dependency-aware task scheduling
  - Parallel execution of independent tasks
  - Rollback on failure
  - Checkpoint creation and restoration

- **Plan Strategies**
  - Chain-of-thought planning
  - Tree-of-thought exploration
  - Reflection-based plan refinement
  - Self-critique and correction

### 5. Repository Intelligence
- **Incremental Indexing**
  - Watch filesystem for changes
  - Update index incrementally
  - Cache invalidation strategies

- **AST-Based Analysis**
  - Parse multiple languages with Tree-sitter
  - Build symbol tables
  - Generate call graphs
  - Track imports/exports

- **Semantic Search**
  - Combine BM25 + embeddings
  - AST-aware search
  - Git history integration
  - Dependency graph traversal

### 6. Memory Enhancements
- **Long-term Memory**
  - Persistent storage with SQLite
  - Vector embeddings for semantic retrieval
  - Summarization of old conversations
  - Knowledge graph construction

- **Working Memory**
  - Short-term context buffer
  - Priority-based eviction
  - Compression techniques

### 7. Security & Permissions
- **Sandboxed Execution**
  - Docker/Firecracker integration
  - Linux namespaces isolation
  - Resource limits enforcement

- **Permission Layers**
  - Policy-based access control
  - Human approval checkpoints
  - Audit logging

### 8. Model Routing
- **Intelligent Model Selection**
  - Route simple tasks to small models
  - Use large models for complex reasoning
  - Specialized models for code generation
  - Embedding models for retrieval

- **Cost Optimization**
  - Track token usage and costs
  - Budget enforcement
  - Model fallback strategies

### 9. GUI Integration (Qt 6)
- **Main Window**
  - Conversation view
  - File explorer
  - Task progress visualization
  - Settings panel

- **Components**
  - Chat interface with markdown support
  - Diff viewer for code changes
  - Terminal emulator
  - Agent status dashboard

### 10. Testing Framework
- **Unit Tests**
  - GoogleTest integration
  - Mock providers for testing
  - Coverage reporting

- **Integration Tests**
  - End-to-end agent workflows
  - Tool execution tests
  - Performance benchmarks

---

## 🏗️ ARCHITECTURE IMPROVEMENTS

### Current State
```
┌─────────────────────────────────────────┐
│              Kernel                      │
├─────────────────────────────────────────┤
│  Scheduler  │  Planner  │  AgentManager │
├─────────────────────────────────────────┤
│     Context Engine  │   Tool Registry   │
├─────────────────────────────────────────┤
│     Memory Manager  │   EventBus        │
└─────────────────────────────────────────┘
```

### Target Architecture
```
┌──────────────────────────────────────────────┐
│                  UI Layer                     │
│         (Qt GUI / CLI / Web Interface)        │
├──────────────────────────────────────────────┤
│               API Gateway                     │
├──────────────────────────────────────────────┤
│                 Kernel                        │
│  ┌─────────────────────────────────────────┐  │
│  │            Event Bus                     │  │
│  └─────────────────────────────────────────┘  │
├──────────┬──────────┬──────────┬─────────────┤
│ Scheduler│ Planner  │  Agents  │  Tools      │
├──────────┼──────────┼──────────┼─────────────┤
│  Memory  │ Context  │Repository│  Sandbox    │
│ Manager  │ Engine   │  Index   │  Manager    │
├──────────┴──────────┴──────────┴─────────────┤
│           Model Provider Abstraction          │
│  (OpenAI, Anthropic, Ollama, Local, etc.)    │
└──────────────────────────────────────────────┘
```

---

## 📊 METRICS & MONITORING

### Implemented Metrics
- Tool execution statistics
- Context token usage
- Cache hit/miss rates
- Conversation state tracking

### Planned Metrics
- Agent performance metrics
- Task completion rates
- Cost tracking per task/agent
- Latency measurements
- Error rates and patterns
- Resource utilization (CPU, memory, GPU)

---

## 🔧 INTEGRATION POINTS

### External Services
- **LLM Providers**: OpenAI, Anthropic, Google, Ollama, LM Studio, vLLM
- **Vector Databases**: SQLite-vec, Qdrant, Milvus, Weaviate
- **Container Runtimes**: Docker, Firecracker, WSL
- **Language Servers**: Any LSP-compatible server

### File Formats
- JSON for configuration and data exchange
- SQLite for persistent storage
- Markdown for documentation and responses

---

## 🚀 NEXT STEPS

1. **Complete Stub Implementations**
   - Fill in FileSystem, TerminalExecutor, GitManager stubs
   - Implement actual LSP client
   - Add HTTP client with CPR/libcurl

2. **Add Provider Implementations**
   - OpenAI provider
   - Anthropic provider
   - Ollama provider for local models

3. **Build Event System**
   - Complete EventBus implementation
   - Add event handlers for all subsystems

4. **Create Test Suite**
   - Unit tests for ContextEngine
   - Unit tests for ToolRegistry
   - Integration tests

5. **Documentation**
   - API documentation
   - User guide
   - Contribution guidelines

---

## 📝 SUMMARY

This development cycle added two major components to AIOS:

1. **Context Engine** - A sophisticated context management system that handles conversation state, file ranking, symbol tracking, and prompt building with token budget awareness.

2. **Tool Registry** - A comprehensive tool framework with 11 tool categories covering filesystem operations, terminal execution, git, search, LSP, network, code analysis, testing, building, memory, and context management.

These components form the foundation for building intelligent coding agents that can understand codebases, manage complex tasks, and interact with development tools safely and efficiently.
