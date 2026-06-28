// AIOS - MINI Coding Agent Operating System
// Planner - Generates plans, breaks down tasks, and manages planning strategies

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

namespace aios {

enum class PlanStatus {
    Pending,
    InProgress,
    Completed,
    Failed,
    Cancelled
};

struct PlanStep {
    std::string id;
    std::string description;
    std::string action;
    std::vector<std::string> dependencies;  // IDs of steps that must complete first
    std::unordered_map<std::string, std::string> parameters;
    PlanStatus status;
    std::string result;
    std::string error_message;
    int retry_count;
    int max_retries;
    std::chrono::milliseconds estimated_duration;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
};

struct Plan {
    std::string id;
    std::string goal;
    std::string description;
    std::vector<PlanStep> steps;
    PlanStatus status;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::unordered_map<std::string, std::string> context;
    std::string current_step_id;
    int completed_steps;
    int total_steps;
};

struct PlannerStats {
    size_t total_plans;
    size_t active_plans;
    size_t completed_plans;
    size_t failed_plans;
    size_t total_steps_executed;
    double average_plan_duration_ms;
};

class Planner {
public:
    Planner();
    ~Planner();
    
    // Initialize planner
    bool initialize();
    
    // Shutdown planner
    void shutdown();
    
    // Stop all planning operations
    void stop();
    
    // Create a new plan from a goal
    std::string createPlan(const std::string& goal, 
                           const std::string& description = "");
    
    // Create a plan with custom steps
    std::string createPlan(const std::string& goal,
                           std::vector<PlanStep> steps,
                           const std::string& description = "");
    
    // Execute a plan
    bool executePlan(const std::string& plan_id);
    
    // Execute next step in a plan
    bool executeNextStep(const std::string& plan_id);
    
    // Pause plan execution
    bool pausePlan(const std::string& plan_id);
    
    // Resume paused plan
    bool resumePlan(const std::string& plan_id);
    
    // Cancel a plan
    bool cancelPlan(const std::string& plan_id);
    
    // Get plan details
    std::optional<Plan> getPlan(const std::string& plan_id) const;
    
    // Get plan status
    std::optional<PlanStatus> getPlanStatus(const std::string& plan_id) const;
    
    // Get current step
    std::optional<PlanStep> getCurrentStep(const std::string& plan_id) const;
    
    // Update step result
    bool updateStepResult(const std::string& plan_id, 
                          const std::string& step_id,
                          const std::string& result);
    
    // Mark step as complete
    bool completeStep(const std::string& plan_id,
                      const std::string& step_id);
    
    // Mark step as failed
    bool failStep(const std::string& plan_id,
                  const std::string& step_id,
                  const std::string& error);
    
    // Get planner statistics
    PlannerStats getStats() const;
    
    // Check if planner is running
    bool isRunning() const { return running_; }
    
    // Set step executor callback
    using StepExecutor = std::function<bool(const std::string& plan_id, const PlanStep& step)>;
    void setStepExecutor(StepExecutor executor);
    
    // Set plan completion callback
    using PlanCallback = std::function<void(const Plan&)>;
    void setOnPlanComplete(PlanCallback callback);
    void setOnPlanFail(PlanCallback callback);
    
    // Get pending steps (ready to execute)
    std::vector<PlanStep> getPendingSteps(const std::string& plan_id) const;
    
    // Add context to a plan
    bool addContext(const std::string& plan_id,
                    const std::string& key,
                    const std::string& value);
    
    // Get context from a plan
    std::optional<std::string> getContext(const std::string& plan_id,
                                           const std::string& key) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    StepExecutor step_executor_;
    PlanCallback on_complete_;
    PlanCallback on_fail_;
    
    std::string generateId() const;
    void updatePlanStatus(const std::string& plan_id, PlanStatus status);
    bool canExecuteStep(const Plan& plan, const PlanStep& step) const;
    void recalculatePlanProgress(Plan& plan);
};

} // namespace aios
