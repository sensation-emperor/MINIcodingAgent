#include "RAGService.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QThreadPool>
#include <QRunnable>
#include <QCache>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace Brahma {

// ============================================================================
// Internal Implementation Details
// ============================================================================

struct RAGService::Private {
    QMutex mutex;
    std::unique_ptr<EmbeddingModel> embeddingModel;
    std::unique_ptr<VectorIndex> vectorIndex;
    
    // Configuration
    int chunkSize = 512;
    int chunkOverlap = 50;
    int maxContextLength = 4096;
    bool cachingEnabled = true;
    int cacheSize = 1000;
    
    // Cache for search results
    QCache<QString, QVector<SearchResult>> searchCache;
    
    // Indexing state
    QMap<QString, bool> indexedFiles;
    QMap<QString, qint64> fileTimestamps;
    
    // Statistics
    int totalChunks = 0;
    qint64 indexSize = 0;
    
    Private() : searchCache(cacheSize) {}
};

// ============================================================================
// Helper Classes for Default Implementations
// ============================================================================

class DefaultEmbeddingModel : public EmbeddingModel {
private:
    bool loaded = false;
    int dim = 384; // Default dimension for sentence-transformers
    
public:
    bool initialize(const QString& modelPath) override {
        // In production, this would load a real model (e.g., sentence-transformers via ONNX)
        Q_UNUSED(modelPath);
        loaded = true;
        qDebug() << "[RAG] Embedding model initialized (simulated)";
        return true;
    }
    
    QVector<float> generateEmbedding(const QString& text) override {
        // Simple hash-based pseudo-embedding for demonstration
        // In production, use actual neural network inference
        QVector<float> embedding(dim, 0.0f);
        
        QByteArray hash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
        for (int i = 0; i < qMin(dim, hash.size()); ++i) {
            embedding[i] = (static_cast<uchar>(hash[i]) / 255.0f) * 2.0f - 1.0f;
        }
        
        // Normalize
        float norm = 0.0f;
        for (float v : embedding) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 0) {
            for (float& v : embedding) v /= norm;
        }
        
        return embedding;
    }
    
    QVector<QVector<float>> generateBatchEmbeddings(const QStringList& texts) override {
        QVector<QVector<float>> embeddings;
        embeddings.reserve(texts.size());
        
        for (const auto& text : texts) {
            embeddings.append(generateEmbedding(text));
        }
        
        return embeddings;
    }
    
    int dimension() const override { return dim; }
    bool isLoaded() const override { return loaded; }
};

class DefaultVectorIndex : public VectorIndex {
private:
    QVector<DocumentChunk> chunks;
    QMap<QString, int> chunkIndex; // id -> index
    mutable QMutex mutex;
    
    float cosineSimilarity(const QVector<float>& a, const QVector<float>& b) const {
        if (a.isEmpty() || b.isEmpty() || a.size() != b.size()) return 0.0f;
        
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (int i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
        
        float denom = std::sqrt(normA) * std::sqrt(normB);
        return (denom > 0) ? dot / denom : 0.0f;
    }
    
public:
    void addDocument(const DocumentChunk& chunk) override {
        QMutexLocker locker(&mutex);
        if (chunkIndex.contains(chunk.id)) {
            int idx = chunkIndex[chunk.id];
            chunks[idx] = chunk;
        } else {
            chunkIndex[chunk.id] = chunks.size();
            chunks.append(chunk);
        }
    }
    
    void addDocuments(const QVector<DocumentChunk>& docs) override {
        for (const auto& doc : docs) {
            addDocument(doc);
        }
    }
    
    QVector<SearchResult> search(const SearchQuery& query) override {
        QMutexLocker locker(&mutex);
        QVector<SearchResult> results;
        
        if (chunks.isEmpty() || query.text.isEmpty()) {
            return results;
        }
        
        // Generate query embedding (in production, use actual model)
        DefaultEmbeddingModel tempModel;
        QVector<float> queryEmbedding = tempModel.generateEmbedding(query.text);
        
        // Brute-force similarity search (in production, use FAISS or similar)
        for (const auto& chunk : chunks) {
            if (!query.filePatterns.isEmpty()) {
                bool matches = false;
                for (const auto& pattern : query.filePatterns) {
                    if (chunk.sourceFile.contains(pattern)) {
                        matches = true;
                        break;
                    }
                }
                if (!matches) continue;
            }
            
            float score = cosineSimilarity(queryEmbedding, chunk.embedding);
            
            if (score >= query.minScore) {
                SearchResult result;
                result.chunk = chunk;
                result.relevanceScore = score;
                result.matchType = query.useSemanticSearch ? "semantic" : "keyword";
                
                // Simple highlighting
                QStringList words = query.text.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
                for (const auto& word : words) {
                    if (chunk.content.contains(word, Qt::CaseInsensitive)) {
                        result.highlights.append(word);
                    }
                }
                
                results.append(result);
            }
        }
        
        // Sort by relevance
        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            return a.relevanceScore > b.relevanceScore;
        });
        
        // Limit results
        if (results.size() > query.maxResults) {
            results.resize(query.maxResults);
        }
        
        return results;
    }
    
    bool removeDocument(const QString& id) override {
        QMutexLocker locker(&mutex);
        if (!chunkIndex.contains(id)) {
            return false;
        }
        
        int idx = chunkIndex.take(id);
        chunks.removeAt(idx);
        
        // Rebuild index
        chunkIndex.clear();
        for (int i = 0; i < chunks.size(); ++i) {
            chunkIndex[chunks[i].id] = i;
        }
        
        return true;
    }
    
    void clear() override {
        QMutexLocker locker(&mutex);
        chunks.clear();
        chunkIndex.clear();
    }
    
    int size() const override {
        QMutexLocker locker(&mutex);
        return chunks.size();
    }
    
    bool save(const QString& path) override {
        QMutexLocker locker(&mutex);
        
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        
        QJsonArray array;
        for (const auto& chunk : chunks) {
            QJsonObject obj;
            obj["id"] = chunk.id;
            obj["content"] = chunk.content;
            obj["sourceFile"] = chunk.sourceFile;
            obj["startLine"] = chunk.startLine;
            obj["endLine"] = chunk.endLine;
            obj["timestamp"] = chunk.timestamp;
            
            // Serialize metadata
            QJsonObject metaObj;
            for (auto it = chunk.metadata.begin(); it != chunk.metadata.end(); ++it) {
                metaObj[it.key()] = QJsonValue::fromVariant(it.value());
            }
            obj["metadata"] = metaObj;
            
            // Serialize embedding
            QJsonArray embArray;
            for (float v : chunk.embedding) {
                embArray.append(v);
            }
            obj["embedding"] = embArray;
            
            array.append(obj);
        }
        
        QJsonDocument doc(array);
        file.write(doc.toJson());
        file.close();
        
        return true;
    }
    
    bool load(const QString& path) override {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        if (!doc.isArray()) {
            return false;
        }
        
        QMutexLocker locker(&mutex);
        chunks.clear();
        chunkIndex.clear();
        
        for (const auto& val : doc.array()) {
            QJsonObject obj = val.toObject();
            
            DocumentChunk chunk;
            chunk.id = obj["id"].toString();
            chunk.content = obj["content"].toString();
            chunk.sourceFile = obj["sourceFile"].toString();
            chunk.startLine = obj["startLine"].toInt();
            chunk.endLine = obj["endLine"].toInt();
            chunk.timestamp = obj["timestamp"].toInteger();
            
            // Deserialize metadata
            QJsonObject metaObj = obj["metadata"].toObject();
            for (auto it = metaObj.begin(); it != metaObj.end(); ++it) {
                chunk.metadata[it.key()] = it.value().toVariant();
            }
            
            // Deserialize embedding
            QJsonArray embArray = obj["embedding"].toArray();
            for (const auto& embVal : embArray) {
                chunk.embedding.append(embVal.toDouble());
            }
            
            chunkIndex[chunk.id] = chunks.size();
            chunks.append(chunk);
        }
        
        return true;
    }
};

// ============================================================================
// RAGService Implementation
// ============================================================================

RAGService::RAGService(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

RAGService::~RAGService() {
    shutdown();
}

bool RAGService::initialize(const QString& embeddingModelPath, const QString& indexPath) {
    QMutexLocker locker(&d->mutex);
    
    // Initialize embedding model
    d->embeddingModel = std::make_unique<DefaultEmbeddingModel>();
    if (!d->embeddingModel->initialize(embeddingModelPath)) {
        qCritical() << "[RAG] Failed to initialize embedding model";
        return false;
    }
    
    // Initialize vector index
    d->vectorIndex = std::make_unique<DefaultVectorIndex>();
    
    // Try to load existing index
    if (!indexPath.isEmpty() && QFileInfo::exists(indexPath)) {
        if (d->vectorIndex->load(indexPath)) {
            d->totalChunks = d->vectorIndex->size();
            qDebug() << "[RAG] Loaded index with" << d->totalChunks << "chunks";
        }
    }
    
    qDebug() << "[RAG] Service initialized successfully";
    return true;
}

void RAGService::shutdown() {
    QMutexLocker locker(&d->mutex);
    
    if (d->vectorIndex) {
        d->vectorIndex->clear();
    }
    
    d->embeddingModel.reset();
    d->vectorIndex.reset();
    d->searchCache.clear();
    
    qDebug() << "[RAG] Service shut down";
}

QFuture<bool> RAGService::indexFile(const QString& filePath) {
    return QtConcurrent::run([this, filePath]() {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            emit indexingFailed(filePath, "File does not exist");
            return false;
        }
        
        emit indexingStarted(filePath);
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit indexingFailed(filePath, "Cannot open file");
            return false;
        }
        
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();
        
        // Determine file type and chunk accordingly
        QVector<DocumentChunk> chunks;
        QString ext = fileInfo.suffix().toLower();
        
        if (ext == "md" || ext == "markdown") {
            chunks = chunkMarkdownFile(content, filePath);
        } else if (ext == "py" || ext == "cpp" || ext == "h" || ext == "js" || ext == "ts") {
            chunks = chunkCodeFile(content, filePath, ext);
        } else {
            chunks = chunkDocument(content, filePath);
        }
        
        if (chunks.isEmpty()) {
            emit indexingFailed(filePath, "No chunks generated");
            return false;
        }
        
        // Add to index
        d->vectorIndex->addDocuments(chunks);
        d->indexedFiles[filePath] = true;
        d->fileTimestamps[filePath] = fileInfo.lastModified().toMSecsSinceEpoch();
        d->totalChunks = d->vectorIndex->size();
        
        emit indexingCompleted(filePath, chunks.size());
        return true;
    });
}

QFuture<bool> RAGService::indexDirectory(const QString& dirPath, const QStringList& patterns) {
    return QtConcurrent::run([this, dirPath, patterns]() {
        QDir dir(dirPath);
        if (!dir.exists()) {
            return false;
        }
        
        QStringList filters;
        if (patterns.isEmpty()) {
            filters = {"*.py", "*.cpp", "*.h", "*.hpp", "*.js", "*.ts", "*.md", "*.txt", "*.rst"};
        } else {
            filters = patterns;
        }
        
        dir.setNameFilters(filters);
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
        
        QFileInfoList files = dir.entryInfoList();
        int total = files.size();
        int current = 0;
        int successCount = 0;
        
        for (const auto& fileInfo : files) {
            emit indexingProgress(fileInfo.filePath(), ++current, total);
            
            QFuture<bool> future = indexFile(fileInfo.filePath());
            if (future.result()) {
                successCount++;
            }
        }
        
        qDebug() << "[RAG] Indexed" << successCount << "of" << total << "files";
        return successCount > 0;
    });
}

QFuture<bool> RAGService::indexText(const QString& text, const QString& sourceId) {
    return QtConcurrent::run([this, text, sourceId]() {
        if (text.trimmed().isEmpty()) {
            return false;
        }
        
        QString id = sourceId.isEmpty() 
            ? QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Md5).toHex()
            : sourceId;
        
        QVector<DocumentChunk> chunks = chunkDocument(text, id);
        
        if (chunks.isEmpty()) {
            return false;
        }
        
        d->vectorIndex->addDocuments(chunks);
        d->totalChunks = d->vectorIndex->size();
        
        return true;
    });
}

QFuture<bool> RAGService::indexCodeFile(const QString& filePath, const QString& language) {
    // Delegate to indexFile which already handles code files
    return indexFile(filePath);
}

QFuture<QVector<SearchResult>> RAGService::search(const SearchQuery& query) {
    return QtConcurrent::run([this, query]() {
        // Check cache first
        if (d->cachingEnabled) {
            QString cacheKey = query.text + "_" + QString::number(query.maxResults);
            auto cached = d->searchCache.object(cacheKey);
            if (cached) {
                return *cached;
            }
        }
        
        QVector<SearchResult> results = d->vectorIndex->search(query);
        
        // Cache results
        if (d->cachingEnabled && !results.isEmpty()) {
            QString cacheKey = query.text + "_" + QString::number(query.maxResults);
            d->searchCache.insert(cacheKey, new QVector<SearchResult>(results));
        }
        
        emit searchCompleted(results.size());
        return results;
    });
}

QFuture<QVector<SearchResult>> RAGService::semanticSearch(const QString& query, int maxResults) {
    SearchQuery q;
    q.text = query;
    q.maxResults = maxResults;
    q.useSemanticSearch = true;
    q.useKeywordSearch = false;
    return search(q);
}

QFuture<QVector<SearchResult>> RAGService::keywordSearch(const QString& query, int maxResults) {
    SearchQuery q;
    q.text = query;
    q.maxResults = maxResults;
    q.useSemanticSearch = false;
    q.useKeywordSearch = true;
    return search(q);
}

QFuture<QVector<SearchResult>> RAGService::hybridSearch(const QString& query, int maxResults) {
    SearchQuery q;
    q.text = query;
    q.maxResults = maxResults;
    q.useSemanticSearch = true;
    q.useKeywordSearch = true;
    return search(q);
}

QFuture<QString> RAGService::generateContext(const QString& query, int maxTokens) {
    return QtConcurrent::run([this, query, maxTokens]() {
        SearchQuery q;
        q.text = query;
        q.maxResults = 20;
        q.minScore = 0.6f;
        
        QVector<SearchResult> results = search(q).result();
        
        if (results.isEmpty()) {
            return QString("No relevant context found for: ") + query;
        }
        
        QString context;
        int tokenCount = 0;
        int avgTokensPerChunk = 100; // Rough estimate
        
        for (const auto& result : results) {
            if (tokenCount + avgTokensPerChunk > maxTokens) {
                break;
            }
            
            context += "---\nSource: " + result.chunk.sourceFile + "\n";
            context += "Relevance: " + QString::number(result.relevanceScore, 'f', 2) + "\n";
            context += result.chunk.content + "\n\n";
            
            tokenCount += avgTokensPerChunk;
        }
        
        return context;
    });
}

QFuture<QString> RAGService::generateCodeContext(const QString& query, const QString& language) {
    return QtConcurrent::run([this, query, language]() {
        SearchQuery q;
        q.text = query;
        q.maxResults = 15;
        q.minScore = 0.65f;
        q.includeCodeSnippets = true;
        q.includeDocumentation = false;
        
        // Filter by language if specified
        if (!language.isEmpty()) {
            q.filePatterns.append("*." + language);
        }
        
        QVector<SearchResult> results = search(q).result();
        
        if (results.isEmpty()) {
            return QString("No relevant code found for: ") + query;
        }
        
        QString context;
        for (const auto& result : results) {
            context += "```" + language + "\n";
            context += "// From: " + result.chunk.sourceFile + "\n";
            context += result.chunk.content + "\n";
            context += "```\n\n";
        }
        
        return context;
    });
}

QFuture<QString> RAGService::generateDocContext(const QString& query) {
    return QtConcurrent::run([this, query]() {
        SearchQuery q;
        q.text = query;
        q.maxResults = 10;
        q.minScore = 0.7f;
        q.includeCodeSnippets = false;
        q.includeDocumentation = true;
        
        QVector<SearchResult> results = search(q).result();
        
        if (results.isEmpty()) {
            return QString("No relevant documentation found for: ") + query;
        }
        
        QString context;
        for (const auto& result : results) {
            context += "## " + result.chunk.sourceFile + "\n\n";
            context += result.chunk.content + "\n\n";
        }
        
        return context;
    });
}

bool RAGService::rebuildIndex() {
    QMutexLocker locker(&d->mutex);
    
    if (d->vectorIndex) {
        d->vectorIndex->clear();
    }
    
    d->vectorIndex = std::make_unique<DefaultVectorIndex>();
    d->indexedFiles.clear();
    d->totalChunks = 0;
    
    qDebug() << "[RAG] Index rebuilt";
    return true;
}

bool RAGService::optimizeIndex() {
    // In production, this would perform index optimization (e.g., HNSW graph optimization)
    qDebug() << "[RAG] Index optimized";
    emit indexOptimized();
    return true;
}

bool RAGService::saveIndex(const QString& path) {
    QMutexLocker locker(&d->mutex);
    
    if (!d->vectorIndex) {
        return false;
    }
    
    bool success = d->vectorIndex->save(path);
    if (success) {
        QFileInfo fi(path);
        d->indexSize = fi.size();
        qDebug() << "[RAG] Index saved to" << path << "(" << d->indexSize << "bytes)";
    }
    
    return success;
}

bool RAGService::loadIndex(const QString& path) {
    QMutexLocker locker(&d->mutex);
    
    if (!d->vectorIndex) {
        d->vectorIndex = std::make_unique<DefaultVectorIndex>();
    }
    
    bool success = d->vectorIndex->load(path);
    if (success) {
        d->totalChunks = d->vectorIndex->size();
        QFileInfo fi(path);
        d->indexSize = fi.size();
        qDebug() << "[RAG] Index loaded from" << path << "(" << d->totalChunks << "chunks)";
    }
    
    return success;
}

void RAGService::clearIndex() {
    QMutexLocker locker(&d->mutex);
    
    if (d->vectorIndex) {
        d->vectorIndex->clear();
    }
    
    d->indexedFiles.clear();
    d->fileTimestamps.clear();
    d->totalChunks = 0;
    d->indexSize = 0;
    d->searchCache.clear();
    
    qDebug() << "[RAG] Index cleared";
}

int RAGService::getDocumentCount() const {
    return d->indexedFiles.size();
}

int RAGService::getChunkCount() const {
    return d->totalChunks;
}

qint64 RAGService::getIndexSize() const {
    return d->indexSize;
}

QStringList RAGService::getIndexedFiles() const {
    return d->indexedFiles.keys();
}

QMap<QString, int> RAGService::getFileStats() const {
    QMap<QString, int> stats;
    for (auto it = d->indexedFiles.begin(); it != d->indexedFiles.end(); ++it) {
        stats[it.key()] = 1; // Could be extended to track chunks per file
    }
    return stats;
}

void RAGService::setChunkSize(int size) {
    d->chunkSize = size;
}

void RAGService::setChunkOverlap(int overlap) {
    d->chunkOverlap = overlap;
}

void RAGService::setMaxContextLength(int length) {
    d->maxContextLength = length;
}

void RAGService::enableCaching(bool enabled) {
    d->cachingEnabled = enabled;
    if (!enabled) {
        d->searchCache.clear();
        emit cacheCleared();
    }
}

void RAGService::setCacheSize(int size) {
    d->cacheSize = size;
    d->searchCache.setMaxCost(size);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

QVector<DocumentChunk> RAGService::chunkDocument(const QString& text, const QString& sourceId) {
    QVector<DocumentChunk> chunks;
    
    if (text.trimmed().isEmpty()) {
        return chunks;
    }
    
    // Split into paragraphs/sentences
    QStringList paragraphs = text.split(QRegularExpression("\n\\s*\n"), Qt::SkipEmptyParts);
    
    QString currentChunk;
    int startLine = 1;
    int lineNum = 1;
    
    for (const auto& para : paragraphs) {
        if (currentChunk.length() + para.length() > d->chunkSize) {
            if (!currentChunk.trimmed().isEmpty()) {
                DocumentChunk chunk;
                chunk.id = QCryptographicHash::hash(currentChunk.toUtf8(), QCryptographicHash::Md5).toHex();
                chunk.content = currentChunk.trimmed();
                chunk.sourceFile = sourceId;
                chunk.startLine = startLine;
                chunk.endLine = lineNum - 1;
                chunk.timestamp = QDateTime::currentMSecsSinceEpoch();
                chunk.embedding = d->embeddingModel->generateEmbedding(chunk.content);
                
                chunks.append(chunk);
            }
            
            currentChunk = para + "\n\n";
            startLine = lineNum;
        } else {
            currentChunk += para + "\n\n";
        }
        
        lineNum += para.count('\n') + 1;
    }
    
    // Add remaining chunk
    if (!currentChunk.trimmed().isEmpty()) {
        DocumentChunk chunk;
        chunk.id = QCryptographicHash::hash(currentChunk.toUtf8(), QCryptographicHash::Md5).toHex();
        chunk.content = currentChunk.trimmed();
        chunk.sourceFile = sourceId;
        chunk.startLine = startLine;
        chunk.endLine = lineNum;
        chunk.timestamp = QDateTime::currentMSecsSinceEpoch();
        chunk.embedding = d->embeddingModel->generateEmbedding(chunk.content);
        
        chunks.append(chunk);
    }
    
    return chunks;
}

QVector<DocumentChunk> RAGService::chunkCodeFile(const QString& content, const QString& filePath, const QString& language) {
    QVector<DocumentChunk> chunks;
    
    if (content.trimmed().isEmpty()) {
        return chunks;
    }
    
    // Split code by functions/classes
    QStringList lines = content.split('\n');
    QString currentChunk;
    int startLine = 1;
    int braceCount = 0;
    bool inFunction = false;
    
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        currentChunk += line + "\n";
        
        // Track braces for scope detection
        braceCount += line.count('{') - line.count('}');
        
        // Detect function/class start
        if (line.contains(QRegularExpression("^(def|class|function|struct|interface)\\s+\\w+"))) {
            inFunction = true;
            if (startLine == 1) startLine = i + 1;
        }
        
        // Chunk at function boundaries or when size limit reached
        bool shouldChunk = false;
        if (inFunction && braceCount == 0) {
            shouldChunk = true;
            inFunction = false;
        } else if (currentChunk.length() > d->chunkSize) {
            shouldChunk = true;
        }
        
        if (shouldChunk && !currentChunk.trimmed().isEmpty()) {
            DocumentChunk chunk;
            chunk.id = QCryptographicHash::hash(currentChunk.toUtf8(), QCryptographicHash::Md5).toHex();
            chunk.content = currentChunk;
            chunk.sourceFile = filePath;
            chunk.startLine = startLine;
            chunk.endLine = i + 1;
            chunk.timestamp = QDateTime::currentMSecsSinceEpoch();
            
            // Add language metadata
            chunk.metadata["language"] = language;
            chunk.metadata["type"] = inFunction ? "function" : "code_block";
            
            chunk.embedding = d->embeddingModel->generateEmbedding(chunk.content);
            chunks.append(chunk);
            
            currentChunk.clear();
            startLine = i + 2;
        }
    }
    
    // Add remaining chunk
    if (!currentChunk.trimmed().isEmpty()) {
        DocumentChunk chunk;
        chunk.id = QCryptographicHash::hash(currentChunk.toUtf8(), QCryptographicHash::Md5).toHex();
        chunk.content = currentChunk;
        chunk.sourceFile = filePath;
        chunk.startLine = startLine;
        chunk.endLine = lines.size();
        chunk.timestamp = QDateTime::currentMSecsSinceEpoch();
        chunk.metadata["language"] = language;
        chunk.embedding = d->embeddingModel->generateEmbedding(chunk.content);
        chunks.append(chunk);
    }
    
    return chunks;
}

QVector<DocumentChunk> RAGService::chunkMarkdownFile(const QString& content, const QString& filePath) {
    // Similar to chunkDocument but respects markdown structure
    QVector<DocumentChunk> chunks;
    
    QStringList sections = content.split(QRegularExpression("\n#\\s+"), Qt::SkipEmptyParts);
    
    for (int i = 0; i < sections.size(); ++i) {
        QString section = sections[i];
        if (i > 0) {
            // Add back the header
            section = "# " + section;
        }
        
        if (section.trimmed().isEmpty()) continue;
        
        DocumentChunk chunk;
        chunk.id = QCryptographicHash::hash(section.toUtf8(), QCryptographicHash::Md5).toHex();
        chunk.content = section;
        chunk.sourceFile = filePath;
        chunk.startLine = 1; // Would need better line tracking
        chunk.endLine = section.count('\n') + 1;
        chunk.timestamp = QDateTime::currentMSecsSinceEpoch();
        chunk.metadata["type"] = "markdown_section";
        chunk.embedding = d->embeddingModel->generateEmbedding(chunk.content);
        
        chunks.append(chunk);
    }
    
    return chunks;
}

QString RAGService::extractKeywords(const QString& text) {
    // Simple keyword extraction (in production, use TF-IDF or RAKE)
    QStringList words = text.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
    
    QMap<QString, int> freq;
    for (const auto& word : words) {
        if (word.length() > 3) {
            freq[word.toLower()]++;
        }
    }
    
    // Get top 10 keywords
    QList<QPair<QString, int>> sorted;
    for (auto it = freq.begin(); it != freq.end(); ++it) {
        sorted.append(qMakePair(it.key(), it.value()));
    }
    
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    QStringList keywords;
    for (int i = 0; i < qMin(10, sorted.size()); ++i) {
        keywords.append(sorted[i].first);
    }
    
    return keywords.join(", ");
}

QString RAGService::preprocessText(const QString& text) {
    // Basic preprocessing
    QString processed = text;
    processed = processed.replace(QRegularExpression("\\s+"), " ");
    processed = processed.trimmed();
    return processed;
}

QFuture<void> RAGService::updateCache(const QString& query, const QVector<SearchResult>& results) {
    return QtConcurrent::run([this, query, results]() {
        if (d->cachingEnabled && !results.isEmpty()) {
            QString cacheKey = query + "_10";
            d->searchCache.insert(cacheKey, new QVector<SearchResult>(results));
        }
    });
}

QFuture<QVector<SearchResult>> RAGService::searchCache(const QString& query) {
    return QtConcurrent::run([this, query]() {
        if (d->cachingEnabled) {
            QString cacheKey = query + "_10";
            auto cached = d->searchCache.object(cacheKey);
            if (cached) {
                return *cached;
            }
        }
        return QVector<SearchResult>();
    });
}

} // namespace Brahma
