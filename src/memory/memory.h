// AIOS - MINI Coding Agent Operating System
// Memory Manager - Manages short-term, long-term, vector, and knowledge graph memory

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>
#include "database/DatabaseEngine.h"
#include "vector/VectorStore.h"
#include "knowledge/KnowledgeGraph.h"

namespace aios {

struct MemoryEntry {
    std::string id;
    std::string key;
    std::string value;
    std::string category;  // "conversation", "session", "repository", "long_term"
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point accessed_at;
    size_t access_count = 0;
    size_t size_bytes = 0;
    std::vector<std::string> tags;
    std::string checksum;
};

struct MemoryStats {
    size_t total_entries = 0;
    size_t total_size_bytes = 0;
    size_t conversation_memory_entries = 0;
    size_t session_memory_entries = 0;
    size_t repository_memory_entries = 0;
    size_t long_term_memory_entries = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
};

class MemoryManager {
public:
    static MemoryManager& instance();

    MemoryManager();
    ~MemoryManager();
    
    // Initialize memory subsystem
    bool initialize();
    void shutdown();
    void stop();
    
    // Store a memory entry
    bool store(const std::string& key, const std::string& value, 
               const std::string& category = "session",
               const std::vector<std::string>& tags = {});
    
    // Semantic Vector Storage & Retrieval
    bool storeSemantic(const std::string& key, const std::string& value,
                       const std::string& category = "long_term",
                       const std::vector<std::string>& tags = {});

    std::vector<VectorSearchResult> searchSemantic(const std::string& query, 
                                                  size_t top_k = 5, 
                                                  float min_similarity = 0.0f) const;

    // Knowledge Graph Integration
    bool addKnowledgeNode(const KnowledgeNode& node);
    bool addKnowledgeEdge(const std::string& from_id, const std::string& to_id, 
                         RelationType relation, float weight = 1.0f,
                         const std::string& description = "");
    std::vector<KnowledgeNode> queryRelatedKnowledge(const std::string& start_node_id, size_t max_depth = 2) const;
    std::vector<KnowledgeNode> findBugFix(const std::string& error_text) const;

    // Retrieve a memory entry
    std::optional<std::string> retrieve(const std::string& key);
    std::optional<std::string> retrieve(const std::string& key, const std::string& category);
    
    // Delete a memory entry
    bool remove(const std::string& key);
    bool clearCategory(const std::string& category);
    
    // Search memories by pattern or tags
    std::vector<MemoryEntry> search(const std::string& pattern, 
                                    const std::string& category = "",
                                    size_t max_results = 100);
    
    std::vector<MemoryEntry> searchByTags(const std::vector<std::string>& tags,
                                          size_t max_results = 100);
    
    // Statistics & Limits
    MemoryStats getStats() const;
    void setMemoryLimit(size_t limit_bytes);
    size_t getMemoryUsage() const;
    void compact();
    
    // Serialization & Persistence
    std::string exportToJson() const;
    bool importFromJson(const std::string& json_data);
    std::vector<MemoryEntry> getRecent(size_t count = 10) const;
    std::vector<MemoryEntry> getFrequentlyAccessed(size_t count = 10) const;
    
    void touch(const std::string& key);
    bool contains(const std::string& key) const;
    std::vector<std::string> getKeys(const std::string& category = "") const;
    
    void setPersistencePath(const std::string& path);
    bool persist();
    bool load();

    // Access to Subsystems
    std::shared_ptr<VectorStore> getVectorStore() const { return vector_store_; }
    std::shared_ptr<KnowledgeGraph> getKnowledgeGraph() const { return knowledge_graph_; }
    std::shared_ptr<DatabaseEngine> getDatabaseEngine() const { return database_engine_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    
    std::shared_ptr<VectorStore> vector_store_;
    std::shared_ptr<KnowledgeGraph> knowledge_graph_;
    std::shared_ptr<DatabaseEngine> database_engine_;

    void evictIfNeeded();
    std::string generateId() const;
    std::string computeChecksum(const std::string& data) const;
    void updateAccessTime(MemoryEntry& entry);
};

} // namespace aios
