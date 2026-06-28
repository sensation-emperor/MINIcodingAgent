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
        access_iterators[key] = std::prev(access_order.end());
    }
    
    void removeAccessOrder(const std::string& key) {
        auto it = access_iterators.find(key);
        if (it != access_iterators.end()) {
            access_order.erase(it->second);
            access_iterators.erase(it);
        }
    }
};

MemoryManager::MemoryManager() : impl_(std::make_unique<Impl>()) {}

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
    
    // Persist before shutdown if path is set
    if (!impl_->persistence_path.empty()) {
        persist();
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
    
    if (!impl_->running) {
        LOG_ERROR("MemoryManager not initialized");
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    
    MemoryEntry entry;
    entry.id = generateId();
    entry.key = key;
    entry.value = value;
    entry.category = category;
    entry.created_at = now;
    entry.accessed_at = now;
    entry.access_count = 1;
    entry.size_bytes = value.size();
    entry.tags = tags;
    entry.checksum = computeChecksum(value);
    
    // Check if key already exists and remove old entry
    auto existing = impl_->entries.find(key);
    if (existing != impl_->entries.end()) {
        impl_->current_memory_usage -= existing->second.size_bytes;
        impl_->removeAccessOrder(key);
    }
    
    impl_->entries[key] = std::move(entry);
    impl_->updateAccessOrder(key);
    impl_->current_memory_usage += value.size();
    
    evictIfNeeded();
    
    LOG_DEBUG("Stored memory entry: key={}, category={}, size={}", 
              key, category, value.size());
    
    return true;
}

std::optional<std::string> MemoryManager::retrieve(const std::string& key) {
    return retrieve(key, "");
}

std::optional<std::string> MemoryManager::retrieve(const std::string& key, 
                                                    const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!impl_->running) {
        LOG_ERROR("MemoryManager not initialized");
        return std::nullopt;
    }
    
    auto it = impl_->entries.find(key);
    if (it == impl_->entries.end()) {
        impl_->cache_misses++;
        LOG_DEBUG("Memory miss: key={}", key);
        return std::nullopt;
    }
    
    if (!category.empty() && it->second.category != category) {
        impl_->cache_misses++;
        LOG_DEBUG("Memory category mismatch: key={}, expected={}, actual={}",
                  key, category, it->second.category);
        return std::nullopt;
    }
    
    impl_->cache_hits++;
    impl_->updateAccessOrder(key);
    updateAccessTime(it->second);
    
    LOG_DEBUG("Memory hit: key={}, category={}", key, it->second.category);
    return it->second.value;
}

bool MemoryManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!impl_->running) {
        return false;
    }
    
    auto it = impl_->entries.find(key);
    if (it == impl_->entries.end()) {
        return false;
    }
    
    impl_->current_memory_usage -= it->second.size_bytes;
    impl_->removeAccessOrder(key);
    impl_->entries.erase(it);
    
    LOG_DEBUG("Removed memory entry: key={}", key);
    return true;
}

bool MemoryManager::clearCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!impl_->running) {
        return false;
    }
    
    std::vector<std::string> keys_to_remove;
    for (const auto& [key, entry] : impl_->entries) {
        if (entry.category == category) {
            keys_to_remove.push_back(key);
        }
    }
    
    for (const auto& key : keys_to_remove) {
        impl_->removeAccessOrder(key);
        impl_->entries.erase(key);
    }
    
    // Recalculate memory usage
    impl_->current_memory_usage = 0;
    for (const auto& [key, entry] : impl_->entries) {
        impl_->current_memory_usage += entry.size_bytes;
    }
    
    LOG_INFO("Cleared category '{}', removed {} entries", category, keys_to_remove.size());
    return true;
}

std::vector<MemoryEntry> MemoryManager::search(const std::string& pattern,
                                                const std::string& category,
                                                size_t max_results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MemoryEntry> results;
    
    if (!impl_->running) {
        return results;
    }
    
    for (const auto& [key, entry] : impl_->entries) {
        if (results.size() >= max_results) {
            break;
        }
        
        if (!category.empty() && entry.category != category) {
            continue;
        }
        
        // Search in key, value, and tags
        bool matches = false;
        if (entry.key.find(pattern) != std::string::npos) {
            matches = true;
        } else if (entry.value.find(pattern) != std::string::npos) {
            matches = true;
        } else {
            for (const auto& tag : entry.tags) {
                if (tag.find(pattern) != std::string::npos) {
                    matches = true;
                    break;
                }
            }
        }
        
        if (matches) {
            results.push_back(entry);
        }
    }
    
    // Sort by access time (most recent first)
    std::sort(results.begin(), results.end(),
              [](const MemoryEntry& a, const MemoryEntry& b) {
                  return a.accessed_at > b.accessed_at;
              });
    
    return results;
}

std::vector<MemoryEntry> MemoryManager::searchByTags(const std::vector<std::string>& tags,
                                                      size_t max_results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MemoryEntry> results;
    
    if (!impl_->running) {
        return results;
    }
    
    std::set<std::string> search_tags(tags.begin(), tags.end());
    
    for (const auto& [key, entry] : impl_->entries) {
        if (results.size() >= max_results) {
            break;
        }
        
        // Check if any search tag matches entry tags
        bool matches = false;
        for (const auto& entry_tag : entry.tags) {
            if (search_tags.count(entry_tag) > 0) {
                matches = true;
                break;
            }
        }
        
        if (matches) {
            results.push_back(entry);
        }
    }
    
    return results;
}

MemoryStats MemoryManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    MemoryStats stats{};
    stats.total_entries = impl_->entries.size();
    stats.total_size_bytes = impl_->current_memory_usage;
    stats.cache_hits = impl_->cache_hits;
    stats.cache_misses = impl_->cache_misses;
    
    for (const auto& [key, entry] : impl_->entries) {
        if (entry.category == "conversation") {
            stats.conversation_memory_entries++;
        } else if (entry.category == "session") {
            stats.session_memory_entries++;
        } else if (entry.category == "repository") {
            stats.repository_memory_entries++;
        } else if (entry.category == "long_term") {
            stats.long_term_memory_entries++;
        }
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
    
    if (!impl_->running) {
        return;
    }
    
    LOG_INFO("Compacting memory...");
    
    // Remove entries that haven't been accessed recently
    // Keep at least 50% of entries
    size_t min_keep = impl_->entries.size() / 2;
    size_t removed = 0;
    
    while (impl_->access_order.size() > min_keep && !impl_->access_order.empty()) {
        std::string oldest_key = impl_->access_order.back();
        impl_->removeAccessOrder(oldest_key);
        
        auto it = impl_->entries.find(oldest_key);
        if (it != impl_->entries.end()) {
            impl_->current_memory_usage -= it->second.size_bytes;
            impl_->entries.erase(it);
            removed++;
        }
    }
    
    LOG_INFO("Memory compaction complete, removed {} entries", removed);
}

std::string MemoryManager::exportToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j = json::array();
    
    for (const auto& [key, entry] : impl_->entries) {
        json entry_json;
        entry_json["id"] = entry.id;
        entry_json["key"] = entry.key;
        entry_json["value"] = entry.value;
        entry_json["category"] = entry.category;
        entry_json["created_at"] = std::chrono::system_clock::to_time_t(entry.created_at);
        entry_json["accessed_at"] = std::chrono::system_clock::to_time_t(entry.accessed_at);
        entry_json["access_count"] = entry.access_count;
        entry_json["size_bytes"] = entry.size_bytes;
        entry_json["tags"] = entry.tags;
        entry_json["checksum"] = entry.checksum;
        j.push_back(entry_json);
    }
    
    return j.dump(2);
}

bool MemoryManager::importFromJson(const std::string& json_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        json j = json::parse(json_data);
        
        if (!j.is_array()) {
            LOG_ERROR("Invalid JSON format for memory import");
            return false;
        }
        
        for (const auto& entry_json : j) {
            MemoryEntry entry;
            entry.id = entry_json.value("id", "");
            entry.key = entry_json.value("key", "");
            entry.value = entry_json.value("value", "");
            entry.category = entry_json.value("category", "session");
            
            auto created_ts = entry_json.value("created_at", 0);
            auto accessed_ts = entry_json.value("accessed_at", 0);
            entry.created_at = std::chrono::system_clock::from_time_t(created_ts);
            entry.accessed_at = std::chrono::system_clock::from_time_t(accessed_ts);
            
            entry.access_count = entry_json.value("access_count", 0);
            entry.size_bytes = entry_json.value("size_bytes", entry.value.size());
            entry.tags = entry_json.value("tags", std::vector<std::string>{});
            entry.checksum = entry_json.value("checksum", "");
            
            // Skip if key already exists
            if (impl_->entries.count(entry.key) > 0) {
                continue;
            }
            
            impl_->entries[entry.key] = std::move(entry);
            impl_->updateAccessOrder(entry.key);
            impl_->current_memory_usage += impl_->entries[entry.key].size_bytes;
        }
        
        LOG_INFO("Imported {} memory entries from JSON", j.size());
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to import memory from JSON: {}", e.what());
        return false;
    }
}

std::vector<MemoryEntry> MemoryManager::getRecent(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MemoryEntry> results;
    
    if (!impl_->running) {
        return results;
    }
    
    std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> sorted;
    for (const auto& [key, entry] : impl_->entries) {
        sorted.emplace_back(entry.accessed_at, key);
    }
    
    std::sort(sorted.begin(), sorted.end(), std::greater<>());
    
    for (size_t i = 0; i < std::min(count, sorted.size()); ++i) {
        auto it = impl_->entries.find(sorted[i].second);
        if (it != impl_->entries.end()) {
            results.push_back(it->second);
        }
    }
    
    return results;
}

std::vector<MemoryEntry> MemoryManager::getFrequentlyAccessed(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MemoryEntry> results;
    
    if (!impl_->running) {
        return results;
    }
    
    std::vector<std::pair<size_t, std::string>> sorted;
    for (const auto& [key, entry] : impl_->entries) {
        sorted.emplace_back(entry.access_count, key);
    }
    
    std::sort(sorted.begin(), sorted.end(), std::greater<>());
    
    for (size_t i = 0; i < std::min(count, sorted.size()); ++i) {
        auto it = impl_->entries.find(sorted[i].second);
        if (it != impl_->entries.end()) {
            results.push_back(it->second);
        }
    }
    
    return results;
}

void MemoryManager::touch(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!impl_->running) {
        return;
    }
    
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        impl_->updateAccessOrder(key);
        updateAccessTime(it->second);
    }
}

bool MemoryManager::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->entries.count(key) > 0;
}

std::vector<std::string> MemoryManager::getKeys(const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    
    for (const auto& [key, entry] : impl_->entries) {
        if (category.empty() || entry.category == category) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

void MemoryManager::setPersistencePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->persistence_path = path;
}

bool MemoryManager::persist() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (impl_->persistence_path.empty()) {
        LOG_ERROR("No persistence path set");
        return false;
    }
    
    try {
        std::ofstream file(impl_->persistence_path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open persistence file: {}", impl_->persistence_path);
            return false;
        }
        
        file << exportToJson();
        file.close();
        
        LOG_INFO("Memory persisted to {}", impl_->persistence_path);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to persist memory: {}", e.what());
        return false;
    }
}

bool MemoryManager::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (impl_->persistence_path.empty()) {
        LOG_ERROR("No persistence path set");
        return false;
    }
    
    std::ifstream file(impl_->persistence_path);
    if (!file.is_open()) {
        LOG_WARN("Persistence file not found: {}", impl_->persistence_path);
        return false;
    }
    
    try {
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        
        return importFromJson(content);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load memory: {}", e.what());
        return false;
    }
}

void MemoryManager::evictIfNeeded() {
    // Must be called with mutex held
    
    while (impl_->current_memory_usage > impl_->memory_limit_bytes && 
           !impl_->access_order.empty()) {
        std::string oldest_key = impl_->access_order.back();
        impl_->removeAccessOrder(oldest_key);
        
        auto it = impl_->entries.find(oldest_key);
        if (it != impl_->entries.end()) {
            impl_->current_memory_usage -= it->second.size_bytes;
            impl_->entries.erase(it);
            LOG_DEBUG("Evicted memory entry: key={}", oldest_key);
        }
    }
}

std::string MemoryManager::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    
    auto id = dist(gen);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << id;
    return ss.str();
}

std::string MemoryManager::computeChecksum(const std::string& data) const {
    // Simple hash-based checksum
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV-1a offset basis
    for (char c : data) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 0x100000001b3ULL;  // FNV-1a prime
    }
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

void MemoryManager::updateAccessTime(MemoryEntry& entry) {
    entry.accessed_at = std::chrono::system_clock::now();
    entry.access_count++;
}

} // namespace aios
