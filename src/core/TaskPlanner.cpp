#include "TaskPlanner.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMutexLocker>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logTaskPlanner, "brahma.taskplanner")

namespace Brahma {

// Task JSON Serialization
QJsonObject Task::toJson() const {
    QJsonObject json;
    json["id"] = id;
    json["title"] = title;
    json["description"] = description;
    json["type"] = static_cast<int>(type);
    json["priority"] = static_cast<int>(priority);
    json["status"] = static_cast<int>(status);
    json["assignedAgent"] = assignedAgent;
    json["createdAt"] = createdAt.toString(Qt::ISODate);
    json["updatedAt"] = updatedAt.toString(Qt::ISODate);
    if (startedAt.isValid()) json["startedAt"] = startedAt.toString(Qt::ISODate);
    if (completedAt.isValid()) json["completedAt"] = completedAt.toString(Qt::ISODate);
    json["estimatedMinutes"] = estimatedMinutes;
    json["actualMinutes"] = actualMinutes;
    json["progressPercent"] = progressPercent;
    if (!errorMessage.isEmpty()) json["errorMessage"] = errorMessage;
    if (!metadata.isEmpty()) json["metadata"] = metadata;
    
    QJsonArray depsArray;
    for (const auto& dep : dependencies) {
        QJsonObject depObj;
        depObj["taskId"] = dep.taskId;
        depObj["dependencyType"] = dep.dependencyType;
        depsArray.append(depObj);
    }
    json["dependencies"] = depsArray;
    
    QJsonArray resArray;
    for (const auto& res : resources) {
        QJsonObject resObj;
        resObj["resourceId"] = res.resourceId;
        resObj["resourceType"] = res.resourceType;
        resObj["accessMode"] = res.accessMode;
        resArray.append(resObj);
    }
    json["resources"] = resArray;
    
    QJsonArray subtasksArray;
    for (const auto& sub : subtasks) {
        subtasksArray.append(sub);
    }
    json["subtasks"] = subtasksArray;
    
    QJsonArray tagsArray;
    for (const auto& tag : tags) {
        tagsArray.append(tag);
    }
    json["tags"] = tagsArray;
    
    return json;
}

Task Task::fromJson(const QJsonObject& json) {
    Task task;
    task.id = json["id"].toString();
    task.title = json["title"].toString();
    task.description = json["description"].toString();
    task.type = static_cast<TaskType>(json["type"].toInt(static_cast<int>(TaskType::Planning)));
    task.priority = static_cast<TaskPriority>(json["priority"].toInt(static_cast<int>(TaskPriority::Normal)));
    task.status = static_cast<TaskStatus>(json["status"].toInt(static_cast<int>(TaskStatus::Pending)));
    task.assignedAgent = json["assignedAgent"].toString();
    task.createdAt = QDateTime::fromString(json["createdAt"].toString(), Qt::ISODate);
    task.updatedAt = QDateTime::fromString(json["updatedAt"].toString(), Qt::ISODate);
    if (json.contains("startedAt")) {
        task.startedAt = QDateTime::fromString(json["startedAt"].toString(), Qt::ISODate);
    }
    if (json.contains("completedAt")) {
        task.completedAt = QDateTime::fromString(json["completedAt"].toString(), Qt::ISODate);
    }
    task.estimatedMinutes = json["estimatedMinutes"].toInt(0);
    task.actualMinutes = json["actualMinutes"].toInt(0);
    task.progressPercent = json["progressPercent"].toInt(0);
    task.errorMessage = json["errorMessage"].toString("");
    task.metadata = json["metadata"].toObject();
    
    if (json.contains("dependencies")) {
        QJsonArray depsArray = json["dependencies"].toArray();
        for (const auto& depVal : depsArray) {
            QJsonObject depObj = depVal.toObject();
            TaskDependency dep;
            dep.taskId = depObj["taskId"].toString();
            dep.dependencyType = depObj["dependencyType"].toString("requires");
            task.dependencies.append(dep);
        }
    }
    
    if (json.contains("resources")) {
        QJsonArray resArray = json["resources"].toArray();
        for (const auto& resVal : resArray) {
            QJsonObject resObj = resVal.toObject();
            TaskResource res;
            res.resourceId = resObj["resourceId"].toString();
            res.resourceType = resObj["resourceType"].toString("file");
            res.accessMode = resObj["accessMode"].toString("read");
            task.resources.append(res);
        }
    }
    
    if (json.contains("subtasks")) {
        QJsonArray subtasksArray = json["subtasks"].toArray();
        for (const auto& subVal : subtasksArray) {
            task.subtasks.append(subVal.toString());
        }
    }
    
    if (json.contains("tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        for (const auto& tagVal : tagsArray) {
            task.tags.append(tagVal.toString());
        }
    }
    
    return task;
}

// TaskPlanner Implementation
TaskPlanner::TaskPlanner(QObject* parent) : QObject(parent) {
    qCDebug(logTaskPlanner) << "TaskPlanner initialized";
}

QString TaskPlanner::createTask(const QString& title, const QString& description,
                                 TaskType type, TaskPriority priority) {
    QMutexLocker locker(&m_mutex);
    
    Task task;
    task.title = title;
    task.description = description;
    task.type = type;
    task.priority = priority;
    
    if (!validateTask(task, nullptr)) {
        qCWarning(logTaskPlanner) << "Invalid task creation:" << title;
        return QString();
    }
    
    m_tasks[task.id] = task;
    qCInfo(logTaskPlanner) << "Task created:" << task.id << title;
    
    emit taskCreated(task.id);
    return task.id;
}

bool TaskPlanner::deleteTask(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        qCWarning(logTaskPlanner) << "Task not found:" << taskId;
        return false;
    }
    
    // Remove from other tasks' dependencies
    for (auto& task : m_tasks) {
        task.dependencies.removeIf([&taskId](const TaskDependency& dep) {
            return dep.taskId == taskId;
        });
    }
    
    m_tasks.remove(taskId);
    qCInfo(logTaskPlanner) << "Task deleted:" << taskId;
    emit taskDeleted(taskId);
    return true;
}

bool TaskPlanner::updateTask(const QString& taskId, const Task& updatedTask) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        qCWarning(logTaskPlanner) << "Task not found for update:" << taskId;
        return false;
    }
    
    QString errorMsg;
    if (!validateTask(updatedTask, &errorMsg)) {
        qCWarning(logTaskPlanner) << "Invalid task update:" << errorMsg;
        return false;
    }
    
    Task oldTask = m_tasks[taskId];
    m_tasks[taskId] = updatedTask;
    updateTimestamp(taskId);
    
    emit taskUpdated(taskId);
    if (oldTask.status != updatedTask.status) {
        emit taskStatusChanged(taskId, oldTask.status, updatedTask.status);
    }
    
    return true;
}

Task TaskPlanner::getTask(const QString& taskId) const {
    QMutexLocker locker(&m_mutex);
    return m_tasks.value(taskId, Task());
}

QList<Task> TaskPlanner::getAllTasks() const {
    QMutexLocker locker(&m_mutex);
    return m_tasks.values();
}

QList<Task> TaskPlanner::getTasksByStatus(TaskStatus status) const {
    QMutexLocker locker(&m_mutex);
    QList<Task> result;
    for (const auto& task : m_tasks) {
        if (task.status == status) {
            result.append(task);
        }
    }
    return result;
}

QList<Task> TaskPlanner::getTasksByAgent(const QString& agentId) const {
    QMutexLocker locker(&m_mutex);
    QList<Task> result;
    for (const auto& task : m_tasks) {
        if (task.assignedAgent == agentId) {
            result.append(task);
        }
    }
    return result;
}

QList<Task> TaskPlanner::getTasksByType(TaskType type) const {
    QMutexLocker locker(&m_mutex);
    QList<Task> result;
    for (const auto& task : m_tasks) {
        if (task.type == type) {
            result.append(task);
        }
    }
    return result;
}

QList<Task> TaskPlanner::getBlockedTasks() const {
    QMutexLocker locker(&m_mutex);
    QList<Task> blocked;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::Blocked || 
            (task.status == TaskStatus::Pending && !areDependenciesMet(task.id))) {
            blocked.append(task);
        }
    }
    return blocked;
}

bool TaskPlanner::startTask(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    Task& task = m_tasks[taskId];
    if (task.status != TaskStatus::Pending && task.status != TaskStatus::Blocked) {
        return false;
    }
    
    if (!areDependenciesMet(taskId)) {
        task.status = TaskStatus::Blocked;
        return false;
    }
    
    TaskStatus oldStatus = task.status;
    task.status = TaskStatus::InProgress;
    task.startedAt = QDateTime::currentDateTime();
    task.progressPercent = qMax(task.progressPercent, 1);
    updateTimestamp(taskId);
    
    qCInfo(logTaskPlanner) << "Task started:" << taskId;
    emit taskStarted(taskId);
    emit taskStatusChanged(taskId, oldStatus, TaskStatus::InProgress);
    return true;
}

bool TaskPlanner::completeTask(const QString& taskId, int actualMinutes) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    Task& task = m_tasks[taskId];
    TaskStatus oldStatus = task.status;
    
    task.status = TaskStatus::Completed;
    task.completedAt = QDateTime::currentDateTime();
    task.progressPercent = 100;
    if (actualMinutes > 0) {
        task.actualMinutes = actualMinutes;
    } else if (task.startedAt.isValid()) {
        task.actualMinutes = qRound(task.startedAt.secsTo(task.completedAt) / 60.0);
    }
    updateTimestamp(taskId);
    
    qCInfo(logTaskPlanner) << "Task completed:" << taskId << "in" << task.actualMinutes << "minutes";
    emit taskCompleted(taskId);
    emit taskStatusChanged(taskId, oldStatus, TaskStatus::Completed);
    return true;
}

bool TaskPlanner::failTask(const QString& taskId, const QString& errorMessage) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    Task& task = m_tasks[taskId];
    TaskStatus oldStatus = task.status;
    
    task.status = TaskStatus::Failed;
    task.errorMessage = errorMessage;
    task.completedAt = QDateTime::currentDateTime();
    updateTimestamp(taskId);
    
    qCWarning(logTaskPlanner) << "Task failed:" << taskId << errorMessage;
    emit taskFailed(taskId, errorMessage);
    emit taskStatusChanged(taskId, oldStatus, TaskStatus::Failed);
    return true;
}

bool TaskPlanner::cancelTask(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    Task& task = m_tasks[taskId];
    TaskStatus oldStatus = task.status;
    
    task.status = TaskStatus::Cancelled;
    updateTimestamp(taskId);
    
    qCInfo(logTaskPlanner) << "Task cancelled:" << taskId;
    emit taskStatusChanged(taskId, oldStatus, TaskStatus::Cancelled);
    return true;
}

bool TaskPlanner::updateProgress(const QString& taskId, int percent) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    Task& task = m_tasks[taskId];
    percent = qBound(0, percent, 100);
    
    if (task.progressPercent != percent) {
        task.progressPercent = percent;
        updateTimestamp(taskId);
        emit progressUpdated(taskId, percent);
    }
    return true;
}

bool TaskPlanner::addDependency(const QString& taskId, const QString& dependsOnId, 
                                 const QString& dependencyType) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId) || !m_tasks.contains(dependsOnId)) {
        return false;
    }
    
    // Prevent circular dependencies
    if (dependsOnId == taskId) return false;
    
    Task& task = m_tasks[taskId];
    for (const auto& dep : task.dependencies) {
        if (dep.taskId == dependsOnId) return false; // Already exists
    }
    
    TaskDependency dep;
    dep.taskId = dependsOnId;
    dep.dependencyType = dependencyType;
    task.dependencies.append(dep);
    
    // Update status if needed
    if (task.status == TaskStatus::Pending) {
        task.status = TaskStatus::Blocked;
    }
    
    updateTimestamp(taskId);
    qCInfo(logTaskPlanner) << "Dependency added:" << taskId << "depends on" << dependsOnId;
    emit dependencyAdded(taskId, dependsOnId);
    return true;
}

bool TaskPlanner::removeDependency(const QString& taskId, const QString& dependsOnId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    Task& task = m_tasks[taskId];
    int initialSize = task.dependencies.size();
    task.dependencies.removeIf([&dependsOnId](const TaskDependency& dep) {
        return dep.taskId == dependsOnId;
    });
    
    if (task.dependencies.size() < initialSize) {
        updateTimestamp(taskId);
        return true;
    }
    return false;
}

bool TaskPlanner::areDependenciesMet(const QString& taskId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) return false;
    
    const Task& task = m_tasks[taskId];
    for (const auto& dep : task.dependencies) {
        if (!m_tasks.contains(dep.taskId)) continue;
        
        const Task& depTask = m_tasks[dep.taskId];
        if (depTask.status != TaskStatus::Completed) {
            return false;
        }
    }
    return true;
}

QList<Task> TaskPlanner::getReadyTasks() const {
    QMutexLocker locker(&m_mutex);
    QList<Task> ready;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::Pending || task.status == TaskStatus::Blocked) {
            bool depsMet = true;
            for (const auto& dep : task.dependencies) {
                if (m_tasks.contains(dep.taskId)) {
                    if (m_tasks[dep.taskId].status != TaskStatus::Completed) {
                        depsMet = false;
                        break;
                    }
                }
            }
            if (depsMet) {
                ready.append(task);
            }
        }
    }
    return ready;
}

QList<Task> TaskPlanner::generateSubtasks(const QString& parentTaskId, 
                                           const QList<QString>& subtaskTitles) {
    QMutexLocker locker(&m_mutex);
    QList<Task> created;
    
    if (!m_tasks.contains(parentTaskId)) return created;
    
    Task& parent = m_tasks[parentTaskId];
    for (const auto& title : subtaskTitles) {
        Task subtask;
        subtask.title = title;
        subtask.description = "Subtask of: " + parent.title;
        subtask.type = parent.type;
        subtask.priority = parent.priority;
        subtask.metadata["parentTaskId"] = parentTaskId;
        
        m_tasks[subtask.id] = subtask;
        parent.subtasks.append(subtask.id);
        addDependency(parentTaskId, subtask.id, "blocks");
        
        created.append(subtask);
        emit taskCreated(subtask.id);
    }
    
    updateTimestamp(parentTaskId);
    return created;
}

TaskPriority TaskPlanner::calculateCriticalPath() const {
    QMutexLocker locker(&m_mutex);
    TaskPriority highest = TaskPriority::Low;
    
    for (const auto& task : m_tasks) {
        if (task.status != TaskStatus::Completed && 
            task.status != TaskStatus::Cancelled) {
            if (static_cast<int>(task.priority) > static_cast<int>(highest)) {
                highest = task.priority;
            }
        }
    }
    return highest;
}

int TaskPlanner::estimateTotalTime() const {
    QMutexLocker locker(&m_mutex);
    int total = 0;
    for (const auto& task : m_tasks) {
        if (task.status != TaskStatus::Completed && 
            task.status != TaskStatus::Cancelled) {
            total += task.estimatedMinutes;
        }
    }
    return total;
}

int TaskPlanner::calculateCompletionPercentage() const {
    QMutexLocker locker(&m_mutex);
    if (m_tasks.isEmpty()) return 0;
    
    int completed = 0;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::Completed) {
            completed++;
        }
    }
    return qRound(100.0 * completed / m_tasks.size());
}

bool TaskPlanner::saveToFile(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(logTaskPlanner) << "Cannot open file for writing:" << filePath;
        return false;
    }
    
    QJsonDocument doc = exportToJson();
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qCInfo(logTaskPlanner) << "Tasks saved to:" << filePath;
    return true;
}

bool TaskPlanner::loadFromFile(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logTaskPlanner) << "Cannot open file for reading:" << filePath;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(logTaskPlanner) << "JSON parse error:" << error.errorString();
        return false;
    }
    
    importFromJson(doc);
    qCInfo(logTaskPlanner) << "Tasks loaded from:" << filePath;
    emit planReady();
    return true;
}

QJsonDocument TaskPlanner::exportToJson() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject root;
    QJsonArray tasksArray;
    
    for (const auto& task : m_tasks) {
        tasksArray.append(task.toJson());
    }
    
    root["tasks"] = tasksArray;
    root["version"] = "1.0";
    root["exportedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QJsonDocument(root);
}

void TaskPlanner::importFromJson(const QJsonDocument& doc) {
    QMutexLocker locker(&m_mutex);
    
    clearAll();
    QJsonObject root = doc.object();
    QJsonArray tasksArray = root["tasks"].toArray();
    
    for (const auto& taskVal : tasksArray) {
        Task task = Task::fromJson(taskVal.toObject());
        m_tasks[task.id] = task;
    }
    
    qCInfo(logTaskPlanner) << "Imported" << m_tasks.size() << "tasks";
}

void TaskPlanner::clearAll() {
    QMutexLocker locker(&m_mutex);
    m_tasks.clear();
    qCInfo(logTaskPlanner) << "All tasks cleared";
}

int TaskPlanner::getActiveTaskCount() const {
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::InProgress || 
            task.status == TaskStatus::Pending ||
            task.status == TaskStatus::Blocked) {
            count++;
        }
    }
    return count;
}

int TaskPlanner::getCompletedTaskCount() const {
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == TaskStatus::Completed) {
            count++;
        }
    }
    return count;
}

QStringList TaskPlanner::getAvailableAgents() const {
    QMutexLocker locker(&m_mutex);
    QStringList agents;
    for (const auto& task : m_tasks) {
        if (!task.assignedAgent.isEmpty() && !agents.contains(task.assignedAgent)) {
            agents.append(task.assignedAgent);
        }
    }
    return agents;
}

bool TaskPlanner::validateTask(const Task& task, QString* errorMsg) const {
    if (task.title.trimmed().isEmpty()) {
        if (errorMsg) *errorMsg = "Task title cannot be empty";
        return false;
    }
    if (task.estimatedMinutes < 0) {
        if (errorMsg) *errorMsg = "Estimated minutes cannot be negative";
        return false;
    }
    return true;
}

void TaskPlanner::updateTimestamp(const QString& taskId) {
    if (m_tasks.contains(taskId)) {
        m_tasks[taskId].updatedAt = QDateTime::currentDateTime();
    }
}

TaskStatus TaskPlanner::getNextStatus(TaskStatus current) const {
    switch (current) {
        case TaskStatus::Pending: return TaskStatus::InProgress;
        case TaskStatus::InProgress: return TaskStatus::Completed;
        case TaskStatus::Blocked: return TaskStatus::Pending;
        default: return current;
    }
}

} // namespace Brahma
