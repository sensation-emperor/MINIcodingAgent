#include "planner/planner.h"
#include "providers/ModelProvider.h"
#include "providers/ModelRouter.h"
#include "events/EventBus.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

namespace aios {

Planner::Planner() 
    : executor_(std::make_unique<TaskGraphExecutor>()) {}

Planner::~Planner() = default;

bool Planner::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("Initializing Advanced Planner with CoT, ToT, and Reflection strategies...");
    return true;
}

void Planner::shutdown() {
    stop();
}

void Planner::stop() {
    if (executor_) {
        executor_->cancel();
    }
}

void Planner::setModelProvider(std::shared_ptr<ModelProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    model_provider_ = provider;
}

void Planner::setModelRouter(std::shared_ptr<ModelRouter> router) {
    std::lock_guard<std::mutex> lock(mutex_);
    model_router_ = router;
}

void Planner::setEventBus(std::shared_ptr<EventBus> event_bus) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_bus_ = event_bus;
    if (executor_) {
        executor_->setEventBus(event_bus);
    }
}

std::string Planner::callModel(const std::string& prompt, const std::string& system_prompt) {
    std::vector<Message> msgs;
    if (!system_prompt.empty()) {
        msgs.push_back({"system", system_prompt});
    }
    msgs.push_back({"user", prompt});

    if (model_router_) {
        auto resp = model_router_->route(AgentType::Planner, msgs);
        if (resp.success) return resp.content;
    } else if (model_provider_) {
        auto resp = model_provider_->chat(msgs);
        if (resp.success) return resp.content;
    }

    // Default rule-based fallback response
    return R"(```json
{
  "nodes": [
    {"id": "research", "title": "Research Codebase", "description": "Inspect files", "assigned_agent_type": 1, "dependencies": []},
    {"id": "code", "title": "Write Implementation", "description": "Apply edits", "assigned_agent_type": 2, "dependencies": ["research"]},
    {"id": "test", "title": "Run Test Suite", "description": "Execute tests", "assigned_agent_type": 3, "dependencies": ["code"]}
  ]
}
```)";
}

TaskGraph Planner::parsePlanJsonToGraph(const std::string& text, const std::string& goal) {
    TaskGraph graph(goal);
    try {
        std::regex json_regex(R"(```(?:json)?\s*(\{[\s\S]*?\})\s*```)", std::regex::icase);
        std::smatch match;
        std::string json_str;
        if (std::regex_search(text, match, json_regex)) {
            json_str = match[1].str();
        } else {
            json_str = text;
        }

        auto j = nlohmann::json::parse(json_str);
        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& nj : j["nodes"]) {
                TaskNode node;
                node.id = nj.value("id", "task_" + std::to_string(graph.size() + 1));
                node.title = nj.value("title", "Task");
                node.description = nj.value("description", "");
                node.assigned_agent_type = static_cast<AgentType>(nj.value("assigned_agent_type", 2));
                if (nj.contains("dependencies") && nj["dependencies"].is_array()) {
                    for (const auto& d : nj["dependencies"]) {
                        node.dependencies.push_back(d.get<std::string>());
                    }
                }
                graph.addNode(node);
            }
        }
    } catch (...) {
        // Fallback default 3-node linear graph
        TaskNode n1{"n1", "Research", "Research files", AgentType::Researcher, "", {}, {}, {}, "", "", TaskNodeState::Pending, "", 0, 2};
        TaskNode n2{"n2", "Code", "Implement changes", AgentType::Coder, "", {"n1"}, {}, {}, "", "", TaskNodeState::Pending, "", 0, 2};
        TaskNode n3{"n3", "Test", "Verify build and tests", AgentType::Tester, "", {"n2"}, {}, {}, "", "", TaskNodeState::Pending, "", 0, 2};
        graph.addNode(n1);
        graph.addNode(n2);
        graph.addNode(n3);
    }
    return graph;
}

TaskGraph Planner::createPlan(const std::string& goal, 
                             PlanStrategyType strategy,
                             const std::unordered_map<std::string, std::string>& context) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.total_plans_created++;

    switch (strategy) {
        case PlanStrategyType::TreeOfThought:
            stats_.tot_plans++;
            return planWithToT(goal, 3, context);
        case PlanStrategyType::Reflection:
            stats_.reflection_plans++;
            return planWithReflection(goal, context);
        case PlanStrategyType::ChainOfThought:
        default:
            stats_.cot_plans++;
            return planWithCoT(goal, context);
    }
}

TaskGraph Planner::planWithCoT(const std::string& goal,
                              const std::unordered_map<std::string, std::string>& /*context*/) {
    LOG_INFO("Planner: Generating Chain-of-Thought (CoT) Plan for: {}", goal);

    std::string system_prompt = 
        "You are an expert software planning agent.\n"
        "Generate a Directed Acyclic Graph (DAG) of tasks in a ```json code block.\n"
        "Each node has 'id', 'title', 'description', 'assigned_agent_type' (1=Researcher, 2=Coder, 3=Tester, 4=Reviewer, 8=Debugger), and 'dependencies'.";

    std::string prompt = "Goal: " + goal + "\nDecompose this goal into a DAG task graph.";
    std::string model_res = callModel(prompt, system_prompt);

    return parsePlanJsonToGraph(model_res, goal);
}

double Planner::evaluateBranchViability(const std::string& goal, const TaskGraph& graph, std::string& notes) {
    if (graph.empty()) return 0.0;
    if (graph.hasCycle()) return 0.0;

    double score = 70.0;
    // Reward balanced dependencies and inclusion of research & testing
    bool has_researcher = false;
    bool has_tester = false;
    for (const auto& node : graph.getAllNodes()) {
        if (node.assigned_agent_type == AgentType::Researcher) has_researcher = true;
        if (node.assigned_agent_type == AgentType::Tester) has_tester = true;
    }
    if (has_researcher) score += 15.0;
    if (has_tester) score += 15.0;

    notes = "Evaluated graph with " + std::to_string(graph.size()) + " nodes. Score: " + std::to_string(score);
    return score;
}

TaskGraph Planner::planWithToT(const std::string& goal,
                              size_t num_branches,
                              const std::unordered_map<std::string, std::string>& context) {
    LOG_INFO("Planner: Generating Tree-of-Thought (ToT) with {} candidate branches for: {}", num_branches, goal);

    std::vector<TreeBranchCandidate> candidates;

    // Branch 1: Incremental / Minimalist
    {
        std::string prompt = "Goal: " + goal + "\nStrategy: Minimalist, surgical incremental change.";
        TaskGraph g = parsePlanJsonToGraph(callModel(prompt), goal);
        std::string notes;
        double score = evaluateBranchViability(goal, g, notes);
        candidates.push_back({"branch_minimal", "Incremental surgical modifications", g, score, notes});
    }

    // Branch 2: Modular / Comprehensive
    {
        std::string prompt = "Goal: " + goal + "\nStrategy: Comprehensive modular architecture with full testing.";
        TaskGraph g = parsePlanJsonToGraph(callModel(prompt), goal);
        std::string notes;
        double score = evaluateBranchViability(goal, g, notes);
        candidates.push_back({"branch_modular", "Full modular architecture", g, score, notes});
    }

    // Branch 3: Conservative / Defensive
    if (num_branches >= 3) {
        std::string prompt = "Goal: " + goal + "\nStrategy: Defensive implementation with thorough validation & rollback checkpoints.";
        TaskGraph g = parsePlanJsonToGraph(callModel(prompt), goal);
        std::string notes;
        double score = evaluateBranchViability(goal, g, notes);
        candidates.push_back({"branch_defensive", "Defensive with safety checkpoints", g, score, notes});
    }

    // Select branch with highest score
    auto best_it = std::max_element(candidates.begin(), candidates.end(), 
        [](const TreeBranchCandidate& a, const TreeBranchCandidate& b) {
            return a.score < b.score;
        });

    LOG_INFO("Planner: Selected ToT Branch [{}] with score {}", best_it->branch_id, best_it->score);
    return best_it->graph;
}

TaskGraph Planner::planWithReflection(const std::string& goal,
                                     const std::unordered_map<std::string, std::string>& context) {
    LOG_INFO("Planner: Generating Reflection-Refined Plan for: {}", goal);

    // Step 1: Draft initial plan
    TaskGraph draft = planWithCoT(goal, context);

    // Step 2: Self-Critique & Refine
    std::string critique_prompt = 
        "Critique and refine this execution graph for goal: " + goal + "\n"
        "Draft Graph JSON:\n" + draft.toJsonString() + "\n\n"
        "Ensure all edge cases, missing file prerequisites, and verification steps are covered.";

    std::string refined_res = callModel(critique_prompt, "You are a master code architect reviewing execution graphs.");
    TaskGraph refined_graph = parsePlanJsonToGraph(refined_res, goal);

    if (refined_graph.empty()) {
        return draft;
    }
    return refined_graph;
}

TaskGraphExecutionSummary Planner::executePlan(TaskGraph& graph, TaskNodeHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.executed_graphs++;

    if (handler) {
        executor_->setNodeHandler(handler);
    }

    auto summary = executor_->execute(graph);
    if (summary.success) {
        stats_.successful_graphs++;
    }
    return summary;
}

PlannerStats Planner::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace aios
