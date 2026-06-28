// AIOS - MINI Coding Agent Operating System
// Scheduler - Manages task scheduling, priorities, and execution

#pragma once

#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <set>

namespace aios {

enum class TaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

enum class TaskState {
    Pending,
    Running,
    Paused,
    Completed,
    Failed,
    Cancelled
};

struct Task {
    std::string id;
    std::string name;
    std::string description;
    TaskPriority priority;
    TaskState state;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::function<void()> work;
    std::function<void(const std::exception&)> on_error;
    std::function<void()> on_complete;
    std::string result;
    std::string error_message;
    size_t retry_count;
    size_t max_retries;
    std::chrono::milliseconds timeout;
    std::set<std::string> dependencies;
    std::string agent_id;
    bool cancellable;
};

struct SchedulerStats {
    size_t total_tasks;
    size_t pending_tasks;
    size_t running_tasks;
    size_t completed_tasks;
    size_t failed_tasks;
    size_t cancelled_tasks;
    size_t worker_threads;
    double average_execution_time_ms;
};

class Scheduler {
public:
    explicit Scheduler(size_t num_workers = 4);
    ~Scheduler();
    
    // Initialize scheduler
    bool initialize();
    
    // Shutdown scheduler gracefully
    void shutdown();
    
    // Stop accepting new tasks and wait for completion
    void stop();
    
    // Submit a task
    std::string submit(std::function<void()> work,
                       const std::string& name = "",
                       TaskPriority priority = TaskPriority::Normal);
    
    // Submit a task with full configuration
    std::string submit(Task task);
    
    // Submit a task with dependencies
    std::string submitWithDeps(std::function<void()> work,
                               const std::set<std::string>& dependencies,
                               const std::string& name = "",
                               TaskPriority priority = TaskPriority::Normal);
    
    // Cancel a task
    bool cancel(const std::string& task_id);
    
    // Pause a task
    bool pause(const std::string& task_id);
    
    // Resume a paused task
    bool resume(const std::string& task_id);
    
    // Get task status
    std::optional<TaskState> getTaskState(const std::string& task_id) const;
    
    // Get task details
    std::optional<Task> getTask(const std::string& task_id) const;
    
    // Wait for task completion
    bool waitForTask(const std::string& task_id, 
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    
    // Wait for all tasks
    void waitForAll();
    
    // Get scheduler statistics
    SchedulerStats getStats() const;
    
    // Set number of worker threads
    void setWorkerCount(size_t count);
    
    // Get current worker count
    size_t getWorkerCount() const;
    
    // Clear completed tasks
    void clearCompleted();
    
    // Get pending tasks count
    size_t getPendingCount() const;
    
    // Get running tasks count
    size_t getRunningCount() const;
    
    // Check if scheduler is running
    bool isRunning() const { return running_; }
    
    // Set default timeout for tasks
    void setDefaultTimeout(std::chrono::milliseconds timeout);
    
    // Set default max retries
    void setDefaultMaxRetries(size_t retries);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    size_t num_workers_;
    std::chrono::milliseconds default_timeout_{30000};
    size_t default_max_retries_{3};
    
    void workerThread();
    void executeTask(Task& task);
    bool canExecute(const Task& task) const;
    std::string generateId() const;
    void updateTaskState(const std::string& task_id, TaskState state);
};

} // namespace aios
