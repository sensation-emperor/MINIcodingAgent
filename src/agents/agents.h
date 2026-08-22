#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <optional>
#include <chrono>

namespace aios {

// Forward declarations
class ModelProvider;
class ToolRegistry;
class EventBus;
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
    AgentType type = AgentType::Generic;
    std::string model_name;
    std::string system_prompt;
    double temperature = 0.7;
    int max_tokens = 4096;
    int max_iterations = 20;
    AgentCapabilities capabilities;
    std::chrono::milliseconds timeout{60000};
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
    size_t total_tasks = 0;
    size_t completed_tasks = 0;
    size_t failed_tasks = 0;
    size_t cancelled_tasks = 0;
    double average_iterations = 0.0;
    double average_execution_time_ms = 0.0;
};

/**
 * @brief Base autonomous agent that perceives, plans, acts, and reflects.
 */
class Agent : public std::enable_shared_from_this<Agent> {
public:
    explicit Agent(AgentConfig config);
    virtual ~Agent();
    
    // Configuration & State
    const AgentConfig& getConfig() const { return config_; }
    AgentConfig& getConfigRef() { return config_; }
    AgentState getState() const { return state_; }
    const std::string& getId() const { return config_.id; }
    AgentType getType() const { return config_.type; }
    
    // Providers & Dependencies
    void setModelProvider(std::shared_ptr<ModelProvider> provider);
    std::shared_ptr<ModelProvider> getModelProvider() const;
    
    void setToolRegistry(std::shared_ptr<ToolRegistry> registry);
    std::shared_ptr<ToolRegistry> getToolRegistry() const;
    
    void setEventBus(std::shared_ptr<EventBus> event_bus);
    std::shared_ptr<EventBus> getEventBus() const;

    // Observability Callbacks
    using StateChangeCallback = std::function<void(AgentState old_state, AgentState new_state)>;
    using ToolCallCallback = std::function<void(const std::string& tool_name, const std::unordered_map<std::string, std::string>& params)>;
    using ThoughtCallback = std::function<void(const std::string& thought)>;

    void onStateChange(StateChangeCallback callback);
    void onToolCall(ToolCallCallback callback);
    void onThought(ThoughtCallback callback);

    // Execution
    virtual AgentResult execute(const std::string& task, 
                                const std::unordered_map<std::string, std::string>& context = {});
    
    void cancel();
    void pause();
    void resume();
    bool isBusy() const;
    std::optional<std::string> getCurrentTask() const;

protected:
    // Lifecycle hooks for specialized agents
    virtual std::string buildSystemPrompt() const;
    virtual std::string think(const std::string& observation,
                              const std::unordered_map<std::string, std::string>& context);
    virtual std::string act(const std::string& action);
    virtual bool reflect(const std::string& result, const std::string& task);
    
    void setState(AgentState new_state);
    void publishEvent(const std::string& event_name, const std::string& json_data);

    AgentConfig config_;
    std::atomic<AgentState> state_{AgentState::Idle};
    mutable std::mutex mutex_;

    struct Impl;
    std::unique_ptr<Impl> impl_;

private:
    AgentResult executeLoop(const std::string& task,
                            const std::unordered_map<std::string, std::string>& context);
};

/**
 * @brief Manages multiple agents and their lifecycle
 */
class AgentManager {
public:
    static AgentManager& instance();
    
    bool initialize();
    void shutdown();
    void stop();
    
    // Agent lifecycle
    std::string createAgent(const AgentConfig& config);
    std::shared_ptr<Agent> getAgent(const std::string& agent_id);
    bool removeAgent(const std::string& agent_id);
    std::vector<std::string> listAgents() const;
    
    // Execution
    AgentResult executeTask(const std::string& agent_id,
                            const std::string& task,
                            const std::unordered_map<std::string, std::string>& context = {});
    
    std::unordered_map<std::string, AgentResult> broadcastTask(
        const std::vector<std::string>& agent_ids,
        const std::string& task,
        const std::unordered_map<std::string, std::string>& context = {});
    
    AgentStats getStats() const;
    
    // Dependencies injection
    void setModelProvider(std::shared_ptr<ModelProvider> provider);
    std::shared_ptr<ModelProvider> getModelProvider() const;

    void setToolRegistry(std::shared_ptr<ToolRegistry> registry);
    std::shared_ptr<ToolRegistry> getToolRegistry() const;

    void setEventBus(std::shared_ptr<EventBus> event_bus);
    std::shared_ptr<EventBus> getEventBus() const;
    
    // Factory registration
    using AgentFactory = std::function<std::shared_ptr<Agent>(const AgentConfig&)>;
    void registerAgentType(AgentType type, AgentFactory factory);
    
private:
    AgentManager();
    ~AgentManager();
    
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
};

} // namespace aios
