#include "SpecializedAgents.h"
#include "AgentToolParser.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <regex>

namespace aios {

// ============================================================================
// PlannerAgent
// ============================================================================

PlannerAgent::PlannerAgent(AgentConfig config)
    : Agent(std::move(config)) {
    config_.type = AgentType::Planner;
    config_.capabilities.can_read_files = true;
    config_.capabilities.can_write_files = false;
    config_.capabilities.can_execute_commands = false;
    config_.capabilities.allowed_tools = {"filesystem", "search"};
}

std::string PlannerAgent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) return config_.system_prompt;

    return R"(You are the Lead Software Architect & Planner Agent in AIOS.
Your objective is to decompose a user request into a clear, dependency-ordered execution plan.
Break down the task into distinct steps assigned to specialized agents:
- Researcher: Reads files, analyzes symbols and architecture.
- Coder: Implements changes, edits files, and creates new modules.
- Tester: Compiles and executes test suites.
- Reviewer: Evaluates code quality and security.

Output your plan as a structured JSON object in a ```json code block:
```json
{
  "goal": "Summary of goal",
  "steps": [
    {
      "id": "step_1",
      "title": "Title of step",
      "description": "Details of what to do",
      "assigned_agent_type": "Researcher",
      "dependencies": [],
      "acceptance_criteria": "How to verify"
    },
    {
      "id": "step_2",
      "title": "Implement feature",
      "description": "Code changes needed",
      "assigned_agent_type": "Coder",
      "dependencies": ["step_1"],
      "acceptance_criteria": "Code builds cleanly"
    }
  ]
}
```
)";
}

AgentPlanResult PlannerAgent::generatePlan(const std::string& task_description,
                                          const std::unordered_map<std::string, std::string>& context) {
    AgentPlanResult plan;
    plan.goal = task_description;

    AgentResult result = execute("Create an execution plan for: " + task_description, context);
    plan.raw_plan = result.content;

    if (!result.success) {
        plan.is_valid = false;
        return plan;
    }

    // Try parsing JSON block from output
    try {
        std::regex json_regex(R"(```(?:json)?\s*(\{[\s\S]*?\})\s*```)", std::regex::icase);
        std::smatch match;
        std::string json_str;
        if (std::regex_search(result.content, match, json_regex)) {
            json_str = match[1].str();
        } else {
            json_str = result.content;
        }

        auto j = nlohmann::json::parse(json_str);
        if (j.contains("goal") && j["goal"].is_string()) {
            plan.goal = j["goal"].get<std::string>();
        }
        if (j.contains("steps") && j["steps"].is_array()) {
            for (const auto& s : j["steps"]) {
                AgentPlanStep step;
                step.id = s.value("id", "step_" + std::to_string(plan.steps.size() + 1));
                step.title = s.value("title", "Task Step");
                step.description = s.value("description", "");
                step.assigned_agent_type = s.value("assigned_agent_type", "Coder");
                step.acceptance_criteria = s.value("acceptance_criteria", "");
                if (s.contains("dependencies") && s["dependencies"].is_array()) {
                    for (const auto& d : s["dependencies"]) {
                        step.dependencies.push_back(d.get<std::string>());
                    }
                }
                plan.steps.push_back(step);
            }
            plan.is_valid = !plan.steps.empty();
        }
    } catch (...) {
        // Fallback: create default 4-step plan
        AgentPlanStep r_step{"step_1", "Research codebase", "Investigate files and symbols", "Researcher", {}, "Context gathered"};
        AgentPlanStep c_step{"step_2", "Implement changes", task_description, "Coder", {"step_1"}, "Code written"};
        AgentPlanStep t_step{"step_3", "Verify tests", "Run build and test suite", "Tester", {"step_2"}, "Tests pass"};
        AgentPlanStep v_step{"step_4", "Code review", "Audit diff for quality and bugs", "Reviewer", {"step_3"}, "Review score >= 75"};
        plan.steps = {r_step, c_step, t_step, v_step};
        plan.is_valid = true;
    }

    return plan;
}

// ============================================================================
// ResearcherAgent
// ============================================================================

ResearcherAgent::ResearcherAgent(AgentConfig config)
    : Agent(std::move(config)) {
    config_.type = AgentType::Researcher;
    config_.capabilities.can_read_files = true;
    config_.capabilities.can_write_files = false;
    config_.capabilities.can_execute_commands = false;
    config_.capabilities.can_access_git = true;
    config_.capabilities.allowed_tools = {"filesystem", "search", "lsp", "context"};
}

std::string ResearcherAgent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) return config_.system_prompt;

    return R"(You are the Researcher Agent in AIOS.
Your job is to thoroughly inspect the codebase, locate relevant files, symbols, definitions, and dependencies.
You DO NOT modify files or run destructive commands.
Use read_file, search_files, list_directory, and grep tools to extract exact context.
Summarize all relevant code structures and findings for the Coder agent.
When done, output 'TASK_COMPLETE' followed by your detailed architectural findings.
)";
}

// ============================================================================
// CoderAgent
// ============================================================================

CoderAgent::CoderAgent(AgentConfig config)
    : Agent(std::move(config)) {
    config_.type = AgentType::Coder;
    config_.capabilities.can_read_files = true;
    config_.capabilities.can_write_files = true;
    config_.capabilities.can_execute_commands = false;
    config_.capabilities.can_access_git = true;
    config_.capabilities.allowed_tools = {"filesystem", "search"};
}

std::string CoderAgent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) return config_.system_prompt;

    return R"(You are the Expert Coder Agent in AIOS.
Your job is to write production-grade, bug-free, clean code according to requirements and research findings.
Use the filesystem tool (operation: 'write' or 'mkdir') to create and update files.
Ensure proper error handling, modern C++ idioms, correct headers, and zero syntax errors.
When all code changes are written, output 'TASK_COMPLETE' with a summary of modified files.
)";
}

// ============================================================================
// TesterAgent
// ============================================================================

TesterAgent::TesterAgent(AgentConfig config)
    : Agent(std::move(config)) {
    config_.type = AgentType::Tester;
    config_.capabilities.can_read_files = true;
    config_.capabilities.can_write_files = false;
    config_.capabilities.can_execute_commands = true;
    config_.capabilities.can_run_tests = true;
    config_.capabilities.allowed_tools = {"terminal", "testing", "filesystem"};
}

std::string TesterAgent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) return config_.system_prompt;

    return R"(You are the Tester Agent in AIOS.
Your job is to build the codebase and run test suites.
Use the terminal / testing tools to execute build commands and test binaries.
Parse stdout/stderr to identify test failures, compiler errors, or memory issues.
Output a clear summary of all passed/failed tests and compiler diagnostics.
When finished, output 'TASK_COMPLETE'.
)";
}

// ============================================================================
// ReviewerAgent
// ============================================================================

ReviewerAgent::ReviewerAgent(AgentConfig config)
    : Agent(std::move(config)) {
    config_.type = AgentType::Reviewer;
    config_.capabilities.can_read_files = true;
    config_.capabilities.can_write_files = false;
    config_.capabilities.can_execute_commands = false;
    config_.capabilities.can_access_git = true;
    config_.capabilities.allowed_tools = {"filesystem", "git", "search"};
}

std::string ReviewerAgent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) return config_.system_prompt;

    return R"(You are the Code Reviewer & Security Auditor Agent in AIOS.
Your job is to critically review code diffs, verify correctness, adherence to conventions, edge-case coverage, and security vulnerabilities.
Provide a quality score between 0 and 100, where >= 75 is passing.
Format your review result in a ```json code block:
```json
{
  "overall_score": 85,
  "passed": true,
  "critical_issues": [],
  "suggestions": ["Add unit tests for edge case X"],
  "comments": "Code is well structured and follows modern standards."
}
```
)";
}

ReviewScore ReviewerAgent::evaluateChanges(const std::string& original_task,
                                          const std::string& diff_or_code,
                                          const std::string& test_results) {
    ReviewScore score;
    std::string prompt = "Review the following code changes for task: " + original_task + "\n\n"
                         "Code / Diff:\n" + diff_or_code + "\n\n"
                         "Test Results:\n" + test_results;

    AgentResult result = execute(prompt);

    if (!result.success) {
        score.overall_score = 0;
        score.passed = false;
        score.comments = "Review execution failed: " + result.error_message;
        return score;
    }

    try {
        std::regex json_regex(R"(```(?:json)?\s*(\{[\s\S]*?\})\s*```)", std::regex::icase);
        std::smatch match;
        std::string json_str;
        if (std::regex_search(result.content, match, json_regex)) {
            json_str = match[1].str();
        } else {
            json_str = result.content;
        }

        auto j = nlohmann::json::parse(json_str);
        score.overall_score = j.value("overall_score", 80);
        score.passed = j.value("passed", score.overall_score >= 75);
        score.comments = j.value("comments", result.content);

        if (j.contains("critical_issues") && j["critical_issues"].is_array()) {
            for (const auto& item : j["critical_issues"]) {
                score.critical_issues.push_back(item.get<std::string>());
            }
        }
        if (j.contains("suggestions") && j["suggestions"].is_array()) {
            for (const auto& item : j["suggestions"]) {
                score.suggestions.push_back(item.get<std::string>());
            }
        }
    } catch (...) {
        bool has_errors = result.content.find("error") != std::string::npos || 
                          result.content.find("FAIL") != std::string::npos;
        score.overall_score = has_errors ? 50 : 85;
        score.passed = score.overall_score >= 75;
        score.comments = result.content;
    }

    return score;
}

// ============================================================================
// DebuggerAgent
// ============================================================================

DebuggerAgent::DebuggerAgent(AgentConfig config)
    : Agent(std::move(config)) {
    config_.type = AgentType::Debugger;
    config_.capabilities.can_read_files = true;
    config_.capabilities.can_write_files = true;
    config_.capabilities.can_execute_commands = true;
    config_.capabilities.allowed_tools = {"filesystem", "terminal", "search"};
}

std::string DebuggerAgent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) return config_.system_prompt;

    return R"(You are the Debugger Agent in AIOS.
Your job is to analyze compiler diagnostics, failed test logs, assertion crashes, and source code to identify root causes.
Formulate minimal, surgical patches to fix the issue without introducing side effects.
Apply the fix using file write/edit tools or output precise correction instructions for the Coder agent.
When the fix has been applied, output 'TASK_COMPLETE'.
)";
}

std::string DebuggerAgent::diagnoseAndFix(const std::string& original_task,
                                         const std::string& error_logs,
                                         const std::string& relevant_code) {
    std::string prompt = "Debug and fix error for task: " + original_task + "\n\n"
                         "Error Diagnostics / Test Failures:\n" + error_logs + "\n\n"
                         "Relevant Code:\n" + relevant_code;

    AgentResult result = execute(prompt);
    return result.content;
}

} // namespace aios
