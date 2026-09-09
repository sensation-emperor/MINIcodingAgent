#include "ConversationManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

namespace brahma {

// ============================================================================
// Conversation Implementation
// ============================================================================

Conversation::Conversation(const QString& title, QObject* parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_title(title)
    , m_createdAt(QDateTime::currentDateTime())
    , m_updatedAt(m_createdAt)
{
}

void Conversation::addMessage(const QString& role, const QString& content,
                              const QString& modelId, int tokens, double latency)
{
    ConversationMessage msg;
    msg.role = role;
    msg.content = content;
    msg.modelId = modelId;
    msg.tokenCount = tokens;
    msg.latency = latency;
    
    m_messages.push_back(msg);
    updateTimestamp();
    
    emit messageAdded(msg);
    emit updated();
}

ConversationMessage Conversation::getMessage(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_messages.size())) {
        return m_messages[index];
    }
    return ConversationMessage{};
}

ConversationMessage Conversation::getMessageById(const QUuid& id) const
{
    auto it = std::find_if(m_messages.begin(), m_messages.end(),
                          [&id](const ConversationMessage& msg) {
                              return msg.id == id;
                          });
    
    if (it != m_messages.end()) {
        return *it;
    }
    return ConversationMessage{};
}

void Conversation::removeMessage(const QUuid& messageId)
{
    auto it = std::find_if(m_messages.begin(), m_messages.end(),
                          [&messageId](const ConversationMessage& msg) {
                              return msg.id == messageId;
                          });
    
    if (it != m_messages.end()) {
        m_messages.erase(it);
        updateTimestamp();
        emit messageRemoved(messageId);
        emit updated();
    }
}

void Conversation::clearMessages()
{
    m_messages.clear();
    updateTimestamp();
    emit messagesCleared();
    emit updated();
}

std::vector<ConversationMessage> Conversation::getContextWindow(int maxTokens) const
{
    std::vector<ConversationMessage> result;
    int currentTokens = 0;
    
    // Start from the end and work backwards
    for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it) {
        if (currentTokens + it->tokenCount > maxTokens && !result.empty()) {
            break;
        }
        result.push_back(*it);
        currentTokens += it->tokenCount;
    }
    
    // Reverse to get chronological order
    std::reverse(result.begin(), result.end());
    return result;
}

QString Conversation::summarize() const
{
    if (m_messages.empty()) {
        return "Empty conversation";
    }
    
    // Simple summarization: first user message + last assistant message
    QString firstUser;
    QString lastAssistant;
    
    for (const auto& msg : m_messages) {
        if (msg.role == "user" && firstUser.isEmpty()) {
            firstUser = msg.content.left(100);
            if (msg.content.length() > 100) firstUser += "...";
        }
        if (msg.role == "assistant") {
            lastAssistant = msg.content.left(100);
            if (msg.content.length() > 100) lastAssistant += "...";
        }
    }
    
    if (!firstUser.isEmpty() && !lastAssistant.isEmpty()) {
        return QString("Started with \"%1\"... Latest: \"%2\"...")
            .arg(firstUser, lastAssistant);
    } else if (!firstUser.isEmpty()) {
        return QString("Started with \"%1\"...").arg(firstUser);
    }
    
    return "Conversation";
}

QString Conversation::toJson() const
{
    QJsonObject obj;
    obj["id"] = m_id.toString(QUuid::WithoutBraces);
    obj["title"] = m_title;
    obj["createdAt"] = m_createdAt.toString(Qt::ISODate);
    obj["updatedAt"] = m_updatedAt.toString(Qt::ISODate);
    obj["archived"] = m_archived;
    
    QJsonArray messagesArray;
    for (const auto& msg : m_messages) {
        QJsonObject msgObj;
        msgObj["id"] = msg.id.toString(QUuid::WithoutBraces);
        msgObj["role"] = msg.role;
        msgObj["content"] = msg.content;
        msgObj["timestamp"] = msg.timestamp.toString(Qt::ISODate);
        msgObj["modelId"] = msg.modelId;
        msgObj["tokenCount"] = msg.tokenCount;
        msgObj["latency"] = msg.latency;
        
        QJsonObject metadata;
        for (auto it = msg.metadata.begin(); it != msg.metadata.end(); ++it) {
            metadata[it.key()] = QJsonValue::fromVariant(it.value());
        }
        msgObj["metadata"] = metadata;
        
        messagesArray.append(msgObj);
    }
    obj["messages"] = messagesArray;
    
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

Conversation* Conversation::fromJson(const QString& json, QObject* parent)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) {
        return nullptr;
    }
    
    QJsonObject obj = doc.object();
    auto* conv = new Conversation(obj["title"].toString(), parent);
    
    conv->m_id = QUuid::fromString("{" + obj["id"].toString() + "}");
    conv->m_createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    conv->m_updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODate);
    conv->m_archived = obj["archived"].toBool(false);
    
    QJsonArray messagesArray = obj["messages"].toArray();
    for (const auto& msgVal : messagesArray) {
        QJsonObject msgObj = msgVal.toObject();
        
        ConversationMessage msg;
        msg.id = QUuid::fromString("{" + msgObj["id"].toString() + "}");
        msg.role = msgObj["role"].toString();
        msg.content = msgObj["content"].toString();
        msg.timestamp = QDateTime::fromString(msgObj["timestamp"].toString(), Qt::ISODate);
        msg.modelId = msgObj["modelId"].toString();
        msg.tokenCount = msgObj["tokenCount"].toInt(0);
        msg.latency = msgObj["latency"].toDouble(0.0);
        
        QJsonObject metadataObj = msgObj["metadata"].toObject();
        for (auto it = metadataObj.begin(); it != metadataObj.end(); ++it) {
            msg.metadata[it.key()] = it.value().toVariant();
        }
        
        conv->m_messages.push_back(msg);
    }
    
    return conv;
}

void Conversation::archive()
{
    m_archived = true;
    updateTimestamp();
    emit updated();
}

void Conversation::restore()
{
    m_archived = false;
    updateTimestamp();
    emit updated();
}

void Conversation::updateTimestamp()
{
    m_updatedAt = QDateTime::currentDateTime();
}

// ============================================================================
// ConversationManager Implementation
// ============================================================================

ConversationManager* ConversationManager::s_instance = nullptr;

ConversationManager* ConversationManager::instance()
{
    if (!s_instance) {
        s_instance = new ConversationManager();
    }
    return s_instance;
}

ConversationManager::ConversationManager(QObject* parent)
    : QObject(parent)
{
    initializeStorage();
}

ConversationManager::~ConversationManager()
{
    qDeleteAll(m_conversations);
    m_conversations.clear();
}

void ConversationManager::initializeStorage()
{
    m_storagePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(m_storagePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString ConversationManager::storageFilePath() const
{
    return QDir(m_storagePath).filePath("conversations.json");
}

Conversation* ConversationManager::createConversation(const QString& title)
{
    auto* conv = new Conversation(title, this);
    m_conversations[conv->id()] = conv;
    m_currentConversation = conv;
    
    emit conversationCreated(conv);
    emit currentConversationChanged(conv);
    
    saveConversation(conv);
    return conv;
}

Conversation* ConversationManager::getConversation(const QUuid& id)
{
    return m_conversations.value(id, nullptr);
}

void ConversationManager::setCurrentConversation(Conversation* conv)
{
    if (m_currentConversation != conv) {
        m_currentConversation = conv;
        emit currentConversationChanged(conv);
    }
}

void ConversationManager::deleteConversation(const QUuid& id)
{
    Conversation* conv = m_conversations.take(id);
    if (conv) {
        if (m_currentConversation == conv) {
            m_currentConversation = nullptr;
            emit currentConversationChanged(nullptr);
        }
        
        deleteFromStorage(id);
        conv->deleteLater();
        
        emit conversationDeleted(id);
    }
}

void ConversationManager::archiveConversation(const QUuid& id)
{
    Conversation* conv = m_conversations.value(id);
    if (conv) {
        conv->archive();
        saveConversation(conv);
        emit conversationArchived(id);
    }
}

QList<Conversation*> ConversationManager::activeConversations() const
{
    QList<Conversation*> result;
    for (auto* conv : m_conversations) {
        if (!conv->isArchived()) {
            result.append(conv);
        }
    }
    // Sort by updated date, newest first
    std::sort(result.begin(), result.end(), [](Conversation* a, Conversation* b) {
        return a->updatedAt() > b->updatedAt();
    });
    return result;
}

QList<Conversation*> ConversationManager::archivedConversations() const
{
    QList<Conversation*> result;
    for (auto* conv : m_conversations) {
        if (conv->isArchived()) {
            result.append(conv);
        }
    }
    return result;
}

QList<Conversation*> ConversationManager::allConversations() const
{
    return m_conversations.values();
}

QList<Conversation*> ConversationManager::searchConversations(const QString& query) const
{
    QList<Conversation*> result;
    QString lowerQuery = query.toLower();
    
    for (auto* conv : m_conversations) {
        bool match = conv->title().toLower().contains(lowerQuery);
        
        // Search in messages
        if (!match) {
            for (const auto& msg : conv->messages()) {
                if (msg.content.toLower().contains(lowerQuery)) {
                    match = true;
                    break;
                }
            }
        }
        
        if (match) {
            result.append(conv);
        }
    }
    
    return result;
}

bool ConversationManager::saveConversation(Conversation* conv)
{
    QFile file(storageFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QJsonObject root;
    QJsonArray conversationsArray;
    
    for (auto* c : m_conversations) {
        QJsonDocument doc = QJsonDocument::fromJson(c->toJson().toUtf8());
        conversationsArray.append(doc.object());
    }
    
    root["conversations"] = conversationsArray;
    root["version"] = 1;
    
    QTextStream out(&file);
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    file.close();
    
    return true;
}

bool ConversationManager::loadConversation(const QUuid& id)
{
    QFile file(storageFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) {
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray conversationsArray = root["conversations"].toArray();
    
    for (const auto& convVal : conversationsArray) {
        QJsonObject convObj = convVal.toObject();
        QString convId = convObj["id"].toString();
        
        if ("{" + convId + "}" == id.toString(QUuid::WithBraces)) {
            Conversation* conv = Conversation::fromJson(QJsonDocument(convObj).toJson(), this);
            if (conv) {
                m_conversations[id] = conv;
                emit conversationsLoaded();
                return true;
            }
        }
    }
    
    return false;
}

bool ConversationManager::deleteFromStorage(const QUuid& id)
{
    // Reload all conversations except the deleted one
    QFile file(storageFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) {
        return true; // Nothing to delete
    }
    
    QJsonObject root = doc.object();
    QJsonArray conversationsArray = root["conversations"].toArray();
    QJsonArray newArray;
    
    for (const auto& convVal : conversationsArray) {
        QJsonObject convObj = convVal.toObject();
        QString convId = convObj["id"].toString();
        
        if ("{" + convId + "}" != id.toString(QUuid::WithBraces)) {
            newArray.append(convObj);
        }
    }
    
    root["conversations"] = newArray;
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    file.close();
    
    return true;
}

void ConversationManager::exportConversation(Conversation* conv, const QString& filePath, 
                                             const QString& format)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    
    if (format == "json") {
        file.write(conv->toJson().toUtf8());
    } else if (format == "txt" || format == "markdown") {
        QTextStream out(&file);
        out << "# " << conv->title() << "\n\n";
        out << "Created: " << conv->createdAt().toString() << "\n";
        out << "Messages: " << conv->messageCount() << "\n\n";
        out << "---\n\n";
        
        for (const auto& msg : conv->messages()) {
            out << "**" << msg.role.toUpper() << "** [" 
                << msg.timestamp.toString("hh:mm:ss") << "]\n";
            out << msg.content << "\n\n";
            out << "---\n\n";
        }
    }
    
    file.close();
}

Conversation* ConversationManager::importConversation(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return nullptr;
    }
    
    QString json = file.readAll();
    file.close();
    
    Conversation* conv = Conversation::fromJson(json, this);
    if (conv) {
        m_conversations[conv->id()] = conv;
        saveConversation(conv);
        emit conversationCreated(conv);
        return conv;
    }
    
    return nullptr;
}

void ConversationManager::autoArchiveOldConversations(int daysThreshold)
{
    QDateTime threshold = QDateTime::currentDateTime().addDays(-daysThreshold);
    
    for (auto* conv : m_conversations) {
        if (!conv->isArchived() && conv->updatedAt() < threshold) {
            conv->archive();
            saveConversation(conv);
            emit conversationArchived(conv->id());
        }
    }
}

void ConversationManager::clearAllConversations()
{
    qDeleteAll(m_conversations);
    m_conversations.clear();
    m_currentConversation = nullptr;
    
    QFile::remove(storageFilePath());
    
    emit currentConversationChanged(nullptr);
    emit conversationsLoaded();
}

int ConversationManager::activeConversationsCount() const
{
    int count = 0;
    for (auto* conv : m_conversations) {
        if (!conv->isArchived()) {
            count++;
        }
    }
    return count;
}

qint64 ConversationManager::totalTokensUsed() const
{
    qint64 total = 0;
    for (auto* conv : m_conversations) {
        for (const auto& msg : conv->messages()) {
            total += msg.tokenCount;
        }
    }
    return total;
}

} // namespace brahma
