#pragma once

#ifdef BUILD_GUI
#include <QObject>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QQueue>
#include <QVector>
#endif

#include <atomic>
#include <memory>
#include <vector>
#include <functional>

namespace brahma {

/**
 * @brief Real-time metrics for model performance tracking
 */
struct ModelMetrics {
    QString modelId;
    qint64 totalRequests = 0;
    qint64 successfulRequests = 0;
    qint64 failedRequests = 0;
    qint64 totalTokens = 0;
    double averageLatency = 0.0; // milliseconds
    double tokensPerSecond = 0.0;
    double successRate = 100.0;
    QDateTime lastUsed;
    QDateTime lastError;
    QString lastErrorMessage;
    
    void recordRequest(bool success, int tokens, double latencyMs) {
        totalRequests++;
        if (success) {
            successfulRequests++;
            totalTokens += tokens;
            
            // Update average latency with exponential moving average
            averageLatency = (averageLatency * 0.9) + (latencyMs * 0.1);
            
            if (totalTokens > 0 && averageLatency > 0) {
                tokensPerSecond = (totalTokens / averageLatency) * 1000.0;
            }
        } else {
            failedRequests++;
            lastError = QDateTime::currentDateTime();
        }
        
        successRate = totalRequests > 0 ? (successfulRequests * 100.0 / totalRequests) : 100.0;
        lastUsed = QDateTime::currentDateTime();
    }
    
    void recordError(const QString& message) {
        failedRequests++;
        lastError = QDateTime::currentDateTime();
        lastErrorMessage = message;
        successRate = totalRequests > 0 ? (successfulRequests * 100.0 / totalRequests) : 100.0;
    }
};

/**
 * @brief Metrics for agent activity tracking
 */
struct AgentMetrics {
    QString agentType;
    qint64 totalActions = 0;
    double totalDurationMs = 0.0;
    double averageDurationMs = 0.0;
    QMap<QString, qint64> actionCounts;
    QDateTime lastActive;
    
    void recordAction(const QString& action, double durationMs) {
        totalActions++;
        totalDurationMs += durationMs;
        averageDurationMs = totalDurationMs / totalActions;
        if (!action.isEmpty()) {
            actionCounts[action]++;
        }
        lastActive = QDateTime::currentDateTime();
    }
};

/**
 * @brief Metrics for tool usage tracking
 */
struct ToolMetrics {
    QString toolName;
    qint64 usageCount = 0;
    qint64 successCount = 0;
    double totalDurationMs = 0.0;
    double averageDurationMs = 0.0;
    double successRate = 100.0;
    QDateTime lastUsed;
    
    void recordUsage(bool success, double durationMs) {
        usageCount++;
        if (success) {
            successCount++;
        }
        totalDurationMs += durationMs;
        averageDurationMs = totalDurationMs / usageCount;
        successRate = usageCount > 0 ? (successCount * 100.0 / usageCount) : 100.0;
        lastUsed = QDateTime::currentDateTime();
    }
};

/**
 * @brief File operation record for tracking filesystem activity
 */
struct FileOperation {
    QString operation;
    QString path;
    qint64 sizeBytes = 0;
    QDateTime timestamp;
};

/**
 * @brief User action record for tracking user interactions
 */
struct UserActionRecord {
    QString action;
    QVariant metadata;
    QDateTime timestamp;
};

/**
 * @brief Aggregated analytics data for a time period
 */
struct AnalyticsSnapshot {
    QDateTime timestamp;
    qint64 totalRequests = 0;
    qint64 totalTokens = 0;
    double averageLatency = 0.0;
    double successRate = 100.0;
    qint64 estimatedCost = 0; // in cents
    int activeConversations = 0;
    int activeAgents = 0;
    
    QMap<QString, qint64> requestsByModel;
    QMap<QString, qint64> tokensByModel;
    QMap<QString, qint64> costByModel;
};

/**
 * @brief Collects and aggregates metrics from all system components
 */
class MetricsCollector : public QObject {
    Q_OBJECT
    
public:
    static MetricsCollector* instance();
    
    // Metric recording
    void recordModelRequest(const QString& modelId, bool success, 
                           int tokens = 0, double latencyMs = 0.0);
    void recordModelError(const QString& modelId, const QString& errorMessage);
    void recordAgentAction(const QString& agentType, const QString& action,
                          double durationMs = 0.0);
    void recordToolUsage(const QString& toolName, bool success,
                        double durationMs = 0.0);
    void recordFileOperation(const QString& operation, const QString& path,
                            qint64 sizeBytes = 0);
    void recordUserAction(const QString& action, const QVariant& metadata = {});
    
    // Cost tracking
    void setModelCost(const QString& modelId, double costPerToken);
    void recordCost(const QString& modelId, int tokens, double cost);
    double getEstimatedCost() const;
    double getModelCost(const QString& modelId) const;
    
    // Querying metrics
    ModelMetrics getModelMetrics(const QString& modelId) const;
    QMap<QString, ModelMetrics> getAllModelMetrics() const;
    AgentMetrics getAgentMetrics(const QString& agentType) const;
    QMap<QString, AgentMetrics> getAllAgentMetrics() const;
    ToolMetrics getToolMetrics(const QString& toolName) const;
    QVector<AnalyticsSnapshot> getSnapshots(int hours = 24) const;
    QList<QString> getAllModelIds() const;
    AnalyticsSnapshot getSnapshot() const;
    AnalyticsSnapshot getSnapshotForPeriod(QDateTime start, QDateTime end) const;
    
    // Historical data
    QVector<AnalyticsSnapshot> getHistory(int hours = 24) const;
    QVector<AnalyticsSnapshot> getHourlyHistory(int days = 7) const;
    QVector<AnalyticsSnapshot> getDailyHistory(int days = 30) const;
    
    // File operations and user actions
    QQueue<FileOperation> getRecentFileOperations(int limit = 100) const;
    QVector<UserActionRecord> getUserActions(int limit = 500) const;
    
    // Performance analysis
    QString getBestPerformingModel() const;
    QString getMostUsedModel() const;
    double getSystemUptime() const; // percentage
    QStringList getSlowModels(double thresholdMs = 1000.0) const;
    
    // Alerts and thresholds
    void setLatencyThreshold(double ms);
    void setErrorRateThreshold(double percent);
    void checkAlerts();
    
    // Export
    QString exportToJson() const;
    void exportToFile(const QString& filePath) const;
    
    // Reset
    void resetMetrics();
    void resetModelMetrics(const QString& modelId);
    
signals:
    void metricsUpdated();
    void agentMetricsUpdated(const QString& agentType);
    void toolMetricsUpdated(const QString& toolName);
    void alertTriggered(const QString& type, const QString& message, double value);
    void thresholdExceeded(const QString& metric, double threshold, double actual);
    
private:
    explicit MetricsCollector(QObject* parent = nullptr);
    ~MetricsCollector() override;
    
    static MetricsCollector* s_instance;
    mutable QMutex m_mutex;
    
    QMap<QString, ModelMetrics> m_modelMetrics;
    QMap<QString, AgentMetrics> m_agentMetrics;
    QMap<QString, ToolMetrics> m_toolMetrics;
    QMap<QString, double> m_modelCosts; // cost per token
    QMap<QString, qint64> m_accumulatedCosts; // total cost per model
    
    QQueue<FileOperation> m_recentFileOperations;
    QVector<UserActionRecord> m_userActions;
    
    QVector<AnalyticsSnapshot> m_hourlySnapshots;
    QVector<AnalyticsSnapshot> m_dailySnapshots;
    
    std::atomic<qint64> m_totalRequests{0};
    std::atomic<qint64> m_totalTokens{0};
    std::atomic<double> m_averageLatency{0.0};
    
    QDateTime m_startTime;
    
    double m_latencyThreshold = 1000.0; // ms
    double m_errorRateThreshold = 5.0; // percent
    
    void updateSnapshots();
    void cleanupOldSnapshots();
    AnalyticsSnapshot aggregateSnapshots(const QVector<AnalyticsSnapshot>& snapshots) const;
};

/**
 * @brief Real-time dashboard widget for displaying analytics
 */
class AnalyticsDashboard : public QWidget {
    Q_OBJECT
    
public:
    explicit AnalyticsDashboard(QWidget* parent = nullptr);
    ~AnalyticsDashboard() override;
    
    // Display modes
    enum class ViewMode {
        Overview,
        Models,
        Agents,
        Costs,
        Performance
    };
    
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_currentMode; }
    
    // Time range
    void setTimeRange(int hours);
    int timeRangeHours() const { return m_timeRangeHours; }
    
    // Auto-refresh
    void setAutoRefresh(bool enabled);
    void setRefreshInterval(int seconds);
    bool isAutoRefreshing() const { return m_autoRefresh; }
    
    // Export
    void exportCurrentView(const QString& filePath);
    
public slots:
    void refresh();
    void resetView();
    
signals:
    void viewModeChanged(ViewMode mode);
    void timeRangeChanged(int hours);
    void exportRequested(const QString& filePath);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private slots:
    void onMetricsUpdated();
    void onAlertTriggered(const QString& type, const QString& message, double value);
    
private:
    void setupUI();
    void drawOverview(QPainter& painter);
    void drawModels(QPainter& painter);
    void drawAgents(QPainter& painter);
    void drawCosts(QPainter& painter);
    void drawPerformance(QPainter& painter);
    void drawChart(QPainter& painter, const QRect& rect, 
                  const QVector<AnalyticsSnapshot>& data, const QString& metric);
    void drawMetricCard(QPainter& painter, int x, int y, int w, int h,
                       const QString& title, const QString& value,
                       const QString& delta, QColor color);
    
    ViewMode m_currentMode = ViewMode::Overview;
    int m_timeRangeHours = 24;
    bool m_autoRefresh = true;
    int m_refreshInterval = 5; // seconds
    
    QTimer* m_refreshTimer;
    MetricsCollector* m_metrics;
    
    // Cached data for rendering
    mutable AnalyticsSnapshot m_currentSnapshot;
    mutable QVector<AnalyticsSnapshot> m_historyData;
    mutable bool m_dataValid = false;
    
    // Colors
    QColor m_primaryColor;
    QColor m_successColor;
    QColor m_warningColor;
    QColor m_errorColor;
    QColor m_chartLineColor;
};

} // namespace brahma
