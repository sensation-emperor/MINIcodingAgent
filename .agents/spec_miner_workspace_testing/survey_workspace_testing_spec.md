# Technical Specification: Workspace Isolation & Automated Testing/Diagnostics Subsystems

**AIOS (MINIcodingAgent) Full Developer Suite**  
**Document**: Specification Survey & Public API Contract  
**Subsystems**: 
- `src/workspace/` (Sandboxed Git Worktree & Multi-Branch Workspace Isolation)
- `src/testing/` (Automated Test Generation & Code Diagnostics Engine)
**Status**: Authoritative Technical Specification  
**Language Standard**: Modern C++23  

---

## 1. Executive Summary & Architectural Overview

The MINIcodingAgent Operating System (AIOS) requires robust, concurrent workspace isolation and an intelligent, closed-loop testing and diagnostics engine to support autonomous multi-agent software engineering. 

When multiple AI agents (e.g. `CoderAgent`, `TesterAgent`, `ResearcherAgent`, `DebuggerAgent`) collaborate or execute concurrent tasks in a shared codebase, they must not interfere with each other's uncommitted edits, corrupt git indexes, escape project directory boundaries, or produce untested, defective code.

### Core Architecture Pillars

```
+========================================================================================================+
|                                    MINIcodingAgent Kernel & Orchestrator                              |
|   (MultiAgentOrchestrator, TaskGraphExecutor, ToolRegistry, EventBus, MemoryManager, KnowledgeGraph)  |
+========================================================================================================+
                                     |                                      |
         +---------------------------+--------------------------------------+---------------------------+
         |                                                                                              |
         v                                                                                              v
+-------------------------------------------------------------+  +-------------------------------------------------------------+
|               WORKSPACE ISOLATION (src/workspace/)          |  |             TESTING & DIAGNOSTICS (src/testing/)            |
+-------------------------------------------------------------+  +-------------------------------------------------------------+
| 1. GitWorktree:                                             |  | 1. TestGenerator:                                           |
|    - Worktree add, remove, lock, prune, list, repair        |  |    - AST-driven unit/integration test synthesis             |
|    - Physical worktree dir allocation (.aios/worktrees/)    |  |    - Boundary value & extreme edge-case generation           |
|    - Isolated .git pointers & concurrent branch access      |  |    - Mock/Stub/Fixture synthesis (GMock, unittest.mock)      |
| 2. BranchSandbox:                                           |  | 2. DiagnosticsEngine:                                       |
|    - Ephemeral branch lifecycle (aios/task-<id>)            |  |    - AST syntax & structural validation                     |
|    - Stashing, staging, micro-commit transactions           |  |    - Compiler error parsing (MSVC, GCC, Clang)               |
|    - 3-Way merge, squash, rebase, conflict detection        |  |    - Linter integration (clang-tidy, ruff, eslint, cargo)   |
| 3. PathContainment:                                         |  |    - Security vulnerability & code smell rules              |
|    - Canonical path resolution & prefix containment         |  | 3. TestRunner:                                              |
|    - Symlink escape protection & directory jail             |  |    - Sandboxed process execution (timeout, memory limit)    |
|    - Read-only vs Read-Write permission enforcement         |  |    - Multi-framework output parser (GTest, pytest, Jest)    |
|    - Sensitive credential protection (.env, id_rsa, .git)   |  | 4. CoverageAnalyzer:                                        |
| 4. SnapshotManager:                                         |  |    - Line, function, and branch coverage measurement        |
|    - Transactional in-memory/disk file snapshots            |  |    - Uncovered span identification & threshold checks       |
|    - Unified & semantic AST diff generation                 |  | 5. Failure Diagnosis & Auto-Repair:                         |
|    - Surgical single-file, hunk, or full-workspace rollback|  |    - Root-cause localization from tracebacks/assertions     |
| 5. WorkspaceManager (Facade) & WorkspaceTools               |  |    - Closed-loop repair patch synthesis for DebuggerAgent   |
+-------------------------------------------------------------+  +-------------------------------------------------------------+
```

---

## 2. Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | Workspace Isolation | `GitWorktree::createWorktree` | Allocates isolated physical worktree directory and attaches new/existing branch | `worktree_id`, `branch_name`, `base_commit_sha`, `is_new_branch` | `Result<WorktreeInfo>` (path, branch, commit) | Returns `WorktreeAlreadyExists`, `GitCommandFailed`, `InvalidPath` | Authoritative Git CLI Spec & Probing |
| 2 | Workspace Isolation | `GitWorktree::removeWorktree` | Prunes and deletes isolated worktree directory from filesystem and git index | `worktree_id` or `worktree_path`, `force_flag` | `Result<bool>` (true on success) | Returns `WorktreeNotFound`, `WorktreeLocked`, `FileSystemError` | Authoritative Git CLI Spec & Probing |
| 3 | Workspace Isolation | `GitWorktree::listWorktrees` | Parses `git worktree list --porcelain` into structured metadata objects | None | `std::vector<WorktreeInfo>` | Returns empty vector on git error | Authoritative Git CLI Spec & Probing |
| 4 | Workspace Isolation | `GitWorktree::lockWorktree` / `unlock` | Locks worktree preventing accidental pruning during long-running agent jobs | `worktree_path`, `reason` | `Result<bool>` | Returns `WorktreeNotFound`, `PermissionDenied` | Authoritative Git CLI Spec & Probing |
| 5 | Workspace Isolation | `GitWorktree::pruneWorktrees` | Cleans up stale or orphaned worktree administrative files in `.git/worktrees` | `expire_duration` (optional) | `Result<size_t>` (count pruned) | Returns `GitCommandFailed` | Authoritative Git CLI Spec & Probing |
| 6 | Branch Sandbox | `BranchSandbox::createEphemeralBranch` | Spawns dedicated temporary branch (e.g. `aios/task_<id>_<uuid>`) | `task_id`, `base_ref` (e.g. `main` or `HEAD`) | `Result<std::string>` (branch name) | Returns `BranchAlreadyExists`, `InvalidBaseRef` | Requirements & Workflow Spec |
| 7 | Branch Sandbox | `BranchSandbox::stageAndCommit` | Transactionally adds files and creates atomic micro-commit | `commit_message`, `file_paths` (optional, default all) | `Result<std::string>` (commit SHA) | Returns `NothingToCommit`, `GitError` | Requirements & Workflow Spec |
| 8 | Branch Sandbox | `BranchSandbox::mergeBranch` | Performs merge with configurable strategy (FastForward, ThreeWay, Squash) | `target_branch`, `source_branch`, `strategy` | `Result<MergeResult>` (success, conflicts, diff) | Returns `MergeConflictDetected`, `BranchNotFound` | Requirements & Workflow Spec |
| 9 | Branch Sandbox | `BranchSandbox::detectConflicts` | Pre-flight dry-run conflict detector identifying conflicting files and chunks | `source_branch`, `target_branch` | `Result<std::vector<ConflictFile>>` | Returns `GitError` | Requirements & Workflow Spec |
| 10 | Branch Sandbox | `BranchSandbox::cleanupEphemeralBranch` | Deletes ephemeral branch and cleans up refs post-merge or on task discard | `branch_name`, `force` | `Result<bool>` | Returns `BranchNotFound`, `BranchNotMerged` (if !force) | Requirements & Workflow Spec |
| 11 | Path Containment | `PathContainment::validatePath` | Canonicalizes and verifies path stays strictly within allowed workspace root | `input_path`, `workspace_root`, `access_mode` (Read/Write) | `Result<std::filesystem::path>` (canonical path) | Returns `PathTraversalDetected`, `AccessDenied`, `NotFound` | Security Requirements & OS Spec |
| 12 | Path Containment | `PathContainment::isContained` | Fast boolean check whether target path is subpath of workspace root | `target_path`, `root_path` | `bool` | Returns `false` on symlink breakout or traversal | Security Requirements & OS Spec |
| 13 | Path Containment | `PathContainment::isProtectedPath` | Blocks write access to `.git/`, `.env`, secret files, system binaries | `canonical_path` | `bool` (true if protected) | Returns `true` for prohibited paths | Security Requirements & OS Spec |
| 14 | Snapshot & Rollback | `SnapshotManager::createSnapshot` | Captures in-memory/disk shadow copy of target files before modification | `snapshot_id`, `file_paths`, `description` | `Result<CheckpointInfo>` | Returns `FileNotFound`, `SnapshotLimitExceeded` | Requirements & Orchestrator Spec |
| 15 | Snapshot & Rollback | `SnapshotManager::generateDiff` | Computes standard Unified Diff and AST-level modified symbol summaries | `snapshot_id` or base commit | `Result<UnifiedDiffReport>` | Returns `SnapshotNotFound` | Requirements & Orchestrator Spec |
| 16 | Snapshot & Rollback | `SnapshotManager::rollbackSnapshot` | Reverts all modified files atomically to exact checkpoint state | `snapshot_id` | `Result<RollbackSummary>` | Returns `SnapshotNotFound`, `FileWriteError` | Requirements & Orchestrator Spec |
| 17 | Snapshot & Rollback | `SnapshotManager::rollbackFile` | Surgically restores a single file to snapshot version without affecting others | `snapshot_id`, `relative_file_path` | `Result<bool>` | Returns `FileNotFoundInSnapshot` | Requirements & Orchestrator Spec |
| 18 | Test Generation | `TestGenerator::generateUnitTests` | Synthesizes language-idiomatic unit test suite from AST signatures | `source_file_path`, `target_symbols`, `language`, `framework` | `Result<GeneratedTestSuite>` (source code, test cases) | Returns `ParsingFailed`, `UnsupportedLanguage` | Requirements & ASTParser Spec |
| 19 | Test Generation | `TestGenerator::generateEdgeCases` | Generates boundary values (nulls, min/max bounds, unicode, large payloads) | `symbol_signature`, `param_types` | `std::vector<TestCaseSpec>` | Returns empty if types unresolvable | Requirements & QA Spec |
| 20 | Test Generation | `TestGenerator::generateMocks` | Synthesizes mock classes and dependency injection shims for external APIs | `interface_symbol`, `framework` (GMock, unittest.mock, Jest) | `Result<std::string>` (mock code) | Returns `SymbolNotAnInterface` | Requirements & QA Spec |
| 21 | Code Diagnostics | `DiagnosticsEngine::runDiagnostics` | Runs multi-tier diagnostics (AST syntax, linters, compiler, security smells) | `target_files`, `config` (enabled linters, severity threshold) | `Result<std::vector<DiagnosticItem>>` | Returns `ToolNotFound`, `ExecutionFailed` | Requirements & Static Analysis Spec |
| 22 | Code Diagnostics | `DiagnosticsEngine::detectVulnerabilities` | AST rule scanner for resource leaks, SQL/cmd injection, unhandled exceptions | `parsed_file`, `ruleset` | `std::vector<DiagnosticItem>` | Returns empty if clean | Requirements & Security Spec |
| 23 | Code Diagnostics | `DiagnosticsEngine::parseCompilerErrors` | Extracts structured error spans (file, line, col, message, code) from stderr | `compiler_output`, `compiler_flavor` (msvc, gcc, clang) | `std::vector<DiagnosticItem>` | Returns empty if parse fails | Build & Verification Spec |
| 24 | Test Execution | `TestRunner::executeTestSuite` | Spawns sandboxed test runner process with timeout and output capture | `test_executable_or_command`, `worktree_path`, `timeout_ms` | `Result<TestExecutionReport>` (pass/fail, duration, cases) | Returns `ProcessTimeout`, `CrashDetected`, `ExecutionFailed` | Requirements & Sandbox Spec |
| 25 | Test Execution | `TestRunner::parseTestOutput` | Normalizes test framework outputs (GTest, pytest, Jest, cargo) into unified DTO | `raw_stdout`, `raw_stderr`, `framework` | `TestExecutionReport` | Sets `success=false` on parse anomaly | Requirements & QA Spec |
| 26 | Coverage Analysis | `CoverageAnalyzer::analyzeCoverage` | Ingests gcov/lcov/llvm-cov/coverage.py reports to calculate metrics | `coverage_report_path_or_data`, `format` | `Result<CoverageReport>` (line %, branch %, uncovered lines) | Returns `InvalidCoverageData` | QA & Metrics Spec |
| 27 | Coverage Analysis | `CoverageAnalyzer::checkThresholds` | Verifies test run satisfies minimum code coverage gates | `coverage_report`, `min_line_pct`, `min_branch_pct` | `CoverageThresholdResult` (passed, deficits) | Returns `ThresholdViolated` | QA & Metrics Spec |
| 28 | Failure Diagnosis | `TestingManager::diagnoseFailure` | Correlates failed test assertions with AST source code to isolate root cause | `test_report`, `source_files`, `repository_index` | `DiagnosticDiagnosisReport` (root cause, failing line, fix) | Returns `SourceNotFound` | Orchestrator & Debugger Spec |
| 29 | Auto-Repair Suggestion| `TestingManager::generateRepairSuggestion` | Synthesizes surgical patch diff for `DebuggerAgent` and `CoderAgent` | `diagnosis_report` | `Result<std::string>` (unified diff patch) | Returns `RepairGenerationFailed` | Orchestrator & Debugger Spec |
| 30 | AIOS Integration | `WorkspaceTools` / `TestingTools` | Adapts Workspace and Testing subsystems into `ToolRegistry` tools | `ToolRegistry` instance | `bool` (registration status) | Returns false on registration collision | ToolRegistry Spec |

---

## 3. Edge Cases

| # | Feature | Input / Condition | Observed & Specified Behavior |
|---|---------|-------------------|-------------------------------|
| 1 | `GitWorktree::createWorktree` | Path already exists on disk as a file or non-empty directory | Fails gracefully with `WorktreeError::DirectoryExists`; does not overwrite unmanaged directories. |
| 2 | `GitWorktree::createWorktree` | Branch name is already checked out in another worktree or main repo | Detects git worktree limitation (git prevents dual checkout); creates unique branch `aios/task_<id>_wt` or attaches in detached HEAD mode. |
| 3 | `GitWorktree::removeWorktree` | Worktree has uncommitted modifications or untracked files | If `force == false`, returns `WorktreeError::DirtyWorktree` with list of uncommitted files; if `force == true`, uses `git worktree remove --force` and cleans folder. |
| 4 | `GitWorktree::removeWorktree` | Worktree directory was deleted manually from disk by user | `git worktree remove` may warn; subsystem calls `git worktree prune` to clean `.git/worktrees` metadata safely. |
| 5 | `PathContainment::validatePath` | Path contains relative traversal sequences (`../../etc/passwd` or `..\..\Windows\System32`) | Resolves canonical path; detects prefix mismatch (`!path.starts_with(workspace_root)`); throws `SecurityException` / returns `PathContainmentError::TraversalViolation`. |
| 6 | `PathContainment::validatePath` | Path uses symlinks or NTFS junction points targeting external folder | Resolves symlink target (`std::filesystem::canonical`); verifies real target path is within workspace root; rejects if points outside. |
| 7 | `PathContainment::validatePath` | Path casing mismatch on Windows (e.g. `c:\users\...` vs `C:\Users\...`) | Normalizes drive letter to uppercase and resolves canonical path before comparison to prevent false positives. |
| 8 | `PathContainment::validatePath` | Write request targeting `.git/config` or `.git/hooks/pre-commit` | Rejects with `PathContainmentError::ProtectedSystemPath`; direct writes to `.git/` internal metadata are strictly forbidden. |
| 9 | `BranchSandbox::mergeBranch` | 3-way merge has conflicting lines in source file | Does not crash or leave broken state; aborts merge (`git merge --abort`), extracts conflict markers and hunks into `MergeConflictReport`, and returns `MergeResult::Conflict`. |
| 10 | `SnapshotManager::rollbackSnapshot` | Target file was deleted or newly created since snapshot was taken | Newly created files are deleted; deleted files are restored from shadow snapshot; modified files are restored to snapshot content. |
| 11 | `TestGenerator::generateUnitTests` | Source code has syntax errors or unsupported language syntax | `ASTParser` returns `ParsedFile{success: false}`; `TestGenerator` emits `TestGenError::ASTParseFailure` with syntax diagnostic rather than generating invalid tests. |
| 12 | `TestGenerator::generateEdgeCases` | Function takes complex pointer/reference cycles or recursive data structures | Generates null pointer, empty container, and shallow initialized mock; avoids infinite recursive fixture generation via depth limiter (`max_depth = 3`). |
| 13 | `TestRunner::executeTestSuite` | Test binary enters infinite loop or deadlocks | Process monitor enforces strict timeout (e.g. `timeout_ms = 30000`); kills process tree (including child processes); captures partial stdout/stderr; returns `TestResult::Timeout`. |
| 14 | `TestRunner::executeTestSuite` | Test binary crashes with Access Violation / SIGSEGV | Captures process exit code (-1073741819 / 139); parses crash signal; flags test as `FatalCrash` with stderr stack dump. |
| 15 | `TestRunner::parseTestOutput` | Test output contains non-UTF-8 binary garbage or mixed ANSI escape codes | Sanitizes ANSI color codes; replaces invalid UTF-8 sequences with replacement character (`\uFFFD`); cleanly parses test assertions. |
| 16 | `DiagnosticsEngine::runDiagnostics` | Linter tool (e.g. `clang-tidy` or `ruff`) is not installed on system | Gracefully skips missing external tool; records warning in diagnostic metadata; proceeds with internal AST-based rule diagnostics. |
| 17 | `TestingManager::diagnoseFailure` | Test failure has no line number in assertion message | Traverses stack trace or cross-references function name with `RepositoryIndex` symbol table to pinpoint best candidate source line. |

---

## 4. Part A: Sandboxed Git Worktree & Multi-Branch Workspace Isolation (`src/workspace/`)

### 4.1 Subsystem Architecture & Responsibilities

The `src/workspace/` subsystem provides physical and logical isolation for AI agents executing in the MINIcodingAgent OS. It is composed of five core modules:

1. **`GitWorktree`**: Manages git worktree lifecycles using git porcelain and plumbing commands.
2. **`BranchSandbox`**: Manages ephemeral branch creation, staging, merging, rebase, and conflict extraction.
3. **`PathContainment`**: Enforces strict filesystem containment, preventing path traversal and protecting sensitive files.
4. **`SnapshotManager`**: Provides lightweight in-memory and disk shadow file snapshotting, diff generation, and transactional rollback.
5. **`WorkspaceManager`**: The unified orchestration facade tying together worktrees, sandboxes, containment, and snapshots.

```
                      +------------------------------------------+
                      |             WorkspaceManager             |
                      |   (Unified Facade & LifeCycle Manager)   |
                      +------------------------------------------+
                               /            |             \
                              /             |              \
                             v              v               v
                +-------------------+ +---------------+ +--------------------+
                |    GitWorktree    | | BranchSandbox | |  SnapshotManager   |
                | (Worktree Engine) | | (Branch Stg.) | | (Diffs & Rollback) |
                +-------------------+ +---------------+ +--------------------+
                             \              |              /
                              \             |             /
                               v            v            v
                      +------------------------------------------+
                      |             PathContainment              |
                      |    (Canonical Bounds, Security Jail)     |
                      +------------------------------------------+
```

---

### 4.2 Module 1: `GitWorktree` (Worktree Lifecycle Engine)

#### 4.2.1 Operational Flow
1. **Worktree Directory Allocation**: Allocates directories under `.aios/worktrees/wt_<task_id>_<timestamp>` or a user-configured sandbox root.
2. **Creation**: Executes `git worktree add -b <ephemeral_branch> <path> <base_commit>`.
3. **Tracking**: Maintains an active registry of worktree descriptors (`WorktreeDescriptor`) indexed by `worktree_id`.
4. **Locking**: Uses `git worktree lock --reason <reason> <path>` to prevent external pruning while an agent is actively compiling or editing.
5. **Removal & Pruning**: Unlocks and executes `git worktree remove --force <path>`, followed by `git worktree prune`.

#### 4.2.2 Public Interface (`src/workspace/GitWorktree.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <mutex>
#include <filesystem>
#include "WorkspaceTypes.h"

namespace aios::workspace {

struct WorktreeInfo {
    std::string id;
    std::filesystem::path path;
    std::string branch;
    std::string head_commit_sha;
    bool is_locked = false;
    std::string lock_reason;
    bool is_detached = false;
    std::chrono::system_clock::time_point created_at;
    std::string owner_task_id;
};

struct CreateWorktreeOptions {
    std::string worktree_id;
    std::string branch_name;
    std::string base_ref = "HEAD";
    bool create_new_branch = true;
    bool detach_head = false;
    bool lock_immediately = false;
    std::string lock_reason;
    std::filesystem::path custom_path;
};

class GitWorktree {
public:
    explicit GitWorktree(std::filesystem::path repository_root);
    ~GitWorktree();

    // Worktree Lifecycle
    WorkspaceResult<WorktreeInfo> createWorktree(const CreateWorktreeOptions& options);
    WorkspaceResult<bool> removeWorktree(const std::string& worktree_id_or_path, bool force = true);
    WorkspaceResult<bool> lockWorktree(const std::string& worktree_id_or_path, const std::string& reason);
    WorkspaceResult<bool> unlockWorktree(const std::string& worktree_id_or_path);
    WorkspaceResult<size_t> pruneWorktrees(const std::string& expire = "now");
    WorkspaceResult<bool> repairWorktrees();

    // Querying
    WorkspaceResult<std::vector<WorktreeInfo>> listWorktrees() const;
    std::optional<WorktreeInfo> getWorktree(const std::string& worktree_id) const;
    bool hasWorktree(const std::string& worktree_id) const;

    // Path Accessors
    std::filesystem::path getRepositoryRoot() const { return repository_root_; }
    std::filesystem::path getWorktreesRoot() const { return worktrees_root_; }

private:
    std::filesystem::path repository_root_;
    std::filesystem::path worktrees_root_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, WorktreeInfo> active_worktrees_;

    WorkspaceResult<std::string> executeGitCommand(const std::vector<std::string>& args, 
                                                   const std::filesystem::path& working_dir = {}) const;
    std::vector<WorktreeInfo> parsePorcelainWorktreeList(const std::string& output) const;
    void refreshActiveWorktreesUnlocked();
};

} // namespace aios::workspace
```

---

### 4.3 Module 2: `BranchSandbox` (Branch & Staging Sandbox)

#### 4.3.1 Operational Flow
1. **Branch Naming Scheme**: `aios/ephemeral/<task_id>/<timestamp>` prevents collision with user branches.
2. **Patch Staging**: Files modified by `CoderAgent` can be staged selectively (`git add <files>`) or in bulk (`git add -A`).
3. **Micro-Commit Transactions**: Captures logical steps as micro-commits with structured commit trailers (`AIOS-Task-ID: <id>`, `AIOS-Agent: CoderAgent`).
4. **Merge & Rebase Operations**:
   - Supports `FastForwardOnly`, `ThreeWayMerge`, `SquashMerge`, and `Rebase`.
5. **Conflict Identification**:
   - Parses git conflict output and inspects conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`).
   - Produces `std::vector<MergeConflict>` detailing file, start/end lines, base version, our version, their version.

#### 4.3.2 Public Interface (`src/workspace/BranchSandbox.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include "WorkspaceTypes.h"

namespace aios::workspace {

enum class MergeStrategy {
    FastForwardOnly,
    ThreeWay,
    Squash,
    Rebase
};

struct MergeConflictChunk {
    int start_line;
    int end_line;
    std::string our_content;
    std::string their_content;
    std::string base_content;
};

struct MergeConflictFile {
    std::string file_path;
    std::vector<MergeConflictChunk> chunks;
    bool is_binary = false;
};

struct MergeResult {
    bool success = false;
    bool has_conflicts = false;
    std::string merge_commit_sha;
    std::string base_branch;
    std::string source_branch;
    std::vector<MergeConflictFile> conflicts;
    std::string error_message;
};

class BranchSandbox {
public:
    explicit BranchSandbox(std::filesystem::path repo_or_worktree_path);
    ~BranchSandbox() = default;

    // Ephemeral Branch Management
    WorkspaceResult<std::string> createEphemeralBranch(const std::string& task_id, const std::string& base_ref = "HEAD");
    WorkspaceResult<bool> checkoutBranch(const std::string& branch_name);
    WorkspaceResult<bool> deleteBranch(const std::string& branch_name, bool force = false);
    WorkspaceResult<std::vector<std::string>> listEphemeralBranches() const;

    // Staging & Micro-Commits
    WorkspaceResult<std::string> stageAndCommit(const std::string& message, 
                                                const std::vector<std::string>& files = {},
                                                const std::unordered_map<std::string, std::string>& metadata = {});
    WorkspaceResult<std::string> createStash(const std::string& message, bool include_untracked = true);
    WorkspaceResult<bool> popStash(int stash_index = 0);
    WorkspaceResult<bool> dropStash(int stash_index = 0);

    // Merge, Rebase & Conflict Handling
    WorkspaceResult<MergeResult> merge(const std::string& source_branch, MergeStrategy strategy = MergeStrategy::ThreeWay);
    WorkspaceResult<MergeResult> rebase(const std::string& upstream_branch);
    WorkspaceResult<bool> abortMerge();
    WorkspaceResult<std::vector<MergeConflictFile>> detectConflicts(const std::string& source_branch, const std::string& target_branch);

    // Diffs
    WorkspaceResult<std::string> getBranchDiff(const std::string& base_branch, const std::string& target_branch) const;
    WorkspaceResult<std::string> getUncommittedDiff() const;

private:
    std::filesystem::path working_path_;

    WorkspaceResult<std::string> runGit(const std::vector<std::string>& args) const;
    std::vector<MergeConflictFile> parseConflictMarkersInFile(const std::filesystem::path& relative_path) const;
};

} // namespace aios::workspace
```

---

### 4.4 Module 3: `PathContainment` (Security Jail & Access Sandbox)

#### 4.4.1 Security Rules & Guarantees
1. **Directory Traversal Defense**: All input paths (relative or absolute) are converted to weakly canonical paths. If the canonical path does not start with the canonical workspace root, access is rejected immediately with `PathContainmentError::TraversalViolation`.
2. **Symlink Resolution & Escape Prevention**: Symlinks are dereferenced using `std::filesystem::canonical`. If a symlink resolves to a target outside the workspace root, file read/write is blocked.
3. **Protected Paths Protection**:
   - `.git/` (and `.git/*` internal metadata files)
   - `.env`, `.env.*`, `*credentials*`, `*.pem`, `*.key`, `id_rsa`, `id_ed25519`
   - `.aios/config.json` (protected workspace config)
4. **Access Modes**:
   - `ReadOnly`: Allows reading allowed files.
   - `ReadWrite`: Allows reading and writing allowed non-protected files.

#### 4.4.2 Public Interface (`src/workspace/PathContainment.h`)

```cpp
#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <mutex>
#include "WorkspaceTypes.h"

namespace aios::workspace {

enum class AccessMode {
    ReadOnly,
    ReadWrite,
    Execute
};

struct PathValidationResult {
    bool is_valid = false;
    std::filesystem::path canonical_path;
    std::filesystem::path relative_path;
    bool is_protected = false;
    std::string violation_reason;
};

class PathContainment {
public:
    explicit PathContainment(std::filesystem::path workspace_root);
    ~PathContainment() = default;

    // Path Validation
    PathValidationResult validate(const std::filesystem::path& target_path, AccessMode mode = AccessMode::ReadWrite) const;
    bool isContained(const std::filesystem::path& target_path) const;
    bool isProtected(const std::filesystem::path& target_path) const;

    // Security Rules Configuration
    void addProtectedPattern(const std::string& glob_or_exact);
    void addAllowedExternalPath(const std::filesystem::path& external_path, AccessMode mode = AccessMode::ReadOnly);

    // Sanitization Helpers
    std::filesystem::path sanitize(const std::filesystem::path& raw_path) const;
    std::string getRelativeString(const std::filesystem::path& target_path) const;

    std::filesystem::path getWorkspaceRoot() const { return workspace_root_; }

private:
    std::filesystem::path workspace_root_;
    std::filesystem::path canonical_root_;
    mutable std::mutex mutex_;

    std::vector<std::string> protected_patterns_;
    std::unordered_map<std::filesystem::path, AccessMode> allowed_external_paths_;

    void initializeDefaultProtectedPatterns();
    bool matchesPattern(const std::string& path_str, const std::string& pattern) const;
};

} // namespace aios::workspace
```

---

### 4.5 Module 4: `SnapshotManager` (Transactional Snapshots & Rollback)

#### 4.5.1 Snapshot Architecture
- **In-Memory Buffer**: For fast per-turn agent rollback during coding cycles.
- **Disk Shadow Store**: Stored under `.aios/snapshots/<snapshot_id>/` containing exact file replicas and a manifest JSON.
- **Unified Diff Engine**: Computes standard unified diffs between current workspace state and any snapshot.
- **Surgical Rollback**:
  - Rollback entire snapshot (reverts all tracked files, deletes files created after snapshot, restores files deleted after snapshot).
  - Rollback specific file (restores only the chosen file).

#### 4.5.2 Public Interface (`src/workspace/SnapshotManager.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <chrono>
#include <mutex>
#include "WorkspaceTypes.h"
#include "PathContainment.h"

namespace aios::workspace {

struct FileSnapshot {
    std::string relative_path;
    std::string content;
    std::string checksum_sha256;
    size_t size_bytes;
    bool existed_at_snapshot;
    std::filesystem::perms permissions;
};

struct CheckpointInfo {
    std::string id;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::string base_git_commit;
    std::unordered_map<std::string, FileSnapshot> files;
    size_t total_size_bytes = 0;
};

struct RollbackSummary {
    bool success = false;
    std::string snapshot_id;
    std::vector<std::string> restored_files;
    std::vector<std::string> deleted_created_files;
    std::vector<std::string> failed_files;
    std::string error_message;
};

struct DiffHunk {
    int old_start;
    int old_lines;
    int new_start;
    int new_lines;
    std::vector<std::string> lines;
};

struct FileDiff {
    std::string old_path;
    std::string new_path;
    bool is_new_file = false;
    bool is_deleted_file = false;
    std::vector<DiffHunk> hunks;
    int additions = 0;
    int deletions = 0;
};

struct UnifiedDiffReport {
    std::string base_snapshot_or_commit;
    std::vector<FileDiff> file_diffs;
    std::string raw_unified_diff;
    int total_additions = 0;
    int total_deletions = 0;
    int total_files_changed = 0;
};

class SnapshotManager {
public:
    SnapshotManager(std::filesystem::path workspace_root, std::shared_ptr<PathContainment> containment);
    ~SnapshotManager() = default;

    // Checkpoint Creation & Management
    WorkspaceResult<CheckpointInfo> createSnapshot(const std::string& snapshot_id, 
                                                  const std::string& description,
                                                  const std::vector<std::string>& files_to_track = {});
    WorkspaceResult<bool> deleteSnapshot(const std::string& snapshot_id);
    std::optional<CheckpointInfo> getSnapshot(const std::string& snapshot_id) const;
    std::vector<CheckpointInfo> listSnapshots() const;
    void clearSnapshots();

    // Rollback Capabilities
    WorkspaceResult<RollbackSummary> rollbackSnapshot(const std::string& snapshot_id);
    WorkspaceResult<bool> rollbackFile(const std::string& snapshot_id, const std::string& relative_path);

    // Diff Generation
    WorkspaceResult<UnifiedDiffReport> generateDiffFromSnapshot(const std::string& snapshot_id) const;
    WorkspaceResult<UnifiedDiffReport> generateDiffBetweenSnapshots(const std::string& old_snapshot_id, 
                                                                    const std::string& new_snapshot_id) const;

private:
    std::filesystem::path workspace_root_;
    std::shared_ptr<PathContainment> containment_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CheckpointInfo> snapshots_;

    std::string computeSha256(const std::string& data) const;
    UnifiedDiffReport computeDiffBetweenContents(const std::string& old_path, const std::string& old_content,
                                                 const std::string& new_path, const std::string& new_content) const;
};

} // namespace aios::workspace
```

---

### 4.6 Module 5: `WorkspaceManager` & `WorkspaceTools` (Unified Orchestration & ToolRegistry)

#### 4.6.1 Public Interface (`src/workspace/WorkspaceManager.h`)

```cpp
#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include "GitWorktree.h"
#include "BranchSandbox.h"
#include "PathContainment.h"
#include "SnapshotManager.h"
#include "WorkspaceTypes.h"

namespace aios::workspace {

class WorkspaceManager {
public:
    static WorkspaceManager& instance();

    bool initialize(const std::filesystem::path& repository_root);
    void shutdown();

    // Subsystem Accessors
    std::shared_ptr<GitWorktree> getWorktreeEngine() const { return worktree_engine_; }
    std::shared_ptr<BranchSandbox> getBranchSandbox() const { return branch_sandbox_; }
    std::shared_ptr<PathContainment> getPathContainment() const { return containment_; }
    std::shared_ptr<SnapshotManager> getSnapshotManager() const { return snapshot_manager_; }

    // High-Level Agent Isolation Operations
    WorkspaceResult<WorktreeInfo> allocateIsolatedAgentWorkspace(const std::string& task_id, 
                                                                 const std::string& base_ref = "HEAD");
    WorkspaceResult<bool> releaseAgentWorkspace(const std::string& task_id, bool merge_changes = false);

private:
    WorkspaceManager() = default;
    ~WorkspaceManager() = default;

    std::filesystem::path root_path_;
    std::shared_ptr<PathContainment> containment_;
    std::shared_ptr<GitWorktree> worktree_engine_;
    std::shared_ptr<BranchSandbox> branch_sandbox_;
    std::shared_ptr<SnapshotManager> snapshot_manager_;
    std::unordered_map<std::string, WorktreeInfo> agent_worktree_map_;
    mutable std::mutex mutex_;
};

} // namespace aios::workspace
```

---

## 5. Part B: Automated Test Generation & Code Diagnostics Engine (`src/testing/`)

### 5.1 Subsystem Architecture & Responsibilities

The `src/testing/` subsystem provides autonomous verification, static code analysis, and closed-loop error remediation for the MINIcodingAgent OS. It consists of six key components:

1. **`TestGenerator`**: AST-driven test synthesis, boundary value edge-case generation, mock and fixture synthesizer.
2. **`DiagnosticsEngine`**: Multi-tier static analyzer, compiler error parser, security vulnerability and code smell detector.
3. **`TestRunner`**: Sandboxed test process execution with timeout/memory enforcement, multi-framework stdout/stderr parser (GTest, pytest, Jest, cargo).
4. **`CoverageAnalyzer`**: Coverage measurement (line, branch, function), deficit locator, coverage gate checker.
5. **`TestingManager`**: Central facade orchestrating test generation, diagnostics, test execution, coverage, and root-cause auto-repair generation.
6. **`TestingTools`**: ToolRegistry adapters (`test_generate`, `test_run`, `diagnostics_scan`, `auto_repair`).

```
+========================================================================================================+
|                                              TestingManager                                            |
|                  (Central Facade, Closed-Loop Testing & Auto-Repair Orchestrator)                      |
+========================================================================================================+
           |                                |                              |                      |
           v                                v                              v                      v
+--------------------+            +--------------------+         +------------------+   +-------------------+
|   TestGenerator    |            | DiagnosticsEngine  |         |    TestRunner    |   | CoverageAnalyzer  |
| - AST Signatures   |            | - AST Rule Scanner |         | - Sandboxed Exec |   | - Line / Branch % |
| - Edge Cases       |            | - Compiler Parsers |         | - GTest, pytest  |   | - Uncovered Spans |
| - Mocks & Fixtures |            | - Linter Bridges   |         | - Jest, cargo    |   | - Gate Thresholds |
+--------------------+            +--------------------+         +------------------+   +-------------------+
           \                                |                              /                      /
            \                               v                             /                      /
             +-----------------------------------------------------------+----------------------+
                                            |
                                            v
                              +---------------------------+
                              | Failure Diagnosis & Fixes |
                              |  (Root-Cause -> Diff Fix) |
                              +---------------------------+
```

---

### 5.2 Module 1: `TestGenerator` (AST-Driven Test & Edge-Case Synthesizer)

#### 5.2.1 Test Synthesis Strategy
1. **Symbol Ingestion**: Receives `CodeSymbol` definitions from `ASTParser` / `RepositoryIndex`.
2. **Signature Analysis**: Analyzes parameter types, return types, and class structure.
3. **Framework Idioms**:
   - C++: Generates `TEST(Suite, Case) { ... }`, `ASSERT_EQ`, `EXPECT_TRUE`, Google Mock `MOCK_METHOD`.
   - Python: Generates `def test_func(): assert ...`, `@pytest.mark.parametrize`, `@pytest.fixture`, `unittest.mock.patch`.
   - JS/TS: Generates `describe('Suite', () => { it('should...', () => { expect(...).toBe(...); }); });`.
4. **Boundary & Edge-Case Generation**:
   - Numeric: `0`, `-1`, `1`, `INT_MAX`, `INT_MIN`, `std::numeric_limits<double>::quiet_NaN()`, `Infinity`.
   - String: `""`, single whitespace `" "`, special symbols (`!@#$%^&*()_+-=[]{}|;':",.<>?/`), multi-byte UTF-8 emojis (`🚀🔥`), 1MB long buffer.
   - Collections: Empty, 1 element, 10,000 elements, duplicate elements, null elements.
   - Pointers/Refs: `nullptr`, dangling ref, cyclic object graph.

#### 5.2.2 Public Interface (`src/testing/TestGenerator.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "TestingTypes.h"
#include "parser/ASTParser.h"

namespace aios::testing {

struct TestGenOptions {
    std::string target_language;  // "cpp", "python", "javascript", "typescript", "rust", "go"
    std::string framework;        // "gtest", "catch2", "pytest", "unittest", "jest", "cargo"
    bool generate_edge_cases = true;
    bool generate_mocks = true;
    bool generate_fixtures = true;
    size_t max_tests_per_function = 5;
    std::string custom_prompt_guidance;
};

struct GeneratedTestCase {
    std::string test_name;
    std::string test_category; // "happy_path", "boundary_value", "error_handling", "concurrency"
    std::string description;
    std::string source_code;
};

struct GeneratedTestSuite {
    std::string file_path;
    std::string target_source_file;
    std::string language;
    std::string framework;
    std::string full_test_file_content;
    std::vector<GeneratedTestCase> test_cases;
    std::vector<std::string> mocked_symbols;
    bool success = true;
    std::string error_message;
};

class TestGenerator {
public:
    TestGenerator() = default;
    ~TestGenerator() = default;

    // Synthesis API
    TestingResult<GeneratedTestSuite> generateTestsForFile(const std::string& source_file_path,
                                                          const std::string& file_content,
                                                          const TestGenOptions& options = {});

    TestingResult<GeneratedTestSuite> generateTestsForSymbol(const CodeSymbol& symbol,
                                                            const std::string& source_file_path,
                                                            const TestGenOptions& options = {});

    // Boundary & Mock Synthesis Helpers
    std::vector<std::string> generateBoundaryValuesForType(const std::string& type_name) const;
    std::string generateMockClass(const CodeSymbol& class_symbol, const std::string& framework) const;

private:
    std::string renderCppGTest(const ParsedFile& pf, const TestGenOptions& opts) const;
    std::string renderPythonPytest(const ParsedFile& pf, const TestGenOptions& opts) const;
    std::string renderJestTypeScript(const ParsedFile& pf, const TestGenOptions& opts) const;
};

} // namespace aios::testing
```

---

### 5.3 Module 2: `DiagnosticsEngine` (Static Analysis & Multi-Tier Code Diagnostics)

#### 5.3.1 Diagnostic Severity & Category Model
- **`DiagnosticSeverity`**: `Fatal`, `Error`, `Warning`, `Information`, `Hint`.
- **`DiagnosticCategory`**: `Syntax`, `Compilation`, `TypeCheck`, `Security`, `ResourceLeak`, `Concurrency`, `CodeSmell`, `Performance`.

#### 5.3.2 Built-In Static AST Rules
1. **Resource Leaks**: `FILE*` without `fclose`, `new` without `delete` or smart pointer, unclosed socket handles.
2. **Concurrency Smells**: Member variable access across threads without mutex or atomic guard, lock acquisition out-of-order.
3. **Security Smells**: `system()` / `popen()` with unsanitized format strings, `strcpy` / `sprintf` buffer overflows, unescaped queries.
4. **Code Smells**: Cyclomatic complexity > 15, function lines > 100, parameter count > 6.

#### 5.3.3 Public Interface (`src/testing/DiagnosticsEngine.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "TestingTypes.h"
#include "parser/ASTParser.h"

namespace aios::testing {

enum class DiagnosticSeverity {
    Fatal,
    Error,
    Warning,
    Information,
    Hint
};

enum class DiagnosticCategory {
    Syntax,
    Compilation,
    TypeCheck,
    Security,
    ResourceLeak,
    Concurrency,
    CodeSmell,
    Performance
};

struct SourceSpan {
    int start_line = 1;
    int start_col = 1;
    int end_line = 1;
    int end_col = 1;
};

struct SuggestedFix {
    std::string description;
    SourceSpan replacement_span;
    std::string replacement_text;
};

struct DiagnosticItem {
    std::string file_path;
    SourceSpan span;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticCategory category = DiagnosticCategory::Compilation;
    std::string rule_id;
    std::string message;
    std::string snippet;
    std::optional<SuggestedFix> fix;
};

struct DiagnosticsReport {
    std::string target_path;
    size_t total_errors = 0;
    size_t total_warnings = 0;
    size_t total_hints = 0;
    std::vector<DiagnosticItem> items;
    bool passed = true; // true if total_errors == 0
};

class DiagnosticsEngine {
public:
    DiagnosticsEngine() = default;
    ~DiagnosticsEngine() = default;

    // Main Scanning API
    TestingResult<DiagnosticsReport> scanFile(const std::string& file_path, const std::string& content);
    TestingResult<DiagnosticsReport> scanDirectory(const std::string& dir_path, const std::vector<std::string>& file_patterns = {});

    // Compiler Error Parsing
    std::vector<DiagnosticItem> parseCompilerOutput(const std::string& output, const std::string& compiler_flavor = "msvc") const;

    // Linter Integration
    std::vector<DiagnosticItem> parseClangTidyJson(const std::string& json_str) const;
    std::vector<DiagnosticItem> parseRuffJson(const std::string& json_str) const;
    std::vector<DiagnosticItem> parseEslintJson(const std::string& json_str) const;

private:
    std::vector<DiagnosticItem> runAstRuleChecks(const ParsedFile& parsed_file, const std::string& content) const;
    void checkResourceLeaks(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
    void checkSecuritySmells(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
    void checkComplexityAndLength(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
};

} // namespace aios::testing
```

---

### 5.4 Module 3: `TestRunner` (Sandboxed Test Process Runner)

#### 5.4.1 Test Execution & Process Isolation
1. Spawns test subprocesses inside the isolated worktree directory.
2. Implements asynchronous timer enforcement (`std::chrono::milliseconds timeout_ms`).
3. Normalizes stdout and stderr into unified `TestExecutionReport`.
4. Parses specific test framework dialects: GoogleTest (`[ RUN ]`, `[ OK ]`, `[ FAILED ]`), pytest (`PASSED`, `FAILED`, failure assertion blocks), Jest (`PASS`, `FAIL`), Cargo (`test ... ok / FAILED`).

#### 5.4.2 Public Interface (`src/testing/TestRunner.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <filesystem>
#include "TestingTypes.h"

namespace aios::testing {

enum class TestStatus {
    Passed,
    Failed,
    Skipped,
    Timeout,
    Crashed
};

struct TestCaseResult {
    std::string suite_name;
    std::string test_name;
    TestStatus status = TestStatus::Passed;
    std::chrono::milliseconds duration{0};
    std::string failure_message;
    std::string failure_file;
    int failure_line = 0;
    std::string stack_trace;
    std::string stdout_output;
};

struct TestExecutionReport {
    std::string command;
    std::filesystem::path working_directory;
    bool success = false;
    size_t total_tests = 0;
    size_t passed_tests = 0;
    size_t failed_tests = 0;
    size_t skipped_tests = 0;
    std::chrono::milliseconds total_duration{0};
    std::vector<TestCaseResult> test_cases;
    std::string raw_stdout;
    std::string raw_stderr;
    int exit_code = 0;
    std::string summary_string;
};

struct TestRunOptions {
    std::filesystem::path working_directory;
    std::chrono::milliseconds timeout{60000};
    std::string filter_expression;
    std::unordered_map<std::string, std::string> environment_vars;
    std::string framework_hint; // "gtest", "pytest", "jest", "cargo", "auto"
};

class TestRunner {
public:
    TestRunner() = default;
    ~TestRunner() = default;

    TestingResult<TestExecutionReport> run(const std::string& test_command_or_executable,
                                          const TestRunOptions& options = {});

    TestExecutionReport parseOutput(const std::string& stdout_str, 
                                    const std::string& stderr_str, 
                                    int exit_code,
                                    const std::string& framework_hint = "auto") const;

private:
    TestExecutionReport parseGTest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
    TestExecutionReport parsePytest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
    TestExecutionReport parseJest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
    TestExecutionReport parseCargo(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
};

} // namespace aios::testing
```

---

### 5.5 Module 4: `CoverageAnalyzer` (Code Coverage Metrics & Deficit Locator)

#### 5.5.1 Public Interface (`src/testing/CoverageAnalyzer.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "TestingTypes.h"

namespace aios::testing {

struct LineRange {
    int start_line;
    int end_line;
};

struct FileCoverage {
    std::string file_path;
    size_t total_lines = 0;
    size_t covered_lines = 0;
    double line_coverage_percent = 0.0;
    size_t total_branches = 0;
    size_t covered_branches = 0;
    double branch_coverage_percent = 0.0;
    size_t total_functions = 0;
    size_t covered_functions = 0;
    double function_coverage_percent = 0.0;
    std::vector<LineRange> uncovered_line_ranges;
};

struct CoverageReport {
    double overall_line_coverage = 0.0;
    double overall_branch_coverage = 0.0;
    double overall_function_coverage = 0.0;
    size_t total_lines = 0;
    size_t covered_lines = 0;
    std::unordered_map<std::string, FileCoverage> files;
    std::string format;
};

struct CoverageGateResult {
    bool passed = false;
    double required_line_percent;
    double actual_line_percent;
    double required_branch_percent;
    double actual_branch_percent;
    std::vector<std::string> deficit_files;
};

class CoverageAnalyzer {
public:
    CoverageAnalyzer() = default;
    ~CoverageAnalyzer() = default;

    TestingResult<CoverageReport> parseLcovInfo(const std::string& lcov_content);
    TestingResult<CoverageReport> parseCoberturaXml(const std::string& xml_content);
    TestingResult<CoverageReport> parseJsonCoverage(const std::string& json_content);

    CoverageGateResult checkThresholds(const CoverageReport& report, 
                                       double min_line_pct = 80.0, 
                                       double min_branch_pct = 70.0) const;
};

} // namespace aios::testing
```

---

### 5.6 Module 5: `TestingManager` & Auto-Repair Recommendation Engine

#### 5.6.1 Closed-Loop Diagnosis & Auto-Repair Workflow
1. When `TestRunner` reports failures (`TestExecutionReport.failed_tests > 0`), `TestingManager::diagnoseFailure` cross-references the failing file and line with `RepositoryIndex` to extract the surrounding AST function body and symbol call graph.
2. Correlates the assertion failure message (e.g. `Expected equality of these values: stats.total_symbols Which is: 1, 3`) with the AST logic.
3. Formulates a structured `DiagnosticDiagnosisReport` containing:
   - Root-cause hypothesis
   - Offending file and line span
   - Suggested surgical patch (unified diff)
4. Communicates the diagnosis to `DebuggerAgent` and logs the failure-and-fix tuple into `KnowledgeGraph` (`BugFix` entity linked via `Fixes` relation to `Symbol`).

#### 5.6.2 Public Interface (`src/testing/TestingManager.h`)

```cpp
#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include "TestingTypes.h"
#include "TestGenerator.h"
#include "DiagnosticsEngine.h"
#include "TestRunner.h"
#include "CoverageAnalyzer.h"
#include "repository/RepositoryIndex.h"

namespace aios::testing {

struct DiagnosticDiagnosisReport {
    bool diagnosed = false;
    std::string failure_summary;
    std::string root_cause_explanation;
    std::string target_file;
    int target_line = 0;
    std::string surrounding_code_context;
    std::string recommended_patch;
    std::vector<std::string> related_symbols;
};

class TestingManager {
public:
    static TestingManager& instance();

    bool initialize(std::shared_ptr<RepositoryIndex> repo_index = nullptr);
    void shutdown();

    // Subsystem Accessors
    std::shared_ptr<TestGenerator> getGenerator() const { return generator_; }
    std::shared_ptr<DiagnosticsEngine> getDiagnostics() const { return diagnostics_; }
    std::shared_ptr<TestRunner> getRunner() const { return runner_; }
    std::shared_ptr<CoverageAnalyzer> getCoverageAnalyzer() const { return coverage_; }

    // High-Level Autonomous Test Operations
    TestingResult<GeneratedTestSuite> generateTestsForPath(const std::string& path, const TestGenOptions& opts = {});
    TestingResult<DiagnosticsReport> runStaticDiagnostics(const std::string& path);
    TestingResult<TestExecutionReport> runTestSuite(const std::string& command, const TestRunOptions& opts = {});

    // Closed-Loop Failure Diagnosis & Auto-Repair
    DiagnosticDiagnosisReport diagnoseFailure(const TestCaseResult& failed_case);
    TestingResult<std::string> generateAutoRepairPatch(const TestCaseResult& failed_case);

private:
    TestingManager() = default;
    ~TestingManager() = default;

    std::shared_ptr<RepositoryIndex> repo_index_;
    std::shared_ptr<TestGenerator> generator_;
    std::shared_ptr<DiagnosticsEngine> diagnostics_;
    std::shared_ptr<TestRunner> runner_;
    std::shared_ptr<CoverageAnalyzer> coverage_;
    mutable std::mutex mutex_;
};

} // namespace aios::testing
```

---

## 6. AIOS Subsystem Integration Architecture

### 6.1 Integration with `TaskGraphExecutor`

The `TaskGraph` DAG supports test generation, diagnostics, and test execution as atomic parallel nodes. The DAG scheduler allocates distinct worktree sandboxes for parallel tasks, preventing file contention.

```
                  [TaskNode 1: PlannerAgent]
                              |
                              v
                 [TaskNode 2: CoderAgent]
                   (Writes src/foo.cpp)
                              |
              +---------------+---------------+
              |                               |
              v                               v
   [TaskNode 3: TestGenerator]   [TaskNode 4: DiagnosticsEngine]
     (Synthesizes test_foo.cpp)     (Static Analysis / Linting)
              |                               |
              +---------------+---------------+
                              |
                              v
                   [TaskNode 5: TestRunner]
                     (Executes test suite)
                              |
                     +--------+--------+
                     |                 |
             (Tests Passed)    (Tests Failed)
                     |                 |
                     v                 v
           [TaskNode 6: Reviewer] [TaskNode 7: Debugger/Auto-Repair]
```

### 6.2 Integration with `MultiAgentOrchestrator`

- **`TesterAgent`**: Invokes `TestingManager::runTestSuite` and `DiagnosticsEngine::scanDirectory`. Emits progress events over `EventBus` (`testing.started`, `testing.completed`).
- **`DebuggerAgent`**: When `TesterAgent` detects failures, `DebuggerAgent` receives `DiagnosticDiagnosisReport` from `TestingManager::diagnoseFailure()`, generates a surgical patch, and passes it to `CoderAgent` for the repair cycle.
- **Git Checkpoints & Rollback**: `Orchestrator` uses `SnapshotManager::createSnapshot()` before coding and `SnapshotManager::rollbackSnapshot()` if tests fail after maximum repair cycles.

### 6.3 Integration with `MemoryManager` & `KnowledgeGraph`

- **`KnowledgeGraph`**: Ingests each resolved test failure as a `BugFix` entity linked to the affected `Symbol` with relation `Fixes`.
- **`VectorStore`**: Stores test failure assertions and diagnostic messages with embeddings. When a new test fails, `MemoryManager::searchSemantic()` retrieves similar historical failure patterns and their verified fixes.

### 6.4 `ToolRegistry` Integration (`WorkspaceTools` & `TestingTools`)

The following tool schemas are registered in `ToolRegistry`:

```json
// Tool: "workspace"
{
  "name": "workspace",
  "category": "Git",
  "operations": ["create_worktree", "remove_worktree", "list_worktrees", "create_snapshot", "rollback_snapshot", "get_diff", "merge_branch"]
}

// Tool: "testing"
{
  "name": "testing",
  "category": "Testing",
  "operations": ["generate_tests", "run_tests", "diagnose_code", "analyze_coverage", "auto_repair"]
}
```

---

## 7. File Layout & CMake Build System Integration

### 7.1 Source Code Directory Layout

```
MINIcodingAgent/
├── src/
│   ├── workspace/
│   │   ├── WorkspaceTypes.h
│   │   ├── PathContainment.h
│   │   ├── PathContainment.cpp
│   │   ├── GitWorktree.h
│   │   ├── GitWorktree.cpp
│   │   ├── BranchSandbox.h
│   │   ├── BranchSandbox.cpp
│   │   ├── SnapshotManager.h
│   │   ├── SnapshotManager.cpp
│   │   ├── WorkspaceManager.h
│   │   ├── WorkspaceManager.cpp
│   │   ├── WorkspaceTools.h
│   │   └── WorkspaceTools.cpp
│   ├── testing/
│   │   ├── TestingTypes.h
│   │   ├── TestGenerator.h
│   │   ├── TestGenerator.cpp
│   │   ├── DiagnosticsEngine.h
│   │   ├── DiagnosticsEngine.cpp
│   │   ├── TestRunner.h
│   │   ├── TestRunner.cpp
│   │   ├── CoverageAnalyzer.h
│   │   ├── CoverageAnalyzer.cpp
│   │   ├── TestingManager.h
│   │   ├── TestingManager.cpp
│   │   ├── TestingTools.h
│   │   └── TestingTools.cpp
```

### 7.2 Unit & Regression Test Suite Layout

```
MINIcodingAgent/
├── tests/
│   ├── test_workspace.cpp       // Worktree lifecycle, path containment, snapshots, rollbacks
│   ├── test_testing_engine.cpp  // Test generator, diagnostics, test runner, coverage, auto-repair
```

### 7.3 `CMakeLists.txt` Updates

The following source files are added to `CORE_SOURCES` in `CMakeLists.txt`:

```cmake
set(CORE_SOURCES
    ...
    # Workspace Subsystem
    src/workspace/PathContainment.cpp
    src/workspace/GitWorktree.cpp
    src/workspace/BranchSandbox.cpp
    src/workspace/SnapshotManager.cpp
    src/workspace/WorkspaceManager.cpp
    src/workspace/WorkspaceTools.cpp

    # Testing & Diagnostics Subsystem
    src/testing/TestGenerator.cpp
    src/testing/DiagnosticsEngine.cpp
    src/testing/TestRunner.cpp
    src/testing/CoverageAnalyzer.cpp
    src/testing/TestingManager.cpp
    src/testing/TestingTools.cpp
)
```

---

## 8. Comprehensive Test & Verification Strategy

### 8.1 Workspace Subsystem Test Matrix (`tests/test_workspace.cpp`)

1. **`PathContainmentTest.PreventsDirectoryTraversal`**: Asserts that `validatePath("../../../etc/passwd")` returns `TraversalViolation`.
2. **`PathContainmentTest.ProtectsGitAndSecretFiles`**: Asserts that writing to `.git/HEAD` or `.env` returns `ProtectedSystemPath`.
3. **`GitWorktreeTest.CreatesAndListsIsolatedWorktree`**: Creates ephemeral worktree, verifies directory exists on disk, asserts porcelain output contains new worktree, removes and prunes.
4. **`BranchSandboxTest.StagesAndCommitsMicroChanges`**: Creates ephemeral branch, stages file, checks commit history SHA.
5. **`SnapshotManagerTest.TransactionalRollbackOnFailure`**: Creates snapshot of 3 files, modifies 2 and deletes 1, calls `rollbackSnapshot()`, verifies exact contents restored.
6. **`SnapshotManagerTest.GeneratesStandardUnifiedDiff`**: Modifies a file, asserts unified diff matches Myers hunk format.

### 8.2 Testing & Diagnostics Engine Test Matrix (`tests/test_testing_engine.cpp`)

1. **`TestGeneratorTest.GeneratesCppGTestFromAST`**: Passes parsed C++ class `MathHelper`, verifies generated GTest code contains `TEST(MathHelperTest, ...)` and assertions.
2. **`TestGeneratorTest.GeneratesPythonPytestWithParametrize`**: Passes parsed Python function, verifies `@pytest.mark.parametrize` and boundary inputs.
3. **`DiagnosticsEngineTest.DetectsResourceLeaksAndSmells`**: Scans C++ file with unclosed file handle, asserts `DiagnosticItem` emitted with `ResourceLeak` category.
4. **`DiagnosticsEngineTest.ParsesCompilerErrorSpans`**: Parses MSVC and GCC compiler error blocks, asserts exact line/column/message captured.
5. **`TestRunnerTest.ParsesGoogleTestOutput`**: Feeds GTest stdout, asserts pass count, fail count, and failing assertion extraction.
6. **`CoverageAnalyzerTest.CalculatesLcovLineAndBranchCoverage`**: Ingests LCOV content, verifies line % and identifies uncovered line ranges.
7. **`TestingManagerTest.DiagnosesFailureAndSuggestsRepair`**: Feeds failed test case, verifies root-cause diagnosis identifies correct function and generates diff patch.

---

## 9. Requirements Traceability Matrix

| Requirement | Source | Design Section | Target Header/Class | Verification Test |
|-------------|--------|----------------|---------------------|-------------------|
| Git Worktree Management | ORIGINAL_REQUEST.md #2 | Section 4.2 | `GitWorktree.h` | `GitWorktreeTest.CreatesAndListsIsolatedWorktree` |
| Multi-Branch Sandbox & Conflict Detection | ORIGINAL_REQUEST.md #2 | Section 4.3 | `BranchSandbox.h` | `BranchSandboxTest.StagesAndCommitsMicroChanges` |
| Path Containment & Security Sandbox | ORIGINAL_REQUEST.md #2 | Section 4.4 | `PathContainment.h` | `PathContainmentTest.PreventsDirectoryTraversal` |
| Snapshotting, Diffs & Rollback | ORIGINAL_REQUEST.md #2 | Section 4.5 | `SnapshotManager.h` | `SnapshotManagerTest.TransactionalRollbackOnFailure` |
| AST-Driven Test Generation | ORIGINAL_REQUEST.md #3 | Section 5.2 | `TestGenerator.h` | `TestGeneratorTest.GeneratesCppGTestFromAST` |
| Code Diagnostics & Static Analysis | ORIGINAL_REQUEST.md #3 | Section 5.3 | `DiagnosticsEngine.h` | `DiagnosticsEngineTest.DetectsResourceLeaksAndSmells` |
| Sandboxed Test Execution & Output Parsing | ORIGINAL_REQUEST.md #3 | Section 5.4 | `TestRunner.h` | `TestRunnerTest.ParsesGoogleTestOutput` |
| Code Coverage Metrics & Thresholds | ORIGINAL_REQUEST.md #3 | Section 5.5 | `CoverageAnalyzer.h` | `CoverageAnalyzerTest.CalculatesLcovLineAndBranchCoverage` |
| Root-Cause Failure Diagnosis & Auto-Repair | ORIGINAL_REQUEST.md #3 | Section 5.6 | `TestingManager.h` | `TestingManagerTest.DiagnosesFailureAndSuggestsRepair` |
| TaskGraph / Orchestrator / Memory Integration | ORIGINAL_REQUEST.md #4 | Section 6.0 | `WorkspaceManager.h`, `TestingManager.h` | `TaskGraphExecutorTest.*`, `OrchestratorTest.*` |
