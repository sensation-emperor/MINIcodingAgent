# Original User Request

## 2026-08-22T02:22:37Z

Build and integrate the Full Developer Suite for MINIcodingAgent (AIOS) consisting of an Interactive Terminal REPL Client, Sandboxed Git Worktree Workspace Isolation, and Automated Test Case Generation & Diagnostics Engine.

Working directory: c:\Users\kaush\Downloads\MINIcodingAgent
Integrity mode: development

## Requirements

### R1. Interactive Terminal REPL & Slash Command Shell
Implement an interactive CLI/REPL terminal interface (`src/cli/`) providing:
- Multi-turn conversational loop with live streaming token printing.
- Built-in slash commands: `/plan`, `/research`, `/code`, `/test`, `/review`, `/diff`, `/rollback`, `/memory`, `/benchmark`, `/exit`.
- Interactive confirmation and approval prompts for dangerous or modifying tool actions.
- Rich terminal formatting with colors, agent role tags, and spinner animations.

### R2. Sandboxed Git Worktree & Multi-Branch Workspace Isolation
Implement a sandboxed workspace isolation subsystem (`src/workspace/`) providing:
- Creation and management of isolated temporary Git worktrees and task branches.
- Safe execution environment preventing dirty state on user's primary working branch.
- Atomic checkpoint creation, diff inspection between sandboxes, and zero-risk rollback mechanisms.

### R3. Automated Test Generation & Code Diagnostics Engine
Implement an automated test generator and semantic diagnostics runner (`src/testing/`) providing:
- Automated generation of unit test cases (GoogleTest / C++ test scaffolds) targeting un-covered symbols extracted by the AST parser.
- Extraction and classification of compiler diagnostics, linker errors, and runtime failures into structured remediation actions for the DebuggerAgent.

### R4. Regression & Multi-Subsystem Verification
Ensure all new subsystems integrate seamlessly with `Orchestrator`, `TaskGraphExecutor`, `ModelRouter`, `MemoryManager`, and existing test suites without regressions.

## Acceptance Criteria

### Interactive CLI & Shell
- [ ] Interactive REPL binary builds and properly handles chat turns, token streaming, and slash commands.
- [ ] Tool execution approval prompts require explicit user confirmation before applying dangerous actions.

### Workspace & Sandboxing
- [ ] Workspace isolation safely spins up and tears down isolated branch/worktree contexts without polluting working directories.
- [ ] Rollback reverts uncommitted modifications atomically upon failure.

### Testing & Diagnostics
- [ ] Automated test generator produces valid test cases for target functions with compilation validation.
- [ ] Diagnostic parser extracts line numbers, error codes, and suggested fixes from compiler output.
- [ ] All test targets (`aios_tests`) compile cleanly and pass 100% of test cases.
