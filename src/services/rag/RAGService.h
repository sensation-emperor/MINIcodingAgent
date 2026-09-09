#ifndef RAGSERVICE_H
#define RAGSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace Brahma {

struct DocumentChunk {
    QString id;
    QString documentId;
    QString content;
    QVector<double> embedding;
    int chunkIndex;
    int startIndex;
    int endIndex;
    QMap<QString, QString> metadata;
    QDateTime createdAt;
};

struct Document {
    QString id;
    QString title;
    QString filePath;
    QString fileType; // pdf, docx, txt, md, py, cpp, etc.
    qint64 fileSize;
    int totalChunks;
    QMap<QString, QString> metadata;
    QDateTime indexedAt;
    bool isActive;
};

struct SearchResult {
    DocumentChunk chunk;
    double similarityScore;
    QString highlightedText;
    int relevanceRank;
};

struct EmbeddingModel {
    QString id;
    QString name;
    QString provider; // local, openai, huggingface
    int dimensions;
    int maxInputLength;
    QString modelPath;
    bool isLoaded;
};

class RAGService : public QObject
{
    Q_OBJECT

public:
    explicit RAGService(QObject *parent = nullptr);
    ~RAGService();

    // Initialization
    bool initialize(const QString &embeddingModelPath = "");
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Document Management
    QString addDocument(const QString &filePath, const QString &title = "", 
                       const QMap<QString, QString> &metadata = QMap<QString, QString>());
    bool removeDocument(const QString &documentId);
    bool updateDocument(const QString &documentId);
    Document getDocument(const QString &documentId) const;
    QVector<Document> listDocuments(bool activeOnly = true) const;
    int getDocumentCount() const;

    // Batch Operations
    QStringList addDocuments(const QStringList &filePaths, 
                            const QMap<QString, QString> &baseMetadata = QMap<QString, QString>());
    bool removeDocuments(const QStringList &documentIds);

    // Search & Query
    QVector<SearchResult> search(const QString &query, int topK = 10, 
                                double minScore = 0.0,
                                const QStringList &documentIds = QStringList()) const;
    QVector<SearchResult> semanticSearch(const QString &query, int topK = 10,
                                        const QMap<QString, QString> &filters = QMap<QString, QString>()) const;
    QString generateAnswer(const QString &query, const QVector<SearchResult> &context,
                          const QString &modelId = "") const;

    // Chunk Management
    QVector<DocumentChunk> getDocumentChunks(const QString &documentId, 
                                            int startIdx = 0, int count = -1) const;
    bool removeChunk(const QString &chunkId);
    DocumentChunk getChunk(const QString &chunkId) const;

    // Index Management
    bool clearIndex();
    bool saveIndex(const QString &filePath) const;
    bool loadIndex(const QString &filePath);
    QString getIndexStats() const;

    // Embedding Model Management
    bool loadEmbeddingModel(const QString &modelPath, const QString &modelName = "");
    bool unloadEmbeddingModel();
    EmbeddingModel getCurrentEmbeddingModel() const;
    QVector<EmbeddingModel> getAvailableEmbeddingModels() const;
    QVector<double> generateEmbedding(const QString &text) const;
    QVector<QVector<double>> generateEmbeddings(const QStringList &texts) const;

    // Configuration
    void setChunkSize(int size) { m_chunkSize = size; }
    void setChunkOverlap(int overlap) { m_chunkOverlap = overlap; }
    void setMaxContextLength(int length) { m_maxContextLength = length; }
    int chunkSize() const { return m_chunkSize; }
    int chunkOverlap() const { return m_chunkOverlap; }
    int maxContextLength() const { return m_maxContextLength; }

    // File Type Support
    static QStringList supportedFileTypes();
    static bool isSupportedFileType(const QString &extension);

signals:
    void initializationComplete(bool success);
    void documentAdded(const QString &documentId, const QString &filePath);
    void documentRemoved(const QString &documentId);
    void documentIndexed(const QString &documentId, int progress);
    void indexingComplete(const QString &documentId, bool success, const QString &error = "");
    void searchCompleted(const QVector<SearchResult> &results);
    void errorOccurred(const QString &message);
    void progressUpdated(int percent, const QString &status);
    void modelLoaded(const QString &modelName);
    void modelUnloaded();

private slots:
    void onIndexingFutureFinished();
    void onEmbeddingFutureFinished();

private:
    // Text Processing
    QStringList splitIntoChunks(const QString &text, const QString &documentId) const;
    QString extractTextFromFile(const QString &filePath) const;
    QString extractTextFromPDF(const QString &filePath) const;
    QString extractTextFromDOCX(const QString &filePath) const;
    QString extractTextFromMarkdown(const QString &filePath) const;
    QString extractTextFromCode(const QString &filePath) const;

    // Vector Operations
    double cosineSimilarity(const QVector<double> &vec1, const QVector<double> &vec2) const;
    QVector<int> topKIndices(const QVector<double> &scores, int k) const;
    QVector<double> normalizeVector(const QVector<double> &vec) const;

    // Index Operations
    void rebuildIndex();
    void optimizeIndex();

    // Utility
    QString generateId() const;
    QString getFileType(const QString &filePath) const;
    QMap<QString, QString> extractMetadata(const QString &filePath) const;

    bool m_initialized;
    int m_chunkSize;
    int m_chunkOverlap;
    int m_maxContextLength;
    
    mutable QMutex m_mutex;
    mutable QMutex m_indexMutex;
    
    QMap<QString, Document> m_documents;
    QMap<QString, DocumentChunk> m_chunks;
    QMap<QString, QVector<double>> m_embeddings; // chunkId -> embedding
    
    EmbeddingModel m_currentEmbeddingModel;
    QMap<QString, EmbeddingModel> m_availableModels;
    
    QFutureWatcher<QStringList> m_indexingWatcher;
    QFutureWatcher<QVector<QVector<double>>> m_embeddingWatcher;
    
    QString m_indexPath;
    QDateTime m_lastOptimization;
};

} // namespace Brahma

#endif // RAGSERVICE_H
