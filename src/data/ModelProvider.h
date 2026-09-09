#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <chrono>

#ifdef BUILD_GUI
#include <QNetworkAccessManager>
#include <QNetworkReply>
#endif

namespace Brahma {

enum class ProviderType {
    Local,
    Cloud
};

enum class ModelCapability {
    Text,
    Chat,
    Vision,
    Tools,
    Embeddings,
    Code
};

enum class ProviderHealth {
    Unknown,
    Healthy,
    Degraded,
    Unhealthy
};

struct ProviderMetrics {
    quint64 totalRequests = 0;
    quint64 successfulRequests = 0;
    quint64 failedRequests = 0;
    double averageLatencyMs = 0.0;
    double tokensPerSecond = 0.0;
    QDateTime lastRequestTime;
    QDateTime lastSuccessfulRequestTime;
    
    double getSuccessRate() const {
        if (totalRequests == 0) return 0.0;
        return (static_cast<double>(successfulRequests) / totalRequests) * 100.0;
    }
};

struct ModelInfo {
    QString id;
    QString name;
    QString providerId;
    QString description;
    int contextLength = 4096;
    int maxTokens = 2048;
    bool supportsVision = false;
    bool supportsTools = false;
    bool supportsStreaming = true;
    QString modelType;  // e.g., "llama", "mistral", "gpt"
    qint64 parameterCount = 0;  // in billions
    QString quantization;  // e.g., "Q4_K_M", "FP16"
    qint64 fileSizeBytes = 0;
    QStringList capabilities;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct ModelProviderConfig {
    QString id;
    QString name;
    ProviderType type = ProviderType::Local;
    QString endpoint;
    QString apiKey;
    QString defaultModel;
    bool enabled = true;
    QList<ModelCapability> capabilities;
    
    // Advanced settings
    double temperature = 0.7;
    int maxTokens = 4096;
    int timeoutSeconds = 60;
    int maxRetries = 3;
    bool streamingEnabled = true;
    
    // Rate limiting
    int requestsPerMinute = 60;
    int tokensPerMinute = 100000;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["type"] = (type == ProviderType::Local) ? "local" : "cloud";
        obj["endpoint"] = endpoint;
        obj["apiKey"] = apiKey;
        obj["defaultModel"] = defaultModel;
        obj["enabled"] = enabled;
        obj["temperature"] = temperature;
        obj["maxTokens"] = maxTokens;
        obj["timeoutSeconds"] = timeoutSeconds;
        obj["maxRetries"] = maxRetries;
        obj["streamingEnabled"] = streamingEnabled;
        obj["requestsPerMinute"] = requestsPerMinute;
        obj["tokensPerMinute"] = tokensPerMinute;
        
        QJsonArray capsArray;
        for (const auto &cap : capabilities) {
            switch (cap) {
                case ModelCapability::Text: capsArray.append("text"); break;
                case ModelCapability::Chat: capsArray.append("chat"); break;
                case ModelCapability::Vision: capsArray.append("vision"); break;
                case ModelCapability::Tools: capsArray.append("tools"); break;
                case ModelCapability::Embeddings: capsArray.append("embeddings"); break;
                case ModelCapability::Code: capsArray.append("code"); break;
            }
        }
        obj["capabilities"] = capsArray;
        
        return obj;
    }
    
    static ModelProviderConfig fromJson(const QJsonObject &obj) {
        ModelProviderConfig config;
        config.id = obj["id"].toString();
        config.name = obj["name"].toString();
        config.type = (obj["type"].toString() == "local") ? ProviderType::Local : ProviderType::Cloud;
        config.endpoint = obj["endpoint"].toString();
        config.apiKey = obj["apiKey"].toString();
        config.defaultModel = obj["defaultModel"].toString();
        config.enabled = obj["enabled"].toBool(true);
        config.temperature = obj["temperature"].toDouble(0.7);
        config.maxTokens = obj["maxTokens"].toInt(4096);
        config.timeoutSeconds = obj["timeoutSeconds"].toInt(60);
        config.maxRetries = obj["maxRetries"].toInt(3);
        config.streamingEnabled = obj["streamingEnabled"].toBool(true);
        config.requestsPerMinute = obj["requestsPerMinute"].toInt(60);
        config.tokensPerMinute = obj["tokensPerMinute"].toInt(100000);
        
        QJsonArray capsArray = obj["capabilities"].toArray();
        for (const auto &cap : capsArray) {
            QString capStr = cap.toString();
            if (capStr == "text") config.capabilities.append(ModelCapability::Text);
            else if (capStr == "chat") config.capabilities.append(ModelCapability::Chat);
            else if (capStr == "vision") config.capabilities.append(ModelCapability::Vision);
            else if (capStr == "tools") config.capabilities.append(ModelCapability::Tools);
            else if (capStr == "embeddings") config.capabilities.append(ModelCapability::Embeddings);
            else if (capStr == "code") config.capabilities.append(ModelCapability::Code);
        }
        
        return config;
    }
};

/**
 * @brief Represents a single AI model provider (local or cloud)
 */
class ModelProvider : public QObject
{
    Q_OBJECT

public:
    explicit ModelProvider(const ModelProviderConfig &config, 
                          QNetworkAccessManager *networkManager,
                          QObject *parent = nullptr);
    ~ModelProvider();

    // Configuration
    void updateConfig(const ModelProviderConfig &config);
    ModelProviderConfig getConfig() const { return m_config; }
    
    // Properties
    QString getName() const { return m_config.name; }
    ProviderType getType() const { return m_config.type; }
    bool isEnabled() const { return m_config.enabled; }
    void setEnabled(bool enabled);
    
    // Capabilities
    bool hasCapability(ModelCapability capability) const;
    QList<ModelCapability> getCapabilities() const { return m_config.capabilities; }
    
    // Model Discovery
    void discoverModels();
    QStringList getAvailableModels() const { return m_availableModels; }
    ModelInfo getModelInfo(const QString &modelId) const;
    
    // Health Check
    void checkHealth();
    ProviderHealth getHealthStatus() const { return m_healthStatus; }
    QString getLastError() const { return m_lastError; }
    
    // Metrics
    ProviderMetrics getMetrics() const { return m_metrics; }
    void resetMetrics();
    
    // Request Methods (to be implemented in cpp)
    void sendChatRequest(const QString &model, const QString &prompt, 
                        bool stream = true);
    void sendCompletionRequest(const QString &model, const QString &prompt,
                              bool stream = true);
    void cancelPendingRequests();
    
    // Serialization
    QJsonObject toJson() const;
    static ModelProvider* fromJson(const QJsonObject &obj, 
                                   QNetworkAccessManager *networkManager,
                                   QObject *parent = nullptr);

signals:
    void healthChanged(ProviderHealth health);
    void modelsDiscovered(const QStringList &models);
    void requestStarted(const QString &requestId);
    void responseReceived(const QString &requestId, const QString &response);
    void responseStream(const QString &requestId, const QString &chunk);
    void requestCompleted(const QString &requestId, double latencyMs, int tokensGenerated);
    void requestFailed(const QString &requestId, const QString &error);
    void errorOccurred(const QString &message);

private slots:
    void onNetworkReplyFinished(QNetworkReply *reply);
    void onNetworkReplyReadyRead(QNetworkReply *reply);
    void onHealthCheckFinished(QNetworkReply *reply);
    void onModelsDiscoveryFinished(QNetworkReply *reply);

private:
    struct PendingRequest {
        QString requestId;
        QString model;
        QString prompt;
        bool streaming;
        QDateTime startTime;
        QNetworkReply *reply;
    };
    
    void processChatResponse(const QByteArray &data, const QString &requestId);
    void processStreamChunk(const QByteArray &data, const QString &requestId);
    QString generateRequestId() const;
    QNetworkRequest createRequest(const QString &endpoint) const;
    QJsonObject buildChatPayload(const QString &model, const QString &prompt) const;
    
    ModelProviderConfig m_config;
    QNetworkAccessManager *m_networkManager;
    QStringList m_availableModels;
    QMap<QString, ModelInfo> m_modelDetails;
    ProviderHealth m_healthStatus = ProviderHealth::Unknown;
    ProviderMetrics m_metrics;
    QString m_lastError;
    QMap<QString, PendingRequest> m_pendingRequests;
    QTimer *m_rateLimitTimer;
    int m_requestsThisMinute = 0;
    std::chrono::steady_clock::time_point m_minuteStart;
};

} // namespace Brahma
