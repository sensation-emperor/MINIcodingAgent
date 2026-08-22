## 2026-08-22T02:23:30Z
<USER_REQUEST>
You are Spec Miner 2 for the MINIcodingAgent (AIOS) Full Developer Suite project.
Your working directory is: c:\Users\kaush\Downloads\MINIcodingAgent\.agents\spec_miner_workspace_testing
Workspace root: c:\Users\kaush\Downloads\MINIcodingAgent
MANDATORY: Read ORIGINAL_REQUEST.md at c:\Users\kaush\Downloads\MINIcodingAgent\ORIGINAL_REQUEST.md before doing anything else.

Objective:
Investigate and specify the complete functional and technical specifications for:
"2. Sandboxed Git Worktree & Multi-Branch Workspace Isolation (src/workspace/)"
"3. Automated Test Generation & Code Diagnostics Engine (src/testing/)"

Cover in detail:
Part A - Workspace Isolation (`src/workspace/`):
1. Git worktree management (create, switch, delete, isolate, list worktrees).
2. Branch isolation & sandbox lifecycle: ephemeral branch creation, patch staging, merge/rebase, conflict detection, cleanup.
3. Path containment and security sandbox: preventing directory traversal, path validation, isolation boundaries.
4. File snapshotting, diff generation, rollback capabilities.
5. Exact public APIs, classes, and file layout for `src/workspace/`.

Part B - Test Generation & Diagnostics Engine (`src/testing/`):
1. Test generation engine: automated unit/integration test synthesis for Python/codebases, edge-case generation, mock generation.
2. Code Diagnostics & static analysis: AST parsing, syntax validation, linter/type-check integration, vulnerability/smell detection.
3. Test execution & coverage runner: isolated test execution, failure diagnosis, auto-repair suggestions.
4. Integration with AIOS TaskGraphExecutor, Orchestrator, and MemoryManager.
5. Exact public APIs, classes, and file layout for `src/testing/`.

Output Requirements:
- Write your findings to `c:\Users\kaush\Downloads\MINIcodingAgent\.agents\spec_miner_workspace_testing\survey_workspace_testing_spec.md`.
- Write your handoff report to `c:\Users\kaush\Downloads\MINIcodingAgent\.agents\spec_miner_workspace_testing\handoff.md`.
- Report completion via `send_message` with path references.
</USER_REQUEST>
