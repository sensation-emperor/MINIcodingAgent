# Implementation & Orchestration Plan

## Objectives
Build and integrate the Full Developer Suite for MINIcodingAgent (AIOS) with:
1. Interactive Terminal REPL & Slash Command Shell (`src/cli/`)
2. Sandboxed Git Worktree & Multi-Branch Workspace Isolation (`src/workspace/`)
3. Automated Test Generation & Code Diagnostics Engine (`src/testing/`)
4. Regression & Multi-Subsystem Verification across Orchestrator, TaskGraphExecutor, ModelRouter, MemoryManager, ensuring 100% test pass rate on `aios_tests`.

## Orchestration Strategy (Project Pattern - Dual Track)

### Phase 0: Survey & Architecture Discovery
- Dispatch 3 Explorers / Spec Miners:
  - Explorer 1: Inspect existing codebase structure, dependencies, existing `aios_tests`, entry points, and interfaces in Orchestrator, TaskGraphExecutor, ModelRouter, MemoryManager.
  - Explorer 2 / Spec Miner 1: Investigate CLI / REPL requirements, command parsing, slash commands, shell UI, streaming/interaction models.
  - Explorer 3 / Spec Miner 2: Investigate Git Worktree isolation and Test Generation / Diagnostics engine specs.
- Synthesize into `PROJECT.md` (Feature Inventory, Architecture, Milestones, Interface Contracts, Code Layout).

### Parallel Tracks
- **E2E Testing Track**:
  - Spawn E2E Testing Orchestrator to build comprehensive opaque-box test suites (Tier 1-4) across CLI, Workspace, Testing engines, and full AIOS integration.
  - Generates `TEST_INFRA.md` and `TEST_READY.md`.

- **Implementation Track**:
  - **Milestone 1**: Terminal REPL & Slash Command Shell (`src/cli/`)
  - **Milestone 2**: Sandboxed Git Worktree & Multi-Branch Workspace Isolation (`src/workspace/`)
  - **Milestone 3**: Automated Test Generation & Code Diagnostics Engine (`src/testing/`)
  - **Milestone 4**: Final Integration, Full Test Suite Execution (Tiers 1-4 + aios_tests), Adversarial Coverage Hardening (Tier 5).

### Gate & Quality Assurance
- Worker implementations strictly audited by Forensic Auditor (`teamwork_preview_auditor`).
- Multi-perspective Reviewers (`teamwork_preview_reviewer`) & Challengers (`teamwork_preview_challenger`).
- Strict AND gating on builds, reviews, and clean forensic audits.
