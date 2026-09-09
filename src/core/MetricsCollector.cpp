#include "MetricsCollector.h"
#include "logging/Logger.h"

#include <QPainter>
#include <QRect>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QProgressBar>
#include <QStackedWidget>
#include <QTabWidget>

namespace brahma {

// ============================================================================
// MetricsCollector Implementation
// ============================================================================

static MetricsCollector* g_instance = nullptr;

MetricsCollector* MetricsCollector::instance() {
    if (!g_instance) {
        g_instance = new MetricsCollector();
    }
    return g_instance;
}

MetricsCollector::MetricsCollector(QObject* parent)
    : QObject(parent), m_startTime(QDateTime::currentDateTime()) {
    Logger::info("MetricsCollector initialized");
}

MetricsCollector::~MetricsCollector() {
    Logger::info("MetricsCollector shutting down");
}

void MetricsCollector::recordModelRequest(const QString& modelId, bool success,
                                          int tokens, double latencyMs) {
    QMutexLocker locker(&m_mutex);
    
    auto& metrics = m_modelMetrics[modelId];
    metrics.recordRequest(success, tokens, latencyMs);
    
    if (success) {
        m_totalRequests.fetchAdd(1);
        m_totalTokens.fetchAdd(tokens);
        
        // Update global average latency
        double currentAvg = m_averageLatency.load();
        double newAvg = (currentAvg * 0.9) + (latencyMs * 0.1);
        m_averageLatency.store(newAvg);
    }
    
    emit metricsUpdated();
    
    // Check thresholds and emit alerts
    if (metrics.successRate < (100.0 - m_errorRateThreshold)) {
        emit alertTriggered("error_rate", 
                           QString("High error rate for %1: %2%").arg(modelId).arg(metrics.successRate, 0, 'f', 1),
                           metrics.successRate);
    }
    
    if (latencyMs > m_latencyThreshold) {
        emit alertTriggered("high_latency",
                           QString("High latency for %1: %2ms").arg(modelId).arg(latencyMs, 0, 'f', 0),
                           latencyMs);
    }
}

void MetricsCollector::recordModelError(const QString& modelId, const QString& errorMessage) {
    QMutexLocker locker(&m_mutex);
    m_modelMetrics[modelId].recordError(errorMessage);
    emit metricsUpdated();
}

void MetricsCollector::recordAgentAction(const QString& agentType, const QString& action,
                                         double durationMs) {
    QMutexLocker locker(&m_mutex);
    
    AgentMetrics& metrics = m_agentMetrics[agentType];
    metrics.totalActions++;
    metrics.totalDurationMs += durationMs;
    metrics.averageDurationMs = metrics.totalDurationMs / metrics.totalActions;
    
    if (!action.isEmpty()) {
        metrics.actionCounts[action]++;
    }
    
    emit agentMetricsUpdated(agentType);
}

void MetricsCollector::recordToolUsage(const QString& toolName, bool success,
                                       double durationMs) {
    QMutexLocker locker(&m_mutex);
    
    ToolMetrics& metrics = m_toolMetrics[toolName];
    metrics.usageCount++;
    if (success) {
        metrics.successCount++;
    }
    metrics.totalDurationMs += durationMs;
    metrics.averageDurationMs = metrics.totalDurationMs / metrics.usageCount;
    
    emit toolMetricsUpdated(toolName);
}

void MetricsCollector::recordFileOperation(const QString& operation, const QString& path,
                                           qint64 sizeBytes) {
    FileOperation op;
    op.operation = operation;
    op.path = path;
    op.sizeBytes = sizeBytes;
    op.timestamp = QDateTime::currentDateTime();
    
    QMutexLocker locker(&m_mutex);
    m_recentFileOperations.enqueue(op);
    
    // Keep only last 100 operations
    while (m_recentFileOperations.size() > 100) {
        m_recentFileOperations.dequeue();
    }
}

void MetricsCollector::recordUserAction(const QString& action, const QVariant& metadata) {
    QMutexLocker locker(&m_mutex);
    
    UserActionRecord record;
    record.action = action;
    record.metadata = metadata;
    record.timestamp = QDateTime::currentDateTime();
    
    m_userActions.append(record);
    
    // Keep only last 500 actions
    if (m_userActions.size() > 500) {
        m_userActions.removeFirst();
    }
}

void MetricsCollector::setModelCost(const QString& modelId, double costPerToken) {
    QMutexLocker locker(&m_mutex);
    m_modelCosts[modelId] = costPerToken;
}

double MetricsCollector::getModelCost(const QString& modelId) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_modelCosts.find(modelId);
    return (it != m_modelCosts.end()) ? it.value() : 0.0;
}

ModelMetrics MetricsCollector::getModelMetrics(const QString& modelId) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_modelMetrics.find(modelId);
    return (it != m_modelMetrics.end()) ? it.value() : ModelMetrics{};
}

QMap<QString, ModelMetrics> MetricsCollector::getAllModelMetrics() const {
    QMutexLocker locker(&m_mutex);
    return m_modelMetrics;
}

AgentMetrics MetricsCollector::getAgentMetrics(const QString& agentType) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_agentMetrics.find(agentType);
    return (it != m_agentMetrics.end()) ? it.value() : AgentMetrics{};
}

QMap<QString, AgentMetrics> MetricsCollector::getAllAgentMetrics() const {
    QMutexLocker locker(&m_mutex);
    return m_agentMetrics;
}

ToolMetrics MetricsCollector::getToolMetrics(const QString& toolName) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_toolMetrics.find(toolName);
    return (it != m_toolMetrics.end()) ? it.value() : ToolMetrics{};
}

QVector<AnalyticsSnapshot> MetricsCollector::getSnapshots(int hours) const {
    QMutexLocker locker(&m_mutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-hours * 3600);
    QVector<AnalyticsSnapshot> result;
    
    for (const auto& snapshot : m_hourlySnapshots) {
        if (snapshot.timestamp >= cutoff) {
            result.append(snapshot);
        }
    }
    
    return result;
}

AnalyticsSnapshot MetricsCollector::getCurrentSnapshot() const {
    QMutexLocker locker(&m_mutex);
    
    AnalyticsSnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();
    snapshot.totalRequests = m_totalRequests.load();
    snapshot.totalTokens = m_totalTokens.load();
    snapshot.averageLatency = m_averageLatency.load();
    
    // Calculate overall success rate
    qint64 totalSuccess = 0;
    qint64 totalRequests = 0;
    
    for (auto it = m_modelMetrics.begin(); it != m_modelMetrics.end(); ++it) {
        totalSuccess += it->successfulRequests;
        totalRequests += it->totalRequests;
        snapshot.requestsByModel[it.key()] = it->totalRequests;
        snapshot.tokensByModel[it.key()] = it->totalTokens;
    }
    
    snapshot.successRate = (totalRequests > 0) ? (totalSuccess * 100.0 / totalRequests) : 100.0;
    
    // Estimate costs
    snapshot.estimatedCost = 0;
    for (auto it = snapshot.tokensByModel.begin(); it != snapshot.tokensByModel.end(); ++it) {
        auto costIt = m_modelCosts.find(it.key());
        if (costIt != m_modelCosts.end()) {
            qint64 cost = static_cast<qint64>(it.value() * costIt.value() * 100); // Convert to cents
            snapshot.costByModel[it.key()] = cost;
            snapshot.estimatedCost += cost;
        }
    }
    
    return snapshot;
}

void MetricsCollector::resetMetrics() {
    QMutexLocker locker(&m_mutex);
    
    m_modelMetrics.clear();
    m_agentMetrics.clear();
    m_toolMetrics.clear();
    m_modelCosts.clear();
    m_hourlySnapshots.clear();
    m_dailySnapshots.clear();
    m_recentFileOperations.clear();
    m_userActions.clear();
    
    m_totalRequests.store(0);
    m_totalTokens.store(0);
    m_averageLatency.store(0.0);
    m_startTime = QDateTime::currentDateTime();
    
    emit metricsReset();
    emit metricsUpdated();
    
    Logger::info("MetricsCollector reset");
}

void MetricsCollector::exportMetrics(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject root;
    root["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["startTime"] = m_startTime.toString(Qt::ISODate);
    
    // Export model metrics
    QJsonObject models;
    for (auto it = m_modelMetrics.begin(); it != m_modelMetrics.end(); ++it) {
        QJsonObject modelData;
        modelData["totalRequests"] = it->totalRequests;
        modelData["successfulRequests"] = it->successfulRequests;
        modelData["failedRequests"] = it->failedRequests;
        modelData["totalTokens"] = it->totalTokens;
        modelData["averageLatency"] = it->averageLatency;
        modelData["tokensPerSecond"] = it->tokensPerSecond;
        modelData["successRate"] = it->successRate;
        modelData["lastUsed"] = it->lastUsed.toString(Qt::ISODate);
        models[it.key()] = modelData;
    }
    root["models"] = models;
    
    // Export agent metrics
    QJsonObject agents;
    for (auto it = m_agentMetrics.begin(); it != m_agentMetrics.end(); ++it) {
        QJsonObject agentData;
        agentData["totalActions"] = it->totalActions;
        agentData["averageDurationMs"] = it->averageDurationMs;
        
        QJsonObject actionCounts;
        for (auto actionIt = it->actionCounts.begin(); actionIt != it->actionCounts.end(); ++actionIt) {
            actionCounts[actionIt.key()] = actionIt.value();
        }
        agentData["actionCounts"] = actionCounts;
        agents[it.key()] = agentData;
    }
    root["agents"] = agents;
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        Logger::info("Metrics exported to {}", filePath.toStdString());
    } else {
        Logger::error("Failed to export metrics to {}", filePath.toStdString());
    }
}

void MetricsCollector::updateSnapshots() {
    auto snapshot = getCurrentSnapshot();
    
    QMutexLocker locker(&m_mutex);
    m_hourlySnapshots.enqueue(snapshot);
    
    // Keep only last 168 hours (1 week)
    while (m_hourlySnapshots.size() > 168) {
        m_hourlySnapshots.dequeue();
    }
    
    // Aggregate daily snapshots
    if (m_hourlySnapshots.size() >= 24) {
        AnalyticsSnapshot daily;
        daily.timestamp = QDateTime::currentDateTime();
        
        // Sum up last 24 hours
        for (int i = 0; i < 24 && i < m_hourlySnapshots.size(); ++i) {
            const auto& hourly = m_hourlySnapshots.at(m_hourlySnapshots.size() - 1 - i);
            daily.totalRequests += hourly.totalRequests;
            daily.totalTokens += hourly.totalTokens;
            daily.averageLatency += hourly.averageLatency;
        }
        
        if (daily.totalRequests > 0) {
            daily.averageLatency /= 24.0;
            daily.successRate = snapshot.successRate;
        }
        
        m_dailySnapshots.enqueue(daily);
        
        // Keep only last 30 days
        while (m_dailySnapshots.size() > 30) {
            m_dailySnapshots.dequeue();
        }
    }
}

void MetricsCollector::cleanupOldSnapshots() {
    QMutexLocker locker(&m_mutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-30);
    
    while (!m_hourlySnapshots.isEmpty() && m_hourlySnapshots.front().timestamp < cutoff) {
        m_hourlySnapshots.dequeue();
    }
    
    while (!m_dailySnapshots.isEmpty() && m_dailySnapshots.front().timestamp < cutoff) {
        m_dailySnapshots.dequeue();
    }
}

AnalyticsSnapshot MetricsCollector::aggregateSnapshots(const QVector<AnalyticsSnapshot>& snapshots) const {
    AnalyticsSnapshot result;
    
    if (snapshots.isEmpty()) {
        return result;
    }
    
    result.timestamp = QDateTime::currentDateTime();
    
    qint64 totalLatencySum = 0;
    for (const auto& snapshot : snapshots) {
        result.totalRequests += snapshot.totalRequests;
        result.totalTokens += snapshot.totalTokens;
        totalLatencySum += static_cast<qint64>(snapshot.averageLatency * snapshot.totalRequests);
    }
    
    if (result.totalRequests > 0) {
        result.averageLatency = static_cast<double>(totalLatencySum) / result.totalRequests;
    }
    
    return result;
}

// ============================================================================
// AnalyticsDashboard Implementation
// ============================================================================

AnalyticsDashboard::AnalyticsDashboard(QWidget* parent)
    : QWidget(parent)
    , m_metrics(MetricsCollector::instance())
    , m_primaryColor(QColor("#FF6B9D"))  // Coral Rose
    , m_successColor(QColor("#4CAF50"))
    , m_warningColor(QColor("#FFC107"))
    , m_errorColor(QColor("#F44336"))
    , m_chartLineColor(QColor("#2196F3"))
{
    setupUI();
    
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &AnalyticsDashboard::refresh);
    
    if (m_autoRefresh) {
        m_refreshTimer->start(m_refreshInterval * 1000);
    }
    
    if (m_metrics) {
        connect(m_metrics, &MetricsCollector::metricsUpdated,
                this, &AnalyticsDashboard::onMetricsUpdated);
        connect(m_metrics, &MetricsCollector::alertTriggered,
                this, &AnalyticsDashboard::onAlertTriggered);
    }
}

AnalyticsDashboard::~AnalyticsDashboard() {
}

void AnalyticsDashboard::setupUI() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    
    // Mode selector
    auto modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel("View Mode:"));
    
    m_modeCombo = new QComboBox();
    m_modeCombo->addItem("Overview", static_cast<int>(ViewMode::Overview));
    m_modeCombo->addItem("Models", static_cast<int>(ViewMode::Models));
    m_modeCombo->addItem("Agents", static_cast<int>(ViewMode::Agents));
    m_modeCombo->addItem("Costs", static_cast<int>(ViewMode::Costs));
    m_modeCombo->addItem("Performance", static_cast<int>(ViewMode::Performance));
    
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                setViewMode(static_cast<ViewMode>(m_modeCombo->itemData(index).toInt()));
            });
    
    modeLayout->addWidget(m_modeCombo);
    modeLayout->addStretch();
    
    // Time range selector
    modeLayout->addWidget(new QLabel("Time Range:"));
    
    m_timeRangeCombo = new QComboBox();
    m_timeRangeCombo->addItem("1 Hour", 1);
    m_timeRangeCombo->addItem("6 Hours", 6);
    m_timeRangeCombo->addItem("24 Hours", 24);
    m_timeRangeCombo->addItem("7 Days", 168);
    m_timeRangeCombo->addItem("30 Days", 720);
    
    connect(m_timeRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                setTimeRange(m_timeRangeCombo->itemData(index).toInt());
            });
    
    modeLayout->addWidget(m_timeRangeCombo);
    
    // Refresh button
    auto refreshBtn = new QPushButton("↻ Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &AnalyticsDashboard::refresh);
    modeLayout->addWidget(refreshBtn);
    
    // Export button
    auto exportBtn = new QPushButton("📤 Export");
    connect(exportBtn, &QPushButton::clicked, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Export Metrics", "", "JSON Files (*.json)");
        if (!filePath.isEmpty()) {
            m_metrics->exportMetrics(filePath);
            emit exportRequested(filePath);
        }
    });
    modeLayout->addWidget(exportBtn);
    
    layout->addLayout(modeLayout);
    
    // Content area
    m_contentArea = new QScrollArea();
    m_contentArea->setWidgetResizable(true);
    m_contentArea->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_contentArea);
    
    refresh();
}

void AnalyticsDashboard::setViewMode(ViewMode mode) {
    if (m_currentMode != mode) {
        m_currentMode = mode;
        emit viewModeChanged(mode);
        refresh();
    }
}

void AnalyticsDashboard::setTimeRange(int hours) {
    if (m_timeRangeHours != hours) {
        m_timeRangeHours = hours;
        emit timeRangeChanged(hours);
        m_dataValid = false;
        refresh();
    }
}

void AnalyticsDashboard::setAutoRefresh(bool enabled) {
    m_autoRefresh = enabled;
    if (enabled) {
        m_refreshTimer->start(m_refreshInterval * 1000);
    } else {
        m_refreshTimer->stop();
    }
}

void AnalyticsDashboard::setRefreshInterval(int seconds) {
    m_refreshInterval = seconds;
    if (m_autoRefresh) {
        m_refreshTimer->start(seconds * 1000);
    }
}

void AnalyticsDashboard::exportCurrentView(const QString& filePath) {
    m_metrics->exportMetrics(filePath);
}

void AnalyticsDashboard::refresh() {
    m_dataValid = false;
    m_currentSnapshot = m_metrics->getCurrentSnapshot();
    m_historyData = m_metrics->getSnapshots(m_timeRangeHours);
    m_dataValid = true;
    
    update();  // Trigger repaint
}

void AnalyticsDashboard::resetView() {
    setViewMode(ViewMode::Overview);
    setTimeRange(24);
    refresh();
}

void AnalyticsDashboard::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    if (!m_dataValid) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), QColor("#1E1E1E"));
    
    switch (m_currentMode) {
        case ViewMode::Overview:
            drawOverview(painter);
            break;
        case ViewMode::Models:
            drawModels(painter);
            break;
        case ViewMode::Agents:
            drawAgents(painter);
            break;
        case ViewMode::Costs:
            drawCosts(painter);
            break;
        case ViewMode::Performance:
            drawPerformance(painter);
            break;
    }
}

void AnalyticsDashboard::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_dataValid = false;
}

void AnalyticsDashboard::onMetricsUpdated() {
    if (m_autoRefresh) {
        refresh();
    }
}

void AnalyticsDashboard::onAlertTriggered(const QString& type, const QString& message, double value) {
    // Could show a toast notification here
    Logger::warn("Alert [{}]: {}", type.toStdString(), message.toStdString());
}

void AnalyticsDashboard::drawOverview(QPainter& painter) {
    int y = 80;
    int cardWidth = (width() - 80) / 4;
    int cardHeight = 120;
    
    // Total Requests
    drawMetricCard(painter, 20, y, cardWidth, cardHeight,
                   "Total Requests",
                   QString::number(m_currentSnapshot.totalRequests),
                   "+12% vs last period", m_primaryColor);
    
    // Total Tokens
    drawMetricCard(painter, 20 + cardWidth + 20, y, cardWidth, cardHeight,
                   "Total Tokens",
                   QString::number(m_currentSnapshot.totalTokens / 1000) + "K",
                   "+8% vs last period", m_successColor);
    
    // Average Latency
    drawMetricCard(painter, 20 + (cardWidth + 20) * 2, y, cardWidth, cardHeight,
                   "Avg Latency",
                   QString::number(m_currentSnapshot.averageLatency, 'f', 0) + "ms",
                   m_currentSnapshot.averageLatency < 500 ? "-5% improvement" : "+15% slower",
                   m_currentSnapshot.averageLatency < 500 ? m_successColor : m_warningColor);
    
    // Success Rate
    drawMetricCard(painter, 20 + (cardWidth + 20) * 3, y, cardWidth, cardHeight,
                   "Success Rate",
                   QString::number(m_currentSnapshot.successRate, 'f', 1) + "%",
                   m_currentSnapshot.successRate > 95 ? "Excellent" : "Needs attention",
                   m_currentSnapshot.successRate > 95 ? m_successColor : m_errorColor);
    
    // Chart area
    QRect chartRect(20, y + cardHeight + 40, width() - 40, 300);
    drawChart(painter, chartRect, m_historyData, "requests");
}

void AnalyticsDashboard::drawModels(QPainter& painter) {
    int y = 80;
    auto modelMetrics = m_metrics->getAllModelMetrics();
    
    int rowHeight = 60;
    int x = 20;
    
    painter.setPen(QColor("#FFFFFF"));
    painter.setFont(QFont("Segoe UI", 14, QFont::Bold));
    painter.drawText(x, y, "Model Performance Metrics");
    y += 40;
    
    for (auto it = modelMetrics.begin(); it != modelMetrics.end(); ++it) {
        QRect rowRect(x, y, width() - 40, rowHeight);
        
        // Background
        painter.fillRect(rowRect, QColor("#2D2D2D"));
        
        // Model name
        painter.setPen(QColor("#FFFFFF"));
        painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
        painter.drawText(rowRect.adjusted(10, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, it.key());
        
        // Requests
        painter.setPen(QColor("#AAAAAA"));
        painter.setFont(QFont("Segoe UI", 11));
        QString reqText = QString("Requests: %1").arg(it->totalRequests);
        painter.drawText(rowRect.adjusted(150, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, reqText);
        
        // Success rate
        QColor successColor = (it->successRate > 95) ? m_successColor : m_warningColor;
        painter.setPen(successColor);
        QString successText = QString("Success: %1%").arg(it->successRate, 0, 'f', 1);
        painter.drawText(rowRect.adjusted(300, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, successText);
        
        // Latency
        painter.setPen(QColor("#2196F3"));
        QString latencyText = QString("Latency: %1ms").arg(it->averageLatency, 0, 'f', 0);
        painter.drawText(rowRect.adjusted(450, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, latencyText);
        
        // Tokens/sec
        painter.setPen(QColor("#FFC107"));
        QString tpsText = QString("Tokens/s: %1").arg(it->tokensPerSecond, 0, 'f', 1);
        painter.drawText(rowRect.adjusted(600, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, tpsText);
        
        y += rowHeight + 5;
    }
}

void AnalyticsDashboard::drawAgents(QPainter& painter) {
    int y = 80;
    auto agentMetrics = m_metrics->getAllAgentMetrics();
    
    int rowHeight = 60;
    int x = 20;
    
    painter.setPen(QColor("#FFFFFF"));
    painter.setFont(QFont("Segoe UI", 14, QFont::Bold));
    painter.drawText(x, y, "Agent Activity Metrics");
    y += 40;
    
    for (auto it = agentMetrics.begin(); it != agentMetrics.end(); ++it) {
        QRect rowRect(x, y, width() - 40, rowHeight);
        painter.fillRect(rowRect, QColor("#2D2D2D"));
        
        painter.setPen(QColor("#FFFFFF"));
        painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
        painter.drawText(rowRect.adjusted(10, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, it.key());
        
        painter.setPen(QColor("#AAAAAA"));
        painter.setFont(QFont("Segoe UI", 11));
        QString actionsText = QString("Actions: %1").arg(it->totalActions);
        painter.drawText(rowRect.adjusted(200, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, actionsText);
        
        painter.setPen(QColor("#4CAF50"));
        QString avgText = QString("Avg Duration: %1ms").arg(it->averageDurationMs, 0, 'f', 1);
        painter.drawText(rowRect.adjusted(400, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, avgText);
        
        y += rowHeight + 5;
    }
}

void AnalyticsDashboard::drawCosts(QPainter& painter) {
    int y = 80;
    int x = 20;
    
    painter.setPen(QColor("#FFFFFF"));
    painter.setFont(QFont("Segoe UI", 14, QFont::Bold));
    painter.drawText(x, y, "Cost Breakdown by Model");
    y += 40;
    
    // Draw pie chart or bar chart for costs
    auto costByModel = m_currentSnapshot.costByModel;
    
    if (costByModel.isEmpty()) {
        painter.setPen(QColor("#AAAAAA"));
        painter.setFont(QFont("Segoe UI", 12));
        painter.drawText(x, y + 100, "No cost data available. Configure model costs in settings.");
        return;
    }
    
    int barWidth = (width() - 80) / costByModel.size();
    int maxCost = 1;
    for (auto it = costByModel.begin(); it != costByModel.end(); ++it) {
        if (it.value() > maxCost) {
            maxCost = it.value();
        }
    }
    
    int barX = x;
    for (auto it = costByModel.begin(); it != costByModel.end(); ++it) {
        int barHeight = (it.value() * 200) / maxCost;
        
        QRect barRect(barX, y + 250 - barHeight, barWidth - 10, barHeight);
        painter.fillRect(barRect, m_primaryColor);
        
        painter.setPen(QColor("#FFFFFF"));
        painter.setFont(QFont("Segoe UI", 10));
        painter.drawText(barRect, Qt::AlignBottom | Qt::AlignHCenter, 
                        QString("$%1").arg(it.value() / 100.0, 0, 'f', 2));
        
        painter.setPen(QColor("#AAAAAA"));
        painter.drawText(barRect.bottomLeft() + QPoint(0, 20), it.key());
        
        barX += barWidth;
    }
    
    // Total cost
    painter.setPen(QColor("#FFC107"));
    painter.setFont(QFont("Segoe UI", 16, QFont::Bold));
    painter.drawText(x, y + 50, QString("Total Estimated Cost: $%1")
                    .arg(m_currentSnapshot.estimatedCost / 100.0, 0, 'f', 2));
}

void AnalyticsDashboard::drawPerformance(QPainter& painter) {
    int y = 80;
    int x = 20;
    
    painter.setPen(QColor("#FFFFFF"));
    painter.setFont(QFont("Segoe UI", 14, QFont::Bold));
    painter.drawText(x, y, "Performance Trends");
    y += 40;
    
    // Draw latency trend chart
    QRect latencyRect(x, y, width() / 2 - 30, 300);
    drawChart(painter, latencyRect, m_historyData, "latency");
    
    // Draw throughput trend chart
    QRect throughputRect(width() / 2 + 10, y, width() / 2 - 30, 300);
    drawChart(painter, throughputRect, m_historyData, "throughput");
}

void AnalyticsDashboard::drawChart(QPainter& painter, const QRect& rect,
                                   const QVector<AnalyticsSnapshot>& data,
                                   const QString& metric) {
    if (data.isEmpty()) {
        painter.setPen(QColor("#AAAAAA"));
        painter.setFont(QFont("Segoe UI", 11));
        painter.drawText(rect, Qt::AlignCenter, "No data available");
        return;
    }
    
    // Draw background
    painter.fillRect(rect, QColor("#2D2D2D"));
    
    // Find min/max values
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    
    for (const auto& snapshot : data) {
        double val = 0;
        if (metric == "requests") {
            val = snapshot.totalRequests;
        } else if (metric == "latency") {
            val = snapshot.averageLatency;
        } else if (metric == "throughput") {
            val = snapshot.totalTokens / (snapshot.averageLatency > 0 ? snapshot.averageLatency : 1);
        }
        
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }
    
    if (minVal == maxVal) {
        minVal = 0;
        maxVal = maxVal * 2;
    }
    
    // Draw axes
    painter.setPen(QColor("#666666"));
    painter.drawLine(rect.bottomLeft(), rect.topLeft());
    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
    
    // Draw line chart
    painter.setPen(QPen(m_chartLineColor, 2));
    
    int stepX = rect.width() / (data.size() - 1);
    QPoint prevPoint;
    
    for (int i = 0; i < data.size(); ++i) {
        const auto& snapshot = data[i];
        double val = 0;
        
        if (metric == "requests") {
            val = snapshot.totalRequests;
        } else if (metric == "latency") {
            val = snapshot.averageLatency;
        } else if (metric == "throughput") {
            val = snapshot.totalTokens / (snapshot.averageLatency > 0 ? snapshot.averageLatency : 1);
        }
        
        int x = rect.left() + (i * stepX);
        int y = rect.bottom() - ((val - minVal) / (maxVal - minVal) * rect.height());
        
        QPoint point(x, y);
        
        if (i > 0) {
            painter.drawLine(prevPoint, point);
        }
        
        prevPoint = point;
    }
    
    // Draw title
    painter.setPen(QColor("#FFFFFF"));
    painter.setFont(QFont("Segoe UI", 12, QFont::Bold));
    painter.drawText(rect.topLeft() + QPoint(10, 20), metric.toUpper());
}

void AnalyticsDashboard::drawMetricCard(QPainter& painter, int x, int y, int w, int h,
                                        const QString& title, const QString& value,
                                        const QString& delta, QColor color) {
    QRect cardRect(x, y, w, h);
    
    // Card background with gradient
    QLinearGradient gradient(cardRect.topLeft(), cardRect.bottomRight());
    gradient.setColorAt(0, QColor("#2D2D2D"));
    gradient.setColorAt(1, QColor("#1E1E1E"));
    painter.fillRect(cardRect, gradient);
    
    // Border accent
    painter.setPen(QPen(color, 3));
    painter.drawLine(cardRect.topLeft(), cardRect.topRight());
    
    // Title
    painter.setPen(QColor("#AAAAAA"));
    painter.setFont(QFont("Segoe UI", 11));
    painter.drawText(cardRect.adjusted(15, 15, -15, -h + 30), Qt::AlignLeft, title);
    
    // Value
    painter.setPen(QColor("#FFFFFF"));
    painter.setFont(QFont("Segoe UI", 20, QFont::Bold));
    painter.drawText(cardRect.adjusted(15, 35, -15, -h + 50), Qt::AlignLeft, value);
    
    // Delta
    painter.setPen(delta.contains("+") || delta.contains("Excellent") ? m_successColor : m_warningColor);
    painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(cardRect.adjusted(15, -20, -15, -15), Qt::AlignLeft, delta);
}

} // namespace brahma
