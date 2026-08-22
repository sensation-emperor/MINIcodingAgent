// AIOS - MINI Coding Agent Operating System
// Memory Manager Implementation

#include "memory/memory.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <random>
#include <cstring>
#include <set>
#include <list>

namespace aios {

using json = nlohmann::json;

struct MemoryManager::Impl {
    std::unordered_map<std::string, MemoryEntry> entries;
    std::string persistence_path;
    size_t memory_limit_bytes = 100 * 1024 * 1024;  // 100MB default
    size_t current_memory_usage = 0;
    bool running = false;
    
    // Statistics
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    
    // LRU tracking
    std::list<std::string> access_order;
    std::unordered_map<std::string, std::list<std::string>::iterator> access_iterators;
    
    void updateAccessOrder(const std::string& key) {
        auto it = access_iterators.find(key);
        if (it != access_iterators.end()) {
            access_order.erase(it->second);
            access_iterators.erase(it);
        }
        access_order.push_front(key);
        access_iterators[key] = access_order.begin();
    }
    
    void removeAccessOrder(const std::string& key) {
        auto it = access_iterators.find(key);
        if (it != access_iterators.end()) {
            access_order.erase(it->second);
            access_iterators.erase(it);
        }
    }
};

MemoryManager& MemoryManager::instance() {
    static MemoryManager instance;
    return instance;
}

MemoryManager::MemoryManager() 
    : impl_(std::make_unique<Impl>()),
      vector_store_(std::make_shared<VectorStore>()),
      knowledge_graph_(std::make_shared<KnowledgeGraph>()),
      database_engine_(std::make_shared<DatabaseEngine>()) {}

MemoryManager::~MemoryManager() {
    stop();
}

bool MemoryManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (impl_->running) {
        LOG_WARN("MemoryManager already initialized");
        return true;
    }
    
    LOG_INFO("Initializing MemoryManager...");
    
    if (database_engine_) {
        database_engine_->initialize();
    }

    // Try to load persisted memory if path is set
    if (!impl_->persistence_path.empty()) {
        load();
    }
    
    impl_->running = true;
    LOG_INFO("MemoryManager initialized successfully");
    return true;
}

void MemoryManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!impl_->running) {
        return;
    }
    
    LOG_INFO("Shutting down MemoryManager...");
    
    if (!impl_->persistence_path.empty()) {
        persist();
    }

    if (database_engine_) {
        database_engine_->shutdown();
    }
    
    impl_->entries.clear();
    impl_->access_order.clear();
    impl_->access_iterators.clear();
    impl_->current_memory_usage = 0;
    impl_->running = false;
    
    LOG_INFO("MemoryManager shutdown complete");
}

void MemoryManager::stop() {
    shutdown();
}

bool MemoryManager::store(const std::string& key, const std::string& value, 
                         const std::string& category,
                         const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    size_t entry_size = key.length() + value.length() + category.length();
    for (const auto& tag : tags) {
        entry_size += tag.length();
    }
    
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        impl_->current_memory_usage -= it->second.size_bytes;
        it->second.value = value;
        it->second.category = category;
        it->second.tags = tags;
        it->second.accessed_at = now;
        it->second.size_bytes = entry_size;
        it->second.checksum = computeChecksum(value);
        impl_->current_memory_usage += entry_size;
        impl_->updateAccessOrder(key);
    } else {
        MemoryEntry entry;
        entry.id = generateId();
        entry.key = key;
        entry.value = value;
        entry.category = category;
        entry.created_at = now;
        entry.accessed_at = now;
        entry.access_count = 1;
        entry.size_bytes = entry_size;
        entry.tags = tags;
        entry.checksum = computeChecksum(value);
        
        impl_->entries[key] = entry;
        impl_->current_memory_usage += entry_size;
        impl_->updateAccessOrder(key);
    }
    
    evictIfNeeded();

    // Persist to database engine
    if (database_engine_) {
        DbMemoryRecord drec;
        drec.id = impl_->entries[key].id;
        drec.key = key;
        drec.value = value;
        drec.category = category;
        drec.tags = tags;
        drec.created_at = now;
        drec.accessed_at = now;
        database_engine_->saveMemory(drec);
    }

    return true;
}

bool MemoryManager::storeSemantic(const std::string& key, const std::string& value,
                                 const std::string& category,
                                 const std::vector<std::string>& tags) {
    store(key, value, category, tags);

    if (vector_store_) {
        std::unordered_map<std::string, std::string> meta;
        meta["key"] = key;
        meta["category"] = category;
        vector_store_->addDocument(key, value, meta);
    }
    return true;
}

std::vector<VectorSearchResult> MemoryManager::searchSemantic(const std::string& query, 
                                                              size_t top_k, 
                                                              float min_similarity) const {
    if (!vector_store_) return {};
    return vector_store_->search(query, top_k, min_similarity);
}

bool MemoryManager::addKnowledgeNode(const KnowledgeNode& node) {
    if (!knowledge_graph_) return false;
    return knowledge_graph_->addNode(node);
}

bool MemoryManager::addKnowledgeEdge(const std::string& from_id, const std::string& to_id, 
                                    RelationType relation, float weight,
                                    const std::string& description) {
    if (!knowledge_graph_) return false;
    return knowledge_graph_->addEdge(from_id, to_id, relation, weight, description);
}

std::vector<KnowledgeNode> MemoryManager::queryRelatedKnowledge(const std::string& start_node_id, size_t max_depth) const {
    if (!knowledge_graph_) return {};
    return knowledge_graph_->findRelatedEntities(start_node_id, max_depth);
}

std::vector<KnowledgeNode> MemoryManager::findBugFix(const std::string& error_text) const {
    if (!knowledge_graph_) return {};
    return knowledge_graph_->findFixForError(error_text);
}

std::optional<std::string> MemoryManager::retrieve(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        impl_->cache_hits++;
        updateAccessTime(it->second);
        impl_->updateAccessOrder(key);
        return it->second.value;
    }

    impl_->cache_misses++;
    return std::nullopt;
}

std::optional<std::string> MemoryManager::retrieve(const std::string& key, const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end() && it->second.category == category) {
        impl_->cache_hits++;
        updateAccessTime(it->second);
        impl_->updateAccessOrder(key);
        return it->second.value;
    }
    
    impl_->cache_misses++;
    return std::nullopt;
}

bool MemoryManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        impl_->current_memory_usage -= it->second.size_bytes;
        impl_->removeAccessOrder(key);
        impl_->entries.erase(it);
        if (vector_store_) vector_store_->removeDocument(key);
        if (database_engine_) database_engine_->deleteMemory(key);
        return true;
    }
    
    return false;
}

bool MemoryManager::clearCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys_to_remove;
    for (const auto& pair : impl_->entries) {
        if (pair.second.category == category) {
            keys_to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& key : keys_to_remove) {
        auto it = impl_->entries.find(key);
        if (it != impl_->entries.end()) {
            impl_->current_memory_usage -= it->second.size_bytes;
            impl_->removeAccessOrder(key);
            impl_->entries.erase(it);
        }
    }
    
    if (database_engine_) database_engine_->clearCategory(category);
    return true;
}

std::vector<MemoryEntry> MemoryManager::search(const std::string& pattern, 
                                               const std::string& category,
                                               size_t max_results) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> results;
    
    for (const auto& pair : impl_->entries) {
        if (!category.empty() && pair.second.category != category) {
            continue;
        }
        
        if (pattern.empty() || 
            pair.first.find(pattern) != std::string::npos ||
            pair.second.value.find(pattern) != std::string::npos) {
            results.push_back(pair.second);
            if (results.size() >= max_results) {
                break;
            }
        }
    }
    
    return results;
}

std::vector<MemoryEntry> MemoryManager::searchByTags(const std::vector<std::string>& tags,
                                                     size_t max_results) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> results;
    
    for (const auto& pair : impl_->entries) {
        bool all_tags_match = true;
        for (const auto& tag : tags) {
            if (std::find(pair.second.tags.begin(), pair.second.tags.end(), tag) == pair.second.tags.end()) {
                all_tags_match = false;
                break;
            }
        }
        
        if (all_tags_match) {
            results.push_back(pair.second);
            if (results.size() >= max_results) {
                break;
            }
        }
    }
    
    return results;
}

MemoryStats MemoryManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    MemoryStats stats;
    stats.total_entries = impl_->entries.size();
    stats.total_size_bytes = impl_->current_memory_usage;
    stats.cache_hits = impl_->cache_hits;
    stats.cache_misses = impl_->cache_misses;
    
    for (const auto& pair : impl_->entries) {
        if (pair.second.category == "conversation") stats.conversation_memory_entries++;
        else if (pair.second.category == "session") stats.session_memory_entries++;
        else if (pair.second.category == "repository") stats.repository_memory_entries++;
        else if (pair.second.category == "long_term") stats.long_term_memory_entries++;
    }
    
    return stats;
}

void MemoryManager::setMemoryLimit(size_t limit_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->memory_limit_bytes = limit_bytes;
    evictIfNeeded();
}

size_t MemoryManager::getMemoryUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->current_memory_usage;
}

void MemoryManager::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> keys_to_remove;
    
    for (const auto& pair : impl_->entries) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - pair.second.accessed_at).count();
        if (age > 24 && pair.second.access_count < 2 && pair.second.category == "session") {
            keys_to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& key : keys_to_remove) {
        auto it = impl_->entries.find(key);
        if (it != impl_->entries.end()) {
            impl_->current_memory_usage -= it->second.size_bytes;
            impl_->removeAccessOrder(key);
            impl_->entries.erase(it);
        }
    }
}

std::string MemoryManager::exportToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j;
    j["entries"] = json::array();
    
    for (const auto& pair : impl_->entries) {
        json entry_json;
        entry_json["id"] = pair.second.id;
        entry_json["key"] = pair.second.key;
        entry_json["value"] = pair.second.value;
        entry_json["category"] = pair.second.category;
        entry_json["access_count"] = pair.second.access_count;
        entry_json["size_bytes"] = pair.second.size_bytes;
        entry_json["tags"] = pair.second.tags;
        entry_json["checksum"] = pair.second.checksum;
        j["entries"].push_back(entry_json);
    }
    
    return j.dump(2);
}

bool MemoryManager::importFromJson(const std::string& json_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto j = json::parse(json_data);
        if (!j.contains("entries") || !j["entries"].is_array()) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        for (const auto& entry_json : j["entries"]) {
            MemoryEntry entry;
            entry.id = entry_json.value("id", generateId());
            entry.key = entry_json.value("key", "");
            entry.value = entry_json.value("value", "");
            entry.category = entry_json.value("category", "session");
            entry.access_count = entry_json.value("access_count", 1);
            entry.size_bytes = entry_json.value("size_bytes", entry.key.length() + entry.value.length());
            entry.created_at = now;
            entry.accessed_at = now;
            entry.checksum = entry_json.value("checksum", computeChecksum(entry.value));
            
            if (entry_json.contains("tags") && entry_json["tags"].is_array()) {
                for (const auto& tag : entry_json["tags"]) {
                    entry.tags.push_back(tag.get<std::string>());
                }
            }
            
            if (!entry.key.empty()) {
                impl_->entries[entry.key] = entry;
                impl_->current_memory_usage += entry.size_bytes;
                impl_->updateAccessOrder(entry.key);
            }
        }
        
        evictIfNeeded();
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<MemoryEntry> MemoryManager::getRecent(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> result;
    
    for (const auto& key : impl_->access_order) {
        auto it = impl_->entries.find(key);
        if (it != impl_->entries.end()) {
            result.push_back(it->second);
            if (result.size() >= count) {
                break;
            }
        }
    }
    
    return result;
}

std::vector<MemoryEntry> MemoryManager::getFrequentlyAccessed(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> result;
    
    for (const auto& pair : impl_->entries) {
        result.push_back(pair.second);
    }
    
    std::sort(result.begin(), result.end(), 
              [](const MemoryEntry& a, const MemoryEntry& b) {
                  return a.access_count > b.access_count;
              });
    
    if (result.size() > count) {
        result.resize(count);
    }
    
    return result;
}

void MemoryManager::touch(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        updateAccessTime(it->second);
        impl_->updateAccessOrder(key);
    }
}

bool MemoryManager::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->entries.find(key) != impl_->entries.end();
}

std::vector<std::string> MemoryManager::getKeys(const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    for (const auto& pair : impl_->entries) {
        if (category.empty() || pair.second.category == category) {
            keys.push_back(pair.first);
        }
    }
    return keys;
}

void MemoryManager::setPersistencePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->persistence_path = path;
}

bool MemoryManager::persist() {
    if (impl_->persistence_path.empty()) return false;
    try {
        std::ofstream file(impl_->persistence_path);
        if (file.is_open()) {
            file << exportToJson();
            return true;
        }
    } catch (...) {}
    return false;
}

bool MemoryManager::load() {
    if (impl_->persistence_path.empty()) return false;
    try {
        std::ifstream file(impl_->persistence_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return importFromJson(buffer.str());
        }
    } catch (...) {}
    return false;
}

void MemoryManager::evictIfNeeded() {
    while (impl_->current_memory_usage > impl_->memory_limit_bytes && !impl_->access_order.empty()) {
        std::string lru_key = impl_->access_order.back();
        auto it = impl_->entries.find(lru_key);
        if (it != impl_->entries.end()) {
            impl_->current_memory_usage -= it->second.size_bytes;
            impl_->entries.erase(it);
        }
        impl_->access_order.pop_back();
        impl_->access_iterators.erase(lru_key);
    }
}

std::string MemoryManager::generateId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    return "mem_" + std::to_string(dis(gen));
}

std::string MemoryManager::computeChecksum(const std::string& data) const {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : data) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return std::to_string(hash);
}

void MemoryManager::updateAccessTime(MemoryEntry& entry) {
    entry.accessed_at = std::chrono::system_clock::now();
    entry.access_count++;
}

} // namespace aios
