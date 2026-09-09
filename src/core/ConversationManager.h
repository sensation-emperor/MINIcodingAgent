#pragma once

#ifdef BUILD_GUI
#include <QObject>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <QUuid>
#include <QVariant>
#include <QList>
#endif

#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <cstdint>

// Forward declarations for Qt types if GUI is not built
#ifndef BUILD_GUI
class QObject {
public:
    virtual ~QObject() = default;
    void setParent(void*) {}
    void* parent() const { return nullptr; }
};
#define Q_OBJECT
#endif

namespace brahma {

#ifndef BUILD_GUI
// Minimal implementations for non-GUI build
struct QUuid {
    static QUuid createUuid() { return QUuid{}; }
    bool operator==(const QUuid& other) const { return data1 == other.data1; }
    uint32_t data1 = 0;
};

class QString {
public:
    QString() = default;
    QString(const std::string& s) : m_str(s) {}
    QString(const char* s) : m_str(s ? s : "") {}
    const std::string& toStdString() const { return m_str; }
    const char* c_str() const { return m_str.c_str(); }
private:
    std::string m_str;
};

class QDateTime {
public:
    static QDateTime currentDateTime() { return QDateTime{}; }
    qint64 toMSecsSinceEpoch() const { return 0; }
private:
    qint64 m_msecs = 0;
};

template<typename K, typename V>
class QMap {
public:
    void insert(const K&, const V&) {}
    V value(const K&) const { return V{}; }
    bool contains(const K&) const { return false; }
};

class QVariant {
public:
    QVariant() = default;
    template<typename T> QVariant(const T&) {}
};

template<typename T>
class QList {
public:
    void append(const T&) {}
    int size() const { return 0; }
};
#endif

/**
 * @brief Represents a single message in a conversation
 */
struct ConversationMessage {
    QUuid id;
    QString role; // "user", "assistant", "system"
    QString content;
    QDateTime timestamp;
    QString modelId;
    int tokenCount;
    double latency; // seconds
    QMap<QString, QVariant> metadata;
    
    ConversationMessage() : tokenCount(0), latency(0.0) {
        id = QUuid::createUuid();
        timestamp = QDateTime::currentDateTime();
    }
};

/**
 * @brief Represents a conversation session with multiple messages
 */
class Conversation : public QObject {
    Q_OBJECT
    
public:
    explicit Conversation(const QString& title = "New Conversation", QObject* parent = nullptr);
    ~Conversation() override = default;
    
    // Properties
    QUuid id() const { return m_id; }
    QString title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; emit titleChanged(); }
    QDateTime createdAt() const { return m_createdAt; }
    QDateTime updatedAt() const { return m_updatedAt; }
    int messageCount() const { return m_messages.size(); }
    bool isArchived() const { return m_archived; }
    
    // Message management
    void addMessage(const QString& role, const QString& content, 
                   const QString& modelId = "", int tokens = 0, double latency = 0.0);
    ConversationMessage getMessage(int index) const;
    ConversationMessage getMessageById(const QUuid& id) const;
    void removeMessage(const QUuid& messageId);
    void clearMessages();
    
    const std::vector<ConversationMessage>& messages() const { return m_messages; }
    
    // Context window management
    std::vector<ConversationMessage> getContextWindow(int maxTokens) const;
    QString summarize() const;
    
    // Serialization
    QString toJson() const;
    static Conversation* fromJson(const QString& json, QObject* parent = nullptr);
    
    // Archive/restore
    void archive();
    void restore();
    
signals:
    void messageAdded(const ConversationMessage& msg);
    void messageRemoved(const QUuid& id);
    void messagesCleared();
    void titleChanged();
    void updated();
    
private:
    QUuid m_id;
    QString m_title;
    std::vector<ConversationMessage> m_messages;
    QDateTime m_createdAt;
    QDateTime m_updatedAt;
    bool m_archived = false;
    
    void updateTimestamp();
};

/**
 * @brief Manages all conversations with persistence
 */
class ConversationManager : public QObject {
    Q_OBJECT
    
public:
    static ConversationManager* instance();
    
    // Conversation lifecycle
    Conversation* createConversation(const QString& title = "New Conversation");
    Conversation* getConversation(const QUuid& id);
    Conversation* getCurrentConversation() const { return m_currentConversation; }
    void setCurrentConversation(Conversation* conv);
    void deleteConversation(const QUuid& id);
    void archiveConversation(const QUuid& id);
    
    // Listing
    QList<Conversation*> activeConversations() const;
    QList<Conversation*> archivedConversations() const;
    QList<Conversation*> allConversations() const;
    
    // Search
    QList<Conversation*> searchConversations(const QString& query) const;
    
    // Persistence
    bool saveConversation(Conversation* conv);
    bool loadConversation(const QUuid& id);
    bool deleteFromStorage(const QUuid& id);
    void exportConversation(Conversation* conv, const QString& filePath, const QString& format = "json");
    Conversation* importConversation(const QString& filePath);
    
    // Cleanup
    void autoArchiveOldConversations(int daysThreshold = 30);
    void clearAllConversations();
    
    // Statistics
    int totalConversations() const { return m_conversations.size(); }
    int activeConversationsCount() const;
    qint64 totalTokensUsed() const;
    
signals:
    void conversationCreated(Conversation* conv);
    void conversationDeleted(const QUuid& id);
    void conversationArchived(const QUuid& id);
    void currentConversationChanged(Conversation* conv);
    void conversationsLoaded();
    
private:
    explicit ConversationManager(QObject* parent = nullptr);
    ~ConversationManager() override;
    
    static ConversationManager* s_instance;
    
    QMap<QUuid, Conversation*> m_conversations;
    Conversation* m_currentConversation = nullptr;
    QString m_storagePath;
    
    void initializeStorage();
    QString storageFilePath() const;
};

} // namespace brahma
