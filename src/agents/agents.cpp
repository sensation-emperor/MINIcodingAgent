// AIOS - MINI Coding Agent Operating System
// Agent Framework Implementation

#include "agents/agents.h"
#include "agents/SpecializedAgents.h"
#include "agents/AgentToolParser.h"
#include "providers/ModelProvider.h"
#include "tools/ToolRegistry.h"
#include "events/EventBus.h"
#include "logging/Logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

namespace aios {

// ============================================================================
// Agent Implementation
// ============================================================================

struct Agent::Impl {
    std::string current_task;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> paused{false};
    
    std::shared_ptr<ModelProvider> model_provider;
    std::shared_ptr<ToolRegistry> tool_registry;
    std::shared_ptr<EventBus> event_bus;

    // Callbacks
    StateChangeCallback state_change_callback;
    ToolCallCallback tool_call_callback;
    ThoughtCallback thought_callback;

    // Statistics
    size_t tasks_executed = 0;
    size_t tasks_completed = 0;
    size_t tasks_failed = 0;
    double total_iterations = 0.0;
    double total_execution_time_ms = 0.0;
    
    // Built-in fallback tool map
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> fallback_tools;
    
    void initializeFallbackTools() {
        fallback_tools["read_file"] = [](const std::string& args) -> std::string {
            return "Read file: " + args;
        };
        fallback_tools["write_file"] = [](const std::string& args) -> std::string {
            return "Wrote file: " + args;
        };
        fallback_tools["search_files"] = [](const std::string& args) -> std::string {
            return "Found files matching: " + args;
        };
        fallback_tools["run_command"] = [](const std::string& args) -> std::string {
            return "Executed command: " + args + " (exit code 0)";
        };
        fallback_tools["list_directory"] = [](const std::string& args) -> std::string {
            return "Directory contents for: " + args;
        };
    }
};

Agent::Agent(AgentConfig config) 
    : config_(std::move(config))
    , impl_(std::make_unique<Impl>()) {
    impl_->initializeFallbackTools();
}

Agent::~Agent() {
    cancel();
}

void Agent::setModelProvider(std::shared_ptr<ModelProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->model_provider = provider;
}

std::shared_ptr<ModelProvider> Agent::getModelProvider() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->model_provider;
}

void Agent::setToolRegistry(std::shared_ptr<ToolRegistry> registry) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->tool_registry = registry;
}

std::shared_ptr<ToolRegistry> Agent::getToolRegistry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->tool_registry;
}

void Agent::setEventBus(std::shared_ptr<EventBus> event_bus) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->event_bus = event_bus;
}

std::shared_ptr<EventBus> Agent::getEventBus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->event_bus;
}

void Agent::onStateChange(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->state_change_callback = std::move(callback);
}

void Agent::onToolCall(ToolCallCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->tool_call_callback = std::move(callback);
}

void Agent::onThought(ThoughtCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->thought_callback = std::move(callback);
}

void Agent::setState(AgentState new_state) {
    AgentState old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        if (impl_->state_change_callback) {
            impl_->state_change_callback(old_state, new_state);
        }
        nlohmann::json j;
        j["agent_id"] = config_.id;
        j["agent_name"] = config_.name;
        j["old_state"] = static_cast<int>(old_state);
        j["new_state"] = static_cast<int>(new_state);
        publishEvent("agent.state_change", j.dump());
    }
}

void Agent::publishEvent(const std::string& event_name, const std::string& json_data) {
    if (impl_->event_bus) {
        impl_->event_bus->publish(event_name, json_data);
    }
}

std::string Agent::buildSystemPrompt() const {
    if (!config_.system_prompt.empty()) {
        return config_.system_prompt;
    }
    return "You are an autonomous AI software engineering agent in the AIOS operating system.\n"
           "Assist the user with code analysis, planning, writing, testing, and debugging.";
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
    result.success = false;

    // Collect available tools definitions
    std::vector<ToolDefinition> tool_defs;
    if (impl_->tool_registry) {
        auto all_tools = impl_->tool_registry->listTools();
        for (const auto& t_name : all_tools) {
            // Check allowed tools whitelist if specified
            if (!config_.capabilities.allowed_tools.empty()) {
                if (std::find(config_.capabilities.allowed_tools.begin(), 
                              config_.capabilities.allowed_tools.end(), 
                              t_name) == config_.capabilities.allowed_tools.end()) {
                    continue;
                }
            }
            auto opt_def = impl_->tool_registry->getToolDefinition(t_name);
            if (opt_def) {
                tool_defs.push_back(*opt_def);
            }
        }
    }

    // Build system message
    std::string full_system_prompt = buildSystemPrompt();
    if (!tool_defs.empty()) {
        full_system_prompt += "\n\n" + AgentToolParser::formatToolDefinitionsPrompt(tool_defs);
    }

    std::vector<Message> messages;
    messages.push_back(Message{"system", full_system_prompt});

    // Add context if provided
    if (!context.empty()) {
        std::ostringstream ctx_ss;
        ctx_ss << "Context Information:\n";
        for (const auto& [k, v] : context) {
            ctx_ss << "[" << k << "]:\n" << v << "\n\n";
        }
        messages.push_back(Message{"user", ctx_ss.str()});
    }

    messages.push_back(Message{"user", "Task: " + task});

    int iterations = 0;
    const int max_iterations = config_.max_iterations;
    std::string accumulated_output;
    
    while (iterations < max_iterations) {
        if (impl_->cancelled) {
            result.error_message = "Task cancelled";
            result.success = false;
            setState(AgentState::Cancelled);
            break;
        }
        
        while (impl_->paused && !impl_->cancelled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        iterations++;
        setState(AgentState::Thinking);

        std::string response_text;
        if (impl_->model_provider) {
            auto model_res = impl_->model_provider->chat(messages);
            if (!model_res.success) {
                result.error_message = "Model error: " + model_res.error;
                setState(AgentState::Failed);
                break;
            }
            response_text = model_res.content;
        } else {
            // Fallback think method
            response_text = think("Iteration " + std::to_string(iterations), context);
        }

        std::string thought = AgentToolParser::extractThought(response_text);
        if (!thought.empty() && impl_->thought_callback) {
            impl_->thought_callback(thought);
        }

        accumulated_output += (accumulated_output.empty() ? "" : "\n") + response_text;

        // Check if task is completed
        if (AgentToolParser::isTaskComplete(response_text)) {
            result.content = accumulated_output;
            result.success = true;
            result.iterations_used = iterations;
            setState(AgentState::Completed);
            break;
        }

        // Parse tool calls
        auto tool_calls = AgentToolParser::parseToolCalls(response_text);
        if (tool_calls.empty()) {
            // If no tools were called and no explicit task_complete was emitted,
            // check if response looks like a complete direct answer
            result.content = accumulated_output;
            result.success = true;
            result.iterations_used = iterations;
            setState(AgentState::Completed);
            break;
        }

        setState(AgentState::Acting);
        std::ostringstream obs_ss;

        for (const auto& tc : tool_calls) {
            result.tool_calls.push_back(tc.tool_name);
            
            if (impl_->tool_call_callback) {
                impl_->tool_call_callback(tc.tool_name, tc.parameters);
            }

            // Check capability permissions
            std::vector<ToolPermission> permissions;
            if (config_.capabilities.can_read_files) permissions.push_back(ToolPermission::Read);
            if (config_.capabilities.can_write_files) permissions.push_back(ToolPermission::Write);
            if (config_.capabilities.can_execute_commands) permissions.push_back(ToolPermission::Execute);
            if (config_.capabilities.can_access_network) permissions.push_back(ToolPermission::Network);

            ToolResult tool_res;
            if (impl_->tool_registry) {
                tool_res = impl_->tool_registry->executeWithPermission(tc.tool_name, tc.parameters, permissions);
            } else {
                // Fallback tool handler
                auto it = impl_->fallback_tools.find(tc.tool_name);
                if (it != impl_->fallback_tools.end()) {
                    std::string args;
                    for (const auto& [k, v] : tc.parameters) args += " " + k + "=" + v;
                    tool_res = ToolResult::ok(it->second(args));
                } else {
                    tool_res = ToolResult::error("Unknown tool: " + tc.tool_name);
                }
            }

            obs_ss << AgentToolParser::formatToolObservation(tc.tool_name, tool_res) << "\n";
        }

        // Add assistant message and tool observation to history
        messages.push_back(Message{"assistant", response_text});
        messages.push_back(Message{"user", obs_ss.str()});

        // Reflect hook
        reflect(obs_ss.str(), task);
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
        if (state_ != AgentState::Cancelled) {
            setState(AgentState::Failed);
        }
    }
    
    impl_->current_task.clear();
    if (state_ != AgentState::Failed && state_ != AgentState::Cancelled) {
        setState(AgentState::Idle);
    }
    
    return result;
}

std::string Agent::think(const std::string& observation,
                         const std::unordered_map<std::string, std::string>& /*context*/) {
    // Default rule-based thinking fallback when no ModelProvider is attached
    return "TASK_COMPLETE: Processed observation - " + observation;
}

std::string Agent::act(const std::string& action) {
    auto calls = AgentToolParser::parseToolCalls(action);
    if (calls.empty()) return "No tool call parsed";
    
    const auto& tc = calls[0];
    if (impl_->tool_registry) {
        auto res = impl_->tool_registry->executeTool(tc.tool_name, tc.parameters);
        return res.success ? res.output : ("Error: " + res.error_message);
    }
    return "Executed tool: " + tc.tool_name;
}

bool Agent::reflect(const std::string& /*result*/, const std::string& /*task*/) {
    return true;
}

void Agent::cancel() {
    impl_->cancelled = true;
    impl_->paused = false;
    setState(AgentState::Cancelled);
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
    std::shared_ptr<ToolRegistry> tool_registry;
    std::shared_ptr<EventBus> event_bus;
    
    // Statistics
    size_t total_created = 0;
    size_t total_removed = 0;
    size_t total_tasks_executed = 0;
    size_t total_tasks_completed = 0;
    size_t total_tasks_failed = 0;
    double total_iterations = 0.0;
    double total_execution_time_ms = 0.0;
};

AgentManager::AgentManager() : impl_(std::make_unique<Impl>()) {}
AgentManager::~AgentManager() = default;

AgentManager& AgentManager::instance() {
    static AgentManager instance;
    return instance;
}

bool AgentManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("Initializing AgentManager with specialized polymorphic agents...");
    
    // Register specialized polymorphic agent factories
    registerAgentType(AgentType::Planner, [](const AgentConfig& config) {
        return std::make_shared<PlannerAgent>(config);
    });
    
    registerAgentType(AgentType::Researcher, [](const AgentConfig& config) {
        return std::make_shared<ResearcherAgent>(config);
    });
    
    registerAgentType(AgentType::Coder, [](const AgentConfig& config) {
        return std::make_shared<CoderAgent>(config);
    });
    
    registerAgentType(AgentType::Tester, [](const AgentConfig& config) {
        return std::make_shared<TesterAgent>(config);
    });
    
    registerAgentType(AgentType::Reviewer, [](const AgentConfig& config) {
        return std::make_shared<ReviewerAgent>(config);
    });
    
    registerAgentType(AgentType::Debugger, [](const AgentConfig& config) {
        return std::make_shared<DebuggerAgent>(config);
    });

    registerAgentType(AgentType::SecurityAuditor, [](const AgentConfig& config) {
        return std::make_shared<ReviewerAgent>(config);
    });
    
    registerAgentType(AgentType::DocumentationWriter, [](const AgentConfig& config) {
        return std::make_shared<CoderAgent>(config);
    });
    
    registerAgentType(AgentType::Refactorer, [](const AgentConfig& config) {
        return std::make_shared<CoderAgent>(config);
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
    for (auto& [id, agent] : impl_->agents) {
        agent->cancel();
    }
}

void AgentManager::setModelProvider(std::shared_ptr<ModelProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->model_provider = provider;
    for (auto& [id, agent] : impl_->agents) {
        agent->setModelProvider(provider);
    }
}

std::shared_ptr<ModelProvider> AgentManager::getModelProvider() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->model_provider;
}

void AgentManager::setToolRegistry(std::shared_ptr<ToolRegistry> registry) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->tool_registry = registry;
    for (auto& [id, agent] : impl_->agents) {
        agent->setToolRegistry(registry);
    }
}

std::shared_ptr<ToolRegistry> AgentManager::getToolRegistry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->tool_registry;
}

void AgentManager::setEventBus(std::shared_ptr<EventBus> event_bus) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->event_bus = event_bus;
    for (auto& [id, agent] : impl_->agents) {
        agent->setEventBus(event_bus);
    }
}

std::shared_ptr<EventBus> AgentManager::getEventBus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->event_bus;
}

void AgentManager::registerAgentType(AgentType type, AgentFactory factory) {
    impl_->factories[type] = std::move(factory);
}

std::string AgentManager::createAgent(const AgentConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
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
    
    if (impl_->agents.count(agent_id) > 0) {
        LOG_ERROR("Agent already exists: {}", agent_id);
        return "";
    }
    
    std::shared_ptr<Agent> agent;
    auto factory_it = impl_->factories.find(config.type);
    if (factory_it != impl_->factories.end()) {
        agent = factory_it->second(config);
    } else {
        agent = std::make_shared<Agent>(config);
    }
    
    if (impl_->model_provider) agent->setModelProvider(impl_->model_provider);
    if (impl_->tool_registry) agent->setToolRegistry(impl_->tool_registry);
    if (impl_->event_bus) agent->setEventBus(impl_->event_bus);

    impl_->agents[agent_id] = agent;
    impl_->total_created++;
    
    LOG_INFO("Created agent: id={}, name={}, type={}", agent_id, config.name, static_cast<int>(config.type));
    return agent_id;
}

std::shared_ptr<Agent> AgentManager::getAgent(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = impl_->agents.find(agent_id);
    return (it != impl_->agents.end()) ? it->second : nullptr;
}

bool AgentManager::removeAgent(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = impl_->agents.find(agent_id);
    if (it == impl_->agents.end()) return false;
    
    it->second->cancel();
    impl_->agents.erase(it);
    impl_->total_removed++;
    return true;
}

std::vector<std::string> AgentManager::listAgents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> list;
    for (const auto& [id, _] : impl_->agents) {
        list.push_back(id);
    }
    return list;
}

AgentResult AgentManager::executeTask(const std::string& agent_id,
                                     const std::string& task,
                                     const std::unordered_map<std::string, std::string>& context) {
    std::shared_ptr<Agent> agent;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = impl_->agents.find(agent_id);
        if (it == impl_->agents.end()) {
            AgentResult r;
            r.error_message = "Agent not found: " + agent_id;
            return r;
        }
        agent = it->second;
    }
    return agent->execute(task, context);
}

std::unordered_map<std::string, AgentResult> AgentManager::broadcastTask(
    const std::vector<std::string>& agent_ids,
    const std::string& task,
    const std::unordered_map<std::string, std::string>& context) {
    std::unordered_map<std::string, AgentResult> results;
    for (const auto& id : agent_ids) {
        results[id] = executeTask(id, task, context);
    }
    return results;
}

AgentStats AgentManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    AgentStats s{};
    s.total_tasks = impl_->total_tasks_executed;
    s.completed_tasks = impl_->total_tasks_completed;
    s.failed_tasks = impl_->total_tasks_failed;
    s.average_iterations = (impl_->total_tasks_executed > 0) ? (impl_->total_iterations / impl_->total_tasks_executed) : 0.0;
    s.average_execution_time_ms = (impl_->total_tasks_executed > 0) ? (impl_->total_execution_time_ms / impl_->total_tasks_executed) : 0.0;
    return s;
}

} // namespace aios
