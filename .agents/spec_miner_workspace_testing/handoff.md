# Handoff Report: Specification Mining for Workspace Isolation & Testing Engine

**Agent**: Spec Miner 2 (`.agents/spec_miner_workspace_testing/`)  
**Target Subsystems**: 
- `src/workspace/` (Sandboxed Git Worktree & Multi-Branch Workspace Isolation)
- `src/testing/` (Automated Test Generation & Code Diagnostics Engine)  
**Parent / Caller**: Orchestrator (`d4f130db-4c0e-43aa-b376-8faa299098d0`)  
**Status**: Hard Handoff (Task Complete)

---

## 1. Observation

1. **User Request & Mission**:
   - `ORIGINAL_REQUEST.md` lines 12–16 requires building and integrating:
     - 2. Sandboxed Git Worktree & Multi-Branch Workspace Isolation (`src/workspace/`)
     - 3. Automated Test Generation & Code Diagnostics Engine (`src/testing/`)
     - 4. Regression & Multi-Subsystem Verification across Orchestrator, TaskGraphExecutor, ModelRouter, MemoryManager, and existing test suites (`aios_tests` 100% pass).
2. **Existing Codebase State**:
   - `src/` currently houses core subsystems (`src/taskgraph/`, `src/memory/`, `src/providers/`, `src/parser/`, `src/repository/`, `src/tools/`, `src/agents/`, `src/kernel/`, `src/events/`, `src/logging/`).
   - `src/git/git.h` and `src/sandbox/sandbox.h` currently contain minimal stub classes (`bool initialize() { return true; }`).
   - `src/tools/ToolRegistry.cpp` contains placeholder tools for `GitTools` (lines 280–380) and `TestingTools` (lines 827–890) returning hardcoded mock strings.
   - `src/agents/Orchestrator.cpp` lines 152–175 currently calls mock git stashes for checkpoints and rollbacks.
   - `src/parser/ASTParser.h` lines 1–82 and `src/repository/RepositoryIndex.h` lines 1–83 provide language symbol extraction and hybrid search.
3. **Environment & Tool Probing**:
   - Git CLI is fully functional. Probed `git worktree list --porcelain` and verified physical directory creation and branch binding (`git worktree add -b <branch> <path> <commit>`) and cleanup (`git worktree remove --force <path>`, `git branch -D <branch>`).
   - Visual Studio 2022 MSBuild is present at `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe` and CMake at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.
   - `build\Release\aios_tests.exe` executed (65 tests from 18 suites: 61 passed, 4 failed in preexisting symbol/caller and memory eviction tests).

---

## 2. Logic Chain

1. **Need for Sandboxed Workspace Isolation (`src/workspace/`)**:
   - In a concurrent multi-agent system, multiple agents execute tasks in parallel. Direct edits in the working tree cause race conditions, dirty git index collisions, and unrecoverable workspace corruption.
   - Using Git Worktrees (`GitWorktree`) allows physical file segregation into `.aios/worktrees/wt_<task_id>` while sharing the underlying repository object database.
   - Ephemeral branch isolation (`BranchSandbox`) guarantees that uncommitted or failing agent edits remain isolated in temporary branches (`aios/ephemeral/<task_id>`) until validated by tests.
   - Path containment (`PathContainment`) is essential to prevent prompt injection or agent escape via directory traversal (`../..`) or symlink attacks targeting sensitive files (`.git/`, `.env`, system directories).
   - Snapshotting and diff generation (`SnapshotManager`) provides sub-millisecond in-memory checkpoints, unified diff computation, and surgical single-file or full-workspace rollbacks.
2. **Need for Automated Testing & Code Diagnostics Engine (`src/testing/`)**:
   - Automated code generation requires immediate verification. `TestGenerator` leverages `ASTParser` and `RepositoryIndex` to synthesize language-idiomatic unit and integration tests across C++, Python, JavaScript/TypeScript, Rust, and Go.
   - Extreme edge cases (boundary values, nulls, NaNs, large payloads) and mocks/fixtures (Google Mock, pytest fixtures) ensure comprehensive coverage.
   - Multi-tier static diagnostics (`DiagnosticsEngine`) catches syntax errors, compiler warnings, linter violations (clang-tidy, ruff, eslint), and security/resource-leak code smells before running tests.
   - Sandboxed execution (`TestRunner`) executes tests with strict timeouts, process tree termination, and output normalization (GTest, pytest, Jest, cargo).
   - Closed-loop failure diagnosis (`TestingManager`) correlates failed assertions with AST function spans to generate surgical diff patches for `DebuggerAgent` and `CoderAgent`.

---

## 3. Caveats

1. **Subprocess vs Native Library**: The design specifies process-based execution of Git CLI commands (`git worktree ...`) and compiler/test binaries (`aios_tests.exe`, `pytest`, `cargo test`) for maximum cross-platform compatibility and stability without binary lock issues.
2. **Windows Path Formatting**: Windows paths require case-insensitive normalization and canonicalization to prevent false containment violations when comparing paths (e.g. `c:\` vs `C:\`).
3. **Pre-existing Test Failures**: 4 pre-existing test failures in `RepositoryIndexTest` and `MemoryManagerTest` are documented and must be resolved by the regression/verification milestone.

---

## 4. Conclusion

The functional and technical specifications for `src/workspace/` and `src/testing/` have been exhaustively probed, designed, and documented in `survey_workspace_testing_spec.md`.

The specification includes:
- 30 Discovered and probed features with explicit inputs, outputs, error behaviors, and discovery methods.
- 17 Rigorously analyzed edge cases and mitigation behaviors.
- Complete C++23 public class signatures and data transfer objects for all 10 modules:
  - `src/workspace/`: `GitWorktree`, `BranchSandbox`, `PathContainment`, `SnapshotManager`, `WorkspaceManager`, `WorkspaceTools`
  - `src/testing/`: `TestGenerator`, `DiagnosticsEngine`, `TestRunner`, `CoverageAnalyzer`, `TestingManager`, `TestingTools`
- Concrete integration contracts with `TaskGraphExecutor`, `MultiAgentOrchestrator`, `MemoryManager`, `KnowledgeGraph`, `EventBus`, and `ToolRegistry`.
- Detailed CMakeLists.txt updates, file layout, and unit test suites (`tests/test_workspace.cpp`, `tests/test_testing_engine.cpp`).

---

## 5. Verification Method

1. **Inspect Specification Artifacts**:
   - View `c:\Users\kaush\Downloads\MINIcodingAgent\.agents\spec_miner_workspace_testing\survey_workspace_testing_spec.md`
   - View `c:\Users\kaush\Downloads\MINIcodingAgent\.agents\spec_miner_workspace_testing\handoff.md`
2. **Verify Git Worktree Functionality**:
   - Run `git worktree list --porcelain` to verify worktree query parsing.
   - Run `git status` to verify working tree integrity.
3. **Verify Build & Test Status**:
   - Run `.\build\Release\aios_tests.exe` to inspect existing baseline test executions.
