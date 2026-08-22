#include "Orchestrator.h"
#include "AgentToolParser.h"
#include "events/EventBus.h"
#include "tools/ToolRegistry.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>

namespace aios {

MultiAgentOrchestrator::MultiAgentOrchestrator(OrchestratorConfig config)
    : config_(std::move(config)) {
}

MultiAgentOrchestrator::~MultiAgentOrchestrator() {
    cancel();
}

void MultiAgentOrchestrator::setModelProvider(std::shared_ptr<ModelProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    model_provider_ = provider;
    if (planner_) planner_->setModelProvider(provider);
    if (researcher_) researcher_->setModelProvider(provider);
    if (coder_) coder_->setModelProvider(provider);
    if (tester_) tester_->setModelProvider(provider);
    if (reviewer_) reviewer_->setModelProvider(provider);
    if (debugger_) debugger_->setModelProvider(provider);
}

void MultiAgentOrchestrator::setToolRegistry(std::shared_ptr<ToolRegistry> registry) {
    std::lock_guard<std::mutex> lock(mutex_);
    tool_registry_ = registry;
    if (planner_) planner_->setToolRegistry(registry);
    if (researcher_) researcher_->setToolRegistry(registry);
    if (coder_) coder_->setToolRegistry(registry);
    if (tester_) tester_->setToolRegistry(registry);
    if (reviewer_) reviewer_->setToolRegistry(registry);
    if (debugger_) debugger_->setToolRegistry(registry);
}

void MultiAgentOrchestrator::setEventBus(std::shared_ptr<EventBus> event_bus) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_bus_ = event_bus;
    if (planner_) planner_->setEventBus(event_bus);
    if (researcher_) researcher_->setEventBus(event_bus);
    if (coder_) coder_->setEventBus(event_bus);
    if (tester_) tester_->setEventBus(event_bus);
    if (reviewer_) reviewer_->setEventBus(event_bus);
    if (debugger_) debugger_->setEventBus(event_bus);
}

void MultiAgentOrchestrator::onProgress(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_callback_ = std::move(callback);
}

void MultiAgentOrchestrator::cancel() {
    cancelled_ = true;
    if (planner_) planner_->cancel();
    if (researcher_) researcher_->cancel();
    if (coder_) coder_->cancel();
    if (tester_) tester_->cancel();
    if (reviewer_) reviewer_->cancel();
    if (debugger_) debugger_->cancel();
}

void MultiAgentOrchestrator::emitProgress(const std::string& step, const std::string& agent, const std::string& status, const std::string& details) {
    if (progress_callback_) {
        progress_callback_(step, agent, status, details);
    }

    if (event_bus_) {
        nlohmann::json j;
        j["step"] = step;
        j["agent"] = agent;
        j["status"] = status;
        j["details"] = details;
        event_bus_->publish("orchestrator.step_progress", j.dump());
    }

    LOG_INFO("[Orchestrator] Step: {} | Agent: {} | Status: {} | Details: {}", step, agent, status, details);
}

void MultiAgentOrchestrator::initializeAgents() {
    if (planner_ && researcher_ && coder_ && tester_ && reviewer_ && debugger_) {
        return;
    }

    AgentConfig base_config;
    base_config.model_name = config_.default_model_name;

    AgentConfig p_cfg = base_config;
    p_cfg.id = "planner_01";
    p_cfg.name = "Lead Planner";
    planner_ = std::make_shared<PlannerAgent>(p_cfg);

    AgentConfig r_cfg = base_config;
    r_cfg.id = "researcher_01";
    r_cfg.name = "Codebase Researcher";
    researcher_ = std::make_shared<ResearcherAgent>(r_cfg);

    AgentConfig c_cfg = base_config;
    c_cfg.id = "coder_01";
    c_cfg.name = "Core Coder";
    coder_ = std::make_shared<CoderAgent>(c_cfg);

    AgentConfig t_cfg = base_config;
    t_cfg.id = "tester_01";
    t_cfg.name = "Quality Tester";
    tester_ = std::make_shared<TesterAgent>(t_cfg);

    AgentConfig rev_cfg = base_config;
    rev_cfg.id = "reviewer_01";
    rev_cfg.name = "Code Reviewer";
    reviewer_ = std::make_shared<ReviewerAgent>(rev_cfg);

    AgentConfig d_cfg = base_config;
    d_cfg.id = "debugger_01";
    d_cfg.name = "Diagnostic Debugger";
    debugger_ = std::make_shared<DebuggerAgent>(d_cfg);

    // Wire dependencies
    if (model_provider_) {
        planner_->setModelProvider(model_provider_);
        researcher_->setModelProvider(model_provider_);
        coder_->setModelProvider(model_provider_);
        tester_->setModelProvider(model_provider_);
        reviewer_->setModelProvider(model_provider_);
        debugger_->setModelProvider(model_provider_);
    }

    if (tool_registry_) {
        planner_->setToolRegistry(tool_registry_);
        researcher_->setToolRegistry(tool_registry_);
        coder_->setToolRegistry(tool_registry_);
        tester_->setToolRegistry(tool_registry_);
        reviewer_->setToolRegistry(tool_registry_);
        debugger_->setToolRegistry(tool_registry_);
    }

    if (event_bus_) {
        planner_->setEventBus(event_bus_);
        researcher_->setEventBus(event_bus_);
        coder_->setEventBus(event_bus_);
        tester_->setEventBus(event_bus_);
        reviewer_->setEventBus(event_bus_);
        debugger_->setEventBus(event_bus_);
    }
}

std::string MultiAgentOrchestrator::createGitCheckpoint() {
    if (!config_.auto_git_checkpoint || !tool_registry_) return "";

    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string checkpoint_id = "checkpoint_" + std::to_string(now);

    std::unordered_map<std::string, std::string> params;
    params["operation"] = "stash";
    params["message"] = checkpoint_id;
    tool_registry_->executeTool("git", params);

    return checkpoint_id;
}

bool MultiAgentOrchestrator::rollbackGitCheckpoint(const std::string& checkpoint_id) {
    if (checkpoint_id.empty() || !tool_registry_) return false;

    emitProgress("Rollback", "Orchestrator", "IN_PROGRESS", "Restoring git checkpoint " + checkpoint_id);
    std::unordered_map<std::string, std::string> params;
    params["operation"] = "stash";
    params["subcommand"] = "pop";
    auto res = tool_registry_->executeTool("git", params);
    return res.success;
}

std::shared_ptr<PlannerAgent> MultiAgentOrchestrator::getPlanner() { initializeAgents(); return planner_; }
std::shared_ptr<ResearcherAgent> MultiAgentOrchestrator::getResearcher() { initializeAgents(); return researcher_; }
std::shared_ptr<CoderAgent> MultiAgentOrchestrator::getCoder() { initializeAgents(); return coder_; }
std::shared_ptr<TesterAgent> MultiAgentOrchestrator::getTester() { initializeAgents(); return tester_; }
std::shared_ptr<ReviewerAgent> MultiAgentOrchestrator::getReviewer() { initializeAgents(); return reviewer_; }
std::shared_ptr<DebuggerAgent> MultiAgentOrchestrator::getDebugger() { initializeAgents(); return debugger_; }

WorkflowResult MultiAgentOrchestrator::runWorkflow(const std::string& task_description,
                                                  const std::unordered_map<std::string, std::string>& context) {
    auto start_time = std::chrono::steady_clock::now();
    cancelled_ = false;

    initializeAgents();

    WorkflowResult result;
    result.task = task_description;

    emitProgress("Workflow", "Orchestrator", "STARTED", "Beginning task: " + task_description);

    // ========================================================================
    // Step 1: Planning
    // ========================================================================
    if (cancelled_) { result.errors.push_back("Workflow cancelled"); return result; }
    emitProgress("Planning", "PlannerAgent", "STARTED", "Decomposing task into subtasks...");
    
    result.plan = planner_->generatePlan(task_description, context);
    result.total_iterations += 1;

    if (!result.plan.is_valid || result.plan.steps.empty()) {
        emitProgress("Planning", "PlannerAgent", "FAILED", "Failed to generate valid plan");
        result.errors.push_back("Planning failed");
        return result;
    }
    emitProgress("Planning", "PlannerAgent", "COMPLETED", "Created " + std::to_string(result.plan.steps.size()) + " plan steps");

    // ========================================================================
    // Step 2: Codebase Research
    // ========================================================================
    if (cancelled_) { result.errors.push_back("Workflow cancelled"); return result; }
    emitProgress("Research", "ResearcherAgent", "STARTED", "Analyzing repository context...");

    std::unordered_map<std::string, std::string> research_context = context;
    research_context["goal"] = result.plan.goal;
    AgentResult research_result = researcher_->execute("Research relevant files and symbols for task: " + task_description, research_context);
    result.total_iterations += research_result.iterations_used;
    emitProgress("Research", "ResearcherAgent", "COMPLETED", "Context collected");

    // ========================================================================
    // Step 3: Git Checkpoint
    // ========================================================================
    std::string checkpoint_id = createGitCheckpoint();

    // ========================================================================
    // Step 4: Coding Implementation
    // ========================================================================
    if (cancelled_) { result.errors.push_back("Workflow cancelled"); return result; }
    emitProgress("Coding", "CoderAgent", "STARTED", "Implementing code modifications...");

    std::unordered_map<std::string, std::string> coder_context = context;
    coder_context["research_findings"] = research_result.content;
    coder_context["plan"] = result.plan.raw_plan;

    AgentResult coder_result = coder_->execute("Implement the solution for: " + task_description, coder_context);
    result.total_iterations += coder_result.iterations_used;

    if (!coder_result.success) {
        emitProgress("Coding", "CoderAgent", "FAILED", coder_result.error_message);
        result.errors.push_back("Coder error: " + coder_result.error_message);
    } else {
        emitProgress("Coding", "CoderAgent", "COMPLETED", "Code changes applied");
    }

    // ========================================================================
    // Step 5: Testing & Debug Feedback Loop
    // ========================================================================
    bool tests_passed = false;
    int repair_cycle = 0;
    std::string last_test_output;

    while (repair_cycle <= config_.max_repair_cycles && !cancelled_) {
        emitProgress("Testing", "TesterAgent", "STARTED", "Running test and build validation (Cycle " + std::to_string(repair_cycle) + ")...");
        
        AgentResult test_result = tester_->execute("Run build and test suite for recent changes", context);
        result.total_iterations += test_result.iterations_used;
        last_test_output = test_result.content;

        if (test_result.success && test_result.content.find("FAIL") == std::string::npos && 
            test_result.content.find("Error:") == std::string::npos) {
            tests_passed = true;
            emitProgress("Testing", "TesterAgent", "COMPLETED", "All tests and builds passed");
            break;
        }

        // Test or build failed -> Trigger DebuggerAgent
        repair_cycle++;
        result.repair_cycles_used = repair_cycle;

        if (repair_cycle > config_.max_repair_cycles) {
            emitProgress("Testing", "TesterAgent", "FAILED", "Max repair cycles exceeded");
            result.errors.push_back("Tests failed after " + std::to_string(config_.max_repair_cycles) + " repair cycles: " + last_test_output);
            break;
        }

        emitProgress("Debugging", "DebuggerAgent", "STARTED", "Analyzing failure diagnostics (Cycle " + std::to_string(repair_cycle) + ")...");
        std::string debug_fix = debugger_->diagnoseAndFix(task_description, test_result.content, coder_result.content);
        
        // Feed debug fix back to CoderAgent
        emitProgress("Coding", "CoderAgent", "STARTED", "Applying patch from Debugger...");
        std::unordered_map<std::string, std::string> patch_context = context;
        patch_context["debug_fix"] = debug_fix;
        patch_context["last_errors"] = test_result.content;
        coder_->execute("Apply patch to resolve failure: " + debug_fix, patch_context);
    }

    // Rollback if unrecoverable
    if (!tests_passed && config_.rollback_on_unrecoverable_failure && !checkpoint_id.empty()) {
        rollbackGitCheckpoint(checkpoint_id);
    }

    // ========================================================================
    // Step 6: Code Review
    // ========================================================================
    if (!cancelled_) {
        emitProgress("Review", "ReviewerAgent", "STARTED", "Auditing code changes and diff...");
        result.review = reviewer_->evaluateChanges(task_description, coder_result.content, last_test_output);
        emitProgress("Review", "ReviewerAgent", "COMPLETED", "Review Score: " + std::to_string(result.review.overall_score) + "/100 (Passed: " + (result.review.passed ? "true" : "false") + ")");
    }

    // ========================================================================
    // Finalize Result
    // ========================================================================
    auto end_time = std::chrono::steady_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.success = tests_passed && result.review.passed && result.errors.empty();
    result.summary = "Completed task: " + task_description + " | Review Score: " + 
                     std::to_string(result.review.overall_score) + " | Repair Cycles: " + 
                     std::to_string(result.repair_cycles_used);

    emitProgress("Workflow", "Orchestrator", result.success ? "COMPLETED" : "FINISHED_WITH_WARNINGS", result.summary);

    return result;
}

} // namespace aios
