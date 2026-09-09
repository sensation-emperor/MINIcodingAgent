# UnnatSystems Brahma Coder

**An Autonomous AI-Powered Coding Agent Operating System**

UnnatSystems Brahma Coder is a high-performance C++23 application that orchestrates multiple AI models (both local and cloud-based) to perform complex software development tasks autonomously. The system features a professional Qt 6 GUI harness for visual workflow management, real-time monitoring, and seamless model switching.

## Core Mission

Build a **proper GUI harness** that allows users to:
- Run local AI models (LM Studio, Ollama, llama.cpp, etc.)
- Connect to cloud AI providers (OpenAI, Anthropic, Google, OpenRouter)
- Manage multi-agent workflows with visual feedback
- Monitor task execution, code generation, and testing in real-time
- Control permissions, view logs, and manage conversations

## Quick Start

```bash
# Clone repository
git clone https://github.com/unnat-systems/brahma-coder.git
cd brahma-coder

# Build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build .

# Run GUI
./brahma_coder_gui
```

For detailed setup instructions, see [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) and [docs/MODEL_SETUP.md](docs/MODEL_SETUP.md).

---

# Recommended Tech Stack

## Core Language

### C++23

**Why?**

- Maximum performance
- Excellent multithreading
- GPU support
- Low memory overhead
- Easy native integrations
- Good for long-running agent systems

## Libraries

- **Boost** - General utilities
- **fmt** - Fast formatting
- **spdlog** - Logging
- **asio** - Async I/O
- **range-v3** - Range operations

## AI Runtime

Use multiple providers with one abstraction: `ModelProvider`

**Local:**
- LM Studio
- Ollama
- llama.cpp
- TensorRT-LLM

**Cloud:**
- OpenAI
- Anthropic
- Google
- OpenRouter

Never hardcode APIs - use the routing layer.

## GUI

### Qt 6

**Reasons:**
- Professional
- Cross-platform
- Fast
- Native feeling
- Excellent C++ support

## Local Database

### SQLite

Stores everything:
- Conversations
- Tasks
- Memory
- Embeddings
- Settings
- Logs
- Projects
- Indexes
- Agents
- Permissions
- Events
- Metrics

## Vector Database

### Initially: SQLite + sqlite-vec

### Later Options:
- Qdrant
- Milvus
- Weaviate

## Search Engine

Hybrid retrieval combining:
- BM25
- Embeddings
- AST search
- Regex
- Git history
- Dependency graph

## Parsing

### Tree-sitter

Supports: C++, Python, Rust, Go, Java, C#, TypeScript, JavaScript, PHP, Lua, etc.

## AST

Tree-sitter for structural analysis

## LSP

Language Server Protocol integration

## Embeddings

**Local Models:**
- bge-large
- nomic-embed
- gte-large
- e5-large

## Message Bus

- Boost ASIO, or
- ZeroMQ, or
- Custom Event Bus

## Thread Pool

Custom work-stealing scheduler with:
- Priority queues
- Fibers (optional)

## JSON

- nlohmann/json, or
- simdjson

## HTTP

- CPR, or
- libcurl

## WebSocket

Boost.Beast

## Memory System

SQLite + Embeddings + Knowledge Graph + Summaries

## Code Execution

- Docker
- Firecracker
- WSL
- Linux namespaces

## Sandbox

Container runtime (Docker initially, Kubernetes later)

## Logging

spdlog

## Testing

- GoogleTest
- Catch2
- doctest

## Dependency Injection

Simple custom DI (avoid giant frameworks)

## Build System

CMake

## Package Manager

vcpkg or Conan

## Repository Index

Stores:
- AST
- Imports/Exports
- Symbols
- Call Graph
- References
- Inheritance
- File Hashes/Checksums
- Dependencies
- Embeddings
- Documentation
- Git history

---

# Internal Architecture

```
Application -> Kernel -> Scheduler -> Planner -> Agents -> Tools -> Memory -> Execution -> Verification -> Reflection -> UI
```

---

# Long-Term Vision

Your system should eventually support:

- Multi-agent orchestration
- Parallel task graphs
- Incremental repository indexing
- Semantic retrieval
- Automatic context compression
- Self-reflection and critique
- Tool/plugin marketplace
- Model routing across local and cloud providers
- Long-term memory and knowledge graphs
- Multi-language code intelligence
- Secure sandboxed execution
- Workflow automation
- Human approval checkpoints
- Distributed execution across multiple machines
- GPU-aware scheduling
- Offline-first operation with optional cloud augmentation

---

# High Level Architecture

```
                        USER
                          |
                          v
                 Request Interpreter
                          |
            Intent + Constraints Parser
                          |
        +------------------+------------------+
        v                  v                  v
 Planner Agent      Context Engine    Memory Engine
        |                  |                  |
        +------------------+------------------+
                          v
                 Task Graph Generator
                          |
            DAG / Workflow / Dependency Graph
                          |
        +------------------+------------------+
        v                  v                  v
 Code Agent      Search Agent     Test Agent
        v                  v                  v
 Documentation    Tool Runner    Terminal Agent
        |                  |                  |
        +------------------+------------------+
                          v
               Execution Environment
                          v
             Observation + Verification
                          v
              Reflection / Self Critique
                          v
             Continue / Retry / Finish
```

This is vastly simplified. Real systems are significantly more complicated.

---

# Key Insight

If your goal is to build a system comparable to Claude Code, Google Antigravity, Codex, or the latest agentic coding systems, you're no longer building "an AI model." You're building an **operating system around an LLM**.

The LLM is often only **10-30%** of the total system.

The remaining **70-90%** consists of orchestration, memory, planning, tools, execution environments, context management, verification, recovery, and hundreds of supporting systems.

---

# Documentation

- **[DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md)** - Current development progress tracking
- **[PROJECT.md](PROJECT.md)** - Detailed technical specification
- **[FEATURES_DEVELOPMENT_PLAN.md](FEATURES_DEVELOPMENT_PLAN.md)** - Feature roadmap
- **[BUILD.md](BUILD.md)** - Build instructions
- **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** - Quick start guide
- **[docs/GUI_GUIDE.md](docs/GUI_GUIDE.md)** - GUI usage documentation
- **[docs/MODEL_SETUP.md](docs/MODEL_SETUP.md)** - Model configuration guide

---

# License

MIT License

---

*Last updated: 2026-09-09*
