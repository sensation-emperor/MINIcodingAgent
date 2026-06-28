// AIOS - MINI Coding Agent Operating System
// Memory Manager - Manages short-term, long-term, and session memory

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>

namespace aios {

struct MemoryEntry {
    std::string id;
    std::string key;
    std::string value;
    std::string category;  // "conversation", "session", "repository", "long_term"
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point accessed_at;
    size_t access_count;
    size_t size_bytes;
    std::vector<std::string> tags;
    std::string checksum;
};

struct MemoryStats {
    size_t total_entries;
    size_t total_size_bytes;
    size_t conversation_memory_entries;
    size_t session_memory_entries;
    size_t repository_memory_entries;
    size_t long_term_memory_entries;
    size_t cache_hits;
    size_t cache_misses;
};

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();
    
    // Initialize memory subsystem
    bool initialize();
    
    // Shutdown and cleanup
    void shutdown();
    
    // Stop operations gracefully
    void stop();
    
    // Store a memory entry
    bool store(const std::string& key, const std::string& value, 
               const std::string& category = "session",
               const std::vector<std::string>& tags = {});
    
    // Retrieve a memory entry
    std::optional<std::string> retrieve(const std::string& key);
    
    // Retrieve with category filter
    std::optional<std::string> retrieve(const std::string& key, const std::string& category);
    
    // Delete a memory entry
    bool remove(const std::string& key);
    
    // Delete all entries in a category
    bool clearCategory(const std::string& category);
    
    // Search memories by pattern
    std::vector<MemoryEntry> search(const std::string& pattern, 
                                    const std::string& category = "",
                                    size_t max_results = 100);
    
    // Search by tags
    std::vector<MemoryEntry> searchByTags(const std::vector<std::string>& tags,
                                          size_t max_results = 100);
    
    // Get memory statistics
    MemoryStats getStats() const;
    
    // Set memory limit (in bytes)
    void setMemoryLimit(size_t limit_bytes);
    
    // Get current memory usage
    size_t getMemoryUsage() const;
    
    // Compact memory (remove old/unused entries)
    void compact();
    
    // Export memory to JSON string
    std::string exportToJson() const;
    
    // Import memory from JSON string
    bool importFromJson(const std::string& json_data);
    
    // Get recent entries
    std::vector<MemoryEntry> getRecent(size_t count = 10) const;
    
    // Get frequently accessed entries
    std::vector<MemoryEntry> getFrequentlyAccessed(size_t count = 10) const;
    
    // Update access time for a key
    void touch(const std::string& key);
    
    // Check if key exists
    bool contains(const std::string& key) const;
    
    // Get all keys in category
    std::vector<std::string> getKeys(const std::string& category = "") const;
    
    // Set persistence path for long-term memory
    void setPersistencePath(const std::string& path);
    
    // Save to persistent storage
    bool persist();
    
    // Load from persistent storage
    bool load();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    
    // Internal methods
    void evictIfNeeded();
    std::string generateId() const;
    std::string computeChecksum(const std::string& data) const;
    void updateAccessTime(MemoryEntry& entry);
};

} // namespace aios
