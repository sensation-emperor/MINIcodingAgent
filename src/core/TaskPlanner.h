#ifndef TASKPLANNER_H
#define TASKPLANNER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <functional>

namespace Brahma {

enum class TaskStatus {
    Pending,
    InProgress,
    Blocked,
    Completed,
    Failed,
    Cancelled
};

enum class TaskPriority {
    Low,
    Normal,
    High,
    Critical
};

enum class TaskType {
    Research,
    Planning,
    Coding,
    Testing,
    Review,
    Debugging,
    Documentation,
    Deployment
};

struct TaskDependency {
    QString taskId;
    QString dependencyType; // "blocks", "requires", "relates"
};

struct TaskResource {
    QString resourceId;
    QString resourceType; // "file", "url", "model", "agent"
    QString accessMode; // "read", "write", "execute"
};

struct Task {
    QString id;
    QString title;
    QString description;
    TaskType type;
    TaskPriority priority;
    TaskStatus status;
    QString assignedAgent;
    QList<TaskDependency> dependencies;
    QList<TaskResource> resources;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime startedAt;
    QDateTime completedAt;
    int estimatedMinutes;
    int actualMinutes;
    int progressPercent;
    QString errorMessage;
    QJsonObject metadata;
    QList<QString> subtasks;
    QList<QString> tags;
    
    Task() : type(TaskType::Planning), priority(TaskPriority::Normal), 
             status(TaskStatus::Pending), estimatedMinutes(0), 
             actualMinutes(0), progressPercent(0) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        createdAt = QDateTime::currentDateTime();
        updatedAt = createdAt;
    }
    
    QJsonObject toJson() const;
    static Task fromJson(const QJsonObject& json);
};

class TaskPlanner : public QObject {
    Q_OBJECT
    
public:
    explicit TaskPlanner(QObject* parent = nullptr);
    
    // Task Management
    QString createTask(const QString& title, const QString& description,
                       TaskType type, TaskPriority priority = TaskPriority::Normal);
    bool deleteTask(const QString& taskId);
    bool updateTask(const QString& taskId, const Task& updatedTask);
    Task getTask(const QString& taskId) const;
    QList<Task> getAllTasks() const;
    QList<Task> getTasksByStatus(TaskStatus status) const;
    QList<Task> getTasksByAgent(const QString& agentId) const;
    QList<Task> getTasksByType(TaskType type) const;
    QList<Task> getBlockedTasks() const;
    
    // Status Management
    bool startTask(const QString& taskId);
    bool completeTask(const QString& taskId, int actualMinutes = 0);
    bool failTask(const QString& taskId, const QString& errorMessage);
    bool cancelTask(const QString& taskId);
    bool updateProgress(const QString& taskId, int percent);
    
    // Dependency Management
    bool addDependency(const QString& taskId, const QString& dependsOnId, 
                       const QString& dependencyType = "requires");
    bool removeDependency(const QString& taskId, const QString& dependsOnId);
    bool areDependenciesMet(const QString& taskId) const;
    QList<Task> getReadyTasks() const; // Tasks with all dependencies met
    
    // Planning & Scheduling
    QList<Task> generateSubtasks(const QString& parentTaskId, 
                                 const QList<QString>& subtaskTitles);
    TaskPriority calculateCriticalPath() const;
    int estimateTotalTime() const;
    int calculateCompletionPercentage() const;
    
    // Persistence
    bool saveToFile(const QString& filePath);
    bool loadFromFile(const QString& filePath);
    QJsonDocument exportToJson() const;
    void importFromJson(const QJsonDocument& doc);
    
    // Utilities
    void clearAll();
    int getActiveTaskCount() const;
    int getCompletedTaskCount() const;
    QStringList getAvailableAgents() const;
    
signals:
    void taskCreated(const QString& taskId);
    void taskUpdated(const QString& taskId);
    void taskDeleted(const QString& taskId);
    void taskStatusChanged(const QString& taskId, TaskStatus oldStatus, TaskStatus newStatus);
    void taskStarted(const QString& taskId);
    void taskCompleted(const QString& taskId);
    void taskFailed(const QString& taskId, const QString& error);
    void dependencyAdded(const QString& taskId, const QString& dependencyId);
    void progressUpdated(const QString& taskId, int percent);
    void planReady();
    
private:
    QMap<QString, Task> m_tasks;
    mutable QMutex m_mutex;
    
    bool validateTask(const Task& task, QString& errorMsg) const;
    void updateTimestamp(const QString& taskId);
    TaskStatus getNextStatus(TaskStatus current) const;
};

} // namespace Brahma

#endif // TASKPLANNER_H
