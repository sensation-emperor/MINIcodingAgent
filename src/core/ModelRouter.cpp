#include "ModelRouter.h"
#include <QFile>
#include <QJsonArray>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

namespace Brahma {

ModelRouter* ModelRouter::s_instance = nullptr;

ModelRouter::ModelRouter(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_healthMonitorTimer(new QTimer(this))
{
    if (!s_instance) {
        s_instance = this;
    }
    
    connect(m_healthMonitorTimer, &QTimer::timeout, this, [this]() {
        checkAllProvidersHealth();
    });
    
    initDefaultProviders();
    startHealthMonitor();
}

ModelRouter::~ModelRouter()
{
    m_providers.clear();
}

ModelRouter* ModelRouter::instance()
{
    return s_instance;
}

void ModelRouter::initDefaultProviders()
{
    // Add default local providers
    ModelProviderConfig lmStudioConfig;
    lmStudioConfig.name = "LM Studio";
    lmStudioConfig.type = ProviderType::Local;
    lmStudioConfig.endpoint = "http://localhost:1234/v1";
    lmStudioConfig.apiKey = "";  // LM Studio doesn't require API key
    lmStudioConfig.defaultModel = "local-model";
    lmStudioConfig.enabled = true;
    lmStudioConfig.capabilities = {ModelCapability::Text, ModelCapability::Chat};
    addProvider(lmStudioConfig);
    
    ModelProviderConfig ollamaConfig;
    ollamaConfig.name = "Ollama";
    ollamaConfig.type = ProviderType::Local;
    ollamaConfig.endpoint = "http://localhost:11434/api";
    ollamaConfig.apiKey = "";
    ollamaConfig.defaultModel = "llama3";
    ollamaConfig.enabled = true;
    ollamaConfig.capabilities = {ModelCapability::Text, ModelCapability::Chat};
    addProvider(ollamaConfig);
    
    // Add default cloud providers (disabled by default - user must add API keys)
    ModelProviderConfig openaiConfig;
    openaiConfig.name = "OpenAI";
    openaiConfig.type = ProviderType::Cloud;
    openaiConfig.endpoint = "https://api.openai.com/v1";
    openaiConfig.apiKey = "";
    openaiConfig.defaultModel = "gpt-4o";
    openaiConfig.enabled = false;
    openaiConfig.capabilities = {ModelCapability::Text, ModelCapability::Chat, ModelCapability::Vision, ModelCapability::Tools};
    addProvider(openaiConfig);
    
    ModelProviderConfig anthropicConfig;
    anthropicConfig.name = "Anthropic";
    anthropicConfig.type = ProviderType::Cloud;
    anthropicConfig.endpoint = "https://api.anthropic.com";
    anthropicConfig.apiKey = "";
    anthropicConfig.defaultModel = "claude-3-5-sonnet-20241022";
    anthropicConfig.enabled = false;
    anthropicConfig.capabilities = {ModelCapability::Text, ModelCapability::Chat, ModelCapability::Vision, ModelCapability::Tools};
    addProvider(anthropicConfig);
    
    ModelProviderConfig googleConfig;
    googleConfig.name = "Google AI";
    googleConfig.type = ProviderType::Cloud;
    googleConfig.endpoint = "https://generativelanguage.googleapis.com/v1beta";
    googleConfig.apiKey = "";
    googleConfig.defaultModel = "gemini-1.5-pro";
    googleConfig.enabled = false;
    googleConfig.capabilities = {ModelCapability::Text, ModelCapability::Chat, ModelCapability::Vision, ModelCapability::Tools};
    addProvider(googleConfig);
}

void ModelRouter::startHealthMonitor()
{
    m_healthMonitorTimer->setInterval(30000);  // Check every 30 seconds
    m_healthMonitorTimer->start();
}

QString ModelRouter::generateProviderId(const QString &name, ProviderType type)
{
    QString typeStr = (type == ProviderType::Local) ? "local" : "cloud";
    return QString("%1_%2").arg(typeStr, name.toLower().replace(" ", "_"));
}

bool ModelRouter::addProvider(const ModelProviderConfig &config)
{
    QString providerId = generateProviderId(config.name, config.type);
    
    if (m_providers.contains(providerId)) {
        qWarning() << "Provider already exists:" << providerId;
        return false;
    }
    
    auto provider = std::make_unique<ModelProvider>(config, m_networkManager, this);
    
    // Connect signals
    connect(provider.get(), &ModelProvider::healthChanged, this, [this, providerId](ProviderHealth health) {
        emit providerHealthChanged(providerId, health);
    });
    
    connect(provider.get(), &ModelProvider::modelsDiscovered, this, [this, providerId](const QStringList &models) {
        emit modelsDiscovered(providerId, models);
    });
    
    m_providers[providerId] = std::move(provider);
    m_healthStatus[providerId] = ProviderHealth::Unknown;
    
    // Initialize metrics
    ProviderMetrics metrics;
    metrics.totalRequests = 0;
    metrics.successfulRequests = 0;
    metrics.failedRequests = 0;
    metrics.averageLatencyMs = 0;
    metrics.tokensPerSecond = 0;
    m_metrics[providerId] = metrics;
    
    emit providerAdded(providerId);
    qDebug() << "Provider added:" << providerId;
    
    // Trigger initial health check
    QTimer::singleShot(1000, this, [this, providerId]() {
        if (m_providers.contains(providerId)) {
            m_providers[providerId]->checkHealth();
        }
    });
    
    return true;
}

bool ModelRouter::removeProvider(const QString &providerId)
{
    if (!m_providers.contains(providerId)) {
        return false;
    }
    
    m_providers.erase(providerId);
    m_healthStatus.remove(providerId);
    m_metrics.remove(providerId);
    
    emit providerRemoved(providerId);
    qDebug() << "Provider removed:" << providerId;
    
    return true;
}

bool ModelRouter::updateProvider(const QString &providerId, const ModelProviderConfig &config)
{
    if (!m_providers.contains(providerId)) {
        return false;
    }
    
    m_providers[providerId]->updateConfig(config);
    qDebug() << "Provider updated:" << providerId;
    
    return true;
}

ModelProvider* ModelRouter::getProvider(const QString &providerId)
{
    if (!m_providers.contains(providerId)) {
        return nullptr;
    }
    return m_providers[providerId].get();
}

QList<ModelProvider*> ModelRouter::getAllProviders() const
{
    QList<ModelProvider*> providers;
    for (const auto &provider : m_providers.values()) {
        providers.append(provider.get());
    }
    return providers;
}

QList<ModelProvider*> ModelRouter::getEnabledProviders() const
{
    QList<ModelProvider*> providers;
    for (const auto &provider : m_providers.values()) {
        if (provider->isEnabled()) {
            providers.append(provider.get());
        }
    }
    return providers;
}

QList<ModelProvider*> ModelRouter::getProvidersByCapability(ModelCapability capability) const
{
    QList<ModelProvider*> providers;
    for (const auto &provider : m_providers.values()) {
        if (provider->isEnabled() && provider->hasCapability(capability)) {
            providers.append(provider.get());
        }
    }
    return providers;
}

QStringList ModelRouter::getAvailableModels(const QString &providerId) const
{
    QStringList allModels;
    
    if (!providerId.isEmpty()) {
        if (m_providers.contains(providerId)) {
            return m_providers[providerId]->getAvailableModels();
        }
        return allModels;
    }
    
    // Get models from all enabled providers
    for (const auto &provider : m_providers.values()) {
        if (provider->isEnabled()) {
            allModels.append(provider->getAvailableModels());
        }
    }
    
    allModels.removeDuplicates();
    return allModels;
}

ModelInfo ModelRouter::getModelInfo(const QString &modelId) const
{
    ModelInfo info;
    info.id = modelId;
    info.name = modelId;
    info.providerId = "";
    
    // Search through providers to find the model
    for (const auto &provider : m_providers.values()) {
        if (provider->getAvailableModels().contains(modelId)) {
            info.providerId = generateProviderId(provider->getName(), provider->getType());
            info.name = modelId;
            break;
        }
    }
    
    return info;
}

bool ModelRouter::isModelAvailable(const QString &modelId) const
{
    for (const auto &provider : m_providers.values()) {
        if (provider->isEnabled() && provider->getAvailableModels().contains(modelId)) {
            return true;
        }
    }
    return false;
}

QString ModelRouter::selectBestProvider(const QString &modelId, ModelCapability requiredCapability)
{
    // First, try to find a provider that has this specific model
    for (const auto &provider : m_providers.values()) {
        if (provider->isEnabled() && 
            provider->getAvailableModels().contains(modelId) &&
            isProviderHealthy(generateProviderId(provider->getName(), provider->getType()))) {
            return generateProviderId(provider->getName(), provider->getType());
        }
    }
    
    // Fallback: find any healthy provider with the required capability
    QList<ModelProvider*> candidates = getProvidersByCapability(requiredCapability);
    for (ModelProvider *provider : candidates) {
        QString providerId = generateProviderId(provider->getName(), provider->getType());
        if (isProviderHealthy(providerId)) {
            return providerId;
        }
    }
    
    return "";
}

void ModelRouter::checkAllProvidersHealth()
{
    for (const auto &provider : m_providers.values()) {
        provider->checkHealth();
    }
}

QMap<QString, ProviderHealth> ModelRouter::getHealthStatus() const
{
    return m_healthStatus;
}

bool ModelRouter::isProviderHealthy(const QString &providerId) const
{
    if (!m_healthStatus.contains(providerId)) {
        return false;
    }
    return m_healthStatus[providerId] == ProviderHealth::Healthy;
}

ProviderMetrics ModelRouter::getMetrics(const QString &providerId) const
{
    if (!m_metrics.contains(providerId)) {
        return ProviderMetrics();
    }
    return m_metrics[providerId];
}

QMap<QString, ProviderMetrics> ModelRouter::getAllMetrics() const
{
    return m_metrics;
}

void ModelRouter::resetMetrics(const QString &providerId)
{
    if (providerId.isEmpty()) {
        // Reset all
        for (auto &metrics : m_metrics) {
            metrics.totalRequests = 0;
            metrics.successfulRequests = 0;
            metrics.failedRequests = 0;
            metrics.averageLatencyMs = 0;
            metrics.tokensPerSecond = 0;
        }
    } else if (m_metrics.contains(providerId)) {
        m_metrics[providerId].totalRequests = 0;
        m_metrics[providerId].successfulRequests = 0;
        m_metrics[providerId].failedRequests = 0;
        m_metrics[providerId].averageLatencyMs = 0;
        m_metrics[providerId].tokensPerSecond = 0;
    }
}

bool ModelRouter::saveConfiguration(const QString &filePath)
{
    QJsonObject root;
    QJsonArray providersArray;
    
    for (const auto &provider : m_providers.values()) {
        providersArray.append(provider->toConfig().toJson());
    }
    
    root["providers"] = providersArray;
    root["defaultProvider"] = m_defaultProviderId;
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "Failed to open file for writing:" << filePath;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    emit configurationSaved();
    qDebug() << "Configuration saved to:" << filePath;
    
    return true;
}

bool ModelRouter::loadConfiguration(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open file for reading:" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) {
        qCritical() << "Invalid JSON in configuration file";
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Clear existing providers
    m_providers.clear();
    
    // Load providers from config
    QJsonArray providersArray = root["providers"].toArray();
    for (const QJsonValue &value : providersArray) {
        ModelProviderConfig config = ModelProviderConfig::fromJson(value.toObject());
        addProvider(config);
    }
    
    // Restore default provider
    if (root.contains("defaultProvider")) {
        m_defaultProviderId = root["defaultProvider"].toString();
    }
    
    emit configurationLoaded();
    qDebug() << "Configuration loaded from:" << filePath;
    
    return true;
}

void ModelRouter::onProviderHealthCheck(const QString &providerId, ProviderHealth health)
{
    m_healthStatus[providerId] = health;
    emit providerHealthChanged(providerId, health);
}

void ModelRouter::onModelsDiscovered(const QString &providerId, const QStringList &models)
{
    emit modelsDiscovered(providerId, models);
}

} // namespace Brahma
