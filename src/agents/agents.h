#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <optional>

namespace aios {

// Forward declarations
class ModelProvider;
class Task;

enum class AgentType {
    Planner,
    Researcher,
    Coder,
    Tester,
    Reviewer,
    SecurityAuditor,
    DocumentationWriter,
    Refactorer,
    Debugger,
    Generic
};

enum class AgentState {
    Idle,
    Thinking,
    Acting,
    Waiting,
    Completed,
    Failed,
    Cancelled
};

struct AgentCapabilities {
    bool can_read_files = true;
    bool can_write_files = true;
    bool can_execute_commands = false;
    bool can_access_network = false;
    bool can_access_git = true;
    bool can_run_tests = false;
    std::vector<std::string> allowed_tools;
};

struct AgentConfig {
    std::string id;
    std::string name;
    AgentType type;
    std::string model_name;
    std::string system_prompt;
    double temperature = 0.7;
    int max_tokens = 4096;
    int max_iterations = 50;
    AgentCapabilities capabilities;
    std::chrono::milliseconds timeout{30000};
    size_t max_retries = 3;
};

struct AgentResult {
    std::string content;
    bool success = false;
    std::string error_message;
    int iterations_used = 0;
    std::vector<std::string> tool_calls;
    std::chrono::milliseconds execution_time{0};
};

struct AgentStats {
    size_t total_tasks;
    size_t completed_tasks;
    size_t failed_tasks;
    size_t cancelled_tasks;
    double average_iterations;
    double average_execution_time_ms;
};

/**
 * @brief Autonomous agent that can perceive, plan, act, and reflect
 */
class Agent {
public:
    explicit Agent(AgentConfig config);
    ~Agent();
    
    // Get agent configuration
    const AgentConfig& getConfig() const { return config_; }
    
    // Get current state
    AgentState getState() const { return state_; }
    
    // Get agent ID
    const std::string& getId() const { return config_.id; }
    
    // Execute a task
    AgentResult execute(const std::string& task, 
                        const std::unordered_map<std::string, std::string>& context = {});
    
    // Cancel current execution
    void cancel();
    
    // Pause execution
    void pause();
    
    // Resume execution
    void resume();
    
    // Check if busy
    bool isBusy() const;
    
    // Get current task
    std::optional<std::string> getCurrentTask() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    AgentConfig config_;
    std::atomic<AgentState> state_{AgentState::Idle};
    mutable std::mutex mutex_;
    
    // Internal execution loop
    AgentResult executeLoop(const std::string& task,
                            const std::unordered_map<std::string, std::string>& context);
    
    // Think step - generate next action
    std::string think(const std::string& observation,
                      const std::unordered_map<std::string, std::string>& context);
    
    // Act step - execute tool calls
    std::string act(const std::string& action);
    
    // Reflect step - critique result
    bool reflect(const std::string& result, const std::string& task);
};

/**
 * @brief Manages multiple agents and their lifecycle
 */
class AgentManager {
public:
    static AgentManager& instance();
    
    // Initialize agent manager
    bool initialize();
    
    // Shutdown all agents
    void shutdown();
    
    // Stop all agents gracefully
    void stop();
    
    // Create a new agent
    std::string createAgent(const AgentConfig& config);
    
    // Get an agent by ID
    std::shared_ptr<Agent> getAgent(const std::string& agent_id);
    
    // Remove an agent
    bool removeAgent(const std::string& agent_id);
    
    // List all agents
    std::vector<std::string> listAgents() const;
    
    // Execute task on an agent
    AgentResult executeTask(const std::string& agent_id,
                            const std::string& task,
                            const std::unordered_map<std::string, std::string>& context = {});
    
    // Broadcast task to multiple agents
    std::unordered_map<std::string, AgentResult> broadcastTask(
        const std::vector<std::string>& agent_ids,
        const std::string& task,
        const std::unordered_map<std::string, std::string>& context = {});
    
    // Get statistics
    AgentStats getStats() const;
    
    // Set default model provider
    void setModelProvider(std::shared_ptr<ModelProvider> provider);
    
    // Register agent type factory
    using AgentFactory = std::function<std::shared_ptr<Agent>(const AgentConfig&)>;
    void registerAgentType(AgentType type, AgentFactory factory);
    
private:
    AgentManager() = default;
    ~AgentManager() = default;
    
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
};

} // namespace aios
