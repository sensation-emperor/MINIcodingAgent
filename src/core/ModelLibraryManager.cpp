#include "core/ModelLibraryManager.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QUuid>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QRegularExpression>

namespace Brahma {

ModelLibraryManager::ModelLibraryManager(QObject *parent)
    : QObject(parent)
    , m_storageWatcher(new QFileSystemWatcher(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    initStoragePath();
    loadDownloadState();
    
    connect(m_storageWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        emit storageChanged(getTotalStorageUsed(), getAvailableStorage());
    });
}

ModelLibraryManager::~ModelLibraryManager()
{
    // Cancel all active downloads
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply) {
            it->reply->abort();
            it->reply->deleteLater();
        }
        if (it->file) {
            it->file->close();
            it->file->deleteLater();
        }
    }
}

void ModelLibraryManager::initStoragePath()
{
    // Default to user's home directory under .brahma/models
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) 
                         + "/.brahma/models";
    
    QDir dir;
    if (!dir.exists(defaultPath)) {
        dir.mkpath(defaultPath);
    }
    
    m_storagePath = defaultPath;
    m_storageWatcher->addPath(m_storagePath);
}

void ModelLibraryManager::searchModels(const QString &query, const QStringList &filters)
{
    // Check cache first
    QString cacheKey = query + "_" + filters.join("_");
    if (m_catalogCache.contains(cacheKey)) {
        auto cacheTime = m_cacheTimestamp.secsTo(QDateTime::currentDateTime());
        if (cacheTime < CACHE_VALIDITY_SECONDS) {
            emit searchResultsReady(m_catalogCache[cacheKey]);
            return;
        }
    }
    
    // Fetch from Hugging Face API
    QList<ModelCard> results = fetchFromHuggingFace(query, filters);
    
    // Cache results
    m_catalogCache[cacheKey] = results;
    m_cacheTimestamp = QDateTime::currentDateTime();
    
    emit searchResultsReady(results);
}

void ModelLibraryManager::browseCatalog(const QString &category, int page, int pageSize)
{
    // For now, return featured models
    // In production, this would call Hugging Face API with proper pagination
    QList<ModelCard> models = getFeaturedModels();
    
    // Apply pagination
    int start = (page - 1) * pageSize;
    int end = qMin(start + pageSize, models.size());
    
    if (start < models.size()) {
        emit catalogLoaded(models.mid(start, end - start));
    } else {
        emit catalogLoaded(QList<ModelCard>());
    }
}

QList<ModelCard> ModelLibraryManager::getFeaturedModels() const
{
    // Return a curated list of popular models
    // This would normally come from an API or curated config
    QList<ModelCard> featured;
    
    // Llama family
    ModelCard llama3_8b;
    llama3_8b.id = "meta-llama/Meta-Llama-3-8B-GGUF";
    llama3_8b.name = "Llama 3 8B Instruct";
    llama3_8b.publisher = "Meta AI";
    llama3_8b.description = "Meta's latest open-source language model with 8B parameters, optimized for chat and instruction following.";
    llama3_8b.modelType = "llama";
    llama3_8b.parameterCount = 8;
    llama3_8b.fileSizeBytes = 4.7 * 1024 * 1024 * 1024LL;  // ~4.7GB for Q4_K_M
    llama3_8b.contextLength = 8192;
    llama3_8b.maxTokens = 2048;
    llama3_8b.capabilities = {"text", "chat", "code"};
    llama3_8b.formats = {"GGUF", "Safetensors"};
    llama3_8b.quantization = "Q4_K_M";
    llama3_8b.license = "Llama 3 Community License";
    llama3_8b.downloads = 1500000;
    llama3_8b.likes = 25000;
    llama3_8b.lastUpdated = QDateTime::currentDateTime().addDays(-5);
    llama3_8b.hfRepoId = "meta-llama/Meta-Llama-3-8B-Instruct-GGUF";
    llama3_8b.isFeatured = true;
    featured.append(llama3_8b);
    
    ModelCard llama3_70b;
    llama3_70b.id = "meta-llama/Meta-Llama-3-70B-GGUF";
    llama3_70b.name = "Llama 3 70B Instruct";
    llama3_70b.publisher = "Meta AI";
    llama3_70b.description = "Large-scale 70B parameter model with exceptional reasoning and coding capabilities.";
    llama3_70b.modelType = "llama";
    llama3_70b.parameterCount = 70;
    llama3_70b.fileSizeBytes = 40.0 * 1024 * 1024 * 1024LL;  // ~40GB for Q4_K_M
    llama3_70b.contextLength = 8192;
    llama3_70b.maxTokens = 2048;
    llama3_70b.capabilities = {"text", "chat", "code", "reasoning"};
    llama3_70b.formats = {"GGUF", "Safetensors"};
    llama3_70b.quantization = "Q4_K_M";
    llama3_70b.license = "Llama 3 Community License";
    llama3_70b.downloads = 800000;
    llama3_70b.likes = 18000;
    llama3_70b.lastUpdated = QDateTime::currentDateTime().addDays(-5);
    llama3_70b.hfRepoId = "meta-llama/Meta-Llama-3-70B-Instruct-GGUF";
    llama3_70b.isFeatured = true;
    featured.append(llama3_70b);
    
    // Mistral family
    ModelCard mistral_7b;
    mistral_7b.id = "mistralai/Mistral-7B-Instruct-v0.3-GGUF";
    mistral_7b.name = "Mistral 7B Instruct v0.3";
    mistral_7b.publisher = "Mistral AI";
    mistral_7b.description = "Highly efficient 7B model with excellent performance across diverse tasks.";
    mistral_7b.modelType = "mistral";
    mistral_7b.parameterCount = 7;
    mistral_7b.fileSizeBytes = 4.1 * 1024 * 1024 * 1024LL;
    mistral_7b.contextLength = 32768;
    mistral_7b.maxTokens = 4096;
    mistral_7b.capabilities = {"text", "chat", "code"};
    mistral_7b.formats = {"GGUF", "Safetensors"};
    mistral_7b.quantization = "Q4_K_M";
    mistral_7b.license = "Apache 2.0";
    mistral_7b.downloads = 2000000;
    mistral_7b.likes = 30000;
    mistral_7b.lastUpdated = QDateTime::currentDateTime().addDays(-10);
    mistral_7b.hfRepoId = "mistralai/Mistral-7B-Instruct-v0.3";
    mistral_7b.isFeatured = true;
    featured.append(mistral_7b);
    
    // Phi-3
    ModelCard phi3_mini;
    phi3_mini.id = "microsoft/Phi-3-mini-4k-instruct-GGUF";
    phi3_mini.name = "Phi-3 Mini 4K Instruct";
    phi3_mini.publisher = "Microsoft";
    phi3_mini.description = "Compact 3.8B model with surprising capabilities, perfect for resource-constrained environments.";
    phi3_mini.modelType = "phi";
    phi3_mini.parameterCount = 3.8;
    phi3_mini.fileSizeBytes = 2.3 * 1024 * 1024 * 1024LL;
    phi3_mini.contextLength = 4096;
    phi3_mini.maxTokens = 1024;
    phi3_mini.capabilities = {"text", "chat", "reasoning"};
    phi3_mini.formats = {"GGUF", "ONNX"};
    phi3_mini.quantization = "Q4_K_M";
    phi3_mini.license = "MIT";
    phi3_mini.downloads = 900000;
    phi3_mini.likes = 15000;
    phi3_mini.lastUpdated = QDateTime::currentDateTime().addDays(-15);
    phi3_mini.hfRepoId = "microsoft/Phi-3-mini-4k-instruct";
    phi3_mini.isFeatured = true;
    featured.append(phi3_mini);
    
    // Gemma
    ModelCard gemma_7b;
    gemma_7b.id = "google/gemma-7b-it-GGUF";
    gemma_7b.name = "Gemma 7B Instruct";
    gemma_7b.publisher = "Google";
    gemma_7b.description = "Google's lightweight open model built from Gemini research.";
    gemma_7b.modelType = "gemma";
    gemma_7b.parameterCount = 7;
    gemma_7b.fileSizeBytes = 4.5 * 1024 * 1024 * 1024LL;
    gemma_7b.contextLength = 8192;
    gemma_7b.maxTokens = 2048;
    gemma_7b.capabilities = {"text", "chat", "code"};
    gemma_7b.formats = {"GGUF", "Safetensors"};
    gemma_7b.quantization = "Q4_K_M";
    gemma_7b.license = "Gemma Terms of Use";
    gemma_7b.downloads = 700000;
    gemma_7b.likes = 12000;
    gemma_7b.lastUpdated = QDateTime::currentDateTime().addDays(-20);
    gemma_7b.hfRepoId = "google/gemma-7b-it";
    gemma_7b.isFeatured = true;
    featured.append(gemma_7b);
    
    // Mixtral
    ModelCard mixtral_8x7b;
    mixtral_8x7b.id = "mistralai/Mixtral-8x7B-Instruct-v0.1-GGUF";
    mixtral_8x7b.name = "Mixtral 8x7B Instruct";
    mixtral_8x7b.publisher = "Mistral AI";
    mixtral_8x7b.description = "Sparse Mixture of Experts model with 47B total parameters, 12B active per token.";
    mixtral_8x7b.modelType = "mixtral";
    mixtral_8x7b.parameterCount = 47;
    mixtral_8x7b.fileSizeBytes = 26.0 * 1024 * 1024 * 1024LL;
    mixtral_8x7b.contextLength = 32768;
    mixtral_8x7b.maxTokens = 4096;
    mixtral_8x7b.capabilities = {"text", "chat", "code", "reasoning"};
    mixtral_8x7b.formats = {"GGUF", "Safetensors"};
    mixtral_8x7b.quantization = "Q4_K_M";
    mixtral_8x7b.license = "Apache 2.0";
    mixtral_8x7b.downloads = 1200000;
    mixtral_8x7b.likes = 22000;
    mixtral_8x7b.lastUpdated = QDateTime::currentDateTime().addDays(-30);
    mixtral_8x7b.hfRepoId = "mistralai/Mixtral-8x7B-Instruct-v0.1";
    mixtral_8x7b.isFeatured = true;
    featured.append(mixtral_8x7b);
    
    return featured;
}

void ModelLibraryManager::downloadModel(const QString &modelId, const QString &format,
                                        const QString &quantization)
{
    // Generate download ID
    QString downloadId = generateDownloadId();
    
    // Build download URL (Hugging Face example)
    // Format: https://huggingface.co/{repo}/resolve/main/{filename}
    QString repoId = modelId.split("/").value(0);
    QString filename = QString("%1_%2.gguf").arg(modelId.split("/").value(1), quantization);
    QString urlStr = QString("https://huggingface.co/%1/resolve/main/%2")
                        .arg(modelId, filename);
    
    QUrl url(urlStr);
    
    // Create destination file
    QString destPath = QString("%1/%2").arg(m_storagePath, filename);
    QFile *file = new QFile(destPath);
    
    if (!file->open(QIODevice::WriteOnly)) {
        emit downloadFailed(downloadId, "Cannot create file: " + file->errorString());
        file->deleteLater();
        return;
    }
    
    // Start download
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);
    
    // Setup active download
    ActiveDownload download;
    download.id = downloadId;
    download.reply = reply;
    download.file = file;
    download.url = url;
    download.item.id = downloadId;
    download.item.modelId = modelId;
    download.item.modelName = modelId.split("/").value(1);
    download.item.publisher = repoId;
    download.item.format = format;
    download.item.quantization = quantization;
    download.item.status = DownloadStatus::Downloading;
    download.item.startTime = QDateTime::currentDateTime();
    download.item.paused = false;
    
    m_activeDownloads[downloadId] = download;
    m_downloadHistory[downloadId] = download.item;
    
    emit downloadStarted(downloadId);
    
    // Connect signals
    connect(reply, &QNetworkReply::downloadProgress, this, [this, downloadId](qint64 received, qint64 total) {
        onDownloadProgress(received, total);
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, downloadId]() {
        onDownloadFinished();
    });
    
    connect(reply, &QNetworkReply::errorOccurred, this, [this, downloadId](QNetworkReply::NetworkError code) {
        onDownloadError(code);
    });
}

void ModelLibraryManager::pauseDownload(const QString &downloadId)
{
    if (!m_activeDownloads.contains(downloadId)) {
        return;
    }
    
    ActiveDownload &download = m_activeDownloads[downloadId];
    
    if (download.reply && download.item.status == DownloadStatus::Downloading) {
        download.reply->abort();
        download.item.status = DownloadStatus::Paused;
        download.item.paused = true;
        
        emit downloadPaused(downloadId);
    }
}

void ModelLibraryManager::resumeDownload(const QString &downloadId)
{
    if (!m_activeDownloads.contains(downloadId)) {
        // Check if it's in history (completed/paused)
        if (m_downloadHistory.contains(downloadId)) {
            ModelDownloadItem item = m_downloadHistory[downloadId];
            // Restart download
            downloadModel(item.modelId, item.format, item.quantization);
        }
        return;
    }
    
    ActiveDownload &download = m_activeDownloads[downloadId];
    
    if (download.item.status == DownloadStatus::Paused) {
        // Resume by restarting the download
        // Note: Proper resume would require Range header support
        downloadModel(download.item.modelId, download.item.format, download.item.quantization);
    }
}

void ModelLibraryManager::cancelDownload(const QString &downloadId)
{
    if (!m_activeDownloads.contains(downloadId)) {
        return;
    }
    
    ActiveDownload download = m_activeDownloads.take(downloadId);
    
    if (download.reply) {
        download.reply->abort();
        download.reply->deleteLater();
    }
    
    if (download.file) {
        download.file->close();
        download.file->remove();  // Delete partial file
        download.file->deleteLater();
    }
    
    download.item.status = DownloadStatus::Cancelled;
    download.item.completedTime = QDateTime::currentDateTime();
    m_downloadHistory[downloadId] = download.item;
    
    emit downloadCancelled(downloadId);
}

void ModelLibraryManager::retryDownload(const QString &downloadId)
{
    if (m_downloadHistory.contains(downloadId)) {
        ModelDownloadItem item = m_downloadHistory[downloadId];
        downloadModel(item.modelId, item.format, item.quantization);
    }
}

QList<ModelDownloadItem> ModelLibraryManager::getActiveDownloads() const
{
    QList<ModelDownloadItem> active;
    for (const auto &download : m_activeDownloads.values()) {
        active.append(download.item);
    }
    return active;
}

QList<ModelDownloadItem> ModelLibraryManager::getCompletedDownloads() const
{
    QList<ModelDownloadItem> completed;
    for (const auto &item : m_downloadHistory.values()) {
        if (item.status == DownloadStatus::Completed) {
            completed.append(item);
        }
    }
    return completed;
}

QList<ModelDownloadItem> ModelLibraryManager::getAllDownloads() const
{
    QList<ModelDownloadItem> all = m_downloadHistory.values();
    
    // Add active downloads
    for (const auto &download : m_activeDownloads.values()) {
        bool found = false;
        for (auto &item : all) {
            if (item.id == download.item.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            all.append(download.item);
        }
    }
    
    return all;
}

ModelDownloadItem ModelLibraryManager::getDownloadStatus(const QString &downloadId) const
{
    if (m_activeDownloads.contains(downloadId)) {
        return m_activeDownloads[downloadId].item;
    }
    if (m_downloadHistory.contains(downloadId)) {
        return m_downloadHistory[downloadId];
    }
    return ModelDownloadItem();
}

QList<LocalModelInfo> ModelLibraryManager::scanLocalModels(const QString &directory) const
{
    QString scanDir = directory.isEmpty() ? m_storagePath : directory;
    QList<LocalModelInfo> models;
    
    QDir dir(scanDir);
    if (!dir.exists()) {
        return models;
    }
    
    // Search for model files (GGUF, Safetensors, etc.)
    QStringList filters = {"*.gguf", "*.bin", "*.safetensors", "*.pt", "*.pth"};
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files);
    
    QFileInfoList files = dir.entryInfoList();
    
    for (const QFileInfo &file : files) {
        LocalModelInfo info = validateModelFile(file.absoluteFilePath());
        if (info.isValid) {
            models.append(info);
        }
    }
    
    return models;
}

bool ModelLibraryManager::importLocalModel(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists()) {
        emit errorOccurred("File does not exist: " + filePath);
        return false;
    }
    
    // Validate the model file
    LocalModelInfo info = validateModelFile(filePath);
    
    if (!info.isValid) {
        emit errorOccurred("Invalid model file: " + info.errorMessage);
        return false;
    }
    
    // Copy to storage if not already there
    if (!filePath.startsWith(m_storagePath)) {
        QString destPath = QString("%1/%2").arg(m_storagePath, fileInfo.fileName());
        
        if (!QFile::copy(filePath, destPath)) {
            emit errorOccurred("Failed to copy file to storage: " + destPath);
            return false;
        }
    }
    
    m_localModels.append(info);
    emit localModelsScanned(m_localModels);
    
    return true;
}

bool ModelLibraryManager::removeLocalModel(const QString &modelId)
{
    // Find the model in local models
    for (int i = 0; i < m_localModels.size(); ++i) {
        if (m_localModels[i].name == modelId || m_localModels[i].path.contains(modelId)) {
            QString path = m_localModels[i].path;
            
            // Delete the file
            if (QFile::remove(path)) {
                m_localModels.removeAt(i);
                emit localModelsScanned(m_localModels);
                return true;
            } else {
                emit errorOccurred("Failed to delete model file: " + path);
                return false;
            }
        }
    }
    
    emit errorOccurred("Model not found: " + modelId);
    return false;
}

LocalModelInfo ModelLibraryManager::validateModelFile(const QString &filePath) const
{
    LocalModelInfo info;
    info.path = filePath;
    info.isValid = false;
    
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists()) {
        info.errorMessage = "File does not exist";
        return info;
    }
    
    info.name = fileInfo.fileName();
    info.fileSizeBytes = fileInfo.size();
    info.modifiedDate = fileInfo.lastModified();
    
    // Determine format from extension
    QString ext = fileInfo.suffix().toLower();
    if (ext == "gguf") {
        info.format = "GGUF";
    } else if (ext == "safetensors") {
        info.format = "Safetensors";
    } else if (ext == "bin") {
        info.format = "PyTorch";
    } else if (ext == "pt" || ext == "pth") {
        info.format = "PyTorch";
    } else {
        info.errorMessage = "Unsupported format: " + ext;
        return info;
    }
    
    // Extract metadata from filename (heuristic)
    QString fileName = fileInfo.fileName().toLower();
    
    // Try to extract quantization
    QRegularExpression quantRegex("([qf][0-9]+_[a-z0-9]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch quantMatch = quantRegex.match(fileName);
    if (quantMatch.hasMatch()) {
        info.quantization = quantMatch.captured(1).toUpper();
    }
    
    // Try to extract parameter count
    QRegularExpression paramRegex("(\\d+)(b|m)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch paramMatch = paramRegex.match(fileName);
    if (paramMatch.hasMatch()) {
        int value = paramMatch.captured(1).toInt();
        QString unit = paramMatch.captured(2).toLower();
        if (unit == "b") {
            info.parameterCount = value;  // Billions
        } else if (unit == "m") {
            info.parameterCount = value / 1000;  // Millions to billions
        }
    }
    
    // Basic validation - file should be reasonably sized for a model
    if (info.fileSizeBytes < 100 * 1024 * 1024) {  // Less than 100MB
        info.errorMessage = "File too small to be a valid model";
        return info;
    }
    
    info.isValid = true;
    return info;
}

QString ModelLibraryManager::getModelStoragePath() const
{
    return m_storagePath;
}

qint64 ModelLibraryManager::getTotalStorageUsed() const
{
    qint64 total = 0;
    
    QDir dir(m_storagePath);
    if (!dir.exists()) {
        return 0;
    }
    
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::NoSort);
    for (const QFileInfo &file : files) {
        total += file.size();
    }
    
    return total;
}

qint64 ModelLibraryManager::getAvailableStorage() const
{
    QStorageInfo storage(m_storagePath);
    return storage.bytesAvailable();
}

void ModelLibraryManager::setStoragePath(const QString &path)
{
    if (m_storageWatcher) {
        m_storageWatcher->removePath(m_storagePath);
    }
    
    m_storagePath = path;
    
    QDir dir;
    if (!dir.exists(m_storagePath)) {
        dir.mkpath(m_storagePath);
    }
    
    m_storageWatcher->addPath(m_storagePath);
    emit storageChanged(getTotalStorageUsed(), getAvailableStorage());
}

bool ModelLibraryManager::cleanupIncompleteDownloads()
{
    bool cleaned = false;
    
    // Remove partial download files (.part or incomplete files)
    QDir dir(m_storagePath);
    if (!dir.exists()) {
        return false;
    }
    
    // Look for common partial file extensions
    QStringList partialFilters = {"*.part", "*.download", "*.tmp"};
    dir.setNameFilters(partialFilters);
    
    QFileInfoList partialFiles = dir.entryInfoList(QDir::Files);
    for (const QFileInfo &file : partialFiles) {
        if (QFile::remove(file.absoluteFilePath())) {
            cleaned = true;
        }
    }
    
    // Also check download history for failed/cancelled downloads
    for (auto it = m_downloadHistory.begin(); it != m_downloadHistory.end(); ) {
        if (it->status == DownloadStatus::Failed || 
            it->status == DownloadStatus::Cancelled) {
            // Remove old entries (older than 7 days)
            if (it->completedTime.daysTo(QDateTime::currentDateTime()) > 7) {
                it = m_downloadHistory.erase(it);
                cleaned = true;
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
    
    saveDownloadState();
    return cleaned;
}

ModelCard ModelLibraryManager::getModelDetails(const QString &modelId) const
{
    // First check featured models
    QList<ModelCard> featured = getFeaturedModels();
    for (const ModelCard &card : featured) {
        if (card.id == modelId || card.hfRepoId == modelId) {
            return card;
        }
    }
    
    // Check local models
    for (const LocalModelInfo &local : m_localModels) {
        if (local.name == modelId) {
            ModelCard card;
            card.id = local.path;
            card.name = local.name;
            card.isLocal = true;
            card.fileSizeBytes = local.fileSizeBytes;
            card.parameterCount = local.parameterCount;
            card.quantization = local.quantization;
            card.formats = {local.format};
            return card;
        }
    }
    
    // Would normally fetch from API here
    return ModelCard();
}

QStringList ModelLibraryManager::getAvailableFormats(const QString &modelId) const
{
    ModelCard card = getModelDetails(modelId);
    return card.formats;
}

QStringList ModelLibraryManager::getAvailableQuantizations(const QString &modelId, const QString &format) const
{
    Q_UNUSED(format);
    
    // Common GGUF quantizations
    if (format == "GGUF" || format.isEmpty()) {
        return {"Q2_K", "Q3_K_S", "Q3_K_M", "Q3_K_L", 
                "Q4_0", "Q4_K_S", "Q4_K_M", 
                "Q5_0", "Q5_K_S", "Q5_K_M", 
                "Q6_K", "Q8_0", "FP16", "FP32"};
    }
    
    return {};
}

QString ModelLibraryManager::generateDownloadId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ModelLibraryManager::saveDownloadState()
{
    // Save download history to JSON file
    QString stateFile = QString("%1/download_state.json").arg(m_storagePath);
    
    QJsonArray array;
    for (const auto &item : m_downloadHistory.values()) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["modelId"] = item.modelId;
        obj["modelName"] = item.modelName;
        obj["publisher"] = item.publisher;
        obj["format"] = item.format;
        obj["quantization"] = item.quantization;
        obj["fileSizeBytes"] = item.fileSizeBytes;
        obj["downloadedBytes"] = item.downloadedBytes;
        obj["status"] = static_cast<int>(item.status);
        obj["startTime"] = item.startTime.toString(Qt::ISODate);
        obj["completedTime"] = item.completedTime.toString(Qt::ISODate);
        array.append(obj);
    }
    
    QJsonDocument doc(array);
    
    QFile file(stateFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void ModelLibraryManager::loadDownloadState()
{
    QString stateFile = QString("%1/download_state.json").arg(m_storagePath);
    
    QFile file(stateFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) {
        return;
    }
    
    QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        QJsonObject obj = value.toObject();
        
        ModelDownloadItem item;
        item.id = obj["id"].toString();
        item.modelId = obj["modelId"].toString();
        item.modelName = obj["modelName"].toString();
        item.publisher = obj["publisher"].toString();
        item.format = obj["format"].toString();
        item.quantization = obj["quantization"].toString();
        item.fileSizeBytes = obj["fileSizeBytes"].toVariant().toLongLong();
        item.downloadedBytes = obj["downloadedBytes"].toVariant().toLongLong();
        item.status = static_cast<DownloadStatus>(obj["status"].toInt());
        item.startTime = QDateTime::fromString(obj["startTime"].toString(), Qt::ISODate);
        item.completedTime = QDateTime::fromString(obj["completedTime"].toString(), Qt::ISODate);
        
        m_downloadHistory[item.id] = item;
    }
}

void ModelLibraryManager::onDownloadProgress(qint64 received, qint64 total)
{
    // Find which download this progress is for
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply && it->reply->property("received").toLongLong() == received) {
            continue;
        }
        
        // Update the most recent active download
        it->item.downloadedBytes = received;
        it->item.fileSizeBytes = total;
        
        // Calculate speed and ETA
        QDateTime now = QDateTime::currentDateTime();
        QDateTime start = it->item.startTime;
        int elapsedSeconds = start.secsTo(now);
        
        if (elapsedSeconds > 0) {
            it->item.downloadSpeed = static_cast<double>(received) / elapsedSeconds;
        }
        
        if (it->item.downloadSpeed > 0 && total > 0) {
            qint64 remaining = total - received;
            it->item.etaSeconds = static_cast<int>(remaining / it->item.downloadSpeed);
        }
        
        double progress = (total > 0) ? (static_cast<double>(received) / total * 100.0) : 0.0;
        
        emit downloadProgress(it->id, progress, received);
        
        // Store received bytes for next comparison
        it->reply->setProperty("received", received);
        break;
    }
}

void ModelLibraryManager::onDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    // Find the download
    ActiveDownload *download = nullptr;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply == reply) {
            download = &(*it);
            break;
        }
    }
    
    if (!download) {
        reply->deleteLater();
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        onDownloadError(reply->error());
        return;
    }
    
    // Close the file
    download->file->close();
    
    // Update status
    download->item.status = DownloadStatus::Completed;
    download->item.completedTime = QDateTime::currentDateTime();
    download->item.downloadedBytes = download->item.fileSizeBytes;
    
    QString savedPath = download->file->fileName();
    
    // Move from active to history
    m_downloadHistory[download->id] = download->item;
    m_activeDownloads.remove(download->id);
    
    // Cleanup
    download->file->deleteLater();
    reply->deleteLater();
    
    emit downloadCompleted(download->id, savedPath);
    
    saveDownloadState();
}

void ModelLibraryManager::onDownloadError(QNetworkReply::NetworkError code)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    // Find the download
    ActiveDownload *download = nullptr;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply == reply) {
            download = &(*it);
            break;
        }
    }
    
    if (!download) {
        reply->deleteLater();
        return;
    }
    
    download->item.status = DownloadStatus::Failed;
    download->item.errorMessage = reply->errorString();
    
    emit downloadFailed(download->id, reply->errorString());
    
    // Keep in active for potential retry, but mark as failed
    download->file->close();
    download->file->deleteLater();
    download->file = nullptr;
    download->reply->deleteLater();
    download->reply = nullptr;
    
    saveDownloadState();
}

QList<ModelCard> ModelLibraryManager::fetchFromHuggingFace(const QString &query, const QStringList &filters)
{
    // This would make actual API calls to Hugging Face
    // For now, return filtered featured models
    
    QList<ModelCard> allModels = getFeaturedModels();
    QList<ModelCard> results;
    
    for (const ModelCard &model : allModels) {
        bool matches = true;
        
        // Filter by query
        if (!query.isEmpty()) {
            QString q = query.toLower();
            if (!model.name.toLower().contains(q) && 
                !model.description.toLower().contains(q) &&
                !model.publisher.toLower().contains(q)) {
                matches = false;
            }
        }
        
        // Filter by capabilities
        for (const QString &filter : filters) {
            if (!model.capabilities.contains(filter.toLower())) {
                matches = false;
                break;
            }
        }
        
        if (matches) {
            results.append(model);
        }
    }
    
    return results;
}

QList<ModelCard> ModelLibraryManager::fetchFromLocalCache() const
{
    // Return models from local cache
    QList<ModelCard> cached;
    
    for (const LocalModelInfo &local : m_localModels) {
        ModelCard card;
        card.id = local.path;
        card.name = local.name;
        card.isLocal = true;
        card.fileSizeBytes = local.fileSizeBytes;
        card.parameterCount = local.parameterCount;
        card.quantization = local.quantization;
        card.formats = {local.format};
        cached.append(card);
    }
    
    return cached;
}

} // namespace Brahma
