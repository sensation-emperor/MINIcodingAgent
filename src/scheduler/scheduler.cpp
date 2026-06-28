// AIOS - MINI Coding Agent Operating System
// Scheduler Implementation

#include "scheduler/scheduler.h"
#include "logging/Logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace aios {

struct Scheduler::Impl {
    std::unordered_map<std::string, Task> tasks;
    std::priority_queue<std::pair<int, std::string>, 
                        std::vector<std::pair<int, std::string>>,
                        std::greater<>> task_queue;
    std::vector<std::thread> workers;
    
    // Statistics
    size_t total_submitted = 0;
    size_t total_completed = 0;
    size_t total_failed = 0;
    size_t total_cancelled = 0;
    double total_execution_time_ms = 0.0;
};

Scheduler::Scheduler(size_t num_workers) : impl_(std::make_unique<Impl>()), num_workers_(num_workers) {}

Scheduler::~Scheduler() {
    stop();
}

bool Scheduler::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        LOG_WARN("Scheduler already initialized");
        return true;
    }
    
    LOG_INFO("Initializing Scheduler with {} worker threads", num_workers_);
    
    running_ = true;
    stopping_ = false;
    
    // Start worker threads
    for (size_t i = 0; i < num_workers_; ++i) {
        impl_->workers.emplace_back(&Scheduler::workerThread, this);
    }
    
    LOG_INFO("Scheduler initialized successfully");
    return true;
}

void Scheduler::shutdown() {
    stop();
}

void Scheduler::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) {
            return;
        }
        
        LOG_INFO("Stopping Scheduler...");
        stopping_ = true;
    }
    
    cv_.notify_all();
    
    // Wait for all workers to finish
    for (auto& worker : impl_->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    impl_->workers.clear();
    running_ = false;
    
    LOG_INFO("Scheduler stopped");
}

std::string Scheduler::submit(std::function<void()> work,
                               const std::string& name,
                               TaskPriority priority) {
    Task task;
    task.name = name;
    task.work = std::move(work);
    task.priority = priority;
    task.state = TaskState::Pending;
    task.created_at = std::chrono::system_clock::now();
    task.retry_count = 0;
    task.max_retries = default_max_retries_;
    task.timeout = default_timeout_;
    task.cancellable = true;
    
    return submit(std::move(task));
}

std::string Scheduler::submit(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stopping_) {
        LOG_WARN("Scheduler is stopping, rejecting new task");
        return "";
    }
    
    task.id = generateId();
    task.state = TaskState::Pending;
    task.created_at = std::chrono::system_clock::now();
    
    if (task.max_retries == 0) {
        task.max_retries = default_max_retries_;
    }
    
    if (task.timeout.count() == 0) {
        task.timeout = default_timeout_;
    }
    
    impl_->tasks[task.id] = std::move(task);
    impl_->total_submitted++;
    
    // Add to priority queue (higher priority = lower number in min-heap)
    int priority_value = static_cast<int>(impl_->tasks[task.id].priority);
    impl_->task_queue.emplace(-priority_value, task.id);
    
    cv_.notify_one();
    
    LOG_DEBUG("Task submitted: id={}, name={}, priority={}", 
              task.id, task.name, priority_value);
    
    return task.id;
}

std::string Scheduler::submitWithDeps(std::function<void()> work,
                                       const std::set<std::string>& dependencies,
                                       const std::string& name,
                                       TaskPriority priority) {
    Task task;
    task.name = name;
    task.work = std::move(work);
    task.priority = priority;
    task.dependencies = dependencies;
    
    return submit(std::move(task));
}

bool Scheduler::cancel(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->tasks.find(task_id);
    if (it == impl_->tasks.end()) {
        return false;
    }
    
    Task& task = it->second;
    
    if (!task.cancellable) {
        LOG_WARN("Task {} is not cancellable", task_id);
        return false;
    }
    
    if (task.state == TaskState::Completed || 
        task.state == TaskState::Cancelled) {
        return false;
    }
    
    task.state = TaskState::Cancelled;
    impl_->total_cancelled++;
    
    LOG_INFO("Task cancelled: id={}", task_id);
    return true;
}

bool Scheduler::pause(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->tasks.find(task_id);
    if (it == impl_->tasks.end()) {
        return false;
    }
    
    Task& task = it->second;
    
    if (task.state != TaskState::Running && task.state != TaskState::Pending) {
        return false;
    }
    
    task.state = TaskState::Paused;
    LOG_DEBUG("Task paused: id={}", task_id);
    return true;
}

bool Scheduler::resume(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->tasks.find(task_id);
    if (it == impl_->tasks.end()) {
        return false;
    }
    
    Task& task = it->second;
    
    if (task.state != TaskState::Paused) {
        return false;
    }
    
    task.state = TaskState::Pending;
    
    // Re-add to queue
    int priority_value = static_cast<int>(task.priority);
    impl_->task_queue.emplace(-priority_value, task_id);
    
    cv_.notify_one();
    
    LOG_DEBUG("Task resumed: id={}", task_id);
    return true;
}

std::optional<TaskState> Scheduler::getTaskState(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->tasks.find(task_id);
    if (it == impl_->tasks.end()) {
        return std::nullopt;
    }
    
    return it->second.state;
}

std::optional<Task> Scheduler::getTask(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->tasks.find(task_id);
    if (it == impl_->tasks.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

bool Scheduler::waitForTask(const std::string& task_id, 
                             std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = impl_->tasks.find(task_id);
            if (it == impl_->tasks.end()) {
                return false;
            }
            
            TaskState state = it->second.state;
            
            if (state == TaskState::Completed) {
                return true;
            }
            if (state == TaskState::Failed || state == TaskState::Cancelled) {
                return false;
            }
        }
        
        if (timeout.count() > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= timeout) {
                return false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Scheduler::waitForAll() {
    while (true) {
        bool has_pending = false;
        bool has_running = false;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            for (const auto& [id, task] : impl_->tasks) {
                if (task.state == TaskState::Pending || 
                    task.state == TaskState::Running ||
                    task.state == TaskState::Paused) {
                    has_pending = true;
                    break;
                }
            }
        }
        
        if (!has_pending) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

SchedulerStats Scheduler::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SchedulerStats stats{};
    stats.total_tasks = impl_->total_submitted;
    stats.worker_threads = num_workers_;
    
    for (const auto& [id, task] : impl_->tasks) {
        switch (task.state) {
            case TaskState::Pending:
            case TaskState::Paused:
                stats.pending_tasks++;
                break;
            case TaskState::Running:
                stats.running_tasks++;
                break;
            case TaskState::Completed:
                stats.completed_tasks++;
                break;
            case TaskState::Failed:
                stats.failed_tasks++;
                break;
            case TaskState::Cancelled:
                stats.cancelled_tasks++;
                break;
        }
    }
    
    if (stats.completed_tasks > 0) {
        stats.average_execution_time_ms = impl_->total_execution_time_ms / stats.completed_tasks;
    }
    
    return stats;
}

void Scheduler::setWorkerCount(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        LOG_WARN("Cannot change worker count while running");
        return;
    }
    
    num_workers_ = count;
}

size_t Scheduler::getWorkerCount() const {
    return num_workers_;
}

void Scheduler::clearCompleted() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> to_remove;
    
    for (const auto& [id, task] : impl_->tasks) {
        if (task.state == TaskState::Completed || 
            task.state == TaskState::Cancelled) {
            to_remove.push_back(id);
        }
    }
    
    for (const auto& id : to_remove) {
        impl_->tasks.erase(id);
    }
    
    LOG_DEBUG("Cleared {} completed tasks", to_remove.size());
}

size_t Scheduler::getPendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [id, task] : impl_->tasks) {
        if (task.state == TaskState::Pending || task.state == TaskState::Paused) {
            count++;
        }
    }
    return count;
}

size_t Scheduler::getRunningCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [id, task] : impl_->tasks) {
        if (task.state == TaskState::Running) {
            count++;
        }
    }
    return count;
}

void Scheduler::setDefaultTimeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_timeout_ = timeout;
}

void Scheduler::setDefaultMaxRetries(size_t retries) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_max_retries_ = retries;
}

void Scheduler::workerThread() {
    while (!stopping_) {
        std::string task_id;
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            cv_.wait(lock, [this]() {
                return stopping_ || !impl_->task_queue.empty();
            });
            
            if (stopping_) {
                break;
            }
            
            // Get highest priority task
            while (!impl_->task_queue.empty()) {
                auto [neg_priority, id] = impl_->task_queue.top();
                impl_->task_queue.pop();
                
                auto it = impl_->tasks.find(id);
                if (it != impl_->tasks.end() && it->second.state == TaskState::Pending) {
                    if (canExecute(it->second)) {
                        task_id = id;
                        it->second.state = TaskState::Running;
                        it->second.started_at = std::chrono::system_clock::now();
                        break;
                    }
                }
            }
        }
        
        if (!task_id.empty()) {
            Task* task_ptr = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = impl_->tasks.find(task_id);
                if (it != impl_->tasks.end()) {
                    task_ptr = &it->second;
                }
            }
            
            if (task_ptr) {
                executeTask(*task_ptr);
            }
        }
    }
}

void Scheduler::executeTask(Task& task) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        LOG_DEBUG("Executing task: id={}, name={}", task.id, task.name);
        
        if (task.work) {
            task.work();
        }
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            task.state = TaskState::Completed;
            task.completed_at = std::chrono::system_clock::now();
            impl_->total_completed++;
            impl_->total_execution_time_ms += duration;
        }
        
        if (task.on_complete) {
            task.on_complete();
        }
        
        LOG_DEBUG("Task completed: id={}, duration={:.2f}ms", task.id, duration);
        
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            task.error_message = e.what();
            
            if (task.retry_count < task.max_retries) {
                task.retry_count++;
                task.state = TaskState::Pending;
                
                // Re-add to queue with backoff
                int priority_value = static_cast<int>(task.priority);
                impl_->task_queue.emplace(-priority_value, task.id);
                
                LOG_WARN("Task failed, retrying ({}/{}): id={}, error={}", 
                         task.retry_count, task.max_retries, task.id, e.what());
            } else {
                task.state = TaskState::Failed;
                impl_->total_failed++;
                
                if (task.on_error) {
                    task.on_error(e);
                }
                
                LOG_ERROR("Task failed permanently: id={}, error={}", task.id, e.what());
            }
        }
    }
}

bool Scheduler::canExecute(const Task& task) const {
    // Check if all dependencies are completed
    for (const auto& dep_id : task.dependencies) {
        auto it = impl_->tasks.find(dep_id);
        if (it == impl_->tasks.end()) {
            LOG_WARN("Dependency not found: {}", dep_id);
            return false;
        }
        
        if (it->second.state != TaskState::Completed) {
            return false;
        }
    }
    
    return true;
}

std::string Scheduler::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    static std::atomic<uint64_t> counter{0};
    
    auto id = dist(gen) ^ (counter++ << 32);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << id;
    return ss.str();
}

void Scheduler::updateTaskState(const std::string& task_id, TaskState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->tasks.find(task_id);
    if (it != impl_->tasks.end()) {
        it->second.state = state;
    }
}

} // namespace aios
