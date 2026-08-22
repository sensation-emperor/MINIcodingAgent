#pragma once

#include "agents.h"
#include "SpecializedAgents.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace aios {

struct OrchestratorConfig {
    int max_repair_cycles = 3;
    int review_pass_score = 75;
    bool auto_git_checkpoint = true;
    bool rollback_on_unrecoverable_failure = true;
    std::string default_model_name = "local-small-llm";
};

struct WorkflowResult {
    bool success = false;
    std::string task;
    AgentPlanResult plan;
    std::string summary;
    std::string final_diff;
    ReviewScore review;
    std::vector<std::string> errors;
    std::chrono::milliseconds total_time{0};
    int total_iterations = 0;
    int repair_cycles_used = 0;
};

class MultiAgentOrchestrator {
public:
    explicit MultiAgentOrchestrator(OrchestratorConfig config = {});
    ~MultiAgentOrchestrator();

    // Dependencies
    void setModelProvider(std::shared_ptr<ModelProvider> provider);
    void setToolRegistry(std::shared_ptr<ToolRegistry> registry);
    void setEventBus(std::shared_ptr<EventBus> event_bus);

    // Callbacks for live progress
    using ProgressCallback = std::function<void(const std::string& step, 
                                                const std::string& agent_name, 
                                                const std::string& status, 
                                                const std::string& details)>;
    void onProgress(ProgressCallback callback);

    // Main workflow execution
    WorkflowResult runWorkflow(const std::string& task_description,
                               const std::unordered_map<std::string, std::string>& context = {});

    // Individual agent accessors
    std::shared_ptr<PlannerAgent> getPlanner();
    std::shared_ptr<ResearcherAgent> getResearcher();
    std::shared_ptr<CoderAgent> getCoder();
    std::shared_ptr<TesterAgent> getTester();
    std::shared_ptr<ReviewerAgent> getReviewer();
    std::shared_ptr<DebuggerAgent> getDebugger();

    void cancel();

private:
    void initializeAgents();
    void emitProgress(const std::string& step, const std::string& agent, const std::string& status, const std::string& details);
    std::string createGitCheckpoint();
    bool rollbackGitCheckpoint(const std::string& checkpoint_id);

    OrchestratorConfig config_;
    std::shared_ptr<ModelProvider> model_provider_;
    std::shared_ptr<ToolRegistry> tool_registry_;
    std::shared_ptr<EventBus> event_bus_;
    ProgressCallback progress_callback_;

    std::shared_ptr<PlannerAgent> planner_;
    std::shared_ptr<ResearcherAgent> researcher_;
    std::shared_ptr<CoderAgent> coder_;
    std::shared_ptr<TesterAgent> tester_;
    std::shared_ptr<ReviewerAgent> reviewer_;
    std::shared_ptr<DebuggerAgent> debugger_;

    std::atomic<bool> cancelled_{false};
    mutable std::mutex mutex_;
};

} // namespace aios
