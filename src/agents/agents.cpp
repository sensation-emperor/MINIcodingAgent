// AIOS - MINI Coding Agent Operating System
// Agent Framework Implementation
// Implements autonomous agents with perception, planning, action, and reflection capabilities

#include "agents/agents.h"
#include "providers/ModelProvider.h"
#include "logging/Logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <functional>

namespace aios {

// ============================================================================
// Agent Implementation
// ============================================================================

struct Agent::Impl {
    std::string current_task;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> paused{false};
    
    // Statistics
    size_t tasks_executed = 0;
    size_t tasks_completed = 0;
    size_t tasks_failed = 0;
    double total_iterations = 0.0;
    double total_execution_time_ms = 0.0;
    
    // Tool registry (simplified - would be injected in production)
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> tools;
    
    void initializeTools() {
        // Register built-in tools
        tools["read_file"] = [this](const std::string& args) -> std::string {
            return executeTool("read_file", args);
        };
        
        tools["write_file"] = [this](const std::string& args) -> std::string {
            return executeTool("write_file", args);
        };
        
        tools["search_files"] = [this](const std::string& args) -> std::string {
            return executeTool("search_files", args);
        };
        
        tools["run_command"] = [this](const std::string& args) -> std::string {
            return executeTool("run_command", args);
        };
        
        tools["list_directory"] = [this](const std::string& args) -> std::string {
            return executeTool("list_directory", args);
        };
    }
    
    std::string executeTool(const std::string& tool_name, const std::string& args) {
        // In production, this would delegate to the actual tool implementations
        // For now, return placeholder indicating tool was invoked
        return "Tool invoked: " + tool_name + " with args: " + args;
    }
};

Agent::Agent(AgentConfig config) 
    : impl_(std::make_unique<Impl>())
    , config_(std::move(config)) {
    impl_->initializeTools();
}

Agent::~Agent() {
    cancel();
}

AgentResult Agent::execute(const std::string& task,
                            const std::unordered_map<std::string, std::string>& context) {
    if (state_ != AgentState::Idle && state_ != AgentState::Waiting) {
        AgentResult result;
        result.error_message = "Agent is busy";
        result.success = false;
        return result;
    }
    
    return executeLoop(task, context);
}

AgentResult Agent::executeLoop(const std::string& task,
                                const std::unordered_map<std::string, std::string>& context) {
    auto start_time = std::chrono::steady_clock::now();
    
    impl_->current_task = task;
    impl_->cancelled = false;
    impl_->paused = false;
    
    AgentResult result;
    std::string observation = "Starting task: " + task;
    std::string accumulated_output;
    
    int iterations = 0;
    const int max_iterations = config_.max_iterations;
    
    while (iterations < max_iterations) {
        // Check for cancellation
        if (impl_->cancelled) {
            result.error_message = "Task cancelled";
            result.success = false;
            state_ = AgentState::Cancelled;
            break;
        }
        
        // Check for pause
        while (impl_->paused && !impl_->cancelled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        iterations++;
        state_ = AgentState::Thinking;
        
        // Think step - determine next action
        std::string action = think(observation, context);
        
        if (action.empty() || action == "DONE") {
            // Task complete
            result.content = accumulated_output;
            result.success = true;
            result.iterations_used = iterations;
            state_ = AgentState::Completed;
            break;
        }
        
        state_ = AgentState::Acting;
        
        // Act step - execute the action
        std::string action_result = act(action);
        
        if (action_result.find("Error:") == 0) {
            // Action failed
            observation = "Action failed: " + action_result;
            accumulated_output += "\nError: " + action_result;
            
            // Reflect on failure
            if (!reflect(action_result, task)) {
                result.error_message = "Critical action failure";
                result.success = false;
                result.iterations_used = iterations;
                state_ = AgentState::Failed;
                break;
            }
        } else {
            // Action succeeded
            observation = "Action result: " + action_result;
            accumulated_output += "\n" + action_result;
            
            // Parse tool calls for tracking
            result.tool_calls.push_back(action);
        }
        
        // Reflect step - critique progress
        if (!reflect(accumulated_output, task)) {
            // Reflection suggests continuing
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.execution_time = duration;
    
    // Update statistics
    impl_->tasks_executed++;
    impl_->total_iterations += iterations;
    impl_->total_execution_time_ms += duration.count();
    
    if (result.success) {
        impl_->tasks_completed++;
    } else {
        impl_->tasks_failed++;
    }
    
    impl_->current_task.clear();
    state_ = AgentState::Idle;
    
    return result;
}

std::string Agent::think(const std::string& observation,
                          const std::unordered_map<std::string, std::string>& context) {
    // In production, this would call the LLM with:
    // - System prompt from config
    // - Current observation
    // - Available tools
    // - Context
    // - Conversation history
    
    // Simplified implementation - parses observation for keywords
    std::string lower_obs = observation;
    std::transform(lower_obs.begin(), lower_obs.end(), lower_obs.begin(), ::tolower);
    
    if (lower_obs.find("read") != std::string::npos && 
        lower_obs.find("file") != std::string::npos) {
        return "read_file:example.cpp";
    }
    
    if (lower_obs.find("write") != std::string::npos && 
        lower_obs.find("file") != std::string::npos) {
        return "write_file:example.cpp:content";
    }
    
    if (lower_obs.find("search") != std::string::npos) {
        return "search_files:*.cpp";
    }
    
    if (lower_obs.find("list") != std::string::npos ||
        lower_obs.find("directory") != std::string::npos) {
        return "list_directory:.";
    }
    
    if (lower_obs.find("run") != std::string::npos ||
        lower_obs.find("execute") != std::string::npos) {
        return "run_command:echo hello";
    }
    
    // Default to done if no action identified
    return "DONE";
}

std::string Agent::act(const std::string& action) {
    // Parse action into tool name and arguments
    size_t colon_pos = action.find(':');
    std::string tool_name;
    std::string args;
    
    if (colon_pos != std::string::npos) {
        tool_name = action.substr(0, colon_pos);
        args = action.substr(colon_pos + 1);
    } else {
        tool_name = action;
    }
    
    // Find and execute tool
    auto it = impl_->tools.find(tool_name);
    if (it != impl_->tools.end()) {
        try {
            return it->second(args);
        } catch (const std::exception& e) {
            return std::string("Error: Tool execution failed: ") + e.what();
        }
    }
    
    return "Error: Unknown tool: " + tool_name;
}

bool Agent::reflect(const std::string& result, const std::string& task) {
    // In production, this would call the LLM to critique the result
    // and determine if the task is complete or needs more work
    
    // Simplified: always return true to continue
    return true;
}

void Agent::cancel() {
    impl_->cancelled = true;
    impl_->paused = false;
}

void Agent::pause() {
    impl_->paused = true;
}

void Agent::resume() {
    impl_->paused = false;
}

bool Agent::isBusy() const {
    return state_ == AgentState::Thinking || 
           state_ == AgentState::Acting ||
           state_ == AgentState::Waiting;
}

std::optional<std::string> Agent::getCurrentTask() const {
    if (impl_->current_task.empty()) {
        return std::nullopt;
    }
    return impl_->current_task;
}

// ============================================================================
// AgentManager Implementation
// ============================================================================

struct AgentManager::Impl {
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents;
    std::unordered_map<AgentType, AgentFactory> factories;
    std::shared_ptr<ModelProvider> model_provider;
    
    // Statistics
    size_t total_created = 0;
    size_t total_removed = 0;
    size_t total_tasks_executed = 0;
    size_t total_tasks_completed = 0;
    size_t total_tasks_failed = 0;
    double total_iterations = 0.0;
    double total_execution_time_ms = 0.0;
};

AgentManager& AgentManager::instance() {
    static AgentManager instance;
    return instance;
}

bool AgentManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_INFO("Initializing AgentManager...");
    
    // Register default agent factories
    registerAgentType(AgentType::Planner, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    registerAgentType(AgentType::Researcher, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    registerAgentType(AgentType::Coder, [](const AgentConfig& config) {
        AgentConfig coder_config = config;
        coder_config.capabilities.can_write_files = true;
        coder_config.capabilities.can_read_files = true;
        return std::make_shared<Agent>(coder_config);
    });
    
    registerAgentType(AgentType::Tester, [](const AgentConfig& config) {
        AgentConfig tester_config = config;
        tester_config.capabilities.can_run_tests = true;
        tester_config.capabilities.can_execute_commands = true;
        return std::make_shared<Agent>(tester_config);
    });
    
    registerAgentType(AgentType::Reviewer, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    registerAgentType(AgentType::SecurityAuditor, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    registerAgentType(AgentType::DocumentationWriter, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    registerAgentType(AgentType::Refactorer, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    registerAgentType(AgentType::Debugger, [](const AgentConfig& config) {
        AgentConfig debugger_config = config;
        debugger_config.capabilities.can_execute_commands = true;
        debugger_config.capabilities.can_run_tests = true;
        return std::make_shared<Agent>(debugger_config);
    });
    
    registerAgentType(AgentType::Generic, [](const AgentConfig& config) {
        return std::make_shared<Agent>(config);
    });
    
    LOG_INFO("AgentManager initialized successfully");
    return true;
}

void AgentManager::shutdown() {
    stop();
    
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->agents.clear();
    impl_->factories.clear();
}

void AgentManager::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_INFO("Stopping all agents...");
    
    for (auto& [id, agent] : impl_->agents) {
        agent->cancel();
    }
    
    LOG_INFO("All agents stopped");
}

std::string AgentManager::createAgent(const AgentConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate unique ID if not provided
    std::string agent_id = config.id;
    if (agent_id.empty()) {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dist;
        static std::atomic<uint64_t> counter{0};
        
        auto id = dist(gen) ^ (counter++ << 32);
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << id;
        agent_id = "agent_" + ss.str();
    }
    
    // Check if agent already exists
    if (impl_->agents.count(agent_id) > 0) {
        LOG_ERROR("Agent already exists: {}", agent_id);
        return "";
    }
    
    // Create agent using factory or default
    std::shared_ptr<Agent> agent;
    
    auto factory_it = impl_->factories.find(config.type);
    if (factory_it != impl_->factories.end()) {
        agent = factory_it->second(config);
    } else {
        agent = std::make_shared<Agent>(config);
    }
    
    impl_->agents[agent_id] = agent;
    impl_->total_created++;
    
    LOG_INFO("Created agent: id={}, name={}, type={}", 
             agent_id, config.name, static_cast<int>(config.type));
    
    return agent_id;
}

std::shared_ptr<Agent> AgentManager::getAgent(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->agents.find(agent_id);
    if (it == impl_->agents.end()) {
        return nullptr;
    }
    
    return it->second;
}

bool AgentManager::removeAgent(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->agents.find(agent_id);
    if (it == impl_->agents.end()) {
        return false;
    }
    
    // Cancel agent first
    it->second->cancel();
    
    impl_->agents.erase(it);
    impl_->total_removed++;
    
    LOG_INFO("Removed agent: {}", agent_id);
    return true;
}

std::vector<std::string> AgentManager::listAgents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(impl_->agents.size());
    
    for (const auto& [id, _] : impl_->agents) {
        ids.push_back(id);
    }
    
    return ids;
}

AgentResult AgentManager::executeTask(const std::string& agent_id,
                                       const std::string& task,
                                       const std::unordered_map<std::string, std::string>& context) {
    auto agent = getAgent(agent_id);
    if (!agent) {
        AgentResult result;
        result.error_message = "Agent not found: " + agent_id;
        result.success = false;
        return result;
    }
    
    impl_->total_tasks_executed++;
    
    auto result = agent->execute(task, context);
    
    if (result.success) {
        impl_->total_tasks_completed++;
    } else {
        impl_->total_tasks_failed++;
    }
    
    impl_->total_iterations += result.iterations_used;
    impl_->total_execution_time_ms += result.execution_time.count();
    
    return result;
}

std::unordered_map<std::string, AgentResult> AgentManager::broadcastTask(
    const std::vector<std::string>& agent_ids,
    const std::string& task,
    const std::unordered_map<std::string, std::string>& context) {
    
    std::unordered_map<std::string, AgentResult> results;
    
    // Execute tasks in parallel
    std::vector<std::thread> threads;
    std::mutex results_mutex;
    
    for (const auto& agent_id : agent_ids) {
        threads.emplace_back([this, &agent_id, &task, &context, &results, &results_mutex]() {
            auto result = executeTask(agent_id, task, context);
            
            std::lock_guard<std::mutex> lock(results_mutex);
            results[agent_id] = std::move(result);
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    return results;
}

AgentStats AgentManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AgentStats stats{};
    stats.total_tasks = impl_->total_tasks_executed;
    stats.completed_tasks = impl_->total_tasks_completed;
    stats.failed_tasks = impl_->total_tasks_failed;
    stats.cancelled_tasks = impl_->total_removed;
    
    if (stats.completed_tasks > 0) {
        stats.average_iterations = impl_->total_iterations / stats.completed_tasks;
        stats.average_execution_time_ms = impl_->total_execution_time_ms / stats.completed_tasks;
    }
    
    return stats;
}

void AgentManager::setModelProvider(std::shared_ptr<ModelProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->model_provider = std::move(provider);
}

void AgentManager::registerAgentType(AgentType type, AgentFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->factories[type] = std::move(factory);
}

} // namespace aios

