#include "taskgraph/TaskGraph.h"
#include "events/EventBus.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

namespace aios {

// ============================================================================
// TaskGraph Implementation
// ============================================================================

TaskGraph::TaskGraph(const std::string& goal) : goal_(goal) {}

bool TaskGraph::addNode(const TaskNode& node) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nodes_.count(node.id) > 0) {
        return false;
    }
    nodes_[node.id] = node;
    
    // Maintain bidirectional dependency consistency
    // 1. For each prerequisite in node.dependencies: register node.id as its dependent
    for (const auto& dep_id : node.dependencies) {
        auto it = nodes_.find(dep_id);
        if (it != nodes_.end()) {
            auto& dep_vec = it->second.dependents;
            if (std::find(dep_vec.begin(), dep_vec.end(), node.id) == dep_vec.end()) {
                dep_vec.push_back(node.id);
            }
        }
    }

    // 2. Check if any existing node already referenced this new node
    for (auto& [existing_id, existing_node] : nodes_) {
        if (existing_id == node.id) continue;
        if (std::find(existing_node.dependencies.begin(), existing_node.dependencies.end(), node.id) != existing_node.dependencies.end()) {
            if (std::find(nodes_[node.id].dependents.begin(), nodes_[node.id].dependents.end(), existing_id) == nodes_[node.id].dependents.end()) {
                nodes_[node.id].dependents.push_back(existing_id);
            }
        }
        if (std::find(existing_node.dependents.begin(), existing_node.dependents.end(), node.id) != existing_node.dependents.end()) {
            if (std::find(nodes_[node.id].dependencies.begin(), nodes_[node.id].dependencies.end(), existing_id) == nodes_[node.id].dependencies.end()) {
                nodes_[node.id].dependencies.push_back(existing_id);
            }
        }
    }

    // 3. For each successor in node.dependents: register node.id in their dependencies
    for (const auto& succ_id : node.dependents) {
        auto it = nodes_.find(succ_id);
        if (it != nodes_.end()) {
            auto& dep_vec = it->second.dependencies;
            if (std::find(dep_vec.begin(), dep_vec.end(), node.id) == dep_vec.end()) {
                dep_vec.push_back(node.id);
            }
        }
    }

    return true;
}

bool TaskGraph::removeNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return false;

    nodes_.erase(it);

    for (auto& [_, n] : nodes_) {
        n.dependencies.erase(std::remove(n.dependencies.begin(), n.dependencies.end(), node_id), n.dependencies.end());
        n.dependents.erase(std::remove(n.dependents.begin(), n.dependents.end(), node_id), n.dependents.end());
    }

    return true;
}

bool TaskGraph::addDependency(const std::string& from_node_id, const std::string& to_node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (from_node_id == to_node_id) return false;
    if (nodes_.count(from_node_id) == 0 || nodes_.count(to_node_id) == 0) {
        return false;
    }

    auto& from_node = nodes_[from_node_id];
    auto& to_node = nodes_[to_node_id];

    if (std::find(from_node.dependents.begin(), from_node.dependents.end(), to_node_id) == from_node.dependents.end()) {
        from_node.dependents.push_back(to_node_id);
    }
    if (std::find(to_node.dependencies.begin(), to_node.dependencies.end(), from_node_id) == to_node.dependencies.end()) {
        to_node.dependencies.push_back(from_node_id);
    }

    return true;
}

std::optional<TaskNode> TaskGraph::getNode(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

TaskNode* TaskGraph::getNodeRef(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_id);
    return (it != nodes_.end()) ? &it->second : nullptr;
}

std::vector<TaskNode> TaskGraph::getAllNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TaskNode> list;
    list.reserve(nodes_.size());
    for (const auto& [_, n] : nodes_) {
        list.push_back(n);
    }
    return list;
}

size_t TaskGraph::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

bool TaskGraph::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.empty();
}

bool TaskGraph::hasCycle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nodes_.empty()) return false;

    // Build directed adjacency map (dependency -> dependent)
    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (const auto& [id, _] : nodes_) {
        adj[id]; // ensure entry exists
    }

    for (const auto& [id, node] : nodes_) {
        for (const auto& dep_id : node.dependencies) {
            if (nodes_.count(dep_id) > 0) {
                if (std::find(adj[dep_id].begin(), adj[dep_id].end(), id) == adj[dep_id].end()) {
                    adj[dep_id].push_back(id);
                }
            }
        }
        for (const auto& succ_id : node.dependents) {
            if (nodes_.count(succ_id) > 0) {
                if (std::find(adj[id].begin(), adj[id].end(), succ_id) == adj[id].end()) {
                    adj[id].push_back(succ_id);
                }
            }
        }
    }

    // 3-Color DFS: White = 0 (unvisited), Gray = 1 (on current recursion path), Black = 2 (completed)
    enum class Color : uint8_t { White = 0, Gray = 1, Black = 2 };
    std::unordered_map<std::string, Color> colors;
    for (const auto& [id, _] : nodes_) {
        colors[id] = Color::White;
    }

    std::function<bool(const std::string&)> dfs = [&](const std::string& u) -> bool {
        colors[u] = Color::Gray;
        for (const auto& v : adj[u]) {
            if (colors[v] == Color::Gray) {
                return true; // Back-edge found -> cycle!
            }
            if (colors[v] == Color::White) {
                if (dfs(v)) return true;
            }
        }
        colors[u] = Color::Black;
        return false;
    };

    for (const auto& [id, _] : nodes_) {
        if (colors[id] == Color::White) {
            if (dfs(id)) return true;
        }
    }

    return false;
}

std::vector<std::string> TaskGraph::getTopologicalOrder() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> order;
    if (nodes_.empty()) return order;

    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> in_degree;
    for (const auto& [id, _] : nodes_) {
        adj[id];
        in_degree[id] = 0;
    }

    for (const auto& [id, node] : nodes_) {
        for (const auto& dep_id : node.dependencies) {
            if (nodes_.count(dep_id) > 0) {
                if (std::find(adj[dep_id].begin(), adj[dep_id].end(), id) == adj[dep_id].end()) {
                    adj[dep_id].push_back(id);
                }
            }
        }
        for (const auto& succ_id : node.dependents) {
            if (nodes_.count(succ_id) > 0) {
                if (std::find(adj[id].begin(), adj[id].end(), succ_id) == adj[id].end()) {
                    adj[id].push_back(succ_id);
                }
            }
        }
    }

    for (const auto& [u, succs] : adj) {
        for (const auto& v : succs) {
            in_degree[v]++;
        }
    }

    std::queue<std::string> q;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) {
            q.push(id);
        }
    }

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();
        order.push_back(curr);

        for (const auto& succ : adj[curr]) {
            if (--in_degree[succ] == 0) {
                q.push(succ);
            }
        }
    }

    if (order.size() != nodes_.size()) {
        return {};
    }

    return order;
}

std::vector<std::vector<std::string>> TaskGraph::getExecutionLevels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::vector<std::string>> levels;
    if (nodes_.empty()) return levels;

    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> in_degree;
    for (const auto& [id, _] : nodes_) {
        adj[id];
        in_degree[id] = 0;
    }

    for (const auto& [id, node] : nodes_) {
        for (const auto& dep_id : node.dependencies) {
            if (nodes_.count(dep_id) > 0) {
                if (std::find(adj[dep_id].begin(), adj[dep_id].end(), id) == adj[dep_id].end()) {
                    adj[dep_id].push_back(id);
                }
            }
        }
        for (const auto& succ_id : node.dependents) {
            if (nodes_.count(succ_id) > 0) {
                if (std::find(adj[id].begin(), adj[id].end(), succ_id) == adj[id].end()) {
                    adj[id].push_back(succ_id);
                }
            }
        }
    }

    for (const auto& [u, succs] : adj) {
        for (const auto& v : succs) {
            in_degree[v]++;
        }
    }

    std::vector<std::string> current_level;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) current_level.push_back(id);
    }

    while (!current_level.empty()) {
        levels.push_back(current_level);
        std::vector<std::string> next_level;

        for (const auto& id : current_level) {
            for (const auto& succ : adj[id]) {
                if (--in_degree[succ] == 0) {
                    next_level.push_back(succ);
                }
            }
        }
        current_level = next_level;
    }

    return levels;
}

std::vector<std::string> TaskGraph::getReadyNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ready;
    for (const auto& [id, node] : nodes_) {
        if (node.state == TaskNodeState::Pending) {
            bool all_done = true;
            for (const auto& dep_id : node.dependencies) {
                auto it = nodes_.find(dep_id);
                if (it == nodes_.end() || it->second.state != TaskNodeState::Completed) {
                    all_done = false;
                    break;
                }
            }
            if (all_done) {
                ready.push_back(id);
            }
        }
    }
    return ready;
}

bool TaskGraph::areDependenciesCompleted(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return false;

    for (const auto& dep_id : it->second.dependencies) {
        auto d_it = nodes_.find(dep_id);
        if (d_it == nodes_.end() || d_it->second.state != TaskNodeState::Completed) {
            return false;
        }
    }
    return true;
}

void TaskGraph::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, node] : nodes_) {
        node.state = TaskNodeState::Pending;
        node.output_data.clear();
        node.error_message.clear();
        node.retry_count = 0;
        node.execution_time = std::chrono::milliseconds(0);
    }
}

std::string TaskGraph::toDotFormat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;
    ss << "digraph TaskGraph {\n";
    ss << "  rankdir=LR;\n";
    ss << "  node [shape=box, style=rounded];\n";

    for (const auto& [id, node] : nodes_) {
        ss << "  \"" << id << "\" [label=\"" << node.title << "\\n(" << id << ")\"];\n";
    }

    for (const auto& [id, node] : nodes_) {
        for (const auto& dep : node.dependents) {
            ss << "  \"" << id << "\" -> \"" << dep << "\";\n";
        }
    }

    ss << "}\n";
    return ss.str();
}

std::string TaskGraph::toJsonString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["goal"] = goal_;
    j["nodes"] = nlohmann::json::array();

    for (const auto& [id, node] : nodes_) {
        nlohmann::json nj;
        nj["id"] = node.id;
        nj["title"] = node.title;
        nj["description"] = node.description;
        nj["assigned_agent_type"] = static_cast<int>(node.assigned_agent_type);
        nj["tool_action"] = node.tool_action;
        nj["dependencies"] = node.dependencies;
        nj["dependents"] = node.dependents;
        nj["state"] = static_cast<int>(node.state);
        nj["retry_count"] = node.retry_count;
        nj["max_retries"] = node.max_retries;
        nj["error_message"] = node.error_message;
        j["nodes"].push_back(nj);
    }

    return j.dump(2);
}

TaskGraph TaskGraph::fromJsonString(const std::string& json_str) {
    TaskGraph graph;
    try {
        auto j = nlohmann::json::parse(json_str);
        graph.setGoal(j.value("goal", ""));

        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& nj : j["nodes"]) {
                TaskNode node;
                node.id = nj.value("id", "");
                node.title = nj.value("title", "");
                node.description = nj.value("description", "");
                node.assigned_agent_type = static_cast<AgentType>(nj.value("assigned_agent_type", 0));
                node.tool_action = nj.value("tool_action", "");
                if (nj.contains("dependencies")) node.dependencies = nj["dependencies"].get<std::vector<std::string>>();
                if (nj.contains("dependents")) node.dependents = nj["dependents"].get<std::vector<std::string>>();
                node.state = static_cast<TaskNodeState>(nj.value("state", 0));
                node.retry_count = nj.value("retry_count", 0);
                node.max_retries = nj.value("max_retries", 2);
                node.error_message = nj.value("error_message", "");
                graph.addNode(node);
            }
        }
    } catch (...) {}
    return graph;
}

// ============================================================================
// TaskGraphExecutor High-Performance Concurrency Implementation
// ============================================================================

struct NodeRuntimeState {
    std::atomic<uint32_t> remaining_dependencies{0};
    std::atomic<TaskNodeState> state{TaskNodeState::Pending};
    std::atomic<int> retry_count{0};
    std::string output_data;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
};

struct TaskGraphExecutor::ExecutionContext {
    TaskGraph& graph;
    std::vector<std::string> node_ids;
    std::unordered_map<std::string, uint32_t> id_to_index;
    std::vector<TaskNode> task_nodes;
    std::vector<std::vector<uint32_t>> adj;
    std::vector<std::unique_ptr<NodeRuntimeState>> runtimes;
    std::atomic<size_t> completed_nodes{0};
    std::atomic<size_t> failed_nodes{0};
    std::atomic<size_t> skipped_nodes{0};
    std::mutex errors_mutex;
    std::vector<std::string> errors;

    explicit ExecutionContext(TaskGraph& g) : graph(g) {}
};

TaskGraphExecutor::TaskGraphExecutor(TaskGraphExecutorConfig config)
    : config_(config) {}

TaskGraphExecutor::~TaskGraphExecutor() {
    cancel();
}

void TaskGraphExecutor::setEventBus(std::shared_ptr<EventBus> event_bus) {
    event_bus_ = event_bus;
}

void TaskGraphExecutor::setNodeHandler(TaskNodeHandler handler) {
    node_handler_ = std::move(handler);
}

void TaskGraphExecutor::onNodeStateChange(NodeStateCallback callback) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_callback_ = std::move(callback);
}

void TaskGraphExecutor::cancel() {
    cancelled_ = true;
    cv_.notify_all();
}

void TaskGraphExecutor::emitEvent(const std::string& event_name, const std::string& json_data) {
    if (event_bus_) {
        event_bus_->publish(event_name, json_data);
    }
}

void TaskGraphExecutor::invalidateDownstream(ExecutionContext& ctx, uint32_t failed_idx) {
    std::queue<uint32_t> to_skip;
    for (uint32_t succ_idx : ctx.adj[failed_idx]) {
        to_skip.push(succ_idx);
    }

    NodeStateCallback cb;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        cb = state_callback_;
    }

    while (!to_skip.empty()) {
        uint32_t curr = to_skip.front();
        to_skip.pop();

        TaskNodeState st = ctx.runtimes[curr]->state.load(std::memory_order_acquire);
        if (st != TaskNodeState::Skipped && st != TaskNodeState::Completed && st != TaskNodeState::Failed) {
            TaskNodeState expected = st;
            if (ctx.runtimes[curr]->state.compare_exchange_strong(expected, TaskNodeState::Skipped, std::memory_order_acq_rel)) {
                ctx.task_nodes[curr].state = TaskNodeState::Skipped;
                ctx.task_nodes[curr].error_message = "Skipped due to upstream failure in " + ctx.node_ids[failed_idx];
                ctx.runtimes[curr]->error_message = ctx.task_nodes[curr].error_message;
                ctx.skipped_nodes.fetch_add(1, std::memory_order_relaxed);

                if (cb) {
                    cb(ctx.node_ids[curr], expected, TaskNodeState::Skipped);
                }

                for (uint32_t next_succ : ctx.adj[curr]) {
                    to_skip.push(next_succ);
                }
            }
        }
    }
}

TaskGraphExecutionSummary TaskGraphExecutor::execute(TaskGraph& graph) {
    auto start_time = std::chrono::steady_clock::now();
    running_ = true;
    cancelled_ = false;

    TaskGraphExecutionSummary summary;
    summary.total_nodes = graph.size();

    if (graph.empty()) {
        summary.success = true;
        running_ = false;
        return summary;
    }

    if (graph.hasCycle()) {
        summary.success = false;
        summary.errors.push_back("TaskGraph contains a cyclic dependency!");
        running_ = false;
        return summary;
    }

    // Decouple DAG topology into flat indexing for lock-free O(1) scheduling
    ExecutionContext ctx(graph);
    ctx.task_nodes = graph.getAllNodes();
    size_t n = ctx.task_nodes.size();
    ctx.node_ids.resize(n);
    ctx.adj.resize(n);
    ctx.runtimes.resize(n);

    for (size_t i = 0; i < n; ++i) {
        ctx.node_ids[i] = ctx.task_nodes[i].id;
        ctx.id_to_index[ctx.task_nodes[i].id] = static_cast<uint32_t>(i);
        ctx.runtimes[i] = std::make_unique<NodeRuntimeState>();
        ctx.runtimes[i]->retry_count.store(ctx.task_nodes[i].retry_count, std::memory_order_relaxed);
        ctx.runtimes[i]->state.store(TaskNodeState::Pending, std::memory_order_relaxed);
    }

    std::vector<uint32_t> initial_in_degrees(n, 0);
    for (size_t i = 0; i < n; ++i) {
        for (const auto& dep_id : ctx.task_nodes[i].dependencies) {
            auto it = ctx.id_to_index.find(dep_id);
            if (it != ctx.id_to_index.end()) {
                uint32_t d = it->second;
                if (std::find(ctx.adj[d].begin(), ctx.adj[d].end(), static_cast<uint32_t>(i)) == ctx.adj[d].end()) {
                    ctx.adj[d].push_back(static_cast<uint32_t>(i));
                    initial_in_degrees[i]++;
                }
            }
        }
        for (const auto& succ_id : ctx.task_nodes[i].dependents) {
            auto it = ctx.id_to_index.find(succ_id);
            if (it != ctx.id_to_index.end()) {
                uint32_t s = it->second;
                if (std::find(ctx.adj[i].begin(), ctx.adj[i].end(), s) == ctx.adj[i].end()) {
                    ctx.adj[i].push_back(s);
                    initial_in_degrees[s]++;
                }
            }
        }
    }

    for (size_t i = 0; i < n; ++i) {
        ctx.runtimes[i]->remaining_dependencies.store(initial_in_degrees[i], std::memory_order_relaxed);
    }

    // Step 2: Initialize ready queue with 0-dependency nodes
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!ready_queue_.empty()) ready_queue_.pop();
        for (size_t i = 0; i < n; ++i) {
            if (initial_in_degrees[i] == 0) {
                ctx.runtimes[i]->state.store(TaskNodeState::Ready, std::memory_order_release);
                ctx.task_nodes[i].state = TaskNodeState::Ready;
                ready_queue_.push(static_cast<uint32_t>(i));
                NodeStateCallback cb;
                {
                    std::lock_guard<std::mutex> slock(state_mutex_);
                    cb = state_callback_;
                }
                if (cb) {
                    cb(ctx.node_ids[i], TaskNodeState::Pending, TaskNodeState::Ready);
                }
            }
        }
    }

    // Step 3: Launch worker threads
    size_t num_workers = std::min(config_.max_concurrency, n);
    if (num_workers == 0) num_workers = 1;
    active_workers_ = 0;

    std::vector<std::thread> workers;
    workers.reserve(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back([this, &ctx]() {
            workerLoop(ctx);
        });
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    running_ = false;
    auto end_time = std::chrono::steady_clock::now();
    summary.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Sync back final states to graph safely under graph mutex
    {
        for (size_t i = 0; i < n; ++i) {
            auto* node_ref = graph.getNodeRef(ctx.node_ids[i]);
            if (node_ref) {
                node_ref->state = ctx.runtimes[i]->state.load(std::memory_order_acquire);
                node_ref->output_data = ctx.runtimes[i]->output_data;
                node_ref->error_message = ctx.runtimes[i]->error_message;
                node_ref->retry_count = ctx.runtimes[i]->retry_count.load(std::memory_order_relaxed);
                node_ref->execution_time = ctx.runtimes[i]->execution_time;
            }
        }
    }

    summary.completed_nodes = ctx.completed_nodes.load(std::memory_order_relaxed);
    summary.failed_nodes = ctx.failed_nodes.load(std::memory_order_relaxed);
    summary.skipped_nodes = ctx.skipped_nodes.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> elock(ctx.errors_mutex);
        summary.errors = ctx.errors;
    }
    summary.success = (summary.failed_nodes == 0 && summary.completed_nodes + summary.skipped_nodes == summary.total_nodes && !cancelled_);

    nlohmann::json j;
    j["success"] = summary.success;
    j["completed"] = summary.completed_nodes;
    j["failed"] = summary.failed_nodes;
    j["skipped"] = summary.skipped_nodes;
    j["duration_ms"] = summary.total_duration.count();
    emitEvent("taskgraph.graph_completed", j.dump());

    return summary;
}

void TaskGraphExecutor::workerLoop(ExecutionContext& ctx) {
    while (!cancelled_) {
        uint32_t node_idx = 0;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]() {
                if (cancelled_) return true;
                if (!ready_queue_.empty()) return true;
                if (active_workers_ == 0 && ready_queue_.empty()) {
                    return true;
                }
                return false;
            });

            if (cancelled_ || (ready_queue_.empty() && active_workers_ == 0)) {
                cv_.notify_all();
                break;
            }

            if (ready_queue_.empty()) {
                continue;
            }

            node_idx = ready_queue_.front();
            ready_queue_.pop();
            active_workers_++;
        }

        TaskNodeState st = ctx.runtimes[node_idx]->state.load(std::memory_order_acquire);
        if (st == TaskNodeState::Skipped) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            active_workers_--;
            if (ready_queue_.empty() && active_workers_ == 0) {
                cv_.notify_all();
            }
            continue;
        }

        processNode(ctx, node_idx);

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            active_workers_--;
            if (ready_queue_.empty() && active_workers_ == 0) {
                cv_.notify_all();
            }
        }
    }
}

void TaskGraphExecutor::processNode(ExecutionContext& ctx, uint32_t node_idx) {
    auto& runtime = *ctx.runtimes[node_idx];
    auto& task_node = ctx.task_nodes[node_idx];
    const std::string& node_id = ctx.node_ids[node_idx];

    TaskNodeState old_state = runtime.state.load(std::memory_order_relaxed);
    runtime.state.store(TaskNodeState::Running, std::memory_order_release);
    task_node.state = TaskNodeState::Running;

    NodeStateCallback cb;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        cb = state_callback_;
    }
    if (cb) {
        cb(node_id, old_state, TaskNodeState::Running);
    }

    auto start_time = std::chrono::steady_clock::now();

    TaskExecutionResult result;
    if (node_handler_) {
        result = node_handler_(task_node);
    } else {
        result.success = true;
        result.output = "Default handler success for " + task_node.title;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    runtime.execution_time = duration;
    task_node.execution_time = duration;

    if (result.success) {
        runtime.output_data = result.output;
        task_node.output_data = result.output;
        runtime.state.store(TaskNodeState::Completed, std::memory_order_release);
        task_node.state = TaskNodeState::Completed;
        ctx.completed_nodes.fetch_add(1, std::memory_order_relaxed);

        if (cb) {
            cb(node_id, TaskNodeState::Running, TaskNodeState::Completed);
        }

        // O(1) Atomic In-Degree Resolution on dependents
        std::vector<uint32_t> newly_ready;
        for (uint32_t succ_idx : ctx.adj[node_idx]) {
            uint32_t prev = ctx.runtimes[succ_idx]->remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1) {
                TaskNodeState exp = TaskNodeState::Pending;
                if (ctx.runtimes[succ_idx]->state.compare_exchange_strong(exp, TaskNodeState::Ready, std::memory_order_acq_rel)) {
                    ctx.task_nodes[succ_idx].state = TaskNodeState::Ready;
                    newly_ready.push_back(succ_idx);
                    if (cb) {
                        cb(ctx.node_ids[succ_idx], TaskNodeState::Pending, TaskNodeState::Ready);
                    }
                }
            }
        }

        if (!newly_ready.empty()) {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                for (uint32_t rid : newly_ready) {
                    ready_queue_.push(rid);
                }
            }
            if (newly_ready.size() == 1) {
                cv_.notify_one();
            } else {
                cv_.notify_all();
            }
        }
    } else {
        int current_retry = runtime.retry_count.fetch_add(1, std::memory_order_relaxed) + 1;
        task_node.retry_count = current_retry;

        if (current_retry <= task_node.max_retries) {
            LOG_WARN("Node [{}] failed, retrying ({}/{})...", node_id, current_retry, task_node.max_retries);
            runtime.state.store(TaskNodeState::Ready, std::memory_order_release);
            task_node.state = TaskNodeState::Ready;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                ready_queue_.push(node_idx);
            }
            cv_.notify_one();
        } else {
            runtime.error_message = result.error;
            task_node.error_message = result.error;
            runtime.state.store(TaskNodeState::Failed, std::memory_order_release);
            task_node.state = TaskNodeState::Failed;
            ctx.failed_nodes.fetch_add(1, std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> elock(ctx.errors_mutex);
                ctx.errors.push_back("Node [" + node_id + "] failed: " + result.error);
            }

            if (cb) {
                cb(node_id, TaskNodeState::Running, TaskNodeState::Failed);
            }

            if (config_.rollback_on_node_failure) {
                invalidateDownstream(ctx, node_idx);
            }
        }
    }
}

} // namespace aios
