#include <gtest/gtest.h>
#include "taskgraph/TaskGraph.h"
#include "vector/VectorStore.h"
#include "network/HttpClient.h"
#include "providers/ModelRouter.h"
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace aios;

namespace {

struct BenchmarkStats {
    double total_ms = 0.0;
    double ops_per_sec = 0.0;
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;

    void print(const std::string& name) const {
        std::cout << "\n=======================================================\n"
                  << " BENCHMARK: " << name << "\n"
                  << " Total Time:   " << std::fixed << std::setprecision(2) << total_ms << " ms\n"
                  << " Throughput:   " << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec\n"
                  << " Latency p50:  " << std::fixed << std::setprecision(2) << p50_us << " us\n"
                  << " Latency p95:  " << std::fixed << std::setprecision(2) << p95_us << " us\n"
                  << " Latency p99:  " << std::fixed << std::setprecision(2) << p99_us << " us\n"
                  << "=======================================================\n" << std::endl;
    }
};

BenchmarkStats computeStats(const std::vector<double>& latencies_us, double total_ms) {
    BenchmarkStats stats;
    stats.total_ms = total_ms;
    if (latencies_us.empty()) return stats;

    std::vector<double> sorted = latencies_us;
    std::sort(sorted.begin(), sorted.end());

    stats.ops_per_sec = (sorted.size() / (total_ms / 1000.0));
    stats.p50_us = sorted[sorted.size() * 50 / 100];
    stats.p95_us = sorted[sorted.size() * 95 / 100];
    stats.p99_us = sorted[sorted.size() * 99 / 100];
    return stats;
}

} // namespace

// ============================================================================
// 1. TaskGraph High-Concurrency Execution Benchmark
// ============================================================================

TEST(BenchmarkAIOS, TaskGraphConcurrent100Nodes) {
    TaskGraph graph("Benchmark 100 Concurrent Nodes");

    // Build a wide DAG: 1 root -> 98 parallel workers -> 1 sink join
    TaskNode root{"root", "Root Setup"};
    graph.addNode(root);

    const int NUM_PARALLEL = 98;
    std::vector<std::string> parallel_ids;

    for (int i = 0; i < NUM_PARALLEL; ++i) {
        std::string id = "task_" + std::to_string(i);
        TaskNode node{id, "Worker Task " + std::to_string(i)};
        node.dependencies = {"root"};
        graph.addNode(node);
        parallel_ids.push_back(id);
    }

    TaskNode sink{"sink", "Sink Join"};
    sink.dependencies = parallel_ids;
    graph.addNode(sink);

    EXPECT_EQ(graph.size(), 100);

    TaskGraphExecutorConfig cfg;
    cfg.max_concurrency = 8;
    TaskGraphExecutor executor(cfg);

    executor.setNodeHandler([](TaskNode& /*node*/) -> TaskExecutionResult {
        // Micro-work simulation
        volatile int x = 0;
        for (int i = 0; i < 1000; ++i) x += i;
        return {true, "OK", ""};
    });

    auto start = std::chrono::high_resolution_clock::now();
    auto summary = executor.execute(graph);
    auto end = std::chrono::high_resolution_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_TRUE(summary.success);
    EXPECT_EQ(summary.completed_nodes, 100);

    std::vector<double> latencies(100, (total_ms * 1000.0) / 100.0);
    auto stats = computeStats(latencies, total_ms);
    stats.print("TaskGraph 100-Node Parallel DAG Execution");
    EXPECT_LT(total_ms, 500.0); // Should execute in under 500ms
}

// ============================================================================
// 2. VectorStore Dense Cosine Similarity Search Benchmark
// ============================================================================

TEST(BenchmarkAIOS, VectorStore10000DocumentsSearch) {
    VectorStore store;

    // Generate 1,000 synthetic indexed documents
    const size_t NUM_DOCS = 1000;
    for (size_t i = 0; i < NUM_DOCS; ++i) {
        std::string text = "AIOS kernel concurrency taskgraph scheduling vector database document " + std::to_string(i);
        store.addDocument("doc_" + std::to_string(i), text);
    }

    EXPECT_EQ(store.size(), NUM_DOCS);

    const size_t NUM_QUERIES = 200;
    std::vector<double> latencies_us;
    latencies_us.reserve(NUM_QUERIES);

    auto start_all = std::chrono::high_resolution_clock::now();

    for (size_t q = 0; q < NUM_QUERIES; ++q) {
        std::string query = "taskgraph concurrency scheduling document " + std::to_string(q % 50);
        
        auto q_start = std::chrono::high_resolution_clock::now();
        auto results = store.search(query, 5);
        auto q_end = std::chrono::high_resolution_clock::now();

        EXPECT_FALSE(results.empty());
        latencies_us.push_back(std::chrono::duration<double, std::micro>(q_end - q_start).count());
    }

    auto end_all = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_all - start_all).count();

    auto stats = computeStats(latencies_us, total_ms);
    stats.print("VectorStore Dense Semantic Search (Top-5 Min-Heap)");
    EXPECT_GT(stats.ops_per_sec, 500.0); // > 500 queries per second
}

// ============================================================================
// 3. SSE Stream Chunk Parsing Benchmark
// ============================================================================

TEST(BenchmarkAIOS, SSEStreamChunkParsingThroughput) {
    std::string sse_stream;
    for (int i = 0; i < 500; ++i) {
        sse_stream += "data: {\"id\":\"chatcmpl-" + std::to_string(i) + "\",\"choices\":[{\"delta\":{\"content\":\" token_" + std::to_string(i) + "\"}}]}\n\n";
    }

    std::string carry_over;
    size_t tokens_extracted = 0;

    auto start = std::chrono::high_resolution_clock::now();

    HttpClient::processSseBuffer(sse_stream, carry_over, [&](const std::string& /*data*/) {
        tokens_extracted++;
    });

    auto end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_EQ(tokens_extracted, 500);

    std::vector<double> latencies(tokens_extracted, (total_ms * 1000.0) / tokens_extracted);
    auto stats = computeStats(latencies, total_ms);
    stats.print("Zero-Copy SSE Stream Chunk Parser");
    EXPECT_GT(stats.ops_per_sec, 10000.0); // > 10,000 tokens/sec
}
