#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <optional>
#include "taskgraph/TaskGraph.h"

namespace aios {

class ModelProvider;
class ModelRouter;
class EventBus;

enum class PlanStrategyType {
    ChainOfThought,
    TreeOfThought,
    Reflection
};

struct TreeBranchCandidate {
    std::string branch_id;
    std::string rationale;
    TaskGraph graph;
    double score = 0.0; // 0.0 to 100.0
    std::string evaluation_notes;

    TreeBranchCandidate() = default;
    TreeBranchCandidate(std::string id, std::string rat, TaskGraph g, double s, std::string notes)
        : branch_id(std::move(id)), rationale(std::move(rat)), graph(std::move(g)), score(s), evaluation_notes(std::move(notes)) {}
};

struct PlannerStats {
    size_t total_plans_created = 0;
    size_t cot_plans = 0;
    size_t tot_plans = 0;
    size_t reflection_plans = 0;
    size_t executed_graphs = 0;
    size_t successful_graphs = 0;
    double average_score = 0.0;
};

/**
 * @brief Advanced Planner: Multi-strategy reasoning and DAG TaskGraph construction
 */
class Planner {
public:
    Planner();
    ~Planner();

    bool initialize();
    void shutdown();
    void stop();

    // Dependencies
    void setModelProvider(std::shared_ptr<ModelProvider> provider);
    void setModelRouter(std::shared_ptr<ModelRouter> router);
    void setEventBus(std::shared_ptr<EventBus> event_bus);

    // Multi-Strategy Planning
    TaskGraph createPlan(const std::string& goal, 
                         PlanStrategyType strategy = PlanStrategyType::ChainOfThought,
                         const std::unordered_map<std::string, std::string>& context = {});

    TaskGraph planWithCoT(const std::string& goal,
                          const std::unordered_map<std::string, std::string>& context = {});

    TaskGraph planWithToT(const std::string& goal,
                          size_t num_branches = 3,
                          const std::unordered_map<std::string, std::string>& context = {});

    TaskGraph planWithReflection(const std::string& goal,
                                 const std::unordered_map<std::string, std::string>& context = {});

    // Plan Execution
    TaskGraphExecutionSummary executePlan(TaskGraph& graph, TaskNodeHandler handler = nullptr);

    // Stats
    PlannerStats getStats() const;

private:
    std::string callModel(const std::string& prompt, const std::string& system_prompt = "");
    TaskGraph parsePlanJsonToGraph(const std::string& json_str, const std::string& goal);
    double evaluateBranchViability(const std::string& goal, const TaskGraph& graph, std::string& notes);

    mutable std::mutex mutex_;
    std::shared_ptr<ModelProvider> model_provider_;
    std::shared_ptr<ModelRouter> model_router_;
    std::shared_ptr<EventBus> event_bus_;
    std::unique_ptr<TaskGraphExecutor> executor_;
    PlannerStats stats_{};
};

} // namespace aios
