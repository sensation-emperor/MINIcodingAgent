#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace aios {

struct DbConversation {
    std::string id;
    std::string title;
    std::chrono::system_clock::time_point created_at;
    std::unordered_map<std::string, std::string> metadata;
};

struct DbMessage {
    std::string id;
    std::string conversation_id;
    std::string role; // "system", "user", "assistant", "tool"
    std::string content;
    size_t tokens = 0;
    std::chrono::system_clock::time_point created_at;
};

struct DbMemoryRecord {
    std::string id;
    std::string key;
    std::string value;
    std::string category; // "short_term", "long_term", "entity", "user_preference"
    std::vector<std::string> tags;
    size_t access_count = 0;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point accessed_at;
    std::vector<float> embedding;
};

class DatabaseEngine {
public:
    static DatabaseEngine& instance();

    explicit DatabaseEngine(const std::string& db_path = "aios_memory.db");
    ~DatabaseEngine();

    bool initialize();
    void shutdown();
    void flush();

    // Conversations
    bool saveConversation(const DbConversation& conv);
    std::optional<DbConversation> getConversation(const std::string& id) const;
    std::vector<DbConversation> listConversations() const;
    bool deleteConversation(const std::string& id);

    // Messages
    bool saveMessage(const DbMessage& msg);
    std::vector<DbMessage> getMessages(const std::string& conversation_id) const;
    bool deleteMessages(const std::string& conversation_id);

    // Memory Records
    bool saveMemory(const DbMemoryRecord& record);
    std::optional<DbMemoryRecord> getMemory(const std::string& key, const std::string& category = "") const;
    std::vector<DbMemoryRecord> getMemoriesByCategory(const std::string& category) const;
    std::vector<DbMemoryRecord> getAllMemories() const;
    bool deleteMemory(const std::string& key);
    bool clearCategory(const std::string& category);

    // Raw JSON export/import
    std::string exportToJson() const;
    bool importFromJson(const std::string& json_str);

private:
    void loadFromDisk();
    void saveToDisk();

    mutable std::mutex mutex_;
    std::string db_path_;
    bool initialized_ = false;

    std::unordered_map<std::string, DbConversation> conversations_;
    std::unordered_map<std::string, std::vector<DbMessage>> messages_by_conv_;
    std::unordered_map<std::string, DbMemoryRecord> memories_;
};

} // namespace aios
