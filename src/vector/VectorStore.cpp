#include "vector/VectorStore.h"
#include "providers/ModelProvider.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <cstring>
#include <cctype>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#define AIOS_X86_SIMD 1
#include <immintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace aios {

namespace {

#if defined(AIOS_X86_SIMD)
bool checkAVX2FMA() {
#if defined(_MSC_VER)
    int info[4] = {0};
    __cpuid(info, 0);
    int nIds = info[0];
    if (nIds < 7) return false;

    __cpuid(info, 1);
    bool osxsave = (info[2] & (1 << 27)) != 0;
    bool fma = (info[2] & (1 << 12)) != 0;
    bool avx = (info[2] & (1 << 28)) != 0;
    if (!osxsave || !fma || !avx) return false;

    // Check OS support for saving AVX registers via XCR0
    unsigned long long xcrFeatureMask = _xgetbv(0);
    if ((xcrFeatureMask & 6) != 6) return false;

    __cpuidex(info, 7, 0);
    bool avx2 = (info[1] & (1 << 5)) != 0;
    return avx2;
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return false;
#endif
}
#endif

bool isAVX2Supported() {
#if defined(AIOS_X86_SIMD)
    static const bool supported = checkAVX2FMA();
    return supported;
#else
    return false;
#endif
}

// Scalar dot product with 4-way loop unrolling to break dependency chains
inline float dotProductScalarInternal(const float* a, const float* b, size_t dim) {
    float sum0 = 0.0f;
    float sum1 = 0.0f;
    float sum2 = 0.0f;
    float sum3 = 0.0f;

    size_t i = 0;
    for (; i + 4 <= dim; i += 4) {
        sum0 += a[i]     * b[i];
        sum1 += a[i + 1] * b[i + 1];
        sum2 += a[i + 2] * b[i + 2];
        sum3 += a[i + 3] * b[i + 3];
    }
    float total = (sum0 + sum1) + (sum2 + sum3);
    for (; i < dim; ++i) {
        total += a[i] * b[i];
    }
    return total;
}

#if defined(AIOS_X86_SIMD)
// AVX2 + FMA vectorized dot product
inline float dotProductAVX2Internal(const float* a, const float* b, size_t dim) {
    __m256 acc0 = _mm256_setzero_ps();
    __m256 acc1 = _mm256_setzero_ps();
    __m256 acc2 = _mm256_setzero_ps();
    __m256 acc3 = _mm256_setzero_ps();

    size_t i = 0;
    // Unroll by 32 floats (4 x 256-bit registers)
    for (; i + 32 <= dim; i += 32) {
        __m256 va0 = _mm256_loadu_ps(a + i);
        __m256 vb0 = _mm256_loadu_ps(b + i);
        acc0 = _mm256_fmadd_ps(va0, vb0, acc0);

        __m256 va1 = _mm256_loadu_ps(a + i + 8);
        __m256 vb1 = _mm256_loadu_ps(b + i + 8);
        acc1 = _mm256_fmadd_ps(va1, vb1, acc1);

        __m256 va2 = _mm256_loadu_ps(a + i + 16);
        __m256 vb2 = _mm256_loadu_ps(b + i + 16);
        acc2 = _mm256_fmadd_ps(va2, vb2, acc2);

        __m256 va3 = _mm256_loadu_ps(a + i + 24);
        __m256 vb3 = _mm256_loadu_ps(b + i + 24);
        acc3 = _mm256_fmadd_ps(va3, vb3, acc3);
    }

    // Process remaining 8-float blocks
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        acc0 = _mm256_fmadd_ps(va, vb, acc0);
    }

    acc0 = _mm256_add_ps(acc0, acc1);
    acc2 = _mm256_add_ps(acc2, acc3);
    acc0 = _mm256_add_ps(acc0, acc2);

    // Horizontal reduction of __m256 into a single scalar float
    __m128 lo = _mm256_castps256_ps128(acc0);
    __m128 hi = _mm256_extractf128_ps(acc0, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    float total = _mm_cvtss_f32(sum);

    // Process remaining scalar tail
    for (; i < dim; ++i) {
        total += a[i] * b[i];
    }
    return total;
}
#endif

} // anonymous namespace

VectorStore::VectorStore() = default;

void VectorStore::setModelProvider(std::shared_ptr<ModelProvider> provider) {
    std::unique_lock lock(rw_mutex_);
    model_provider_ = provider;
}

bool VectorStore::hasAVX2Support() {
    return isAVX2Supported();
}

float VectorStore::dotProduct(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
#if defined(AIOS_X86_SIMD)
    if (isAVX2Supported()) {
        return dotProductAVX2Internal(a, b, dim);
    }
#endif
    return dotProductScalarInternal(a, b, dim);
}

float VectorStore::dotProduct(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0f;
    return dotProduct(a.data(), b.data(), a.size());
}

float VectorStore::cosineSimilarity(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;

#if defined(AIOS_X86_SIMD)
    if (isAVX2Supported() && dim >= 8) {
        __m256 acc_dot0 = _mm256_setzero_ps();
        __m256 acc_dot1 = _mm256_setzero_ps();
        __m256 acc_na0  = _mm256_setzero_ps();
        __m256 acc_na1  = _mm256_setzero_ps();
        __m256 acc_nb0  = _mm256_setzero_ps();
        __m256 acc_nb1  = _mm256_setzero_ps();

        size_t i = 0;
        for (; i + 16 <= dim; i += 16) {
            __m256 va0 = _mm256_loadu_ps(a + i);
            __m256 vb0 = _mm256_loadu_ps(b + i);
            acc_dot0 = _mm256_fmadd_ps(va0, vb0, acc_dot0);
            acc_na0  = _mm256_fmadd_ps(va0, va0, acc_na0);
            acc_nb0  = _mm256_fmadd_ps(vb0, vb0, acc_nb0);

            __m256 va1 = _mm256_loadu_ps(a + i + 8);
            __m256 vb1 = _mm256_loadu_ps(b + i + 8);
            acc_dot1 = _mm256_fmadd_ps(va1, vb1, acc_dot1);
            acc_na1  = _mm256_fmadd_ps(va1, va1, acc_na1);
            acc_nb1  = _mm256_fmadd_ps(vb1, vb1, acc_nb1);
        }
        for (; i + 8 <= dim; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            acc_dot0 = _mm256_fmadd_ps(va, vb, acc_dot0);
            acc_na0  = _mm256_fmadd_ps(va, va, acc_na0);
            acc_nb0  = _mm256_fmadd_ps(vb, vb, acc_nb0);
        }

        acc_dot0 = _mm256_add_ps(acc_dot0, acc_dot1);
        acc_na0  = _mm256_add_ps(acc_na0, acc_na1);
        acc_nb0  = _mm256_add_ps(acc_nb0, acc_nb1);

        auto hsum = [](__m256 v) -> float {
            __m128 lo = _mm256_castps256_ps128(v);
            __m128 hi = _mm256_extractf128_ps(v, 1);
            __m128 s = _mm_add_ps(lo, hi);
            s = _mm_hadd_ps(s, s);
            s = _mm_hadd_ps(s, s);
            return _mm_cvtss_f32(s);
        };

        float dot = hsum(acc_dot0);
        float norm_a = hsum(acc_na0);
        float norm_b = hsum(acc_nb0);

        for (; i < dim; ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }

        if (norm_a <= 0.0f || norm_b <= 0.0f) return 0.0f;
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
#endif

    float dot0 = 0.0f, dot1 = 0.0f;
    float na0 = 0.0f, na1 = 0.0f;
    float nb0 = 0.0f, nb1 = 0.0f;
    size_t i = 0;
    for (; i + 2 <= dim; i += 2) {
        dot0 += a[i] * b[i];
        dot1 += a[i + 1] * b[i + 1];
        na0 += a[i] * a[i];
        na1 += a[i + 1] * a[i + 1];
        nb0 += b[i] * b[i];
        nb1 += b[i + 1] * b[i + 1];
    }
    float dot = dot0 + dot1;
    float norm_a = na0 + na1;
    float norm_b = nb0 + nb1;
    for (; i < dim; ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a <= 0.0f || norm_b <= 0.0f) return 0.0f;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

float VectorStore::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0f;
    return cosineSimilarity(a.data(), b.data(), a.size());
}

void VectorStore::normalizeVector(float* vec, size_t dim) {
    if (!vec || dim == 0) return;
    float norm_sq = dotProduct(vec, vec, dim);
    if (norm_sq > 0.0f) {
        float inv_norm = 1.0f / std::sqrt(norm_sq);
#if defined(AIOS_X86_SIMD)
        if (isAVX2Supported() && dim >= 8) {
            __m256 vinv = _mm256_set1_ps(inv_norm);
            size_t i = 0;
            for (; i + 8 <= dim; i += 8) {
                __m256 v = _mm256_loadu_ps(vec + i);
                v = _mm256_mul_ps(v, vinv);
                _mm256_storeu_ps(vec + i, v);
            }
            for (; i < dim; ++i) {
                vec[i] *= inv_norm;
            }
            return;
        }
#endif
        for (size_t i = 0; i < dim; ++i) {
            vec[i] *= inv_norm;
        }
    }
}

void VectorStore::normalizeVector(std::vector<float>& vec) {
    normalizeVector(vec.data(), vec.size());
}

std::vector<float> VectorStore::computeDeterministicEmbedding(std::string_view text) const {
    std::vector<float> vec(DEFAULT_EMBEDDING_DIM, 0.0f);
    if (text.empty()) return vec;

    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    constexpr size_t DIM_MASK = DEFAULT_EMBEDDING_DIM - 1; // Power-of-two bitwise mask (127 for 128 dim)

    size_t start = 0;
    const size_t len = text.size();

    while (start < len) {
        // Skip non-alphanumeric delimiters
        while (start < len && !std::isalnum(static_cast<unsigned char>(text[start]))) {
            ++start;
        }
        if (start >= len) break;

        size_t end = start;
        uint64_t word_hash = FNV_OFFSET_BASIS;
        char trigram[3] = {0, 0, 0};
        size_t trigram_len = 0;

        while (end < len && std::isalnum(static_cast<unsigned char>(text[end]))) {
            char lower_c = static_cast<char>(std::tolower(static_cast<unsigned char>(text[end])));

            // Whole-word FNV-1a hash
            word_hash ^= static_cast<uint8_t>(lower_c);
            word_hash *= FNV_PRIME;

            // 3-gram sliding window
            trigram[0] = trigram[1];
            trigram[1] = trigram[2];
            trigram[2] = lower_c;
            ++trigram_len;

            if (trigram_len >= 3) {
                uint64_t gh = FNV_OFFSET_BASIS;
                gh ^= static_cast<uint8_t>(trigram[0]); gh *= FNV_PRIME;
                gh ^= static_cast<uint8_t>(trigram[1]); gh *= FNV_PRIME;
                gh ^= static_cast<uint8_t>(trigram[2]); gh *= FNV_PRIME;

                size_t g_idx = static_cast<size_t>(gh & DIM_MASK);
                float g_sign = (gh & (1ULL << 32)) ? 0.5f : -0.5f;
                vec[g_idx] += g_sign;
            }

            ++end;
        }

        // Whole token feature projection
        size_t idx = static_cast<size_t>(word_hash & DIM_MASK);
        float sign = (word_hash & (1ULL << 32)) ? 1.5f : -1.5f;
        vec[idx] += sign;

        start = end;
    }

    normalizeVector(vec.data(), DEFAULT_EMBEDDING_DIM);
    return vec;
}

std::vector<float> VectorStore::generateEmbedding(std::string_view text) const {
    return computeDeterministicEmbedding(text);
}

bool VectorStore::addDocument(const std::string& id, const std::string& text, 
                             const std::unordered_map<std::string, std::string>& metadata,
                             const std::vector<float>& custom_embedding) {
    std::unique_lock lock(rw_mutex_);

    auto it = documents_.find(id);
    size_t target_idx = 0;

    if (it != documents_.end()) {
        target_idx = it->second.matrix_index;
    } else {
        target_idx = doc_ids_.size();
        doc_ids_.push_back(id);
        matrix_.resize((target_idx + 1) * DEFAULT_EMBEDDING_DIM, 0.0f);
    }

    float* row_ptr = &matrix_[target_idx * DEFAULT_EMBEDDING_DIM];

    if (!custom_embedding.empty()) {
        size_t copy_dim = std::min(custom_embedding.size(), DEFAULT_EMBEDDING_DIM);
        std::memcpy(row_ptr, custom_embedding.data(), copy_dim * sizeof(float));
        if (copy_dim < DEFAULT_EMBEDDING_DIM) {
            std::memset(row_ptr + copy_dim, 0, (DEFAULT_EMBEDDING_DIM - copy_dim) * sizeof(float));
        }
        normalizeVector(row_ptr, DEFAULT_EMBEDDING_DIM);
    } else {
        auto embed = computeDeterministicEmbedding(text);
        std::memcpy(row_ptr, embed.data(), DEFAULT_EMBEDDING_DIM * sizeof(float));
    }

    VectorDocument doc;
    doc.id = id;
    doc.text = text;
    doc.metadata = metadata;
    doc.matrix_index = target_idx;
    documents_[id] = std::move(doc);

    return true;
}

std::vector<VectorSearchResult> VectorStore::search(std::string_view query, 
                                                   size_t top_k, 
                                                   float min_similarity) const {
    std::vector<float> query_vec = computeDeterministicEmbedding(query);
    return searchByVector(query_vec, top_k, min_similarity);
}

std::vector<VectorSearchResult> VectorStore::searchByVector(const std::vector<float>& query_vec,
                                                           size_t top_k,
                                                           float min_similarity) const {
    if (query_vec.empty() || top_k == 0) return {};

    // Prepare normalized query buffer padded to DEFAULT_EMBEDDING_DIM
    float q_buf[DEFAULT_EMBEDDING_DIM] = {0.0f};
    size_t copy_dim = std::min(query_vec.size(), DEFAULT_EMBEDDING_DIM);
    std::memcpy(q_buf, query_vec.data(), copy_dim * sizeof(float));
    normalizeVector(q_buf, DEFAULT_EMBEDDING_DIM);

    std::shared_lock lock(rw_mutex_);
    if (documents_.empty()) return {};

    const size_t N = doc_ids_.size();

    // Bounded Min-Heap of size top_k: stores pair<similarity, doc_index>
    using ScorePair = std::pair<float, size_t>;
    auto cmp = [](const ScorePair& a, const ScorePair& b) { return a.first > b.first; };
    std::priority_queue<ScorePair, std::vector<ScorePair>, decltype(cmp)> min_heap(cmp);

    for (size_t i = 0; i < N; ++i) {
        const float* doc_vec = &matrix_[i * DEFAULT_EMBEDDING_DIM];
        float sim = dotProduct(q_buf, doc_vec, DEFAULT_EMBEDDING_DIM);

        if (sim >= min_similarity) {
            if (min_heap.size() < top_k) {
                min_heap.emplace(sim, i);
            } else if (sim > min_heap.top().first) {
                min_heap.pop();
                min_heap.emplace(sim, i);
            }
        }
    }

    if (min_heap.empty()) return {};

    std::vector<VectorSearchResult> results(min_heap.size());
    // Min-heap pops in ascending order; fill from back to front for descending sort
    for (size_t r = min_heap.size(); r > 0; --r) {
        auto [sim, idx] = min_heap.top();
        min_heap.pop();

        const std::string& doc_id = doc_ids_[idx];
        auto it = documents_.find(doc_id);
        if (it != documents_.end()) {
            results[r - 1] = VectorSearchResult{
                doc_id,
                it->second.text,
                it->second.metadata,
                sim
            };
        }
    }

    return results;
}

bool VectorStore::removeDocument(const std::string& id) {
    std::unique_lock lock(rw_mutex_);
    auto it = documents_.find(id);
    if (it == documents_.end()) return false;

    size_t remove_idx = it->second.matrix_index;
    size_t last_idx = doc_ids_.size() - 1;

    // Swap with last element if not already last
    if (remove_idx != last_idx) {
        std::string last_id = doc_ids_[last_idx];
        doc_ids_[remove_idx] = last_id;
        documents_[last_id].matrix_index = remove_idx;

        // Copy vector row
        std::memcpy(&matrix_[remove_idx * DEFAULT_EMBEDDING_DIM], 
                    &matrix_[last_idx * DEFAULT_EMBEDDING_DIM], 
                    DEFAULT_EMBEDDING_DIM * sizeof(float));
    }

    doc_ids_.pop_back();
    matrix_.resize(doc_ids_.size() * DEFAULT_EMBEDDING_DIM);
    documents_.erase(it);

    return true;
}

std::optional<VectorDocument> VectorStore::getDocument(const std::string& id) const {
    std::shared_lock lock(rw_mutex_);
    auto it = documents_.find(id);
    if (it != documents_.end()) return it->second;
    return std::nullopt;
}

void VectorStore::clear() {
    std::unique_lock lock(rw_mutex_);
    documents_.clear();
    doc_ids_.clear();
    matrix_.clear();
}

size_t VectorStore::size() const {
    std::shared_lock lock(rw_mutex_);
    return documents_.size();
}

} // namespace aios
