#include <gtest/gtest.h>
#include "database/DatabaseEngine.h"
#include "vector/VectorStore.h"
#include "knowledge/KnowledgeGraph.h"
#include "memory/memory.h"
#include <filesystem>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>

using namespace aios;

// ============================================================================
// DatabaseEngine Tests
// ============================================================================

TEST(DatabaseEngineTest, StoresAndRetrievesConversationsAndMessages) {
    std::string test_db = "test_aios_db.json";
    if (std::filesystem::exists(test_db)) std::filesystem::remove(test_db);

    DatabaseEngine db(test_db);
    db.initialize();

    DbConversation conv;
    conv.id = "conv_123";
    conv.title = "Refactor TaskGraph";
    conv.created_at = std::chrono::system_clock::now();
    conv.metadata["author"] = "tester";

    EXPECT_TRUE(db.saveConversation(conv));

    auto fetched = db.getConversation("conv_123");
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->title, "Refactor TaskGraph");
    EXPECT_EQ(fetched->metadata["author"], "tester");

    DbMessage msg1{"m1", "conv_123", "user", "Add cycle detection", 10, std::chrono::system_clock::now()};
    DbMessage msg2{"m2", "conv_123", "assistant", "Cycle detection added", 15, std::chrono::system_clock::now()};
    db.saveMessage(msg1);
    db.saveMessage(msg2);

    auto msgs = db.getMessages("conv_123");
    ASSERT_EQ(msgs.size(), 2);
    EXPECT_EQ(msgs[0].content, "Add cycle detection");
    EXPECT_EQ(msgs[1].role, "assistant");

    db.shutdown();
    if (std::filesystem::exists(test_db)) std::filesystem::remove(test_db);
}

// ============================================================================
// VectorStore Tests
// ============================================================================

TEST(VectorStoreTest, ComputesCosineSimilarityAndTopKSearch) {
    VectorStore store;

    // Add documents
    store.addDocument("doc1", "C++ multi-threaded task graph executor");
    store.addDocument("doc2", "Python Django REST framework web application");
    store.addDocument("doc3", "C++ parallel thread pool scheduler");

    EXPECT_EQ(store.size(), 3);

    // Search for C++ thread pool
    auto results = store.search("C++ parallel thread pool", 2);
    ASSERT_GE(results.size(), 1);

    // doc3 or doc1 should be top matches
    EXPECT_TRUE(results[0].id == "doc3" || results[0].id == "doc1");
    EXPECT_GT(results[0].similarity, 0.3f);
}

TEST(VectorStoreTest, HandlesCustomVectorEmbeddings) {
    VectorStore store;

    std::vector<float> vecA = {1.0f, 0.0f, 0.0f};
    std::vector<float> vecB = {0.9f, 0.1f, 0.0f};
    std::vector<float> vecC = {0.0f, 1.0f, 0.0f};

    store.addDocument("vA", "alpha", {}, vecA);
    store.addDocument("vB", "beta", {}, vecB);
    store.addDocument("vC", "gamma", {}, vecC);

    auto results = store.searchByVector(vecA, 2);
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].id, "vA");
    EXPECT_NEAR(results[0].similarity, 1.0f, 0.01f);
    EXPECT_EQ(results[1].id, "vB");
}

TEST(VectorStoreTest, DotProductAndCosineSimilarityPrecision) {
    std::vector<float> v1(VectorStore::DEFAULT_EMBEDDING_DIM, 0.0f);
    std::vector<float> v2(VectorStore::DEFAULT_EMBEDDING_DIM, 0.0f);

    v1[0] = 1.0f;
    v1[1] = 2.0f;
    v1[2] = 3.0f;

    v2[0] = 4.0f;
    v2[1] = 5.0f;
    v2[2] = 6.0f;

    float dot = VectorStore::dotProduct(v1, v2);
    EXPECT_NEAR(dot, 32.0f, 1e-4f);

    VectorStore::normalizeVector(v1);
    VectorStore::normalizeVector(v2);
    float norm_dot = VectorStore::dotProduct(v1, v2);
    float cos_sim = VectorStore::cosineSimilarity(v1, v2);
    EXPECT_NEAR(norm_dot, cos_sim, 1e-5f);
}

TEST(VectorStoreTest, FeatureHashingCaseInvarianceAndDelimiters) {
    VectorStore store;
    auto embed1 = store.generateEmbedding("AIOS Multi-Threaded Task Graph Scheduler");
    auto embed2 = store.generateEmbedding("aios multi-threaded task graph scheduler");
    auto embed3 = store.generateEmbedding("aios, multi_threaded; task: graph / scheduler!");

    EXPECT_EQ(embed1.size(), VectorStore::DEFAULT_EMBEDDING_DIM);
    EXPECT_EQ(embed2.size(), VectorStore::DEFAULT_EMBEDDING_DIM);
    EXPECT_EQ(embed3.size(), VectorStore::DEFAULT_EMBEDDING_DIM);

    float sim12 = VectorStore::cosineSimilarity(embed1, embed2);
    EXPECT_NEAR(sim12, 1.0f, 1e-4f);

    float sim13 = VectorStore::cosineSimilarity(embed1, embed3);
    EXPECT_GT(sim13, 0.95f);
}

TEST(VectorStoreTest, DenseMatrixCompactionOnRemovalAndUpdates) {
    VectorStore store;

    store.addDocument("d1", "Document one about machine learning models");
    store.addDocument("d2", "Document two about compiler optimization passes");
    store.addDocument("d3", "Document three about lock-free concurrency queues");
    store.addDocument("d4", "Document four about database indexing algorithms");

    EXPECT_EQ(store.size(), 4);

    store.addDocument("d2", "Updated document two about SIMD AVX2 acceleration");
    EXPECT_EQ(store.size(), 4);
    auto doc2 = store.getDocument("d2");
    ASSERT_TRUE(doc2.has_value());
    EXPECT_EQ(doc2->text, "Updated document two about SIMD AVX2 acceleration");

    EXPECT_TRUE(store.removeDocument("d3"));
    EXPECT_EQ(store.size(), 3);
    EXPECT_FALSE(store.getDocument("d3").has_value());

    auto results = store.search("SIMD AVX2 acceleration", 3);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].id, "d2");

    EXPECT_TRUE(store.removeDocument("d1"));
    EXPECT_TRUE(store.removeDocument("d2"));
    EXPECT_TRUE(store.removeDocument("d4"));
    EXPECT_EQ(store.size(), 0);
}

TEST(VectorStoreTest, BoundedMinHeapTopKSelectionAndThresholding) {
    VectorStore store;

    for (int i = 0; i < 20; ++i) {
        std::vector<float> vec(VectorStore::DEFAULT_EMBEDDING_DIM, 0.0f);
        vec[0] = static_cast<float>(i + 1) / 20.0f;
        store.addDocument("doc_" + std::to_string(i), "text " + std::to_string(i), {}, vec);
    }
    EXPECT_EQ(store.size(), 20);

    std::vector<float> query(VectorStore::DEFAULT_EMBEDDING_DIM, 0.0f);
    query[0] = 1.0f;

    auto top5 = store.searchByVector(query, 5);
    ASSERT_EQ(top5.size(), 5);
    for (size_t i = 0; i < top5.size(); ++i) {
        EXPECT_NEAR(top5[i].similarity, 1.0f, 0.01f);
    }

    auto top50 = store.searchByVector(query, 50);
    EXPECT_EQ(top50.size(), 20);

    // min_similarity filtering with top_k = 10
    auto filtered = store.searchByVector(query, 10, 0.99f);
    EXPECT_EQ(filtered.size(), 10); // top_k is bounded at 10

    // min_similarity filtering with top_k = 30
    auto filtered_all = store.searchByVector(query, 30, 0.99f);
    EXPECT_EQ(filtered_all.size(), 20); // all 20 collinear unit vectors match threshold
    std::vector<float> perp_query(VectorStore::DEFAULT_EMBEDDING_DIM, 0.0f);
    perp_query[1] = 1.0f;
    auto perp_results = store.searchByVector(perp_query, 5, 0.1f);
    EXPECT_TRUE(perp_results.empty());
}

TEST(VectorStoreTest, HighConcurrencyMultiReaderMultiWriterStress) {
    VectorStore store;

    for (int i = 0; i < 50; ++i) {
        store.addDocument("init_" + std::to_string(i), "Base system document for indexing " + std::to_string(i));
    }

    std::atomic<bool> running{true};
    std::atomic<size_t> total_searches{0};
    std::atomic<size_t> total_writes{0};

    constexpr int NUM_READERS = 6;
    constexpr int NUM_WRITERS = 2;
    std::vector<std::thread> threads;

    for (int r = 0; r < NUM_READERS; ++r) {
        threads.emplace_back([&store, &running, &total_searches, r]() {
            std::mt19937 rng(42 + r);
            std::uniform_int_distribution<int> dist(0, 100);
            while (running.load(std::memory_order_relaxed)) {
                int query_id = dist(rng);
                auto res = store.search("indexing document " + std::to_string(query_id), 3);
                total_searches.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int w = 0; w < NUM_WRITERS; ++w) {
        threads.emplace_back([&store, &running, &total_writes, w]() {
            int counter = 0;
            while (running.load(std::memory_order_relaxed)) {
                std::string doc_id = "writer_" + std::to_string(w) + "_" + std::to_string(counter % 30);
                if ((counter % 4) == 0) {
                    store.removeDocument(doc_id);
                } else {
                    store.addDocument(doc_id, "Dynamic text written by thread " + std::to_string(w) + " count " + std::to_string(counter));
                }
                total_writes.fetch_add(1, std::memory_order_relaxed);
                counter++;
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    running.store(false, std::memory_order_relaxed);

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    EXPECT_GT(total_searches.load(), 500);
    EXPECT_GT(total_writes.load(), 50);
    EXPECT_GT(store.size(), 0);
}

// ============================================================================
// KnowledgeGraph Tests
// ============================================================================

TEST(KnowledgeGraphTest, BuildsGraphAndTraversesNeighbors) {
    KnowledgeGraph kg;
    kg.clear();

    KnowledgeNode sym1{"s1", "TaskGraphExecutor", EntityType::Symbol, "Parallel DAG executor", {}, std::chrono::system_clock::now()};
    KnowledgeNode sym2{"s2", "ThreadPool", EntityType::Symbol, "Worker thread pool", {}, std::chrono::system_clock::now()};
    KnowledgeNode adr1{"adr1", "Use ThreadPool for DAG", EntityType::ArchitectureDecision, "Avoid per-node thread spawning", {}, std::chrono::system_clock::now()};

    kg.addNode(sym1);
    kg.addNode(sym2);
    kg.addNode(adr1);

    kg.addEdge("s1", "s2", RelationType::Calls, 1.0f, "uses thread pool");
    kg.addEdge("adr1", "s1", RelationType::Implements, 1.0f, "applies to executor");

    EXPECT_EQ(kg.nodeCount(), 3);
    EXPECT_EQ(kg.edgeCount(), 2);

    // BFS related entities from s1
    auto related = kg.findRelatedEntities("s1", 2);
    EXPECT_EQ(related.size(), 2);

    // Find ADRs
    auto adrs = kg.getArchitectureDecisions();
    ASSERT_EQ(adrs.size(), 1);
    EXPECT_EQ(adrs[0].name, "Use ThreadPool for DAG");
}

TEST(KnowledgeGraphTest, FindsBugFixForCompilerErrors) {
    KnowledgeGraph kg;
    kg.clear();

    KnowledgeNode bug1{"b1", "Deadlock in TaskGraph ready queue", EntityType::BugFix, 
        "Mutex lock held while notifying condition variable. Fixed by releasing lock before notify.", 
        {}, std::chrono::system_clock::now()};

    kg.addNode(bug1);

    auto fixes = kg.findFixForError("Deadlock in TaskGraph ready queue");
    ASSERT_EQ(fixes.size(), 1);
    EXPECT_EQ(fixes[0].id, "b1");
    EXPECT_NE(fixes[0].description.find("releasing lock"), std::string::npos);
}

// ============================================================================
// Unified MemoryManager Tests
// ============================================================================

TEST(MemoryManagerTest, StoresAndEvictsWorkingMemory) {
    MemoryManager mm;
    mm.initialize();
    mm.setMemoryLimit(500); // 500 bytes limit

    mm.store("k1", "small value 1");
    mm.store("k2", "small value 2");
    
    EXPECT_TRUE(mm.retrieve("k1").has_value());
    EXPECT_TRUE(mm.retrieve("k2").has_value());

    // Exceed limit
    std::string large_str(600, 'X');
    mm.store("k3", large_str);

    // k1 should have been LRU-evicted
    EXPECT_FALSE(mm.retrieve("k1").has_value());
    EXPECT_TRUE(mm.retrieve("k3").has_value());
}

TEST(MemoryManagerTest, StoresAndSearchesSemanticMemory) {
    MemoryManager mm;
    mm.initialize();

    mm.storeSemantic("pref_pkg", "Always use pnpm.cmd on Windows", "user_preference");
    mm.storeSemantic("pref_llm", "LM Studio is the preferred local LLM runtime", "user_preference");

    auto results = mm.searchSemantic("pnpm package manager Windows", 1);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].id, "pref_pkg");
}
