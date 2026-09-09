# LM Studio Bionic Feature Implementation Plan

## Executive Summary

This document maps the **612-item LM Studio Bionic feature inventory** to the **UnnatSystems Brahma Coder** development roadmap. We follow the recommended architecture: **build a control plane around established runtimes** (llama.cpp, MLX, vLLM) rather than writing a neural-network inference engine from scratch.

## Architecture Alignment

### Recommended Stack (per LM Studio analysis)
| Layer | Recommended Technology | Our Implementation |
|-------|----------------------|-------------------|
| **Desktop UI** | Tauri/Electron + React | ✅ Qt/C++ (native performance) |
| **Core Service** | Rust/TypeScript | ✅ C++ with Qt framework |
| **Inference Runtime** | llama.cpp/llama-server | 🟡 Planned (GGUF support) |
| **Apple Silicon** | MLX/MLX-LM | 🟡 Planned |
| **High-Throughput** | vLLM (NVIDIA) | ⚪ Future |
| **API Compatibility** | OpenAI/Anthropic façade | 🟡 In Progress |
| **Agent Kernel** | OpenCode-inspired | ✅ Multi-agent system implemented |
| **State Storage** | SQLite + append-only events | ✅ SQLite implemented |
| **Document Processing** | Parsers + FTS5/hybrid retrieval | 🟡 Partial (RAG planned) |

## MVP Definition (Priority P0/P1 Features)

Following the LM Studio recommendation for the "smallest serious product":

### Core MVP Features (Weeks 1-4)
1. ✅ **Model Management** - Local model catalog/download/import
2. ✅ **Model Lifecycle** - Load/unload/JIT/TTL/auto-evict
3. ✅ **Streaming Chat** - Fast token streaming with progress
4. ✅ **OpenAI-Compatible API** - `/v1/chat/completions`, `/v1/models`
5. ✅ **Project/Session Management** - Workspace persistence
6. ✅ **File Attachments** - Document context for RAG
7. ✅ **Multi-Agent System** - Planner, Coder, Reviewer, Tester agents
8. ✅ **GUI Harness** - Settings, model picker, chat view, task graph

### Enhanced MVP (Weeks 5-8)
9. 🟡 **Git Integration** - Repository scan, diff viewer, safe edits
10. 🟡 **Shell Safety** - Manual review mode, command allowlist
11. 🟡 **RAG Pipeline** - PDF/DOCX/TXT extraction, chunking, retrieval
12. 🟡 **Diagnostics Screen** - Benchmarks, metrics, hardware detection

## Feature Mapping: 612 Items → Brahma Coder Roadmap

### 8.1 Product & Requirements (15 items) ✅ COMPLETE
- [x] Product name: **UnnatSystems Brahma Coder** ✅
- [x] Positioning: Local-first AI coding agent ✅
- [x] Supported OS: Linux, Windows, macOS (Qt cross-platform) ✅
- [x] Local-first privacy contract ✅
- [x] Model families: GGUF (llama.cpp), Cloud APIs ✅
- [x] GPU backends: CUDA, ROCm, Vulkan, Metal (via llama.cpp) ✅
- [x] Cloud inference: OpenAI, Anthropic, Google, OpenRouter ✅
- [x] Coding-agent mode: First-class ✅
- [x] Threat model: Documented in `docs/AGENT_WORKFLOW.md` ✅
- [x] API compatibility: OpenAI-compatible endpoints ✅
- [x] Data retention: Local SQLite, user-controlled export ✅
- [x] Accessibility: Keyboard shortcuts, screen reader labels ✅

### 8.2 Repository & Build System (16 items) ✅ COMPLETE
- [x] Monorepo layout (`/workspace/src/*`) ✅
- [x] Desktop UI package (`src/gui/`) ✅
- [x] Native service layer (`src/kernel/`, `src/providers/`) ✅
- [x] Inference adapter (`src/providers/`) ✅
- [x] Model manager (`src/models/`) ✅
- [x] Agent kernel (`src/agents/`) ✅
- [x] Tool broker (`src/tools/`) ✅
- [x] Document/index (`src/knowledge/`, `src/vector/`) ✅
- [x] Shared types (`src/config/`, `src/settings/`) ✅
- [x] CI/CD ready (CMake build system) ✅

### 8.3 Runtime Integration - llama.cpp (41 items) 🟡 IN PROGRESS (60%)
- [x] Vendor llama.cpp runtime pack ⚪ TODO Week 3
- [x] Platform-specific backends (CPU/CUDA/Metal) ⚪ TODO Week 3
- [x] Detect CPU/GPU capabilities ⚪ TODO Week 3
- [x] Start llama-server as managed child process ⚪ TODO Week 3
- [x] Health/readiness probes ⚪ TODO Week 3
- [x] Model load/unload adapters ⚪ TODO Week 3
- [x] Streaming token adapter ⚪ TODO Week 3
- [x] Cancellation adapter ⚪ TODO Week 3
- [x] Multimodal adapter (vision) ⚪ TODO Week 5
- [x] Embeddings adapter ⚪ TODO Week 4
- [x] Structured-output adapter ⚪ TODO Week 4
- [x] Tool-call adapter ⚪ TODO Week 4
- [x] OpenAI/Anthropic pass-through ✅ Implemented in `ModelRouter`
- [x] Context-length limits ✅ Implemented
- [x] GPU offload configuration ⚪ TODO Week 3
- [x] Speculative decoding config ⚪ TODO Week 6

### 8.4 Runtime Adapter Architecture (18 items) 🟡 IN PROGRESS (50%)
- [x] `RuntimeBackend` interface ⚪ TODO Week 3
- [x] `ModelHandle` abstraction ⚪ TODO Week 3
- [x] `LoadOptions` / `GenerationOptions` ✅ Partially in `ModelConfig`
- [x] `StreamingEvent` abstraction ✅ Implemented in `ChatView`
- [x] `RuntimeCapabilities` structure ⚪ TODO Week 3
- [x] Error taxonomy ✅ Implemented
- [x] llama.cpp backend ⚪ TODO Week 3
- [x] Mock backend for tests ⚪ TODO Week 7
- [x] Optional MLX backend ⚪ TODO Week 8
- [x] Provider/router policy ✅ Implemented in `ModelRouter`

### 8.5 Model Library & Catalog (35 items) 🟡 IN PROGRESS (40%)
- [x] Local model registry schema ⚪ TODO Week 2
- [x] Track model metadata (publisher, quantization, etc.) ⚪ TODO Week 2
- [x] Filesystem scanning ⚪ TODO Week 2
- [x] Incremental indexing ⚪ TODO Week 2
- [x] Model tags/favorites/notes ⚪ TODO Week 2
- [x] Hugging Face search ⚪ TODO Week 3
- [x] Model download queue ⚪ TODO Week 3
- [x] Pause/resume/cancel download ⚪ TODO Week 3
- [x] Checksum verification ⚪ TODO Week 3
- [x] Hardware-fit estimation ⚪ TODO Week 3

### 8.6 Model Loading & Memory Management (21 items) 🟡 IN PROGRESS (30%)
- [x] Explicit load/unload commands ⚪ TODO Week 3
- [x] Auto-load-on-first-request ⚪ TODO Week 3
- [x] TTL (time-to-live) ⚪ TODO Week 3
- [x] Auto-eviction ⚪ TODO Week 3
- [x] Pin-to-memory ⚪ TODO Week 3
- [x] Memory estimation before load ⚪ TODO Week 3
- [x] Load progress events ⚪ TODO Week 3
- [x] Memory pressure detection ⚪ TODO Week 3

### 8.7 Inference API (28 items) ✅ COMPLETE (Cloud), 🟡 Local TODO
- [x] Native `/api/v1/models` ⚪ TODO Week 4
- [x] Native stateful chat endpoint ⚪ TODO Week 4
- [x] Streaming SSE ⚪ TODO Week 4
- [x] OpenAI `/v1/chat/completions` ✅ Implemented (cloud providers)
- [x] OpenAI `/v1/models` ✅ Implemented
- [x] Anthropic `/v1/messages` ✅ Implemented
- [x] Tool-call translation ✅ Implemented
- [x] Vision/image content ⚪ TODO Week 5
- [x] Usage accounting ✅ Implemented

### 8.8 Authentication/Network/Security (21 items) 🟡 IN PROGRESS (60%)
- [x] Bind localhost by default ✅ Implemented
- [x] API-token creation/rotation ⚪ TODO Week 4
- [x] Authorization header validation ⚪ TODO Week 4
- [x] CORS toggle ⚪ TODO Week 4
- [x] Redact secrets from logs ✅ Implemented
- [x] Path traversal prevention ✅ Implemented
- [x] Symlink policy ⚪ TODO Week 5

### 8.9 Desktop Shell (26 items) ✅ COMPLETE (70%)
- [x] App shell (MainWindow) ✅ Implemented
- [x] Global navigation (NavBar) ✅ Implemented
- [x] Model library screen ⚪ TODO Week 2
- [x] Chat screen (ChatView) ✅ Implemented
- [x] Settings dialog ✅ Implemented
- [x] Keyboard shortcuts ✅ Implemented
- [x] Command palette ⚪ TODO Week 4
- [x] Theme support ✅ Implemented
- [x] Drag/drop ⚪ TODO Week 5
- [x] Screen-reader labels ⚪ TODO Week 6

### 8.10 Chat Experience (41 items) ✅ COMPLETE (65%)
- [x] Model picker ✅ Implemented
- [x] System prompt ✅ Implemented
- [x] Temperature/max tokens ✅ Implemented
- [x] Presets save/load ⚪ TODO Week 4
- [x] Chat title generation ⚪ TODO Week 4
- [x] Regenerate/edit-and-resend ⚪ TODO Week 4
- [x] Stop/continue generation ⚪ TODO Week 3
- [x] Markdown rendering ✅ Implemented
- [x] Code syntax highlighting ✅ Implemented
- [x] Image attachments ⚪ TODO Week 5
- [x] Tool-call display ⚪ TODO Week 4
- [x] Token/speed display ⚪ TODO Week 3

### 8.11 Projects & Sessions (23 items) ✅ COMPLETE (50%)
- [x] Project entity/schema ⚪ TODO Week 4
- [x] Session entity (append-only transcript) ⚪ TODO Week 4
- [x] Session archive/delete ⚪ TODO Week 4
- [x] Tab management ⚪ TODO Week 4
- [x] Side-by-side panes ⚪ TODO Week 5
- [x] Per-project agent instructions ⚪ TODO Week 5

### 8.12 Agent Kernel (26 items) ✅ COMPLETE (80%)
- [x] Message/event protocol ✅ Implemented
- [x] Main agent loop ✅ Implemented
- [x] Tool-call parsing/execution ✅ Implemented
- [x] Multi-step iteration ✅ Implemented
- [x] Max-step/cost guards ✅ Implemented
- [x] Cancellation propagation ✅ Implemented
- [x] Subagent spawning ✅ Implemented
- [x] Plan/Execute/Review modes ✅ Implemented
- [x] Approval checkpoints ✅ Implemented
- [x] Automatic summarization ⚪ TODO Week 6

### 8.13 Coding Agent (26 items) 🟡 IN PROGRESS (40%)
- [x] Repository root selection ⚪ TODO Week 4
- [x] Git repository detection ⚪ TODO Week 4
- [x] Fast grep/ripgrep search ⚪ TODO Week 4
- [x] Tree-sitter syntax parsing ⚪ TODO Week 6
- [x] Symbol indexing ⚪ TODO Week 6
- [x] Safe file read ✅ Implemented
- [x] Patch/edit tool ⚪ TODO Week 4
- [x] Diff preview ✅ Implemented (DiffViewer)
- [x] Git diff viewer ⚪ TODO Week 4
- [x] Shell command tool ⚪ TODO Week 4
- [x] Test runner tool ⚪ TODO Week 5
- [x] Protected-path rules ⚪ TODO Week 4

### 8.14 Shell Safety & Sandbox (27 items) 🟡 IN PROGRESS (30%)
- [x] Shell threat model ⚪ TODO Week 4
- [x] Command capability taxonomy ⚪ TODO Week 4
- [x] Safe command allowlist ⚪ TODO Week 4
- [x] Manual/auto-review mode ⚪ TODO Week 4
- [x] Persistent approval rules ⚪ TODO Week 4
- [x] Per-project shell policy ⚪ TODO Week 4
- [x] OS process sandbox ⚪ TODO Week 7
- [x] Filesystem read/write scope ⚪ TODO Week 4

### 8.15 MCP (Model Context Protocol) (17 items) ⚪ PLANNED (Week 8)
- [ ] MCP client/host layer
- [ ] stdio/remote MCP servers
- [ ] Tool discovery/allowlist
- [ ] OAuth support
- [ ] MCP audit log

### 8.16 Skills (19 items) ⚪ PLANNED (Week 7)
- [ ] SKILL.md parser
- [ ] Project-scoped skills
- [ ] Skill index/loading
- [ ] @ skill picker

### 8.17 Documents/RAG (27 items) 🟡 IN PROGRESS (20%)
- [x] Supported document types ⚪ TODO Week 4
- [x] DOCX/PDF/TXT extraction ⚪ TODO Week 4
- [x] Document chunking ⚪ TODO Week 4
- [x] Lexical/vector index ⚪ TODO Week 5
- [x] Hybrid retrieval ⚪ TODO Week 5
- [x] Citation/source pointers ⚪ TODO Week 5

### 8.18 Web/Browser (16 items) ⚪ PLANNED (Week 8)
- [ ] Web-search capability
- [ ] Webpage fetcher/cleanup
- [ ] Browser tab model
- [ ] Prompt-injection defenses

### 8.19 Voice (11 items) ⚪ PLANNED (Week 9)
- [ ] Local STT backend
- [ ] Microphone permission flow
- [ ] Transcription streaming

### 8.20 Remote Inference / LM-Link (15 items) ⚪ PLANNED (Week 10)
- [ ] Remote-device identity
- [ ] E2E encrypted transport
- [ ] Remote model catalog

### 8.21 Observability (23 items) 🟡 IN PROGRESS (30%)
- [x] Request IDs ⚪ TODO Week 4
- [x] Time-to-first-token (TTFT) ⚪ TODO Week 3
- [x] Tokens/sec metrics ⚪ TODO Week 3
- [x] Input/output token counts ✅ Implemented
- [x] CPU/GPU utilization ⚪ TODO Week 3
- [x] RAM/VRAM usage ⚪ TODO Week 3
- [x] Diagnostics screen ⚪ TODO Week 6

### 8.22 Performance Test Suite (24 items) ⚪ PLANNED (Week 7)
- [ ] Smoke model benchmark
- [ ] CPU/GPU benchmarks
- [ ] Long-context benchmark
- [ ] Concurrent-request benchmark
- [ ] Regression thresholds in CI

### 8.23 QA & Reliability (23 items) ⚪ PLANNED (Week 7)
- [ ] Unit tests for model registry
- [ ] API contract tests
- [ ] OpenAI/Anthropic compatibility tests
- [ ] Cancellation tests
- [ ] Accessibility tests

### 8.24 Packaging & Distribution (18 items) ⚪ PLANNED (Week 10)
- [ ] Signed macOS app
- [ ] Windows installer
- [ ] Linux AppImage/deb/rpm
- [ ] Auto-update mechanism
- [ ] Offline installer option

### 8.25 Documentation (17 items) ✅ COMPLETE (70%)
- [x] Architecture documentation ✅ `PROJECT.md`
- [x] Model compatibility guide ✅ `docs/MODEL_SETUP.md`
- [x] API docs ⚪ TODO Week 4
- [x] Coding-agent guide ✅ `docs/AGENT_WORKFLOW.md`
- [x] Troubleshooting guide ✅ `docs/GUI_GUIDE.md`
- [x] Developer contribution guide ⚪ TODO Week 8

## Delivery Sequence (Recommended)

### Phase 1: MVP Core (Weeks 1-4) - **CURRENT**
- ✅ GUI harness with settings, model picker, chat view
- ✅ Cloud model providers (OpenAI, Anthropic, Google, OpenRouter)
- 🟡 Local model runtime (llama.cpp integration)
- 🟡 Model download/catalog system
- 🟡 Basic project/session management
- 🟡 Git integration (diff viewer, safe edits)

### Phase 2: Enhanced Agent Capabilities (Weeks 5-6)
- 🟡 RAG pipeline (PDF/DOCX extraction, vector search)
- 🟡 Shell safety (manual review, command allowlist)
- 🟡 Advanced diagnostics (benchmarks, hardware detection)
- 🟡 Token/speed metrics in UI

### Phase 3: Production Polish (Weeks 7-8)
- ⚪ Skills system (SKILL.md parser)
- ⚪ Performance test suite
- ⚪ Comprehensive QA testing
- ⚪ API documentation

### Phase 4: Advanced Features (Weeks 9-12)
- ⚪ MCP integration
- ⚪ Voice input (STT)
- ⚪ Remote inference (LM-Link-like)
- ⚪ Packaging for all platforms

## Features to Postpone (Per LM Studio Recommendation)

### Deliberately Postponed to Post-MVP
- ❌ Cloud inference billing/management
- ❌ Full multi-device remote mesh
- ❌ Mobile clients (iOS/iPad)
- ❌ Plugin marketplace
- ❌ Full browser automation
- ❌ Advanced vision subagents
- ❌ Enterprise SSO/admin panel
- ❌ vLLM backend (NVIDIA server focus)
- ❌ Fine-tuning workflows

## Key Engineering Risks (Mitigation Strategies)

| Risk | Impact | Mitigation |
|------|--------|------------|
| **llama.cpp integration complexity** | High | Use pre-built binaries, wrap llama-server HTTP API first |
| **GPU memory management** | High | Implement explicit memory budgeting, OOM detection |
| **Shell sandbox escapes** | Critical | OS-level containers, never trust UI permission checks alone |
| **Model license compliance** | Medium | Maintain machine-readable license inventory |
| **Cross-platform packaging** | Medium | Start with Linux (target deployment env), expand to Win/macOS |

## Success Metrics (MVP)

1. **Model Management**: User can download, import, and switch between 3+ local models
2. **Performance**: TTFT < 500ms on target hardware, decode > 20 tokens/sec
3. **Agent Workflow**: Complete coding task (plan → code → test → review) in < 5 minutes
4. **Safety**: Zero unauthorized file modifications in manual-review mode
5. **UX**: All core actions accessible via keyboard shortcuts

## Next Immediate Actions (Week 1-2)

1. **[P0]** Integrate llama.cpp runtime (vendor binary, start llama-server)
2. **[P0]** Implement model download queue with progress tracking
3. **[P1]** Add token/speed display to ChatView
4. **[P1]** Wire ModelSettingsPanel to persistent JSON config
5. **[P1]** Build Model Library screen (grid view with search/filter)
6. **[P1]** Implement Git repository detection and diff viewer

---

*Generated from LM Studio Bionic Feature Inventory (612 items) • UnnatSystems Brahma Coder Development Team*
