// AIOS - MINI Coding Agent Operating System
// Planner Implementation

#include "planner/planner.h"
#include "logging/Logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace aios {

struct Planner::Impl {
    std::unordered_map<std::string, Plan> plans;
    
    // Statistics
    size_t total_created = 0;
    size_t total_completed = 0;
    size_t total_failed = 0;
    double total_duration_ms = 0.0;
    size_t total_steps_executed = 0;
};

Planner::Planner() : impl_(std::make_unique<Impl>()) {}

Planner::~Planner() {
    stop();
}

bool Planner::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        LOG_WARN("Planner already initialized");
        return true;
    }
    
    LOG_INFO("Initializing Planner...");
    running_ = true;
    stopping_ = false;
    
    LOG_INFO("Planner initialized successfully");
    return true;
}

void Planner::shutdown() {
    stop();
}

void Planner::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return;
    }
    
    LOG_INFO("Stopping Planner...");
    stopping_ = true;
    running_ = false;
    
    LOG_INFO("Planner stopped");
}

std::string Planner::createPlan(const std::string& goal, 
                                 const std::string& description) {
    return createPlan(goal, {}, description);
}

std::string Planner::createPlan(const std::string& goal,
                                 std::vector<PlanStep> steps,
                                 const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stopping_) {
        LOG_WARN("Planner is stopping, rejecting new plan");
        return "";
    }
    
    Plan plan;
    plan.id = generateId();
    plan.goal = goal;
    plan.description = description;
    plan.steps = std::move(steps);
    plan.status = PlanStatus::Pending;
    plan.created_at = std::chrono::system_clock::now();
    plan.total_steps = static_cast<int>(plan.steps.size());
    plan.completed_steps = 0;
    
    // Generate IDs for steps if not set
    for (auto& step : plan.steps) {
        if (step.id.empty()) {
            step.id = generateId();
        }
        step.status = PlanStatus::Pending;
        step.retry_count = 0;
        if (step.max_retries == 0) {
            step.max_retries = 3;
        }
    }
    
    impl_->plans[plan.id] = std::move(plan);
    impl_->total_created++;
    
    LOG_INFO("Plan created: id={}, goal={}", plan.id, goal);
    return plan.id;
}

bool Planner::executePlan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        LOG_ERROR("Plan not found: {}", plan_id);
        return false;
    }
    
    Plan& plan = it->second;
    
    if (plan.status == PlanStatus::Completed || 
        plan.status == PlanStatus::Cancelled) {
        LOG_WARN("Cannot execute plan in status: {}", static_cast<int>(plan.status));
        return false;
    }
    
    plan.status = PlanStatus::InProgress;
    plan.started_at = std::chrono::system_clock::now();
    
    LOG_INFO("Executing plan: id={}, goal={}", plan_id, plan.goal);
    
    // Execute all pending steps in order
    while (plan.status == PlanStatus::InProgress) {
        if (!executeNextStep(plan_id)) {
            break;
        }
    }
    
    return plan.status == PlanStatus::Completed;
}

bool Planner::executeNextStep(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    if (plan.status != PlanStatus::InProgress && plan.status != PlanStatus::Pending) {
        return false;
    }
    
    // Find next executable step
    PlanStep* next_step = nullptr;
    for (auto& step : plan.steps) {
        if (step.status == PlanStatus::Pending && canExecuteStep(plan, step)) {
            next_step = &step;
            break;
        }
    }
    
    if (!next_step) {
        // Check if all steps are complete
        bool all_complete = true;
        bool any_failed = false;
        
        for (const auto& step : plan.steps) {
            if (step.status != PlanStatus::Completed) {
                all_complete = false;
            }
            if (step.status == PlanStatus::Failed) {
                any_failed = true;
            }
        }
        
        if (all_complete) {
            plan.status = PlanStatus::Completed;
            plan.completed_at = std::chrono::system_clock::now();
            impl_->total_completed++;
            
            auto duration = std::chrono::duration<double, std::milli>(
                plan.completed_at - plan.started_at).count();
            impl_->total_duration_ms += duration;
            
            LOG_INFO("Plan completed: id={}, duration={:.2f}ms", plan_id, duration);
            
            if (on_complete_) {
                on_complete_(plan);
            }
        } else if (any_failed) {
            plan.status = PlanStatus::Failed;
            impl_->total_failed++;
            
            LOG_ERROR("Plan failed: id={}", plan_id);
            
            if (on_fail_) {
                on_fail_(plan);
            }
        }
        
        return false;
    }
    
    // Execute the step
    next_step->status = PlanStatus::InProgress;
    next_step->started_at = std::chrono::system_clock::now();
    plan.current_step_id = next_step->id;
    
    LOG_DEBUG("Executing step: plan={}, step={}, action={}", 
              plan_id, next_step->id, next_step->action);
    
    bool success = true;
    if (step_executor_) {
        success = step_executor_(plan_id, *next_step);
    }
    
    impl_->total_steps_executed++;
    
    if (success) {
        next_step->status = PlanStatus::Completed;
        next_step->completed_at = std::chrono::system_clock::now();
        plan.completed_steps++;
        recalculatePlanProgress(plan);
        
        LOG_DEBUG("Step completed: plan={}, step={}", plan_id, next_step->id);
    } else {
        if (next_step->retry_count < next_step->max_retries) {
            next_step->retry_count++;
            next_step->status = PlanStatus::Pending;
            
            LOG_WARN("Step failed, will retry ({}/{}): plan={}, step={}", 
                     next_step->retry_count, next_step->max_retries, 
                     plan_id, next_step->id);
        } else {
            next_step->status = PlanStatus::Failed;
            next_step->error_message = "Step execution failed after retries";
            
            LOG_ERROR("Step failed permanently: plan={}, step={}", plan_id, next_step->id);
        }
    }
    
    return true;
}

bool Planner::pausePlan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    if (plan.status != PlanStatus::InProgress) {
        return false;
    }
    
    plan.status = PlanStatus::Pending;
    LOG_INFO("Plan paused: id={}", plan_id);
    return true;
}

bool Planner::resumePlan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    if (plan.status != PlanStatus::Pending) {
        return false;
    }
    
    plan.status = PlanStatus::InProgress;
    LOG_INFO("Plan resumed: id={}", plan_id);
    return true;
}

bool Planner::cancelPlan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    if (plan.status == PlanStatus::Completed || 
        plan.status == PlanStatus::Cancelled) {
        return false;
    }
    
    plan.status = PlanStatus::Cancelled;
    plan.completed_at = std::chrono::system_clock::now();
    
    for (auto& step : plan.steps) {
        if (step.status == PlanStatus::Pending || 
            step.status == PlanStatus::InProgress) {
            step.status = PlanStatus::Cancelled;
        }
    }
    
    LOG_INFO("Plan cancelled: id={}", plan_id);
    return true;
}

std::optional<Plan> Planner::getPlan(const std::string& plan_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

std::optional<PlanStatus> Planner::getPlanStatus(const std::string& plan_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return std::nullopt;
    }
    
    return it->second.status;
}

std::optional<PlanStep> Planner::getCurrentStep(const std::string& plan_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return std::nullopt;
    }
    
    const Plan& plan = it->second;
    
    for (const auto& step : plan.steps) {
        if (step.id == plan.current_step_id) {
            return step;
        }
    }
    
    return std::nullopt;
}

bool Planner::updateStepResult(const std::string& plan_id,
                                const std::string& step_id,
                                const std::string& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    for (auto& step : plan.steps) {
        if (step.id == step_id) {
            step.result = result;
            return true;
        }
    }
    
    return false;
}

bool Planner::completeStep(const std::string& plan_id,
                            const std::string& step_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    for (auto& step : plan.steps) {
        if (step.id == step_id) {
            step.status = PlanStatus::Completed;
            step.completed_at = std::chrono::system_clock::now();
            plan.completed_steps++;
            recalculatePlanProgress(plan);
            return true;
        }
    }
    
    return false;
}

bool Planner::failStep(const std::string& plan_id,
                        const std::string& step_id,
                        const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    Plan& plan = it->second;
    
    for (auto& step : plan.steps) {
        if (step.id == step_id) {
            step.status = PlanStatus::Failed;
            step.error_message = error;
            return true;
        }
    }
    
    return false;
}

PlannerStats Planner::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PlannerStats stats{};
    stats.total_plans = impl_->total_created;
    stats.completed_plans = impl_->total_completed;
    stats.failed_plans = impl_->total_failed;
    stats.total_steps_executed = impl_->total_steps_executed;
    
    for (const auto& [id, plan] : impl_->plans) {
        if (plan.status == PlanStatus::InProgress || 
            plan.status == PlanStatus::Pending) {
            stats.active_plans++;
        }
    }
    
    if (stats.completed_plans > 0) {
        stats.average_plan_duration_ms = impl_->total_duration_ms / stats.completed_plans;
    }
    
    return stats;
}

void Planner::setStepExecutor(StepExecutor executor) {
    std::lock_guard<std::mutex> lock(mutex_);
    step_executor_ = std::move(executor);
}

void Planner::setOnPlanComplete(PlanCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_complete_ = std::move(callback);
}

void Planner::setOnPlanFail(PlanCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_fail_ = std::move(callback);
}

std::vector<PlanStep> Planner::getPendingSteps(const std::string& plan_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PlanStep> pending;
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return pending;
    }
    
    const Plan& plan = it->second;
    
    for (const auto& step : plan.steps) {
        if (step.status == PlanStatus::Pending && canExecuteStep(plan, step)) {
            pending.push_back(step);
        }
    }
    
    return pending;
}

bool Planner::addContext(const std::string& plan_id,
                          const std::string& key,
                          const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return false;
    }
    
    it->second.context[key] = value;
    return true;
}

std::optional<std::string> Planner::getContext(const std::string& plan_id,
                                                 const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it == impl_->plans.end()) {
        return std::nullopt;
    }
    
    auto ctx_it = it->second.context.find(key);
    if (ctx_it == it->second.context.end()) {
        return std::nullopt;
    }
    
    return ctx_it->second;
}

std::string Planner::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    static std::atomic<uint64_t> counter{0};
    
    auto id = dist(gen) ^ (counter++ << 32);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << id;
    return ss.str();
}

void Planner::updatePlanStatus(const std::string& plan_id, PlanStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->plans.find(plan_id);
    if (it != impl_->plans.end()) {
        it->second.status = status;
    }
}

bool Planner::canExecuteStep(const Plan& plan, const PlanStep& step) const {
    // Check if all dependencies are completed
    for (const auto& dep_id : step.dependencies) {
        bool found = false;
        bool completed = false;
        
        for (const auto& s : plan.steps) {
            if (s.id == dep_id) {
                found = true;
                if (s.status == PlanStatus::Completed) {
                    completed = true;
                }
                break;
            }
        }
        
        if (!found || !completed) {
            return false;
        }
    }
    
    return true;
}

void Planner::recalculatePlanProgress(Plan& plan) {
    int completed = 0;
    for (const auto& step : plan.steps) {
        if (step.status == PlanStatus::Completed) {
            completed++;
        }
    }
    plan.completed_steps = completed;
}

} // namespace aios
