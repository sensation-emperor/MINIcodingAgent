#ifndef RAGSERVICE_H
#define RAGSERVICE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QFuture>
#include <QSharedPointer>
#include <memory>

namespace Brahma {

struct DocumentChunk {
    QString id;
    QString content;
    QString sourceFile;
    int startLine;
    int endLine;
    QVector<float> embedding;
    QMap<QString, QVariant> metadata;
    qint64 timestamp;
};

struct SearchQuery {
    QString text;
    int maxResults = 10;
    float minScore = 0.7f;
    QStringList filePatterns;
    QStringList excludePatterns;
    bool useSemanticSearch = true;
    bool useKeywordSearch = false;
    bool includeCodeSnippets = true;
    bool includeDocumentation = true;
};

struct SearchResult {
    DocumentChunk chunk;
    float relevanceScore;
    QString matchType; // "semantic", "keyword", "hybrid"
    QVector<QString> highlights;
};

class EmbeddingModel {
public:
    virtual ~EmbeddingModel() = default;
    virtual bool initialize(const QString& modelPath) = 0;
    virtual QVector<float> generateEmbedding(const QString& text) = 0;
    virtual QVector<QVector<float>> generateBatchEmbeddings(const QStringList& texts) = 0;
    virtual int dimension() const = 0;
    virtual bool isLoaded() const = 0;
};

class VectorIndex {
public:
    virtual ~VectorIndex() = default;
    virtual void addDocument(const DocumentChunk& chunk) = 0;
    virtual void addDocuments(const QVector<DocumentChunk>& chunks) = 0;
    virtual QVector<SearchResult> search(const SearchQuery& query) = 0;
    virtual bool removeDocument(const QString& id) = 0;
    virtual void clear() = 0;
    virtual int size() const = 0;
    virtual bool save(const QString& path) = 0;
    virtual bool load(const QString& path) = 0;
};

class RAGService : public QObject {
    Q_OBJECT

public:
    explicit RAGService(QObject* parent = nullptr);
    ~RAGService();

    // Initialization
    bool initialize(const QString& embeddingModelPath, const QString& indexPath);
    void shutdown();

    // Document Processing
    QFuture<bool> indexFile(const QString& filePath);
    QFuture<bool> indexDirectory(const QString& dirPath, const QStringList& patterns = {});
    QFuture<bool> indexText(const QString& text, const QString& sourceId = {});
    QFuture<bool> indexCodeFile(const QString& filePath, const QString& language);

    // Search Operations
    QFuture<QVector<SearchResult>> search(const SearchQuery& query);
    QFuture<QVector<SearchResult>> semanticSearch(const QString& query, int maxResults = 10);
    QFuture<QVector<SearchResult>> keywordSearch(const QString& query, int maxResults = 10);
    QFuture<QVector<SearchResult>> hybridSearch(const QString& query, int maxResults = 10);

    // Context Generation for LLM
    QFuture<QString> generateContext(const QString& query, int maxTokens = 2048);
    QFuture<QString> generateCodeContext(const QString& query, const QString& language);
    QFuture<QString> generateDocContext(const QString& query);

    // Index Management
    bool rebuildIndex();
    bool optimizeIndex();
    bool saveIndex(const QString& path);
    bool loadIndex(const QString& path);
    void clearIndex();

    // Statistics
    int getDocumentCount() const;
    int getChunkCount() const;
    qint64 getIndexSize() const;
    QStringList getIndexedFiles() const;
    QMap<QString, int> getFileStats() const;

    // Configuration
    void setChunkSize(int size);
    void setChunkOverlap(int overlap);
    void setMaxContextLength(int length);
    void enableCaching(bool enabled);
    void setCacheSize(int size);

signals:
    void indexingStarted(const QString& filePath);
    void indexingProgress(const QString& filePath, int current, int total);
    void indexingCompleted(const QString& filePath, int chunksAdded);
    void indexingFailed(const QString& filePath, const QString& error);
    void searchCompleted(int resultsCount);
    void indexOptimized();
    void cacheCleared();

private:
    struct Private;
    QScopedPointer<Private> d;

    QVector<DocumentChunk> chunkDocument(const QString& text, const QString& sourceId);
    QVector<DocumentChunk> chunkCodeFile(const QString& content, const QString& filePath, const QString& language);
    QVector<DocumentChunk> chunkMarkdownFile(const QString& content, const QString& filePath);
    
    QString extractKeywords(const QString& text);
    QString preprocessText(const QString& text);
    
    QFuture<void> updateCache(const QString& query, const QVector<SearchResult>& results);
    QFuture<QVector<SearchResult>> searchCache(const QString& query);
};

} // namespace Brahma

#endif // RAGSERVICE_H
