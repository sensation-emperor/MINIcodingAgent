#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>
#include "data/ModelProvider.h"

namespace Brahma {

/**
 * @brief Central router for managing all AI model providers (local and cloud)
 * 
 * Features:
 * - Dynamic provider registration/deregistration
 * - Health monitoring with automatic failover
 * - Load balancing across multiple providers
 * - Request routing based on capabilities
 * - Metrics collection and reporting
 */
class ModelRouter : public QObject
{
    Q_OBJECT

public:
    explicit ModelRouter(QObject *parent = nullptr);
    ~ModelRouter();

    // Provider Management
    bool addProvider(const ModelProviderConfig &config);
    bool removeProvider(const QString &providerId);
    bool updateProvider(const QString &providerId, const ModelProviderConfig &config);
    ModelProvider* getProvider(const QString &providerId);
    QList<ModelProvider*> getAllProviders() const;
    QList<ModelProvider*> getEnabledProviders() const;
    QList<ModelProvider*> getProvidersByCapability(ModelCapability capability) const;

    // Model Discovery
    QStringList getAvailableModels(const QString &providerId = QString()) const;
    ModelInfo getModelInfo(const QString &modelId) const;
    bool isModelAvailable(const QString &modelId) const;

    // Request Routing
    QString selectBestProvider(const QString &modelId, ModelCapability requiredCapability = ModelCapability::Text);
    
    // Health Monitoring
    void checkAllProvidersHealth();
    QMap<QString, ProviderHealth> getHealthStatus() const;
    bool isProviderHealthy(const QString &providerId) const;

    // Metrics
    ProviderMetrics getMetrics(const QString &providerId) const;
    QMap<QString, ProviderMetrics> getAllMetrics() const;
    void resetMetrics(const QString &providerId = QString());

    // Persistence
    bool saveConfiguration(const QString &filePath);
    bool loadConfiguration(const QString &filePath);

    // Singleton-like access for global state
    static ModelRouter* instance();

signals:
    void providerAdded(const QString &providerId);
    void providerRemoved(const QString &providerId);
    void providerHealthChanged(const QString &providerId, ProviderHealth health);
    void modelsDiscovered(const QString &providerId, const QStringList &models);
    void configurationSaved();
    void configurationLoaded();
    void errorOccurred(const QString &message);

private slots:
    void onProviderHealthCheck(const QString &providerId, ProviderHealth health);
    void onModelsDiscovered(const QString &providerId, const QStringList &models);

private:
    void initDefaultProviders();
    void startHealthMonitor();
    QString generateProviderId(const QString &name, ProviderType type);
    
    QMap<QString, std::unique_ptr<ModelProvider>> m_providers;
    QMap<QString, ProviderHealth> m_healthStatus;
    QMap<QString, ProviderMetrics> m_metrics;
    QNetworkAccessManager *m_networkManager;
    QTimer *m_healthMonitorTimer;
    QString m_defaultProviderId;
    
    static ModelRouter* s_instance;
};

} // namespace Brahma
