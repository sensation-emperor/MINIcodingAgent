#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>

namespace aios {

enum class EntityType {
    Symbol,
    BugFix,
    ArchitectureDecision,
    UserPreference,
    ProjectTask
};

enum class RelationType {
    Calls,
    Implements,
    Fixes,
    DependsOn,
    Prefers,
    Violates,
    RelatedTo
};

struct KnowledgeNode {
    std::string id;
    std::string name;
    EntityType type = EntityType::Symbol;
    std::string description;
    std::unordered_map<std::string, std::string> properties;
    std::chrono::system_clock::time_point created_at;
};

struct KnowledgeEdge {
    std::string from_id;
    std::string to_id;
    RelationType relation = RelationType::RelatedTo;
    float weight = 1.0f;
    std::string description;
};

class KnowledgeGraph {
public:
    static KnowledgeGraph& instance();

    KnowledgeGraph() = default;
    ~KnowledgeGraph() = default;

    // Node operations
    bool addNode(const KnowledgeNode& node);
    std::optional<KnowledgeNode> getNode(const std::string& id) const;
    bool removeNode(const std::string& id);
    std::vector<KnowledgeNode> getNodesByType(EntityType type) const;

    // Edge operations
    bool addEdge(const KnowledgeEdge& edge);
    bool addEdge(const std::string& from_id, const std::string& to_id, 
                 RelationType relation, float weight = 1.0f, 
                 const std::string& description = "");
    std::vector<KnowledgeEdge> getOutgoingEdges(const std::string& node_id) const;
    std::vector<KnowledgeEdge> getIncomingEdges(const std::string& node_id) const;

    // Graph Traversals & Domain Queries
    std::vector<KnowledgeNode> findRelatedEntities(const std::string& start_node_id, 
                                                  size_t max_depth = 2,
                                                  std::optional<RelationType> relation_filter = std::nullopt) const;

    std::vector<KnowledgeNode> findFixForError(const std::string& error_text) const;
    std::vector<KnowledgeNode> getUserPreferences() const;
    std::vector<KnowledgeNode> getArchitectureDecisions() const;

    // Serialization
    std::string exportToJson() const;
    bool importFromJson(const std::string& json_str);
    void clear();
    size_t nodeCount() const;
    size_t edgeCount() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, KnowledgeNode> nodes_;
    std::unordered_map<std::string, std::vector<KnowledgeEdge>> outgoing_edges_;
    std::unordered_map<std::string, std::vector<KnowledgeEdge>> incoming_edges_;
};

} // namespace aios
