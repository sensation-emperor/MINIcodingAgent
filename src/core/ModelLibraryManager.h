#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include <QDir>
#include <QStandardPaths>
#include "src/data/ModelProvider.h"

namespace Brahma {

enum class DownloadStatus {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled
};

struct ModelDownloadItem {
    QString id;
    QString modelId;
    QString modelName;
    QString publisher;
    QString format;  // GGUF, Safetensors, etc.
    QString quantization;
    qint64 fileSizeBytes;
    qint64 downloadedBytes;
    double downloadSpeed;  // bytes per second
    int etaSeconds;
    DownloadStatus status;
    QString errorMessage;
    QDateTime startTime;
    QDateTime completedTime;
    bool paused;
    
    double getProgress() const {
        if (fileSizeBytes == 0) return 0.0;
        return static_cast<double>(downloadedBytes) / fileSizeBytes * 100.0;
    }
    
    QString getFormattedSize() const {
        return formatBytes(fileSizeBytes);
    }
    
    QString getFormattedSpeed() const {
        return formatBytes(static_cast<qint64>(downloadSpeed)) + "/s";
    }
    
    static QString formatBytes(qint64 bytes) {
        const QStringList units = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = bytes;
        
        while (size >= 1024 && unitIndex < units.size() - 1) {
            size /= 1024;
            unitIndex++;
        }
        
        return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
    }
};

struct LocalModelInfo {
    QString path;
    QString name;
    QString format;
    qint64 fileSizeBytes;
    QDateTime modifiedDate;
    int contextLength;
    int parameterCount;  // in billions
    QString quantization;
    bool isValid;
    QString errorMessage;
};

/**
 * @brief Manages model library, downloads, and local model imports
 * 
 * Features:
 * - Browse Hugging Face model catalog
 * - Download models with pause/resume support
 * - Import local models
 * - Model validation and metadata extraction
 * - Storage management
 */
class ModelLibraryManager : public QObject
{
    Q_OBJECT

public:
    explicit ModelLibraryManager(QObject *parent = nullptr);
    ~ModelLibraryManager();

    // Catalog & Discovery
    void searchModels(const QString &query, const QStringList &filters = {});
    void browseCatalog(const QString &category = "trending", int page = 1, int pageSize = 20);
    QList<ModelCard> getFeaturedModels() const;
    
    // Download Management
    void downloadModel(const QString &modelId, const QString &format = "GGUF",
                      const QString &quantization = "Q4_K_M");
    void pauseDownload(const QString &downloadId);
    void resumeDownload(const QString &downloadId);
    void cancelDownload(const QString &downloadId);
    void retryDownload(const QString &downloadId);
    
    QList<ModelDownloadItem> getActiveDownloads() const;
    QList<ModelDownloadItem> getCompletedDownloads() const;
    QList<ModelDownloadItem> getAllDownloads() const;
    ModelDownloadItem getDownloadStatus(const QString &downloadId) const;
    
    // Local Models
    QList<LocalModelInfo> scanLocalModels(const QString &directory = QString()) const;
    bool importLocalModel(const QString &filePath);
    bool removeLocalModel(const QString &modelId);
    LocalModelInfo validateModelFile(const QString &filePath) const;
    
    // Storage Management
    QString getModelStoragePath() const;
    qint64 getTotalStorageUsed() const;
    qint64 getAvailableStorage() const;
    void setStoragePath(const QString &path);
    bool cleanupIncompleteDownloads();
    
    // Model Metadata
    ModelCard getModelDetails(const QString &modelId) const;
    QStringList getAvailableFormats(const QString &modelId) const;
    QStringList getAvailableQuantizations(const QString &modelId, const QString &format) const;

signals:
    void searchResultsReady(const QList<ModelCard> &results);
    void catalogLoaded(const QList<ModelCard> &models);
    void downloadStarted(const QString &downloadId);
    void downloadProgress(const QString &downloadId, double progress, qint64 downloadedBytes);
    void downloadCompleted(const QString &downloadId, const QString &savedPath);
    void downloadFailed(const QString &downloadId, const QString &error);
    void downloadPaused(const QString &downloadId);
    void downloadResumed(const QString &downloadId);
    void downloadCancelled(const QString &downloadId);
    void localModelsScanned(const QList<LocalModelInfo> &models);
    void storageChanged(qint64 used, qint64 available);
    void errorOccurred(const QString &message);

private slots:
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError code);

private:
    struct ActiveDownload {
        QString id;
        QNetworkReply *reply;
        QFile *file;
        ModelDownloadItem item;
        QUrl url;
    };
    
    void initStoragePath();
    QString generateDownloadId() const;
    void saveDownloadState();
    void loadDownloadState();
    void extractModelMetadata(const QString &filePath);
    QList<ModelCard> fetchFromHuggingFace(const QString &query, const QStringList &filters);
    QList<ModelCard> fetchFromLocalCache() const;
    
    QString m_storagePath;
    QMap<QString, ActiveDownload> m_activeDownloads;
    QMap<QString, ModelDownloadItem> m_downloadHistory;
    QList<LocalModelInfo> m_localModels;
    QFileSystemWatcher *m_storageWatcher;
    QNetworkAccessManager *m_networkManager;
    
    // Cache for catalog data
    mutable QMap<QString, QList<ModelCard>> m_catalogCache;
    mutable QDateTime m_cacheTimestamp;
    static const int CACHE_VALIDITY_SECONDS = 3600;  // 1 hour
};

// Model card structure for catalog display
struct ModelCard {
    QString id;
    QString name;
    QString publisher;
    QString description;
    QString modelType;  // llama, mistral, etc.
    int parameterCount;  // in billions
    QString quantization;
    qint64 fileSizeBytes;
    int contextLength;
    int maxTokens;
    QStringList capabilities;  // text, vision, tools, etc.
    QStringList formats;  // GGUF, Safetensors, etc.
    QString license;
    int downloads;
    int likes;
    QDateTime lastUpdated;
    QString hfRepoId;  // Hugging Face repo ID
    QString coverImageUrl;
    double rating;
    int reviewCount;
    bool isFeatured;
    bool isLocal;
    
    QString getFormattedSize() const {
        return ModelDownloadItem::formatBytes(fileSizeBytes);
    }
    
    QString getFormattedParams() const {
        if (parameterCount >= 1000) {
            return QString("%1T").arg(parameterCount / 1000.0, 0, 'f', 1);
        }
        return QString("%1B").arg(parameterCount, 0, 'f', 1);
    }
};

} // namespace Brahma
