# Metrics Dashboard Guide

## Overview

The **Analytics Dashboard** provides real-time visibility into system performance, model usage, agent activity, and cost tracking. This guide covers all features and usage patterns.

---

## 🎯 Features

### Real-Time Metrics Collection

The `MetricsCollector` singleton tracks:
- **Model Performance**: Requests, success rate, latency, tokens/second
- **Agent Activity**: Actions performed, execution duration
- **Tool Usage**: Invocation counts, success rates, timing
- **File Operations**: Read/write operations with sizes
- **User Actions**: Interactive sessions and commands
- **Cost Tracking**: Per-model cost estimation

### Analytics Dashboard Widget

The `AnalyticsDashboard` provides 5 view modes:

#### 1. Overview Mode
Displays key metrics cards:
- Total Requests (with trend)
- Total Tokens processed
- Average Latency
- Success Rate percentage
- Historical trend chart

#### 2. Models Mode
Per-model breakdown:
- Request count per model
- Success rate with color coding
- Average latency in milliseconds
- Throughput (tokens/second)
- Last error messages

#### 3. Agents Mode
Agent performance metrics:
- Total actions per agent type
- Average action duration
- Action type distribution
- Workload balancing insights

#### 4. Costs Mode
Financial tracking:
- Cost breakdown by model
- Bar chart visualization
- Total estimated cost
- Configurable cost per token

#### 5. Performance Mode
Trend analysis:
- Latency over time (line chart)
- Throughput trends
- Comparative analysis
- Time range selection (1h, 6h, 24h, 7d, 30d)

---

## 🔧 Usage

### Accessing the Dashboard

```cpp
// In your main window or view
#include "core/MetricsCollector.h"

auto* dashboard = new brahma::AnalyticsDashboard(this);
dashboard->setViewMode(AnalyticsDashboard::ViewMode::Overview);
dashboard->setTimeRange(24); // 24 hours
dashboard->setAutoRefresh(true);
dashboard->setRefreshInterval(5); // 5 seconds
```

### Recording Metrics

```cpp
auto* metrics = brahma::MetricsCollector::instance();

// Record model request
metrics->recordModelRequest("gpt-4", true, 150, 234.5);

// Record agent action
metrics->recordAgentAction("CoderAgent", "generate_code", 1250.0);

// Record tool usage
metrics->recordToolUsage("filesystem_write", true, 45.2);

// Set model cost (in dollars per token)
metrics->setModelCost("gpt-4", 0.00003);  // $0.03 per 1K tokens
```

### Exporting Data

```cpp
// Export to JSON file
metrics->exportMetrics("/path/to/metrics_export.json");

// Or via dashboard UI
dashboard->exportCurrentView("/path/to/export.json");
```

### Getting Current Snapshot

```cpp
auto snapshot = metrics->getCurrentSnapshot();

qDebug() << "Total requests:" << snapshot.totalRequests;
qDebug() << "Success rate:" << snapshot.successRate << "%";
qDebug() << "Estimated cost: $" << snapshot.estimatedCost / 100.0;
```

---

## ⚙️ Configuration

### Alert Thresholds

Configure thresholds for automatic alerts:

```cpp
auto* metrics = brahma::MetricsCollector::instance();

// Set latency threshold (milliseconds)
metrics->setLatencyThreshold(1000.0);  // Alert if > 1 second

// Set error rate threshold (percentage)
metrics->setErrorRateThreshold(5.0);   // Alert if error rate > 5%

connect(metrics, &MetricsCollector::alertTriggered,
        [](const QString& type, const QString& message, double value) {
    qWarning() << "ALERT [" << type << "]:" << message;
    // Show notification, send email, etc.
});
```

### Auto-Refresh Settings

```cpp
dashboard->setAutoRefresh(true);
dashboard->setRefreshInterval(5);  // Refresh every 5 seconds
```

### Time Range Selection

Available ranges:
- 1 Hour (recent activity)
- 6 Hours (half-day)
- 24 Hours (full day)
- 168 Hours (7 days)
- 720 Hours (30 days)

---

## 📊 Data Structures

### ModelMetrics

```cpp
struct ModelMetrics {
    QString modelId;
    qint64 totalRequests = 0;
    qint64 successfulRequests = 0;
    qint64 failedRequests = 0;
    qint64 totalTokens = 0;
    double averageLatency = 0.0;      // milliseconds
    double tokensPerSecond = 0.0;
    double successRate = 100.0;       // percentage
    QDateTime lastUsed;
    QDateTime lastError;
    QString lastErrorMessage;
};
```

### AnalyticsSnapshot

```cpp
struct AnalyticsSnapshot {
    QDateTime timestamp;
    qint64 totalRequests = 0;
    qint64 totalTokens = 0;
    double averageLatency = 0.0;
    double successRate = 100.0;
    qint64 estimatedCost = 0;         // cents
    
    QMap<QString, qint64> requestsByModel;
    QMap<QString, qint64> tokensByModel;
    QMap<QString, qint64> costByModel;
};
```

---

## 🎨 Customization

### Styling

The dashboard uses the global theme system:

```cpp
// Colors are defined in Theme.h
// Primary: Coral Rose (#FF6B9D)
// Success: Green (#4CAF50)
// Warning: Amber (#FFC107)
// Error: Red (#F44336)
// Chart: Blue (#2196F3)
```

### Custom View Modes

Extend the dashboard with custom views:

```cpp
class MyCustomDashboard : public AnalyticsDashboard {
    Q_OBJECT
public:
    void drawCustomView(QPainter& painter) {
        // Custom rendering logic
    }
    
protected:
    void paintEvent(QPaintEvent* event) override {
        if (m_currentMode == ViewMode::Custom) {
            drawCustomView(painter);
        } else {
            AnalyticsDashboard::paintEvent(event);
        }
    }
};
```

---

## 🔍 Best Practices

### 1. Performance
- Use auto-refresh sparingly (5-10 second intervals)
- Export large datasets asynchronously
- Limit time range for historical queries

### 2. Cost Tracking
- Configure accurate cost per token for each model
- Include both input and output tokens
- Update costs when provider pricing changes

### 3. Alerting
- Set reasonable thresholds to avoid alert fatigue
- Implement exponential backoff for repeated alerts
- Log alerts for post-mortem analysis

### 4. Data Retention
- Hourly snapshots kept for 7 days (168 entries)
- Daily snapshots kept for 30 days
- Export historical data before cleanup

---

## 🐛 Troubleshooting

### No Data Showing
- Verify metrics are being recorded
- Check time range selection
- Ensure auto-refresh is enabled

### High Memory Usage
- Reduce refresh interval
- Limit time range for charts
- Call `resetMetrics()` periodically

### Incorrect Cost Calculations
- Verify `setModelCost()` is called before requests
- Check cost units (dollars per token vs per 1K tokens)
- Review token counting logic

---

## 📈 Integration Examples

### With ModelRouter

```cpp
// In ModelRouter.cpp
void ModelRouter::recordCompletion(const QString& provider,
                                   int promptTokens,
                                   int completionTokens,
                                   double latencyMs) {
    auto* metrics = MetricsCollector::instance();
    metrics->recordModelRequest(provider, true,
                               promptTokens + completionTokens,
                               latencyMs);
}
```

### With Agent System

```cpp
// In Orchestrator.cpp
void Orchestrator::executeAgentTask(AgentType type, const QString& task) {
    QElapsedTimer timer;
    timer.start();
    
    // ... execute task ...
    
    double duration = timer.elapsed();
    auto* metrics = MetricsCollector::instance();
    metrics->recordAgentAction(agentTypeToString(type), 
                              "task_execution", duration);
}
```

### With Testing Engine

```cpp
// In TestingManager.cpp
TestResult TestingManager::runTests(const QString& command) {
    auto result = executeTestCommand(command);
    
    auto* metrics = MetricsCollector::instance();
    metrics->recordToolUsage("test_runner", result.success,
                            result.durationMs);
    
    return result;
}
```

---

## 📝 API Reference

### MetricsCollector

| Method | Description |
|--------|-------------|
| `instance()` | Get singleton instance |
| `recordModelRequest(...)` | Record model API call |
| `recordModelError(...)` | Record model failure |
| `recordAgentAction(...)` | Record agent activity |
| `recordToolUsage(...)` | Record tool invocation |
| `setModelCost(...)` | Configure cost per token |
| `getModelMetrics(...)` | Get metrics for specific model |
| `getAllModelMetrics()` | Get all model metrics |
| `getCurrentSnapshot()` | Get aggregated current snapshot |
| `getSnapshots(hours)` | Get historical snapshots |
| `exportMetrics(path)` | Export to JSON file |
| `resetMetrics()` | Clear all metrics |

### AnalyticsDashboard

| Method | Description |
|--------|-------------|
| `setViewMode(mode)` | Switch between view modes |
| `setTimeRange(hours)` | Set historical time range |
| `setAutoRefresh(enabled)` | Enable/disable auto-refresh |
| `setRefreshInterval(seconds)` | Set refresh frequency |
| `refresh()` | Manual refresh |
| `exportCurrentView(path)` | Export current view data |
| `resetView()` | Reset to default view |

---

## 🔗 Related Documentation

- `GUI_GUIDE.md` - Complete GUI usage
- `MODEL_SETUP.md` - Model provider configuration
- `DEVELOPMENT_STATUS.md` - Project status overview

---

*Last Updated: 2026-09-09*
*Version: 1.0.0*
