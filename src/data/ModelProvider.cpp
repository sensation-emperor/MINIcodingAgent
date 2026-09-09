#include "data/ModelProvider.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QUuid>
#include <QUrlQuery>

namespace Brahma {

ModelProvider::ModelProvider(const ModelProviderConfig &config,
                             QNetworkAccessManager *networkManager,
                             QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_networkManager(networkManager)
    , m_rateLimitTimer(new QTimer(this))
{
    connect(m_rateLimitTimer, &QTimer::timeout, this, [this]() {
        m_requestsThisMinute = 0;
        m_minuteStart = std::chrono::steady_clock::now();
    });
    
    m_minuteStart = std::chrono::steady_clock::now();
    m_rateLimitTimer->setInterval(60000);  // Reset every minute
    m_rateLimitTimer->start();
}

ModelProvider::~ModelProvider()
{
    cancelPendingRequests();
}

void ModelProvider::updateConfig(const ModelProviderConfig &config)
{
    m_config = config;
}

void ModelProvider::setEnabled(bool enabled)
{
    m_config.enabled = enabled;
}

bool ModelProvider::hasCapability(ModelCapability capability) const
{
    return m_config.capabilities.contains(capability);
}

void ModelProvider::discoverModels()
{
    if (!m_config.enabled) {
        emit errorOccurred("Provider is disabled");
        return;
    }
    
    QString endpoint;
    if (m_config.type == ProviderType::Local) {
        // Local providers (LM Studio, Ollama)
        if (m_config.name == "Ollama") {
            endpoint = QString("%1/tags").arg(m_config.endpoint);
        } else {
            // LM Studio and other OpenAI-compatible
            endpoint = QString("%1/models").arg(m_config.endpoint);
        }
    } else {
        // Cloud providers
        if (m_config.name == "OpenAI") {
            endpoint = QString("%1/models").arg(m_config.endpoint);
        } else if (m_config.name == "Anthropic") {
            // Anthropic doesn't have a models endpoint, use hardcoded list
            m_availableModels = {"claude-3-5-sonnet-20241022", "claude-3-opus-20240229", 
                                "claude-3-sonnet-20240229", "claude-3-haiku-20240307"};
            emit modelsDiscovered(m_availableModels);
            return;
        } else if (m_config.name == "Google AI") {
            endpoint = QString("%1/models?key=%2").arg(m_config.endpoint, m_config.apiKey);
        }
    }
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onModelsDiscoveryFinished(reply);
    });
}

ModelInfo ModelProvider::getModelInfo(const QString &modelId) const
{
    if (m_modelDetails.contains(modelId)) {
        return m_modelDetails[modelId];
    }
    
    // Return basic info if detailed info not available
    ModelInfo info;
    info.id = modelId;
    info.name = modelId;
    info.providerId = m_config.id.isEmpty() ? m_config.name : m_config.id;
    info.supportsStreaming = m_config.streamingEnabled;
    info.contextLength = m_config.maxTokens;
    
    return info;
}

void ModelProvider::checkHealth()
{
    if (!m_config.enabled) {
        m_healthStatus = ProviderHealth::Unknown;
        emit healthChanged(m_healthStatus);
        return;
    }
    
    QString healthEndpoint;
    if (m_config.type == ProviderType::Local) {
        // For local providers, check the base endpoint
        if (m_config.name == "Ollama") {
            healthEndpoint = QString("%1/api/version").arg(m_config.endpoint);
        } else {
            // LM Studio and others - just check if we can reach the endpoint
            healthEndpoint = m_config.endpoint;
        }
    } else {
        // Cloud providers - use their health/status endpoints
        if (m_config.name == "OpenAI") {
            healthEndpoint = "https://api.openai.com/v1/models";
        } else if (m_config.name == "Anthropic") {
            healthEndpoint = "https://api.anthropic.com/v1/models";
        } else if (m_config.name == "Google AI") {
            healthEndpoint = QString("%1/models?key=%2").arg(m_config.endpoint, m_config.apiKey);
        } else {
            healthEndpoint = m_config.endpoint;
        }
    }
    
    QNetworkRequest request = createRequest(healthEndpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    // Set a short timeout for health checks
    reply->setProperty("timeout", 5000);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHealthCheckFinished(reply);
    });
}

ProviderMetrics ModelProvider::getMetrics() const
{
    return m_metrics;
}

void ModelProvider::resetMetrics()
{
    m_metrics = ProviderMetrics();
}

void ModelProvider::sendChatRequest(const QString &model, const QString &prompt, bool stream)
{
    if (!m_config.enabled) {
        emit errorOccurred("Provider is disabled");
        return;
    }
    
    // Rate limiting check
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_minuteStart).count();
    if (elapsed >= 60) {
        m_requestsThisMinute = 0;
        m_minuteStart = now;
    }
    
    if (m_requestsThisMinute >= m_config.requestsPerMinute) {
        emit errorOccurred("Rate limit exceeded. Please wait.");
        return;
    }
    
    QString requestId = generateRequestId();
    QString endpoint;
    
    // Build endpoint based on provider type
    if (m_config.type == ProviderType::Local || m_config.name == "OpenAI") {
        endpoint = QString("%1/chat/completions").arg(m_config.endpoint);
    } else if (m_config.name == "Anthropic") {
        endpoint = "https://api.anthropic.com/v1/messages";
    } else if (m_config.name == "Google AI") {
        endpoint = QString("%1/models/%2:streamGenerateContent?alt=sse&key=%3")
                      .arg(m_config.endpoint, model, m_config.apiKey);
    }
    
    QNetworkRequest request = createRequest(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject payload = buildChatPayload(model, prompt);
    if (!stream) {
        payload["stream"] = false;
    }
    
    QJsonDocument doc(payload);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    QNetworkReply *reply = m_networkManager->post(request, jsonData);
    
    // Store pending request
    PendingRequest pendingReq;
    pendingReq.requestId = requestId;
    pendingReq.model = model;
    pendingReq.prompt = prompt;
    pendingReq.streaming = stream;
    pendingReq.startTime = QDateTime::currentDateTime();
    pendingReq.reply = reply;
    m_pendingRequests[requestId] = pendingReq;
    
    m_requestsThisMinute++;
    m_metrics.totalRequests++;
    m_metrics.lastRequestTime = QDateTime::currentDateTime();
    
    emit requestStarted(requestId);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkReplyFinished(reply);
    });
    
    if (stream) {
        connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
            onNetworkReplyReadyRead(reply);
        });
    }
}

void ModelProvider::sendCompletionRequest(const QString &model, const QString &prompt, bool stream)
{
    // For now, delegate to chat request - can be specialized later
    sendChatRequest(model, prompt, stream);
}

void ModelProvider::cancelPendingRequests()
{
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (it->reply) {
            it->reply->abort();
            it->reply->deleteLater();
        }
    }
    m_pendingRequests.clear();
}

QJsonObject ModelProvider::toJson() const
{
    return m_config.toJson();
}

ModelProvider* ModelProvider::fromJson(const QJsonObject &obj,
                                       QNetworkAccessManager *networkManager,
                                       QObject *parent)
{
    ModelProviderConfig config = ModelProviderConfig::fromJson(obj);
    return new ModelProvider(config, networkManager, parent);
}

void ModelProvider::onNetworkReplyFinished(QNetworkReply *reply)
{
    // Find the request ID
    QString requestId;
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (it->reply == reply) {
            requestId = it->requestId;
            break;
        }
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        QString error = reply->errorString();
        m_lastError = error;
        
        if (!requestId.isEmpty()) {
            m_metrics.failedRequests++;
            emit requestFailed(requestId, error);
            m_pendingRequests.remove(requestId);
        }
        
        // Update health status
        if (reply->property("isHealthCheck").toBool()) {
            m_healthStatus = ProviderHealth::Unhealthy;
            emit healthChanged(m_healthStatus);
        }
        
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    
    // Calculate latency
    double latencyMs = 0;
    if (!requestId.isEmpty() && m_pendingRequests.contains(requestId)) {
        auto startTime = m_pendingRequests[requestId].startTime;
        latencyMs = startTime.msecsTo(QDateTime::currentDateTime());
        m_metrics.averageLatencyMs = (m_metrics.averageLatencyMs * (m_metrics.totalRequests - 1) + latencyMs) / m_metrics.totalRequests;
    }
    
    // Process response
    if (!requestId.isEmpty()) {
        processChatResponse(responseData, requestId);
        
        m_metrics.successfulRequests++;
        m_metrics.lastSuccessfulRequestTime = QDateTime::currentDateTime();
        
        // Estimate tokens (rough approximation: 1 token ≈ 4 characters)
        int estimatedTokens = responseData.length() / 4;
        m_metrics.tokensPerSecond = (latencyMs > 0) ? (estimatedTokens * 1000.0 / latencyMs) : 0;
        
        emit requestCompleted(requestId, latencyMs, estimatedTokens);
        
        m_pendingRequests.remove(requestId);
    }
    
    reply->deleteLater();
}

void ModelProvider::onNetworkReplyReadyRead(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    
    // Find request ID
    QString requestId;
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (it->reply == reply) {
            requestId = it->requestId;
            break;
        }
    }
    
    if (!requestId.isEmpty()) {
        processStreamChunk(data, requestId);
    }
}

void ModelProvider::onHealthCheckFinished(QNetworkReply *reply)
{
    bool isHealthCheck = true;  // Mark as health check for error handling
    
    if (reply->error() != QNetworkReply::NoError) {
        m_healthStatus = ProviderHealth::Unhealthy;
        m_lastError = reply->errorString();
        qDebug() << "Health check failed for" << m_config.name << ":" << m_lastError;
    } else {
        // Check response status
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (statusCode.toInt() >= 200 && statusCode.toInt() < 300) {
            m_healthStatus = ProviderHealth::Healthy;
            m_lastError.clear();
        } else {
            m_healthStatus = ProviderHealth::Degraded;
            m_lastError = QString("HTTP %1").arg(statusCode.toInt());
        }
    }
    
    reply->setProperty("isHealthCheck", isHealthCheck);
    emit healthChanged(m_healthStatus);
    reply->deleteLater();
}

void ModelProvider::onModelsDiscoveryFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Model discovery failed:" << reply->errorString();
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    if (doc.isNull()) {
        qWarning() << "Invalid JSON in models response";
        reply->deleteLater();
        return;
    }
    
    QJsonObject root = doc.object();
    QStringList models;
    
    // Parse based on API format
    if (root.contains("data")) {
        // OpenAI/LM Studio format
        QJsonArray dataArray = root["data"].toArray();
        for (const QJsonValue &value : dataArray) {
            QJsonObject modelObj = value.toObject();
            QString modelId = modelObj["id"].toString();
            if (!modelId.isEmpty()) {
                models.append(modelId);
                
                // Store detailed info
                ModelInfo info;
                info.id = modelId;
                info.name = modelObj.contains("name") ? modelObj["name"].toString() : modelId;
                info.providerId = m_config.name;
                m_modelDetails[modelId] = info;
            }
        }
    } else if (root.contains("models")) {
        // Ollama format
        QJsonArray modelsArray = root["models"].toArray();
        for (const QJsonValue &value : modelsArray) {
            QJsonObject modelObj = value.toObject();
            QString modelId = modelObj["name"].toString();
            if (!modelId.isEmpty()) {
                models.append(modelId);
                
                ModelInfo info;
                info.id = modelId;
                info.name = modelId;
                info.providerId = m_config.name;
                info.details = modelObj["details"].toObject();
                m_modelDetails[modelId] = info;
            }
        }
    }
    
    m_availableModels = models;
    emit modelsDiscovered(models);
    reply->deleteLater();
}

void ModelProvider::processChatResponse(const QByteArray &data, const QString &requestId)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        emit requestFailed(requestId, "Invalid JSON response");
        return;
    }
    
    QJsonObject root = doc.object();
    QString response;
    
    // Parse based on provider format
    if (m_config.name == "Anthropic") {
        // Anthropic format
        QJsonArray contentArray = root["content"].toArray();
        for (const QJsonValue &content : contentArray) {
            QJsonObject contentObj = content.toObject();
            if (contentObj["type"].toString() == "text") {
                response += contentObj["text"].toString();
            }
        }
    } else if (root.contains("choices")) {
        // OpenAI/LM Studio/Ollama format
        QJsonArray choices = root["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            if (firstChoice.contains("message")) {
                QJsonObject message = firstChoice["message"].toObject();
                response = message["content"].toString();
            } else if (firstChoice.contains("text")) {
                response = firstChoice["text"].toString();
            }
        }
    } else if (root.contains("response")) {
        // Some providers use "response" field
        response = root["response"].toString();
    }
    
    if (!response.isEmpty()) {
        emit responseReceived(requestId, response);
    } else {
        emit requestFailed(requestId, "Empty response from model");
    }
}

void ModelProvider::processStreamChunk(const QByteArray &data, const QString &requestId)
{
    QString dataStr = QString::fromUtf8(data);
    
    // Server-Sent Events format: "data: {...}\n\n"
    QStringList lines = dataStr.split("\n", Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        if (line.startsWith("data: ")) {
            QString jsonStr = line.mid(6);  // Remove "data: " prefix
            
            if (jsonStr.trimmed() == "[DONE]") {
                // Stream complete
                continue;
            }
            
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                QString chunk;
                
                // Parse based on provider format
                if (m_config.name == "Anthropic") {
                    if (obj["type"].toString() == "content_block_delta") {
                        QJsonObject delta = obj["delta"].toObject();
                        chunk = delta["text"].toString();
                    }
                } else if (obj.contains("choices")) {
                    QJsonArray choices = obj["choices"].toArray();
                    if (!choices.isEmpty()) {
                        QJsonObject delta = choices[0].toObject()["delta"].toObject();
                        chunk = delta["content"].toString();
                    }
                }
                
                if (!chunk.isEmpty()) {
                    emit responseStream(requestId, chunk);
                }
            }
        }
    }
}

QString ModelProvider::generateRequestId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QNetworkRequest ModelProvider::createRequest(const QString &endpoint) const
{
    QNetworkRequest request(QUrl(endpoint));
    
    // Add authentication headers
    if (m_config.type == ProviderType::Cloud && !m_config.apiKey.isEmpty()) {
        if (m_config.name == "OpenAI" || m_config.name == "LM Studio" || m_config.name == "Ollama") {
            request.setRawHeader("Authorization", QString("Bearer %1").arg(m_config.apiKey).toUtf8());
        } else if (m_config.name == "Anthropic") {
            request.setRawHeader("x-api-key", m_config.apiKey.toUtf8());
            request.setRawHeader("anthropic-version", "2023-06-01");
        } else if (m_config.name == "Google AI") {
            // API key is in URL for Google
        }
    }
    
    // Set timeout
    request.setTransferTimeout(m_config.timeoutSeconds * 1000);
    
    return request;
}

QJsonObject ModelProvider::buildChatPayload(const QString &model, const QString &prompt) const
{
    QJsonObject payload;
    
    if (m_config.name == "Anthropic") {
        // Anthropic Messages API format
        payload["model"] = model.isEmpty() ? m_config.defaultModel : model;
        payload["max_tokens"] = m_config.maxTokens;
        payload["messages"] = QJsonArray{
            QJsonObject{
                {"role", "user"},
                {"content", prompt}
            }
        };
        if (m_config.streamingEnabled) {
            payload["stream"] = true;
        }
    } else if (m_config.name == "Google AI") {
        // Google Generative AI format
        payload["contents"] = QJsonArray{
            QJsonObject{
                {"parts", QJsonArray{
                    QJsonObject{{"text", prompt}}
                }}
            }
        };
        payload["generationConfig"] = QJsonObject{
            {"temperature", m_config.temperature},
            {"maxOutputTokens", m_config.maxTokens}
        };
    } else {
        // OpenAI-compatible format (LM Studio, Ollama, OpenAI)
        payload["model"] = model.isEmpty() ? m_config.defaultModel : model;
        payload["messages"] = QJsonArray{
            QJsonObject{
                {"role", "user"},
                {"content", prompt}
            }
        };
        payload["temperature"] = m_config.temperature;
        payload["max_tokens"] = m_config.maxTokens;
        payload["stream"] = m_config.streamingEnabled;
    }
    
    return payload;
}

} // namespace Brahma
