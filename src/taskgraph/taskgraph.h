#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>
#include "agents/agents.h"

namespace aios {

class EventBus;

enum class TaskNodeState {
    Pending,
    Ready,
    Running,
    Completed,
    Failed,
    Skipped,
    RolledBack
};

struct TaskNode {
    std::string id;
    std::string title;
    std::string description;
    AgentType assigned_agent_type = AgentType::Generic;
    std::string tool_action;
    std::vector<std::string> dependencies;  // Prerequisite node IDs
    std::vector<std::string> dependents;    // Successor node IDs
    std::unordered_map<std::string, std::string> input_params;
    std::string output_data;
    std::string error_message;
    TaskNodeState state = TaskNodeState::Pending;
    std::string checkpoint_id;
    int retry_count = 0;
    int max_retries = 2;
    std::chrono::milliseconds execution_time{0};
    int topological_level = 0;
};

/**
 * @brief Directed Acyclic Graph (DAG) for task decomposition and execution
 */
class TaskGraph {
public:
    TaskGraph() = default;
    explicit TaskGraph(const std::string& goal);

    TaskGraph(const TaskGraph& other) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        goal_ = other.goal_;
        nodes_ = other.nodes_;
    }

    TaskGraph& operator=(const TaskGraph& other) {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            goal_ = other.goal_;
            nodes_ = other.nodes_;
        }
        return *this;
    }

    TaskGraph(TaskGraph&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex_);
        goal_ = std::move(other.goal_);
        nodes_ = std::move(other.nodes_);
    }

    TaskGraph& operator=(TaskGraph&& other) noexcept {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            goal_ = std::move(other.goal_);
            nodes_ = std::move(other.nodes_);
        }
        return *this;
    }

    const std::string& getGoal() const { return goal_; }
    void setGoal(const std::string& goal) { goal_ = goal; }

    // Node management
    bool addNode(const TaskNode& node);
    bool removeNode(const std::string& node_id);
    bool addDependency(const std::string& from_node_id, const std::string& to_node_id);
    
    std::optional<TaskNode> getNode(const std::string& node_id) const;
    TaskNode* getNodeRef(const std::string& node_id);
    std::vector<TaskNode> getAllNodes() const;
    size_t size() const;
    bool empty() const;

    // DAG Validation & Topological Sorting
    bool hasCycle() const;
    bool hasCycles() const { return hasCycle(); }
    std::vector<std::string> getTopologicalOrder() const;
    std::vector<std::vector<std::string>> getExecutionLevels() const;

    // Dynamic Execution Helpers
    std::vector<std::string> getReadyNodes() const;
    bool areDependenciesCompleted(const std::string& node_id) const;
    void reset();

    // Export & Debug
    std::string toDotFormat() const;
    std::string toJsonString() const;
    static TaskGraph fromJsonString(const std::string& json_str);

private:
    std::string goal_;
    std::unordered_map<std::string, TaskNode> nodes_;
    mutable std::mutex mutex_;
};

struct TaskExecutionResult {
    bool success = false;
    std::string output;
    std::string error;
};

using TaskNodeHandler = std::function<TaskExecutionResult(TaskNode& node)>;

struct TaskGraphExecutorConfig {
    size_t max_concurrency = 4;
    bool rollback_on_node_failure = true;
    bool stop_on_critical_failure = true;
    std::chrono::milliseconds task_timeout{60000};
};

struct TaskGraphExecutionSummary {
    bool success = false;
    size_t total_nodes = 0;
    size_t completed_nodes = 0;
    size_t failed_nodes = 0;
    size_t skipped_nodes = 0;
    std::chrono::milliseconds total_duration{0};
    std::vector<std::string> errors;
};

/**
 * @brief High-performance concurrent thread pool executor for TaskGraph DAGs
 * Features:
 * - O(1) atomic in-degree dependency resolution
 * - Fine-grained lock contention elimination
 * - Targeted condition-variable signaling (no thundering herd)
 * - Decoupled runtime execution state and zero-allocation scheduling
 */
class TaskGraphExecutor {
public:
    explicit TaskGraphExecutor(TaskGraphExecutorConfig config = {});
    ~TaskGraphExecutor();

    void setEventBus(std::shared_ptr<EventBus> event_bus);
    void setNodeHandler(TaskNodeHandler handler);

    // Callbacks
    using NodeStateCallback = std::function<void(const std::string& node_id, TaskNodeState old_state, TaskNodeState new_state)>;
    void onNodeStateChange(NodeStateCallback callback);

    // Execution
    TaskGraphExecutionSummary execute(TaskGraph& graph);
    void cancel();
    bool isRunning() const { return running_; }

private:
    struct ExecutionContext;
    void workerLoop(ExecutionContext& ctx);
    void processNode(ExecutionContext& ctx, uint32_t node_idx);
    void invalidateDownstream(ExecutionContext& ctx, uint32_t failed_idx);
    void emitEvent(const std::string& event_name, const std::string& json_data);

    TaskGraphExecutorConfig config_;
    std::shared_ptr<EventBus> event_bus_;
    TaskNodeHandler node_handler_;
    NodeStateCallback state_callback_;

    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<size_t> active_workers_{0};

    std::queue<uint32_t> ready_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    mutable std::mutex state_mutex_;
};

} // namespace aios
