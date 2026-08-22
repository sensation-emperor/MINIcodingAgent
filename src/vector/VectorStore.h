#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <optional>
#include <functional>

namespace aios {

class ModelProvider;

struct VectorDocument {
    std::string id;
    std::string text;
    std::unordered_map<std::string, std::string> metadata;
    size_t matrix_index = 0;
};

struct VectorSearchResult {
    std::string id;
    std::string text;
    std::unordered_map<std::string, std::string> metadata;
    float similarity = 0.0f; // 0.0 to 1.0
};

/**
 * @brief High-performance SIMD-aligned Dense Vector Store
 * Features:
 * - Flat contiguous matrix buffer for vectorized dot products
 * - Zero-allocation std::string_view FNV-1a word & 3-gram feature projection
 * - Bounded O(N log K) min-heap selection
 * - Multi-reader shared_mutex concurrency
 */
class VectorStore {
public:
    static constexpr size_t DEFAULT_EMBEDDING_DIM = 128;

    VectorStore();
    ~VectorStore() = default;

    void setModelProvider(std::shared_ptr<ModelProvider> provider);

    // Vector operations
    bool addDocument(const std::string& id, const std::string& text, 
                    const std::unordered_map<std::string, std::string>& metadata = {},
                    const std::vector<float>& custom_embedding = {});

    std::vector<VectorSearchResult> search(std::string_view query, 
                                          size_t top_k = 5, 
                                          float min_similarity = 0.0f) const;

    std::vector<VectorSearchResult> searchByVector(const std::vector<float>& query_vec,
                                                  size_t top_k = 5,
                                                  float min_similarity = 0.0f) const;

    bool removeDocument(const std::string& id);
    std::optional<VectorDocument> getDocument(const std::string& id) const;
    void clear();
    size_t size() const;

    // Embedding generation & similarity helpers
    std::vector<float> generateEmbedding(std::string_view text) const;
    static float dotProduct(const float* a, const float* b, size_t dim = DEFAULT_EMBEDDING_DIM);
    static float dotProduct(const std::vector<float>& a, const std::vector<float>& b);
    static float cosineSimilarity(const float* a, const float* b, size_t dim = DEFAULT_EMBEDDING_DIM);
    static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    static void normalizeVector(float* vec, size_t dim = DEFAULT_EMBEDDING_DIM);
    static void normalizeVector(std::vector<float>& vec);
    static bool hasAVX2Support();

private:
    std::vector<float> computeDeterministicEmbedding(std::string_view text) const;

    mutable std::shared_mutex rw_mutex_;
    std::shared_ptr<ModelProvider> model_provider_;

    // Document metadata table
    std::unordered_map<std::string, VectorDocument> documents_;
    std::vector<std::string> doc_ids_; // maps matrix row index -> doc_id

    // Flat contiguous vector matrix (row i corresponds to doc_ids_[i])
    std::vector<float> matrix_;
};

} // namespace aios
