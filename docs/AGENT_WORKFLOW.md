# Agent Workflow Guide - UnnatSystems Brahma Coder

## Overview

This guide explains how to use the multi-agent orchestration system in UnnatSystems Brahma Coder. The system employs specialized AI agents that work together to complete complex software development tasks.

---

## Agent Types

### 1. PlannerAgent

**Role:** Task decomposition and dependency ordering

**Capabilities:**
- Breaks complex requests into executable steps
- Creates Directed Acyclic Graph (DAG) of tasks
- Identifies prerequisites and parallel opportunities
- Estimates resource requirements

**When it runs:** First step in any complex task

**Example:**
```
User Request: "Add user authentication to the API"

Planner Output:
1. Analyze existing auth framework [ResearcherAgent]
2. Review database schema [ResearcherAgent]
3. Design token-based auth flow [PlannerAgent]
4. Implement JWT middleware [CoderAgent]
5. Add login/logout endpoints [CoderAgent]
6. Write integration tests [TesterAgent]
7. Security review [ReviewerAgent]
```

---

### 2. ResearcherAgent

**Role:** Codebase exploration and information gathering

**Capabilities:**
- Read-only filesystem access
- Symbol lookup via AST index
- Documentation search
- Dependency analysis
- Git history inspection

**Tools Used:**
- `filesystem_read`
- `search_symbols`
- `read_documentation`
- `git_log`
- `context_query`

**Constraints:**
- Cannot modify files
- Cannot execute commands
- Must report findings with citations

**Example:**
```
Task: "Find all usages of UserRepository"

Researcher Output:
Found 14 references:
- src/services/AuthService.cpp:42
- src/controllers/UserController.cpp:87
- src/tests/test_auth.cpp:23
...
Call graph shows 3 direct callers, 5 indirect
```

---

### 3. CoderAgent

**Role:** Code generation and modification

**Capabilities:**
- Surgical code edits
- New file creation
- Refactoring
- Import management
- Documentation updates

**Tools Used:**
- `write_file`
- `replace_in_file`
- `create_file`
- `format_code`
- `update_imports`

**Safety Features:**
- All changes tracked in diff viewer
- Requires user approval before commit
- Automatic backup snapshots

**Example:**
```
Task: "Implement JWT token generation"

Coder Output:
Modified: src/auth/JWTManager.cpp
+ #include <jwt-cpp/jwt.h>
+ 
+ std::string JWTManager::generateToken(const User& user) {
+     return jwt::create()
+         .set_issuer("brahma-coder")
+         .set_subject(user.id)
+         .set_issued_at(std::chrono::system_clock::now())
+         .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24))
+         .sign(jwt::algorithm::hs256{secret_key});
+ }

Created: src/auth/JWTManager.h
[full header file content]
```

---

### 4. TesterAgent

**Role:** Test generation and execution

**Capabilities:**
- Unit test synthesis
- Integration test creation
- Mock object generation
- Test execution
- Coverage analysis
- Failure diagnosis

**Tools Used:**
- `generate_tests`
- `run_tests`
- `analyze_coverage`
- `create_mocks`
- `diagnose_failure`

**Supported Frameworks:**
- C++: GoogleTest, Catch2
- Python: pytest, unittest
- JavaScript/TypeScript: Jest, Mocha
- Rust: built-in test framework
- Go: testing package

**Example:**
```
Task: "Write tests for JWTManager"

Tester Output:
Generated: src/tests/test_jwt_manager.cpp

Tests:
✓ TokenGeneration_ValidUser_ReturnsValidToken
✓ TokenGeneration_EmptyUser_ThrowsException
✓ TokenValidation_ValidToken_ReturnsTrue
✓ TokenValidation_ExpiredToken_ReturnsFalse
✓ TokenValidation_WrongSecret_ReturnsFalse

Coverage: 94% lines, 87% branches

All tests passed in 1.23s
```

---

### 5. ReviewerAgent

**Role:** Quality assurance and security auditing

**Capabilities:**
- Code quality scoring (0-100)
- Security vulnerability detection
- Performance issue identification
- Best practice compliance
- Edge case analysis
- Maintainability assessment

**Tools Used:**
- `static_analysis`
- `security_scan`
- `performance_profiler`
- `code_smell_detector`
- `complexity_analyzer`

**Review Categories:**
1. **Correctness:** Does it work as intended?
2. **Security:** Any vulnerabilities?
3. **Performance:** Efficiency concerns?
4. **Maintainability:** Clean and understandable?
5. **Testing:** Adequate test coverage?
6. **Documentation:** Properly documented?

**Example:**
```
Task: "Review authentication implementation"

Reviewer Output:
Overall Score: 87/100

Strengths:
✓ Proper input validation
✓ Secure token generation
✓ Good error handling
✓ Comprehensive tests

Issues Found:
⚠️ Warning: Secret key loaded from environment (line 34)
  → Recommendation: Use secret management service
  
⚠️ Warning: No rate limiting on login endpoint
  → Recommendation: Add throttling to prevent brute force
  
ℹ️ Info: Consider adding refresh token rotation
  → Enhancement for improved security

Security Scan: No critical vulnerabilities detected
```

---

### 6. DebuggerAgent

**Role:** Root cause analysis and patch generation

**Capabilities:**
- Stack trace analysis
- Error localization
- Hypothesis generation
- Surgical patch creation
- Regression testing

**Tools Used:**
- `parse_stack_trace`
- `locate_bug`
- `generate_patch`
- `verify_fix`
- `regression_test`

**Debugging Process:**
1. Analyze error message and stack trace
2. Locate source of failure in AST
3. Generate hypotheses for root cause
4. Create minimal reproducer
5. Develop surgical fix
6. Verify with targeted tests
7. Check for regressions

**Example:**
```
Task: "Fix null pointer exception in UserService"

Debugger Output:
Error: NullReferenceException at UserService.cpp:127

Root Cause Analysis:
- getUserById() returns nullptr for missing users
- Caller doesn't check return value
- Dereference causes crash

Hypothesis: Missing null check after database query

Generated Patch:
src/services/UserService.cpp:
- auto user = db->getUserById(id);
+ auto user = db->getUserById(id);
+ if (!user) {
+     throw UserNotFoundException(id);
+ }
  return user->getName();

Verification:
✓ Null pointer exception resolved
✓ Appropriate exception thrown
✓ All existing tests pass
✓ New edge case covered
```

---

## Orchestration Modes

### Mode 1: Orchestrator (Full Pipeline)

**Use Case:** Complex multi-step tasks

**Pipeline:**
```
User Request
    ↓
PlannerAgent → Creates DAG
    ↓
ResearcherAgent → Gathers context
    ↓
Git Snapshot → Creates checkpoint
    ↓
CoderAgent → Implements solution
    ↓
TesterAgent → Validates
    ↓
[If failures] DebuggerAgent → Fixes
    ↓
ReviewerAgent → Quality check
    ↓
User Approval → Commits changes
```

**How to Use:**
1. Select "Orchestrator" mode in GUI
2. Describe your goal
3. Watch progress in Task Graph View
4. Review diffs before accepting

---

### Mode 2: Single Agent

**Use Case:** Focused, specific tasks

**Available Agents:**
- PlannerAgent only
- ResearcherAgent only
- CoderAgent only
- TesterAgent only
- ReviewerAgent only
- DebuggerAgent only

**How to Use:**
1. Select agent from dropdown
2. Provide task-specific input
3. Get direct response

**Example:**
```
Mode: ReviewerAgent only
Input: "Review this pull request for security issues"
Output: Security audit report
```

---

### Mode 3: DAG Planner (Visual)

**Use Case:** Understanding and modifying execution plans

**Features:**
- Visual DAG editor
- Manual task reordering
- Parallel lane assignment
- Dependency modification
- Custom task insertion

**How to Use:**
1. Select "DAG Planner" mode
2. Load or create task graph
3. Drag nodes to rearrange
4. Connect/disconnect dependencies
5. Execute modified plan

---

## Workflow Examples

### Example 1: Feature Implementation

**Goal:** Add password reset functionality

**Steps:**
```
1. PlannerAgent decomposes:
   - Research existing email system
   - Design reset token flow
   - Implement token generation
   - Create reset endpoint
   - Add email template
   - Write tests
   - Security review

2. ResearcherAgent explores:
   - EmailService integration
   - Database schema for tokens
   - Existing auth patterns

3. CoderAgent implements:
   - PasswordResetService class
   - /api/reset-password endpoint
   - Email template
   - Token expiration logic

4. TesterAgent validates:
   - Happy path tests
   - Edge cases (expired tokens, invalid tokens)
   - Integration tests

5. ReviewerAgent audits:
   - Security: token entropy, timing attacks
   - Correctness: all flows covered
   - Performance: no N+1 queries

6. User reviews diffs and approves
```

---

### Example 2: Bug Fix Loop

**Goal:** Fix failing production test

**Steps:**
```
1. User provides error log

2. DebuggerAgent analyzes:
   - Parses stack trace
   - Identifies failing assertion
   - Locates root cause

3. DebuggerAgent generates patch:
   - Minimal surgical fix
   - Preserves existing behavior

4. TesterAgent verifies:
   - Runs previously failing test
   - Checks for regressions
   - Confirms coverage maintained

5. If still failing:
   - Loop back to step 2
   - Generate new hypothesis
   - Try alternative fix

6. Once passing:
   - ReviewerAgent checks quality
   - User approves merge
```

---

### Example 3: Code Review Request

**Goal:** Get feedback on refactored module

**Steps:**
```
1. User selects files and requests review

2. ReviewerAgent analyzes:
   - Code quality metrics
   - Complexity scores
   - Test coverage
   - Documentation completeness

3. ReviewerAgent reports:
   - Overall score: 91/100
   - Strengths identified
   - Suggested improvements
   - Potential issues

4. User decides:
   - Accept suggestions
   - Request CoderAgent to implement fixes
   - Merge as-is
```

---

## Advanced Patterns

### Pattern 1: Parallel Research

Spawn multiple ResearcherAgents simultaneously:
```
Researcher A: Explore authentication module
Researcher B: Explore authorization module
Researcher C: Review database schema
    ↓
Merge findings → Planner creates unified plan
```

---

### Pattern 2: Iterative Refinement

Multiple passes with increasing scrutiny:
```
Pass 1: CoderAgent → Draft implementation
Pass 2: ReviewerAgent → Initial feedback
Pass 3: CoderAgent → Address feedback
Pass 4: TesterAgent → Comprehensive testing
Pass 5: ReviewerAgent → Final audit
```

---

### Pattern 3: A/B Testing Solutions

Generate multiple approaches:
```
CoderAgent A: Implementation with pattern X
CoderAgent B: Implementation with pattern Y
    ↓
TesterAgent → Benchmark both
ReviewerAgent → Compare trade-offs
    ↓
Select best approach
```

---

## Monitoring & Observability

### In GUI

**Task Graph View:**
- Real-time node status updates
- Progress bars per task
- Dependency visualization
- Error indicators

**Chat View:**
- Agent reasoning traces (collapsible)
- Tool call logs
- Token usage statistics
- Timeline of events

**Terminal Widget:**
- Build output
- Test execution logs
- Command results

---

### Metrics Dashboard

**Tracked Metrics:**
- Tasks completed per hour
- Average task duration
- Success/failure rates by agent
- Token consumption per task
- Cost breakdown by provider

---

## Troubleshooting

### Agent Not Responding

**Symptoms:**
- Task stuck in "Running" state
- No output in chat

**Solutions:**
1. Check model connection status
2. Verify task complexity vs model capability
3. Increase timeout in settings
4. Try switching to more capable model
5. Break task into smaller subtasks

---

### Too Many Retries

**Symptoms:**
- DebuggerAgent looping repeatedly
- Same error persisting

**Solutions:**
1. Manually inspect error message
2. Provide additional context to agent
3. Switch to cloud model for better reasoning
4. Break down the problem further
5. Consider manual intervention

---

### Unexpected Changes

**Symptoms:**
- CoderAgent modified wrong files
- Unintended side effects

**Prevention:**
1. Always review diffs before accepting
2. Enable permission prompts
3. Use workspace snapshots
4. Set clear boundaries in task description
5. Start with read-only research phase

---

## Best Practices

### For Complex Tasks

✅ **DO:**
- Let PlannerAgent decompose first
- Allow ResearcherAgent to gather context
- Review intermediate results
- Run comprehensive tests
- Get security review for sensitive changes

❌ **DON'T:**
- Skip research phase
- Rush to code without understanding
- Accept changes without review
- Ignore test failures
- Bypass security checks

---

### For Quick Tasks

✅ **DO:**
- Use single agent mode
- Be specific in requests
- Review critical changes
- Keep context focused

❌ **DON'T:**
- Over-engineer simple fixes
- Run full pipeline for typos
- Forget to save afterwards

---

### For Learning

✅ **DO:**
- Read agent reasoning traces
- Understand why changes suggested
- Compare different agent approaches
- Experiment with different models
- Save successful workflows

---

## Configuration

### Agent-Specific Settings

```json
{
  "agents": {
    "planner": {
      "max_steps": 20,
      "parallel_research": true,
      "require_citations": true
    },
    "coder": {
      "auto_format": true,
      "backup_before_edit": true,
      "max_changes_per_task": 50
    },
    "tester": {
      "coverage_threshold": 80,
      "timeout_seconds": 300,
      "retry_flaky_tests": 2
    },
    "reviewer": {
      "min_score_for_approval": 75,
      "security_scan_enabled": true,
      "performance_check_enabled": true
    },
    "debugger": {
      "max_iterations": 5,
      "generate_reproducer": true,
      "regression_check": true
    }
  }
}
```

---

*Last updated: 2026-09-09*
