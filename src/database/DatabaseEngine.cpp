#include "database/DatabaseEngine.h"
#include "logging/Logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>

namespace aios {

namespace fs = std::filesystem;

DatabaseEngine& DatabaseEngine::instance() {
    static DatabaseEngine instance;
    return instance;
}

DatabaseEngine::DatabaseEngine(const std::string& db_path)
    : db_path_(db_path) {}

DatabaseEngine::~DatabaseEngine() {
    shutdown();
}

bool DatabaseEngine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    loadFromDisk();
    initialized_ = true;
    LOG_INFO("DatabaseEngine initialized at: {}", db_path_);
    return true;
}

void DatabaseEngine::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return;
    saveToDisk();
    initialized_ = false;
}

void DatabaseEngine::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    saveToDisk();
}

bool DatabaseEngine::saveConversation(const DbConversation& conv) {
    std::lock_guard<std::mutex> lock(mutex_);
    conversations_[conv.id] = conv;
    saveToDisk();
    return true;
}

std::optional<DbConversation> DatabaseEngine::getConversation(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conversations_.find(id);
    if (it != conversations_.end()) return it->second;
    return std::nullopt;
}

std::vector<DbConversation> DatabaseEngine::listConversations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DbConversation> res;
    for (const auto& [_, c] : conversations_) {
        res.push_back(c);
    }
    return res;
}

bool DatabaseEngine::deleteConversation(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    conversations_.erase(id);
    messages_by_conv_.erase(id);
    saveToDisk();
    return true;
}

bool DatabaseEngine::saveMessage(const DbMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_by_conv_[msg.conversation_id].push_back(msg);
    saveToDisk();
    return true;
}

std::vector<DbMessage> DatabaseEngine::getMessages(const std::string& conversation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_by_conv_.find(conversation_id);
    if (it != messages_by_conv_.end()) return it->second;
    return {};
}

bool DatabaseEngine::deleteMessages(const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_by_conv_.erase(conversation_id);
    saveToDisk();
    return true;
}

bool DatabaseEngine::saveMemory(const DbMemoryRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    memories_[record.key] = record;
    saveToDisk();
    return true;
}

std::optional<DbMemoryRecord> DatabaseEngine::getMemory(const std::string& key, const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = memories_.find(key);
    if (it != memories_.end()) {
        if (category.empty() || it->second.category == category) {
            return it->second;
        }
    }
    return std::nullopt;
}

std::vector<DbMemoryRecord> DatabaseEngine::getMemoriesByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DbMemoryRecord> res;
    for (const auto& [_, m] : memories_) {
        if (category.empty() || m.category == category) {
            res.push_back(m);
        }
    }
    return res;
}

std::vector<DbMemoryRecord> DatabaseEngine::getAllMemories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DbMemoryRecord> res;
    for (const auto& [_, m] : memories_) {
        res.push_back(m);
    }
    return res;
}

bool DatabaseEngine::deleteMemory(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t erased = memories_.erase(key);
    if (erased > 0) saveToDisk();
    return erased > 0;
}

bool DatabaseEngine::clearCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = memories_.begin(); it != memories_.end(); ) {
        if (it->second.category == category) {
            it = memories_.erase(it);
        } else {
            ++it;
        }
    }
    saveToDisk();
    return true;
}

std::string DatabaseEngine::exportToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;

    // Conversations
    j["conversations"] = nlohmann::json::array();
    for (const auto& [_, c] : conversations_) {
        nlohmann::json cj;
        cj["id"] = c.id;
        cj["title"] = c.title;
        cj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(c.created_at.time_since_epoch()).count();
        cj["metadata"] = c.metadata;
        j["conversations"].push_back(cj);
    }

    // Messages
    j["messages"] = nlohmann::json::object();
    for (const auto& [conv_id, msgs] : messages_by_conv_) {
        nlohmann::json marr = nlohmann::json::array();
        for (const auto& m : msgs) {
            nlohmann::json mj;
            mj["id"] = m.id;
            mj["role"] = m.role;
            mj["content"] = m.content;
            mj["tokens"] = m.tokens;
            mj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(m.created_at.time_since_epoch()).count();
            marr.push_back(mj);
        }
        j["messages"][conv_id] = marr;
    }

    // Memories
    j["memories"] = nlohmann::json::array();
    for (const auto& [_, m] : memories_) {
        nlohmann::json mj;
        mj["id"] = m.id;
        mj["key"] = m.key;
        mj["value"] = m.value;
        mj["category"] = m.category;
        mj["tags"] = m.tags;
        mj["access_count"] = m.access_count;
        mj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(m.created_at.time_since_epoch()).count();
        mj["accessed_at"] = std::chrono::duration_cast<std::chrono::seconds>(m.accessed_at.time_since_epoch()).count();
        mj["embedding"] = m.embedding;
        j["memories"].push_back(mj);
    }

    return j.dump(2);
}

bool DatabaseEngine::importFromJson(const std::string& json_str) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto j = nlohmann::json::parse(json_str);

        if (j.contains("conversations") && j["conversations"].is_array()) {
            conversations_.clear();
            for (const auto& cj : j["conversations"]) {
                DbConversation c;
                c.id = cj.value("id", "");
                c.title = cj.value("title", "");
                c.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(cj.value("created_at", 0LL)));
                if (cj.contains("metadata")) {
                    c.metadata = cj["metadata"].get<std::unordered_map<std::string, std::string>>();
                }
                conversations_[c.id] = c;
            }
        }

        if (j.contains("messages") && j["messages"].is_object()) {
            messages_by_conv_.clear();
            for (auto it = j["messages"].begin(); it != j["messages"].end(); ++it) {
                std::string conv_id = it.key();
                for (const auto& mj : it.value()) {
                    DbMessage m;
                    m.id = mj.value("id", "");
                    m.conversation_id = conv_id;
                    m.role = mj.value("role", "user");
                    m.content = mj.value("content", "");
                    m.tokens = mj.value("tokens", 0ULL);
                    m.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(mj.value("created_at", 0LL)));
                    messages_by_conv_[conv_id].push_back(m);
                }
            }
        }

        if (j.contains("memories") && j["memories"].is_array()) {
            memories_.clear();
            for (const auto& mj : j["memories"]) {
                DbMemoryRecord m;
                m.id = mj.value("id", "");
                m.key = mj.value("key", "");
                m.value = mj.value("value", "");
                m.category = mj.value("category", "session");
                if (mj.contains("tags")) m.tags = mj["tags"].get<std::vector<std::string>>();
                m.access_count = mj.value("access_count", 0ULL);
                m.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(mj.value("created_at", 0LL)));
                m.accessed_at = std::chrono::system_clock::time_point(std::chrono::seconds(mj.value("accessed_at", 0LL)));
                if (mj.contains("embedding")) m.embedding = mj["embedding"].get<std::vector<float>>();
                memories_[m.key] = m;
            }
        }

        saveToDisk();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Database import failed: {}", e.what());
        return false;
    }
}

void DatabaseEngine::loadFromDisk() {
    if (!fs::exists(db_path_)) return;

    std::ifstream file(db_path_);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();
        if (!json_str.empty()) {
            try {
                auto j = nlohmann::json::parse(json_str);
                // parse without locking again
                if (j.contains("conversations") && j["conversations"].is_array()) {
                    for (const auto& cj : j["conversations"]) {
                        DbConversation c;
                        c.id = cj.value("id", "");
                        c.title = cj.value("title", "");
                        c.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(cj.value("created_at", 0LL)));
                        if (cj.contains("metadata")) c.metadata = cj["metadata"].get<std::unordered_map<std::string, std::string>>();
                        conversations_[c.id] = c;
                    }
                }
                if (j.contains("messages") && j["messages"].is_object()) {
                    for (auto it = j["messages"].begin(); it != j["messages"].end(); ++it) {
                        std::string conv_id = it.key();
                        for (const auto& mj : it.value()) {
                            DbMessage m;
                            m.id = mj.value("id", "");
                            m.conversation_id = conv_id;
                            m.role = mj.value("role", "user");
                            m.content = mj.value("content", "");
                            m.tokens = mj.value("tokens", 0ULL);
                            m.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(mj.value("created_at", 0LL)));
                            messages_by_conv_[conv_id].push_back(m);
                        }
                    }
                }
                if (j.contains("memories") && j["memories"].is_array()) {
                    for (const auto& mj : j["memories"]) {
                        DbMemoryRecord m;
                        m.id = mj.value("id", "");
                        m.key = mj.value("key", "");
                        m.value = mj.value("value", "");
                        m.category = mj.value("category", "session");
                        if (mj.contains("tags")) m.tags = mj["tags"].get<std::vector<std::string>>();
                        m.access_count = mj.value("access_count", 0ULL);
                        m.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(mj.value("created_at", 0LL)));
                        m.accessed_at = std::chrono::system_clock::time_point(std::chrono::seconds(mj.value("accessed_at", 0LL)));
                        if (mj.contains("embedding")) m.embedding = mj["embedding"].get<std::vector<float>>();
                        memories_[m.key] = m;
                    }
                }
            } catch (...) {}
        }
    }
}

void DatabaseEngine::saveToDisk() {
    try {
        std::string tmp_path = db_path_ + ".tmp";
        std::ofstream file(tmp_path);
        if (file.is_open()) {
            nlohmann::json j;
            j["conversations"] = nlohmann::json::array();
            for (const auto& [_, c] : conversations_) {
                nlohmann::json cj;
                cj["id"] = c.id;
                cj["title"] = c.title;
                cj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(c.created_at.time_since_epoch()).count();
                cj["metadata"] = c.metadata;
                j["conversations"].push_back(cj);
            }
            j["messages"] = nlohmann::json::object();
            for (const auto& [conv_id, msgs] : messages_by_conv_) {
                nlohmann::json marr = nlohmann::json::array();
                for (const auto& m : msgs) {
                    nlohmann::json mj;
                    mj["id"] = m.id;
                    mj["role"] = m.role;
                    mj["content"] = m.content;
                    mj["tokens"] = m.tokens;
                    mj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(m.created_at.time_since_epoch()).count();
                    marr.push_back(mj);
                }
                j["messages"][conv_id] = marr;
            }
            j["memories"] = nlohmann::json::array();
            for (const auto& [_, m] : memories_) {
                nlohmann::json mj;
                mj["id"] = m.id;
                mj["key"] = m.key;
                mj["value"] = m.value;
                mj["category"] = m.category;
                mj["tags"] = m.tags;
                mj["access_count"] = m.access_count;
                mj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(m.created_at.time_since_epoch()).count();
                mj["accessed_at"] = std::chrono::duration_cast<std::chrono::seconds>(m.accessed_at.time_since_epoch()).count();
                mj["embedding"] = m.embedding;
                j["memories"].push_back(mj);
            }
            file << j.dump(2);
            file.close();
            fs::rename(tmp_path, db_path_);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Database saveToDisk failed: {}", e.what());
    }
}

} // namespace aios
