// AIOS - MINI Coding Agent Operating System
// Core Kernel Implementation

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace aios {

// Forward declarations
class Scheduler;
class Planner;
class MemoryManager;
class ContextEngine;
class AgentManager;
class EventBus;
class ConfigManager;

enum class KernelState {
    Idle,
    Initializing,
    Running,
    Paused,
    Stopping,
    Error
};

/**
 * @brief Main kernel orchestrating all subsystems
 * 
 * The kernel is the central component that manages:
 * - Lifecycle of all subsystems
 * - Event routing
 * - State management
 * - Resource coordination
 */
class Kernel {
public:
    static Kernel& instance();
    
    // Initialize all subsystems
    bool initialize(const std::string& configPath = "");
    
    // Start the main event loop
    void run();
    
    // Stop all subsystems gracefully
    void stop();
    
    // Pause execution
    void pause();
    
    // Resume from pause
    void resume();
    
    // Get current state
    KernelState getState() const { return state_; }
    
    // Check if running
    bool isRunning() const { return state_ == KernelState::Running; }
    
    // Get subsystem accessors
    std::shared_ptr<Scheduler> getScheduler() const { return scheduler_; }
    std::shared_ptr<Planner> getPlanner() const { return planner_; }
    std::shared_ptr<MemoryManager> getMemoryManager() const { return memory_; }
    std::shared_ptr<ContextEngine> getContextEngine() const { return context_; }
    std::shared_ptr<AgentManager> getAgentManager() const { return agents_; }
    std::shared_ptr<EventBus> getEventBus() const { return eventBus_; }
    
    // Register shutdown hook
    void onShutdown(std::function<void()> callback);
    
private:
    Kernel() = default;
    ~Kernel() = default;
    
    // Prevent copying
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;
    
    // Initialize individual subsystems
    bool initializeConfig();
    bool initializeEventBus();
    bool initializeMemory();
    bool initializeContext();
    bool initializePlanner();
    bool initializeScheduler();
    bool initializeAgents();
    bool initializeProviders();
    
    // Main event loop
    void eventLoop();
    
    // State management
    void setState(KernelState newState);
    void handleError(const std::string& error);
    
    // Subsystem pointers
    std::shared_ptr<ConfigManager> config_;
    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<MemoryManager> memory_;
    std::shared_ptr<ContextEngine> context_;
    std::shared_ptr<Planner> planner_;
    std::shared_ptr<Scheduler> scheduler_;
    std::shared_ptr<AgentManager> agents_;
    
    // State
    std::atomic<KernelState> state_{KernelState::Idle};
    std::atomic<bool> shouldStop_{false};
    
    // Shutdown callbacks
    std::vector<std::function<void()>> shutdownHooks_;
};

} // namespace aios
