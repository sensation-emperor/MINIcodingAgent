#pragma once

#include "agents.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace aios {

struct AgentPlanStep {
    std::string id;
    std::string title;
    std::string description;
    std::string assigned_agent_type; // e.g. "Researcher", "Coder", "Tester", "Reviewer"
    std::vector<std::string> dependencies;
    std::string acceptance_criteria;
};

struct AgentPlanResult {
    std::string goal;
    std::vector<AgentPlanStep> steps;
    bool is_valid = false;
    std::string raw_plan;
};

struct ReviewScore {
    int overall_score = 0; // 0-100
    bool passed = false;
    std::vector<std::string> critical_issues;
    std::vector<std::string> suggestions;
    std::string comments;
};

/**
 * @brief Planner Agent: Analyzes user tasks and produces structured task graphs.
 */
class PlannerAgent : public Agent {
public:
    explicit PlannerAgent(AgentConfig config);
    AgentPlanResult generatePlan(const std::string& task_description,
                                 const std::unordered_map<std::string, std::string>& context = {});

protected:
    std::string buildSystemPrompt() const override;
};

/**
 * @brief Researcher Agent: Gathers context, reads files, and inspects symbols without mutating code.
 */
class ResearcherAgent : public Agent {
public:
    explicit ResearcherAgent(AgentConfig config);

protected:
    std::string buildSystemPrompt() const override;
};

/**
 * @brief Coder Agent: Implements features and modifies source code.
 */
class CoderAgent : public Agent {
public:
    explicit CoderAgent(AgentConfig config);

protected:
    std::string buildSystemPrompt() const override;
};

/**
 * @brief Tester Agent: Runs tests and verifies build status.
 */
class TesterAgent : public Agent {
public:
    explicit TesterAgent(AgentConfig config);

protected:
    std::string buildSystemPrompt() const override;
};

/**
 * @brief Reviewer Agent: Audits code quality, diffs, and security.
 */
class ReviewerAgent : public Agent {
public:
    explicit ReviewerAgent(AgentConfig config);
    ReviewScore evaluateChanges(const std::string& original_task,
                                const std::string& diff_or_code,
                                const std::string& test_results = "");

protected:
    std::string buildSystemPrompt() const override;
};

/**
 * @brief Debugger Agent: Pinpoints root causes and formulates targeted patches.
 */
class DebuggerAgent : public Agent {
public:
    explicit DebuggerAgent(AgentConfig config);
    std::string diagnoseAndFix(const std::string& original_task,
                               const std::string& error_logs,
                               const std::string& relevant_code);

protected:
    std::string buildSystemPrompt() const override;
};

} // namespace aios
