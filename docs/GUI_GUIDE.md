# GUI Guide - UnnatSystems Brahma Coder

## Overview

The UnnatSystems Brahma Coder GUI is a Qt 6-based desktop application that provides a visual interface for orchestrating AI-powered coding agents. The design follows a **mobile skeuomorphism-glassmorphism** aesthetic with floating islands, translucent surfaces, and tactile depth.

## Design System

### Theme Colors
- **Primary Gradient:** Coral Rose (`#FF6B9D`) to Sunset Orange (`#FF9A56`)
- **Background:** Dark translucent surfaces with blur effects
- **Light Source:** Virtual light from top-left for consistent shadows
- **Border Radius:** 
  - Floating islands: ≥24px
  - Buttons: ≥20px  
  - Containers: ≥14px

### Core Components

#### 1. Floating Island Navigation Bar
Located at the top of the application window.
- Capsule-shaped with margins (12-16px)
- Zero-overlap touch targets
- View switching buttons
- Status indicators

#### 2. Chat View
Main conversational interface for interacting with AI agents.
- Translucent message bubbles
- Agent avatar pills
- Collapsible thought/reasoning boxes
- Live token streaming display
- Markdown rendering
- Syntax-highlighted code blocks

**Usage:**
```
1. Type your request in the input field at bottom
2. Select agent mode (Orchestrator/Single/DAG)
3. Choose model from dropdown
4. Press Enter or click Send button
5. Watch real-time token streaming
6. Review agent reasoning in collapsible sections
```

#### 3. Task Graph View
Interactive DAG (Directed Acyclic Graph) visualization.
- Real-time task progress monitoring
- Node states: Pending → Ready → Running → Completed/Failed
- Dependency edges visualization
- Click nodes for details
- Parallel execution lanes

**Usage:**
```
1. Navigate to Task Graph tab
2. View current execution plan
3. Click nodes to see task details
4. Monitor progress in real-time
5. Identify bottlenecks or failures
```

#### 4. Diff Viewer
Code modification inspector.
- Side-by-side or unified diff view
- Syntax highlighting
- Line-level change tracking
- Accept/Reject buttons
- Multi-file navigation

**Usage:**
```
1. Agent suggests code changes
2. Diff viewer opens automatically
3. Review each change highlighted
4. Click Accept to apply or Reject to discard
5. Navigate between multiple files
```

#### 5. Terminal Widget
Integrated terminal console.
- Translucent background
- Build output display
- Command execution results
- Test runner output
- Scrollback buffer

**Usage:**
```
1. Navigate to Terminal tab
2. View build/test output
3. Copy error messages
4. Track command execution
```

#### 6. Command Pill Widget
Bottom control capsule.
- **Mode Toggle:** Orchestrator / Single Agent / DAG Planner
- **Model Selector:** Dropdown for choosing AI model
- **CTA Button:** Circular primary action button
- Status LED indicator

#### 7. Settings Dialog (Planned)
Configuration interface.
- Model provider credentials
- Permission policies
- Keyboard shortcuts
- Theme preferences
- Storage locations

## Getting Started

### Launching the Application

```bash
# After building
./build/src/gui/brahma_coder_gui

# Or via CMake
cd build
cmake --build . --target brahma_coder_gui
./brahma_coder_gui
```

### First-Time Setup

1. **Welcome Screen** appears on first launch
2. Configure your preferred AI provider:
   - Local: LM Studio (recommended for privacy)
   - Cloud: OpenAI, Anthropic, etc.
3. Set workspace directory
4. Configure permissions
5. Complete setup wizard

### Basic Workflow

#### Scenario 1: Code Generation
```
1. Open Chat View
2. Type: "Create a REST API endpoint for user authentication"
3. Select: Orchestrator mode
4. Choose model: Llama-3-8B (local) or GPT-4 (cloud)
5. Press Send
6. Watch as Planner decomposes task
7. Researcher explores codebase
8. Coder generates implementation
9. Tester creates tests
10. Review diff and accept changes
```

#### Scenario 2: Bug Fixing
```
1. Paste error message in Chat
2. Select: DebuggerAgent mode
3. Agent analyzes stack trace
4. Locates root cause via AST
5. Suggests surgical patch
6. Review in Diff Viewer
7. Accept to apply fix
8. Tests run automatically
```

#### Scenario 3: Code Review
```
1. Select file in explorer
2. Right-click → "Request Review"
3. ReviewerAgent analyzes:
   - Code quality score (0-100)
   - Security vulnerabilities
   - Performance issues
   - Edge cases
4. Review report in Chat
5. Apply suggested improvements
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New conversation |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save current file |
| `Ctrl+F` | Find in conversation |
| `Ctrl+Enter` | Send message |
| `Esc` | Cancel operation |
| `F5` | Refresh task graph |
| `Ctrl+Shift+T` | Run tests |
| `Ctrl+Shift+B` | Build project |
| `Ctrl+,` | Open settings |

## Model Selection Guide

### When to Use Local Models
- ✅ Privacy-sensitive code
- ✅ Rapid iteration during development
- ✅ Large context requirements
- ✅ Cost-conscious usage
- ✅ Offline work

**Recommended:** LM Studio with Llama-3-70B or Mixtral-8x7B

### When to Use Cloud Models
- ✅ Complex reasoning tasks
- ✅ Highest accuracy needed
- ✅ Specialized knowledge required
- ✅ Local models unavailable
- ✅ Time-critical production fixes

**Recommended:** GPT-4 Turbo or Claude 3 Opus

## Troubleshooting

### GUI Issues

**Problem:** Application won't start
```
Solution:
1. Check Qt 6 installation: `qt6 --version`
2. Verify dependencies: `ldd brahma_coder_gui`
3. Check logs: `~/.unnat/brahma_coder.log`
```

**Problem:** Blank chat window
```
Solution:
1. Restart the application
2. Clear cache: `rm -rf ~/.unnat/cache`
3. Reinstall if persists
```

**Problem:** Token streaming not working
```
Solution:
1. Verify model provider is running
2. Check network connectivity
3. Enable debug logging in settings
4. Try alternative model
```

### Model Connection Issues

**Problem:** Cannot connect to LM Studio
```
Solution:
1. Start LM Studio
2. Load a model
3. Enable local server (port 1234)
4. In Brahma Coder: Settings → Providers → LM Studio → Test Connection
```

**Problem:** Cloud API authentication failed
```
Solution:
1. Verify API key in settings
2. Check key permissions
3. Ensure billing is active
4. Test with curl: `curl https://api.openai.com/v1/models -H "Authorization: Bearer YOUR_KEY"`
```

## Advanced Features

### Custom Workflows (Planned)
Build visual automation pipelines:
1. Drag-and-drop task nodes
2. Connect with dependency arrows
3. Configure conditions and loops
4. Save as reusable template

### Plugin System (Planned)
Extend functionality:
1. Navigate to Plugins tab
2. Browse marketplace
3. Install with one click
4. Configure plugin settings

### Metrics Dashboard (Planned)
Monitor performance:
- Token usage per conversation
- Model latency over time
- Cost tracking
- Success/failure rates

## Best Practices

### For Optimal Performance
1. Use local models for routine tasks
2. Keep context focused and relevant
3. Break complex requests into steps
4. Review diffs before accepting
5. Run tests after modifications

### For Security
1. Enable permission prompts for dangerous operations
2. Review network access requests
3. Audit generated code for vulnerabilities
4. Use sandboxed execution when available
5. Keep API keys secure

### For Collaboration
1. Share workflow templates with team
2. Document custom agent configurations
3. Use conversation branching for experiments
4. Export important conversations
5. Maintain project memory across sessions

## Updates & Changelog

See `CHANGELOG.md` for version history and new features.

---

*Last updated: 2026-09-09*
