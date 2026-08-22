#include "knowledge/KnowledgeGraph.h"
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace aios {

KnowledgeGraph& KnowledgeGraph::instance() {
    static KnowledgeGraph instance;
    return instance;
}

bool KnowledgeGraph::addNode(const KnowledgeNode& node) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_[node.id] = node;
    return true;
}

std::optional<KnowledgeNode> KnowledgeGraph::getNode(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(id);
    if (it != nodes_.end()) return it->second;
    return std::nullopt;
}

bool KnowledgeGraph::removeNode(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.erase(id);
    outgoing_edges_.erase(id);
    incoming_edges_.erase(id);

    // Clean up references in other edges
    for (auto& [_, edges] : outgoing_edges_) {
        edges.erase(std::remove_if(edges.begin(), edges.end(), 
            [&](const KnowledgeEdge& e) { return e.to_id == id; }), edges.end());
    }
    for (auto& [_, edges] : incoming_edges_) {
        edges.erase(std::remove_if(edges.begin(), edges.end(), 
            [&](const KnowledgeEdge& e) { return e.from_id == id; }), edges.end());
    }
    return true;
}

std::vector<KnowledgeNode> KnowledgeGraph::getNodesByType(EntityType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<KnowledgeNode> res;
    for (const auto& [_, n] : nodes_) {
        if (n.type == type) {
            res.push_back(n);
        }
    }
    return res;
}

bool KnowledgeGraph::addEdge(const KnowledgeEdge& edge) {
    std::lock_guard<std::mutex> lock(mutex_);
    outgoing_edges_[edge.from_id].push_back(edge);
    incoming_edges_[edge.to_id].push_back(edge);
    return true;
}

bool KnowledgeGraph::addEdge(const std::string& from_id, const std::string& to_id, 
                            RelationType relation, float weight, 
                            const std::string& description) {
    KnowledgeEdge edge;
    edge.from_id = from_id;
    edge.to_id = to_id;
    edge.relation = relation;
    edge.weight = weight;
    edge.description = description;
    return addEdge(edge);
}

std::vector<KnowledgeEdge> KnowledgeGraph::getOutgoingEdges(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = outgoing_edges_.find(node_id);
    if (it != outgoing_edges_.end()) return it->second;
    return {};
}

std::vector<KnowledgeEdge> KnowledgeGraph::getIncomingEdges(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = incoming_edges_.find(node_id);
    if (it != incoming_edges_.end()) return it->second;
    return {};
}

std::vector<KnowledgeNode> KnowledgeGraph::findRelatedEntities(const std::string& start_node_id, 
                                                             size_t max_depth,
                                                             std::optional<RelationType> relation_filter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<KnowledgeNode> results;
    std::unordered_set<std::string> visited;

    std::queue<std::pair<std::string, size_t>> q;
    q.push({start_node_id, 0});
    visited.insert(start_node_id);

    while (!q.empty()) {
        auto [current_id, depth] = q.front();
        q.pop();

        if (depth >= max_depth) continue;

        // Check outgoing edges
        auto out_it = outgoing_edges_.find(current_id);
        if (out_it != outgoing_edges_.end()) {
            for (const auto& edge : out_it->second) {
                if (relation_filter.has_value() && edge.relation != *relation_filter) continue;

                if (visited.find(edge.to_id) == visited.end()) {
                    visited.insert(edge.to_id);
                    auto n_it = nodes_.find(edge.to_id);
                    if (n_it != nodes_.end()) {
                        results.push_back(n_it->second);
                    }
                    q.push({edge.to_id, depth + 1});
                }
            }
        }

        // Check incoming edges
        auto in_it = incoming_edges_.find(current_id);
        if (in_it != incoming_edges_.end()) {
            for (const auto& edge : in_it->second) {
                if (relation_filter.has_value() && edge.relation != *relation_filter) continue;

                if (visited.find(edge.from_id) == visited.end()) {
                    visited.insert(edge.from_id);
                    auto n_it = nodes_.find(edge.from_id);
                    if (n_it != nodes_.end()) {
                        results.push_back(n_it->second);
                    }
                    q.push({edge.from_id, depth + 1});
                }
            }
        }
    }

    return results;
}

std::vector<KnowledgeNode> KnowledgeGraph::findFixForError(const std::string& error_text) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<KnowledgeNode> matches;

    std::string lower_err = error_text;
    std::transform(lower_err.begin(), lower_err.end(), lower_err.begin(), ::tolower);

    for (const auto& [_, node] : nodes_) {
        if (node.type == EntityType::BugFix) {
            std::string lower_desc = node.description;
            std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);

            std::string lower_name = node.name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

            if (lower_err.find(lower_name) != std::string::npos || lower_desc.find(lower_err) != std::string::npos ||
                lower_err.find(lower_desc) != std::string::npos) {
                matches.push_back(node);
            }
        }
    }

    return matches;
}

std::vector<KnowledgeNode> KnowledgeGraph::getUserPreferences() const {
    return getNodesByType(EntityType::UserPreference);
}

std::vector<KnowledgeNode> KnowledgeGraph::getArchitectureDecisions() const {
    return getNodesByType(EntityType::ArchitectureDecision);
}

std::string KnowledgeGraph::exportToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;

    j["nodes"] = nlohmann::json::array();
    for (const auto& [_, node] : nodes_) {
        nlohmann::json nj;
        nj["id"] = node.id;
        nj["name"] = node.name;
        nj["type"] = static_cast<int>(node.type);
        nj["description"] = node.description;
        nj["properties"] = node.properties;
        nj["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(node.created_at.time_since_epoch()).count();
        j["nodes"].push_back(nj);
    }

    j["edges"] = nlohmann::json::array();
    for (const auto& [_, edge_list] : outgoing_edges_) {
        for (const auto& e : edge_list) {
            nlohmann::json ej;
            ej["from_id"] = e.from_id;
            ej["to_id"] = e.to_id;
            ej["relation"] = static_cast<int>(e.relation);
            ej["weight"] = e.weight;
            ej["description"] = e.description;
            j["edges"].push_back(ej);
        }
    }

    return j.dump(2);
}

bool KnowledgeGraph::importFromJson(const std::string& json_str) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto j = nlohmann::json::parse(json_str);
        nodes_.clear();
        outgoing_edges_.clear();
        incoming_edges_.clear();

        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& nj : j["nodes"]) {
                KnowledgeNode n;
                n.id = nj.value("id", "");
                n.name = nj.value("name", "");
                n.type = static_cast<EntityType>(nj.value("type", 0));
                n.description = nj.value("description", "");
                if (nj.contains("properties")) n.properties = nj["properties"].get<std::unordered_map<std::string, std::string>>();
                n.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(nj.value("created_at", 0LL)));
                nodes_[n.id] = n;
            }
        }

        if (j.contains("edges") && j["edges"].is_array()) {
            for (const auto& ej : j["edges"]) {
                KnowledgeEdge e;
                e.from_id = ej.value("from_id", "");
                e.to_id = ej.value("to_id", "");
                e.relation = static_cast<RelationType>(ej.value("relation", 0));
                e.weight = ej.value("weight", 1.0f);
                e.description = ej.value("description", "");
                outgoing_edges_[e.from_id].push_back(e);
                incoming_edges_[e.to_id].push_back(e);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

void KnowledgeGraph::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.clear();
    outgoing_edges_.clear();
    incoming_edges_.clear();
}

size_t KnowledgeGraph::nodeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

size_t KnowledgeGraph::edgeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, list] : outgoing_edges_) {
        count += list.size();
    }
    return count;
}

} // namespace aios
