#include "RAGService.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QUuid>
#include <QtConcurrent>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace Brahma {

RAGService::RAGService(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_chunkSize(512)
    , m_chunkOverlap(50)
    , m_maxContextLength(4096)
{
    // Setup default embedding models
    EmbeddingModel localModel;
    localModel.id = "local-all-MiniLM";
    localModel.name = "all-MiniLM-L6-v2";
    localModel.provider = "local";
    localModel.dimensions = 384;
    localModel.maxInputLength = 512;
    localModel.isLoaded = false;
    m_availableModels[localModel.id] = localModel;

    EmbeddingModel openaiModel;
    openaiModel.id = "openai-text-embedding";
    openaiModel.name = "text-embedding-ada-002";
    openaiModel.provider = "openai";
    openaiModel.dimensions = 1536;
    openaiModel.maxInputLength = 8191;
    openaiModel.isLoaded = false;
    m_availableModels[openaiModel.id] = openaiModel;

    connect(&m_indexingWatcher, &QFutureWatcher<QStringList>::finished,
            this, &RAGService::onIndexingFutureFinished);
    connect(&m_embeddingWatcher, &QFutureWatcher<QVector<QVector<double>>>::finished,
            this, &RAGService::onEmbeddingFutureFinished);
}

RAGService::~RAGService()
{
    shutdown();
}

bool RAGService::initialize(const QString &embeddingModelPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }

    // Set index path
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    m_indexPath = dir.filePath("rag_index.json");

    // Try to load existing index
    if (QFile::exists(m_indexPath)) {
        loadIndex(m_indexPath);
    }

    // Load embedding model if path provided
    if (!embeddingModelPath.isEmpty()) {
        loadEmbeddingModel(embeddingModelPath);
    }

    m_initialized = true;
    emit initializationComplete(true);
    
    return true;
}

void RAGService::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) return;

    // Save index before shutdown
    saveIndex(m_indexPath);

    // Unload embedding model
    unloadEmbeddingModel();

    // Clear all data
    m_documents.clear();
    m_chunks.clear();
    m_embeddings.clear();

    m_initialized = false;
}

QStringList RAGService::supportedFileTypes()
{
    return QStringList() 
        << "txt" << "md" << "markdown" 
        << "pdf" << "docx" 
        << "py" << "cpp" << "c" << "h" << "hpp" << "java" << "js" << "ts"
        << "rs" << "go" << "rb" << "php" << "cs" << "swift" << "kt"
        << "json" << "xml" << "yaml" << "yml" << "html" << "css";
}

bool RAGService::isSupportedFileType(const QString &extension)
{
    return supportedFileTypes().contains(extension.toLower());
}

QString RAGService::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString RAGService::getFileType(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower();
}

QMap<QString, QString> RAGService::extractMetadata(const QString &filePath) const
{
    QMap<QString, QString> metadata;
    QFileInfo fileInfo(filePath);
    
    metadata["filename"] = fileInfo.fileName();
    metadata["directory"] = fileInfo.dir().path();
    metadata["size"] = QString::number(fileInfo.size());
    metadata["created"] = fileInfo.created().toString(Qt::ISODate);
    metadata["modified"] = fileInfo.lastModified().toString(Qt::ISODate);
    
    return metadata;
}

QString RAGService::addDocument(const QString &filePath, const QString &title, 
                                const QMap<QString, QString> &metadata)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        emit errorOccurred("RAG service not initialized");
        return QString();
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit errorOccurred(QString("File not found: %1").arg(filePath));
        return QString();
    }

    QString fileType = getFileType(filePath);
    if (!isSupportedFileType(fileType)) {
        emit errorOccurred(QString("Unsupported file type: %1").arg(fileType));
        return QString();
    }

    Document doc;
    doc.id = generateId();
    doc.title = title.isEmpty() ? fileInfo.fileName() : title;
    doc.filePath = fileInfo.absoluteFilePath();
    doc.fileType = fileType;
    doc.fileSize = fileInfo.size();
    doc.metadata = extractMetadata(filePath);
    doc.metadata.insert(metadata); // Merge custom metadata
    doc.indexedAt = QDateTime::currentDateTime();
    doc.isActive = true;
    doc.totalChunks = 0;

    m_documents[doc.id] = doc;
    
    emit documentAdded(doc.id, filePath);
    
    // Start async indexing
    QStringList files = QStringList() << filePath;
    m_indexingWatcher.setFuture(QtConcurrent::run([this, files, doc]() -> QStringList {
        // This would be implemented with actual text extraction and chunking
        return files;
    }));

    return doc.id;
}

QStringList RAGService::addDocuments(const QStringList &filePaths, 
                                     const QMap<QString, QString> &baseMetadata)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        emit errorOccurred("RAG service not initialized");
        return QStringList();
    }

    QStringList documentIds;
    
    for (const QString &filePath : filePaths) {
        QString docId = addDocument(filePath, "", baseMetadata);
        if (!docId.isEmpty()) {
            documentIds.append(docId);
        }
    }
    
    return documentIds;
}

bool RAGService::removeDocument(const QString &documentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_documents.contains(documentId)) {
        return false;
    }

    // Remove all chunks associated with this document
    QList<QString> chunkIdsToRemove;
    for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it) {
        if (it.value().documentId == documentId) {
            chunkIdsToRemove.append(it.key());
        }
    }

    for (const QString &chunkId : chunkIdsToRemove) {
        m_chunks.remove(chunkId);
        m_embeddings.remove(chunkId);
    }

    m_documents.remove(documentId);
    
    emit documentRemoved(documentId);
    
    return true;
}

bool RAGService::removeDocuments(const QStringList &documentIds)
{
    bool allSuccess = true;
    for (const QString &id : documentIds) {
        if (!removeDocument(id)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool RAGService::updateDocument(const QString &documentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_documents.contains(documentId)) {
        return false;
    }

    Document &doc = m_documents[documentId];
    
    // Re-extract text and re-chunk
    // Implementation would re-process the document
    
    doc.indexedAt = QDateTime::currentDateTime();
    
    return true;
}

Document RAGService::getDocument(const QString &documentId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_documents.contains(documentId)) {
        return m_documents[documentId];
    }
    
    return Document();
}

QVector<Document> RAGService::listDocuments(bool activeOnly) const
{
    QMutexLocker locker(&m_mutex);
    
    QVector<Document> result;
    
    for (auto it = m_documents.begin(); it != m_documents.end(); ++it) {
        if (!activeOnly || it.value().isActive) {
            result.append(it.value());
        }
    }
    
    return result;
}

int RAGService::getDocumentCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_documents.count();
}

QVector<SearchResult> RAGService::search(const QString &query, int topK, 
                                         double minScore,
                                         const QStringList &documentIds) const
{
    QMutexLocker locker(&m_indexMutex);
    
    QVector<SearchResult> results;
    
    if (!m_initialized || query.trimmed().isEmpty()) {
        return results;
    }

    // Generate embedding for query
    QVector<double> queryEmbedding = generateEmbedding(query);
    if (queryEmbedding.isEmpty()) {
        emit errorOccurred("Failed to generate query embedding");
        return results;
    }

    // Calculate similarity scores
    QMap<QString, double> scores;
    
    for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it) {
        const DocumentChunk &chunk = it.value();
        
        // Filter by document IDs if specified
        if (!documentIds.isEmpty() && !documentIds.contains(chunk.documentId)) {
            continue;
        }

        if (!m_embeddings.contains(it.key())) {
            continue;
        }

        double score = cosineSimilarity(queryEmbedding, m_embeddings[it.key()]);
        
        if (score >= minScore) {
            scores[it.key()] = score;
        }
    }

    // Get top K results
    QVector<int> topIndices = topKIndices(scores.values(), qMin(topK, scores.size()));
    
    int rank = 1;
    for (int idx : topIndices) {
        auto it = scores.begin();
        std::advance(it, idx);
        
        SearchResult result;
        result.chunk = m_chunks[it.key()];
        result.similarityScore = it.value();
        result.relevanceRank = rank++;
        result.highlightedText = result.chunk.content; // Could add highlighting
        
        results.append(result);
    }

    emit searchCompleted(results);
    
    return results;
}

QVector<SearchResult> RAGService::semanticSearch(const QString &query, int topK,
                                                  const QMap<QString, QString> &filters) const
{
    // For now, delegate to search - could add more sophisticated filtering
    return search(query, topK, 0.0, QStringList());
}

QString RAGService::generateAnswer(const QString &query, const QVector<SearchResult> &context,
                                   const QString &modelId) const
{
    if (context.isEmpty()) {
        return "No relevant context found to answer your question.";
    }

    // Build context from search results
    QString contextText;
    for (int i = 0; i < qMin(5, context.size()); ++i) {
        contextText += QString("[Context %1]: %2\n\n").arg(i+1).arg(context[i].chunk.content);
    }

    // This would integrate with the ModelRouter to generate an answer
    // For now, return a simple synthesized response
    return QString("Based on the retrieved context:\n\n%1\n\nYour query was: %2")
        .arg(contextText).arg(query);
}

QVector<DocumentChunk> RAGService::getDocumentChunks(const QString &documentId, 
                                                     int startIdx, int count) const
{
    QMutexLocker locker(&m_mutex);
    
    QVector<DocumentChunk> result;
    
    if (!m_documents.contains(documentId)) {
        return result;
    }

    int idx = 0;
    for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it) {
        if (it.value().documentId == documentId) {
            if (idx >= startIdx) {
                result.append(it.value());
                if (count > 0 && result.size() >= count) {
                    break;
                }
            }
            idx++;
        }
    }
    
    return result;
}

bool RAGService::removeChunk(const QString &chunkId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_chunks.contains(chunkId)) {
        m_chunks.remove(chunkId);
        m_embeddings.remove(chunkId);
        return true;
    }
    
    return false;
}

DocumentChunk RAGService::getChunk(const QString &chunkId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_chunks.contains(chunkId)) {
        return m_chunks[chunkId];
    }
    
    return DocumentChunk();
}

bool RAGService::clearIndex()
{
    QMutexLocker locker(&m_mutex);
    
    m_documents.clear();
    m_chunks.clear();
    m_embeddings.clear();
    
    return true;
}

bool RAGService::saveIndex(const QString &filePath) const
{
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonObject root;
    
    // Serialize documents
    QJsonArray docsArray;
    for (auto it = m_documents.begin(); it != m_documents.end(); ++it) {
        QJsonObject docObj;
        docObj["id"] = it.value().id;
        docObj["title"] = it.value().title;
        docObj["filePath"] = it.value().filePath;
        docObj["fileType"] = it.value().fileType;
        docObj["fileSize"] = it.value().fileSize;
        docObj["totalChunks"] = it.value().totalChunks;
        docObj["indexedAt"] = it.value().indexedAt.toString(Qt::ISODate);
        docObj["isActive"] = it.value().isActive;
        
        QJsonObject metadata;
        for (auto metaIt = it.value().metadata.begin(); metaIt != it.value().metadata.end(); ++metaIt) {
            metadata[metaIt.key()] = metaIt.value();
        }
        docObj["metadata"] = metadata;
        
        docsArray.append(docObj);
    }
    root["documents"] = docsArray;

    // Serialize chunks
    QJsonArray chunksArray;
    for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it) {
        QJsonObject chunkObj;
        chunkObj["id"] = it.value().id;
        chunkObj["documentId"] = it.value().documentId;
        chunkObj["content"] = it.value().content;
        chunkObj["chunkIndex"] = it.value().chunkIndex;
        chunkObj["startIndex"] = it.value().startIndex;
        chunkObj["endIndex"] = it.value().endIndex;
        chunkObj["createdAt"] = it.value().createdAt.toString(Qt::ISODate);
        
        QJsonObject metadata;
        for (auto metaIt = it.value().metadata.begin(); metaIt != it.value().metadata.end(); ++metaIt) {
            metadata[metaIt.key()] = metaIt.value();
        }
        chunkObj["metadata"] = metadata;
        
        chunksArray.append(chunkObj);
    }
    root["chunks"] = chunksArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();
    
    return true;
}

bool RAGService::loadIndex(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonObject root = doc.object();

    // Deserialize documents
    m_documents.clear();
    QJsonArray docsArray = root["documents"].toArray();
    for (const QJsonValue &val : docsArray) {
        QJsonObject docObj = val.toObject();
        
        Document doc;
        doc.id = docObj["id"].toString();
        doc.title = docObj["title"].toString();
        doc.filePath = docObj["filePath"].toString();
        doc.fileType = docObj["fileType"].toString();
        doc.fileSize = docObj["fileSize"].toDouble();
        doc.totalChunks = docObj["totalChunks"].toInt();
        doc.indexedAt = QDateTime::fromString(docObj["indexedAt"].toString(), Qt::ISODate);
        doc.isActive = docObj["isActive"].toBool();
        
        QJsonObject metadata = docObj["metadata"].toObject();
        for (auto it = metadata.begin(); it != metadata.end(); ++it) {
            doc.metadata[it.key()] = it.value().toString();
        }
        
        m_documents[doc.id] = doc;
    }

    // Deserialize chunks
    m_chunks.clear();
    m_embeddings.clear();
    QJsonArray chunksArray = root["chunks"].toArray();
    for (const QJsonValue &val : chunksArray) {
        QJsonObject chunkObj = val.toObject();
        
        DocumentChunk chunk;
        chunk.id = chunkObj["id"].toString();
        chunk.documentId = chunkObj["documentId"].toString();
        chunk.content = chunkObj["content"].toString();
        chunk.chunkIndex = chunkObj["chunkIndex"].toInt();
        chunk.startIndex = chunkObj["startIndex"].toInt();
        chunk.endIndex = chunkObj["endIndex"].toInt();
        chunk.createdAt = QDateTime::fromString(chunkObj["createdAt"].toString(), Qt::ISODate);
        
        QJsonObject metadata = chunkObj["metadata"].toObject();
        for (auto it = metadata.begin(); it != metadata.end(); ++it) {
            chunk.metadata[it.key()] = it.value().toString();
        }
        
        m_chunks[chunk.id] = chunk;
    }

    return true;
}

QString RAGService::getIndexStats() const
{
    QMutexLocker locker(&m_mutex);
    
    return QString("Documents: %1, Chunks: %2, Embeddings: %3")
        .arg(m_documents.size())
        .arg(m_chunks.size())
        .arg(m_embeddings.size());
}

bool RAGService::loadEmbeddingModel(const QString &modelPath, const QString &modelName)
{
    QMutexLocker locker(&m_mutex);
    
    // In a real implementation, this would load the actual model
    // For now, we'll just mark it as loaded
    
    if (m_availableModels.contains(modelName)) {
        m_currentEmbeddingModel = m_availableModels[modelName];
        m_currentEmbeddingModel.modelPath = modelPath;
        m_currentEmbeddingModel.isLoaded = true;
        m_availableModels[modelName] = m_currentEmbeddingModel;
        
        emit modelLoaded(modelName);
        return true;
    }

    // Create new model entry
    EmbeddingModel model;
    model.id = generateId();
    model.name = modelName.isEmpty() ? QFileInfo(modelPath).fileName() : modelName;
    model.provider = "local";
    model.modelPath = modelPath;
    model.dimensions = 384; // Default
    model.maxInputLength = 512;
    model.isLoaded = true;
    
    m_currentEmbeddingModel = model;
    m_availableModels[model.id] = model;
    
    emit modelLoaded(model.name);
    
    return true;
}

bool RAGService::unloadEmbeddingModel()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_currentEmbeddingModel.isLoaded) {
        return true;
    }

    m_currentEmbeddingModel.isLoaded = false;
    
    if (m_availableModels.contains(m_currentEmbeddingModel.id)) {
        m_availableModels[m_currentEmbeddingModel.id] = m_currentEmbeddingModel;
    }

    emit modelUnloaded();
    
    return true;
}

EmbeddingModel RAGService::getCurrentEmbeddingModel() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentEmbeddingModel;
}

QVector<EmbeddingModel> RAGService::getAvailableEmbeddingModels() const
{
    QMutexLocker locker(&m_mutex);
    return m_availableModels.values();
}

QVector<double> RAGService::generateEmbedding(const QString &text) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_currentEmbeddingModel.isLoaded) {
        return QVector<double>();
    }

    // In a real implementation, this would use the actual embedding model
    // For demonstration, we'll generate a pseudo-random embedding
    // In production, this would call llama.cpp, ONNX Runtime, or an API
    
    QVector<double> embedding(m_currentEmbeddingModel.dimensions, 0.0);
    
    // Simple hash-based pseudo-embedding (NOT for production use)
    for (int i = 0; i < m_currentEmbeddingModel.dimensions; ++i) {
        QString hashInput = text + QString::number(i);
        QByteArray hash = QCryptographicHash::hash(hashInput.toUtf8(), QCryptographicHash::Sha256);
        double value = (static_cast<uchar>(hash[i % hash.size()]) / 255.0) * 2.0 - 1.0;
        embedding[i] = value;
    }

    // Normalize
    return normalizeVector(embedding);
}

QVector<QVector<double>> RAGService::generateEmbeddings(const QStringList &texts) const
{
    QVector<QVector<double>> embeddings;
    embeddings.reserve(texts.size());
    
    for (const QString &text : texts) {
        embeddings.append(generateEmbedding(text));
    }
    
    return embeddings;
}

double RAGService::cosineSimilarity(const QVector<double> &vec1, const QVector<double> &vec2) const
{
    if (vec1.isEmpty() || vec2.isEmpty() || vec1.size() != vec2.size()) {
        return 0.0;
    }

    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (int i = 0; i < vec1.size(); ++i) {
        dotProduct += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

QVector<int> RAGService::topKIndices(const QVector<double> &scores, int k) const
{
    if (scores.isEmpty() || k <= 0) {
        return QVector<int>();
    }

    // Create index-score pairs
    QVector<QPair<int, double>> pairs;
    pairs.reserve(scores.size());
    
    for (int i = 0; i < scores.size(); ++i) {
        pairs.append(qMakePair(i, scores[i]));
    }

    // Sort by score descending
    std::sort(pairs.begin(), pairs.end(), [](const QPair<int, double> &a, const QPair<int, double> &b) {
        return a.second > b.second;
    });

    // Take top K indices
    QVector<int> result;
    result.reserve(qMin(k, pairs.size()));
    
    for (int i = 0; i < qMin(k, pairs.size()); ++i) {
        result.append(pairs[i].first);
    }

    return result;
}

QVector<double> RAGService::normalizeVector(const QVector<double> &vec) const
{
    if (vec.isEmpty()) {
        return vec;
    }

    double norm = 0.0;
    for (double v : vec) {
        norm += v * v;
    }

    norm = std::sqrt(norm);
    
    if (norm == 0.0) {
        return vec;
    }

    QVector<double> normalized(vec.size());
    for (int i = 0; i < vec.size(); ++i) {
        normalized[i] = vec[i] / norm;
    }

    return normalized;
}

void RAGService::rebuildIndex()
{
    // Would rebuild the entire index from documents
}

void RAGService::optimizeIndex()
{
    // Would optimize index for faster searches
    m_lastOptimization = QDateTime::currentDateTime();
}

QStringList RAGService::splitIntoChunks(const QString &text, const QString &documentId) const
{
    QStringList chunks;
    
    if (text.isEmpty()) {
        return chunks;
    }

    // Simple chunking by characters - in production would use smarter tokenization
    int pos = 0;
    int chunkIdx = 0;
    
    while (pos < text.length()) {
        int endPos = qMin(pos + m_chunkSize, text.length());
        
        // Try to break at sentence boundary
        if (endPos < text.length()) {
            int lastPeriod = text.lastIndexOf('.', endPos);
            int lastNewline = text.lastIndexOf('\n', endPos);
            int breakPoint = qMax(lastPeriod, lastNewline);
            
            if (breakPoint > pos + m_chunkSize / 2) {
                endPos = breakPoint + 1;
            }
        }

        QString chunk = text.mid(pos, endPos - pos).trimmed();
        if (!chunk.isEmpty()) {
            chunks.append(chunk);
        }

        pos = endPos - m_chunkOverlap;
        if (pos <= 0) pos = endPos;
        
        chunkIdx++;
    }

    return chunks;
}

QString RAGService::extractTextFromFile(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString fileType = fileInfo.suffix().toLower();

    if (fileType == "pdf") {
        return extractTextFromPDF(filePath);
    } else if (fileType == "docx") {
        return extractTextFromDOCX(filePath);
    } else if (fileType == "md" || fileType == "markdown") {
        return extractTextFromMarkdown(filePath);
    } else if (fileType == "txt" || fileType == "json" || fileType == "xml" || 
               fileType == "yaml" || fileType == "yml" || fileType == "html" || fileType == "css") {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setCodec("UTF-8");
            return stream.readAll();
        }
    } else {
        // Assume code file
        return extractTextFromCode(filePath);
    }

    return QString();
}

QString RAGService::extractTextFromPDF(const QString &filePath) const
{
    // Would use a PDF library like Poppler or MuPDF
    // For now, return placeholder
    return "[PDF content extraction requires Poppler library]";
}

QString RAGService::extractTextFromDOCX(const QString &filePath) const
{
    // Would use a DOCX library
    // For now, return placeholder
    return "[DOCX content extraction requires additional library]";
}

QString RAGService::extractTextFromMarkdown(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        return stream.readAll();
    }
    return QString();
}

QString RAGService::extractTextFromCode(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        return stream.readAll();
    }
    return QString();
}

void RAGService::onIndexingFutureFinished()
{
    // Handle indexing completion
}

void RAGService::onEmbeddingFutureFinished()
{
    // Handle embedding generation completion
}

} // namespace Brahma
