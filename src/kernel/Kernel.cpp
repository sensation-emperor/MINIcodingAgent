#include "kernel/Kernel.h"
#include "config/ConfigManager.h"
#include "events/EventBus.h"
#include "memory/memory.h"
#include "context/ContextEngine.h"
#include "planner/planner.h"
#include "scheduler/scheduler.h"
#include "agents/agents.h"
#include "logging/Logger.h"

#include <iostream>
#include <thread>
#include <chrono>

namespace aios {

Kernel& Kernel::instance() {
    static Kernel instance;
    return instance;
}

bool Kernel::initialize(const std::string& configPath) {
    setState(KernelState::Initializing);
    
    LOG_INFO("Initializing AIOS Kernel...");
    
    try {
        if (!initializeConfig()) {
            handleError("Failed to initialize config manager");
            return false;
        }
        
        if (!initializeEventBus()) {
            handleError("Failed to initialize event bus");
            return false;
        }
        
        if (!initializeMemory()) {
            handleError("Failed to initialize memory manager");
            return false;
        }
        
        if (!initializeContext()) {
            handleError("Failed to initialize context engine");
            return false;
        }
        
        if (!initializePlanner()) {
            handleError("Failed to initialize planner");
            return false;
        }
        
        if (!initializeScheduler()) {
            handleError("Failed to initialize scheduler");
            return false;
        }
        
        if (!initializeAgents()) {
            handleError("Failed to initialize agent manager");
            return false;
        }
        
        if (!initializeProviders()) {
            handleError("Failed to initialize model providers");
            return false;
        }
        
        setState(KernelState::Idle);
        LOG_INFO("AIOS Kernel initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        handleError(std::string("Initialization error: ") + e.what());
        return false;
    }
}

void Kernel::run() {
    if (state_ != KernelState::Idle && state_ != KernelState::Paused) {
        LOG_WARN("Kernel not in idle or paused state, cannot run");
        return;
    }
    
    setState(KernelState::Running);
    shouldStop_ = false;
    
    LOG_INFO("Starting AIOS Kernel event loop...");
    
    eventLoop();
}

void Kernel::stop() {
    LOG_INFO("Stopping AIOS Kernel...");
    
    shouldStop_ = true;
    
    // Stop subsystems in reverse order
    if (agents_) agents_->stop();
    if (scheduler_) scheduler_->stop();
    if (planner_) planner_->stop();
    
    // Execute shutdown hooks
    for (auto& hook : shutdownHooks_) {
        try {
            hook();
        } catch (...) {
            LOG_ERROR("Exception in shutdown hook");
        }
    }
    
    setState(KernelState::Idle);
    LOG_INFO("AIOS Kernel stopped");
}

void Kernel::pause() {
    if (state_ == KernelState::Running) {
        setState(KernelState::Paused);
        LOG_INFO("AIOS Kernel paused");
    }
}

void Kernel::resume() {
    if (state_ == KernelState::Paused) {
        setState(KernelState::Running);
        LOG_INFO("AIOS Kernel resumed");
    }
}

void Kernel::onShutdown(std::function<void()> callback) {
    shutdownHooks_.push_back(std::move(callback));
}

bool Kernel::initializeConfig() {
    config_ = std::make_shared<ConfigManager>();
    return config_->load();
}

bool Kernel::initializeEventBus() {
    eventBus_ = std::make_shared<EventBus>();
    return eventBus_->initialize();
}

bool Kernel::initializeMemory() {
    memory_ = std::make_shared<MemoryManager>();
    return memory_->initialize();
}

bool Kernel::initializeContext() {
    context_ = std::make_shared<ContextEngine>();
    return context_->initialize();
}

bool Kernel::initializePlanner() {
    planner_ = std::make_shared<Planner>();
    return planner_->initialize();
}

bool Kernel::initializeScheduler() {
    scheduler_ = std::make_shared<Scheduler>();
    return scheduler_->initialize();
}

bool Kernel::initializeAgents() {
    agents_ = std::shared_ptr<AgentManager>(&AgentManager::instance(), [](AgentManager*){});
    return agents_->initialize();
}

bool Kernel::initializeProviders() {
    // Initialize model providers through the agent manager
    // This will be expanded based on configuration
    return true;
}

void Kernel::eventLoop() {
    while (!shouldStop_ && state_ == KernelState::Running) {
        // Process events from the event bus
        if (eventBus_) {
            eventBus_->processEvents();
        }
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Kernel::setState(KernelState newState) {
    state_ = newState;
    // Publish state change event
    if (eventBus_) {
        eventBus_->publish("kernel.state_changed", std::to_string(static_cast<int>(newState)));
    }
}

void Kernel::handleError(const std::string& error) {
    LOG_ERROR("Kernel error: {}", error);
    setState(KernelState::Error);
}

} // namespace aios
