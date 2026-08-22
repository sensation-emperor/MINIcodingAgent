#include <gtest/gtest.h>
#include "network/HttpClient.h"
#include "providers/ModelProvider.h"
#include "providers/ModelRouter.h"
#include "providers/MockModelProvider.h"
#include "events/EventBus.h"

#include <thread>
#include <vector>
#include <atomic>
#include <future>

using namespace aios;

class MockStreamHttpClient : public HttpClient {
public:
    explicit MockStreamHttpClient(std::vector<std::string> chunks) : chunks_(std::move(chunks)) {}

    HttpResponse postStream(const std::string& /*url*/,
                            const std::string& /*json_body*/,
                            StreamCallback on_chunk,
                            const std::unordered_map<std::string, std::string>& /*headers*/ = {},
                            std::chrono::milliseconds /*timeout*/ = std::chrono::milliseconds(60000)) override {
        std::string carry_over;
        for (const auto& chunk : chunks_) {
            processSseBuffer(chunk, carry_over, on_chunk);
        }
        HttpResponse res;
        res.status_code = 200;
        res.success = true;
        return res;
    }

private:
    std::vector<std::string> chunks_;
};

// ============================================================================
// HttpClient & SSE Streaming Tests
// ============================================================================

TEST(HttpClientTest, ParsesUrlsCorrectly) {
    auto p1 = HttpClient::parseUrl("http://localhost:1234/v1/chat/completions");
    EXPECT_EQ(p1.scheme, "http");
    EXPECT_EQ(p1.host, "localhost");
    EXPECT_EQ(p1.port, 1234);
    EXPECT_EQ(p1.path, "/v1/chat/completions");

    auto p2 = HttpClient::parseUrl("https://api.openai.com/v1/models");
    EXPECT_EQ(p2.scheme, "https");
    EXPECT_EQ(p2.host, "api.openai.com");
    EXPECT_EQ(p2.port, 443);
    EXPECT_EQ(p2.path, "/v1/models");
}

TEST(HttpClientTest, ProcessesSseBufferWithDeltas) {
    std::string sse_stream = 
        "data: {\"choices\": [{\"delta\": {\"content\": \"Hello\"}}]}\n\n"
        "data: {\"choices\": [{\"delta\": {\"content\": \" world\"}}]}\n\n"
        "data: [DONE]\n\n";

    std::string carry_over;
    std::vector<std::string> collected_data;

    HttpClient::processSseBuffer(sse_stream, carry_over, [&](const std::string& data) {
        collected_data.push_back(data);
    });

    ASSERT_EQ(collected_data.size(), 2);
    EXPECT_NE(collected_data[0].find("Hello"), std::string::npos);
    EXPECT_NE(collected_data[1].find(" world"), std::string::npos);
    EXPECT_TRUE(carry_over.empty());
}

TEST(HttpClientTest, HandlesFragmentedSseChunks) {
    std::string chunk1 = "data: {\"choices\": [{\"delta\": {\"con";
    std::string chunk2 = "tent\": \"Fragmented\"}}]}\n\n";

    std::string carry_over;
    std::vector<std::string> collected_data;

    HttpClient::processSseBuffer(chunk1, carry_over, [&](const std::string& data) {
        collected_data.push_back(data);
    });
    EXPECT_EQ(collected_data.size(), 0);
    EXPECT_FALSE(carry_over.empty());

    HttpClient::processSseBuffer(chunk2, carry_over, [&](const std::string& data) {
        collected_data.push_back(data);
    });
    ASSERT_EQ(collected_data.size(), 1);
    EXPECT_NE(collected_data[0].find("Fragmented"), std::string::npos);
    EXPECT_TRUE(carry_over.empty());
}

TEST(HttpClientTest, ZeroCopyMultipleEventsAndNdjson) {
    std::string chunk = 
        ": sse ping comment\n"
        "data: {\"choices\": [{\"delta\": {\"content\": \"First\"}}]}\r\n"
        "\r\n"
        "{\"response\":\"ndjson_token\"}\n"
        "data: {\"choices\": [{\"delta\": {\"content\": \"Second\"}}]}\n\n"
        "data: [DONE]\n\n";

    std::string carry_over;
    std::vector<std::string> results;
    HttpClient::processSseBuffer(chunk, carry_over, [&](const std::string& d) {
        results.push_back(d);
    });

    ASSERT_EQ(results.size(), 3);
    EXPECT_NE(results[0].find("First"), std::string::npos);
    EXPECT_NE(results[1].find("ndjson_token"), std::string::npos);
    EXPECT_NE(results[2].find("Second"), std::string::npos);
    EXPECT_TRUE(carry_over.empty());
}

// ============================================================================
// ModelProvider Fast-Path Token Extraction Tests
// ============================================================================

TEST(ModelProviderTest, FastPathTokenExtractionWithEscapedCharacters) {
    std::vector<std::string> chunks = {
        "data: {\"choices\": [{\"delta\": {\"content\": \"Hello\\n\"}}]}\n\n",
        "data: {\"choices\": [{\"delta\": {\"content\": \"\\\"World\\\"\\t\"}}]}\n\n",
        "data: {\"choices\": [{\"delta\": {\"content\": \"\\\\Path\\\\\"}}]}\n\n",
        "data: [DONE]\n\n"
    };

    auto mock_http = std::make_shared<MockStreamHttpClient>(chunks);
    LMStudioProvider provider("http://localhost:1234/v1", mock_http);

    std::string streamed;
    std::vector<Message> msgs = {{"user", "Hi"}};
    ModelResponse resp = provider.chatStream(msgs, [&](const std::string& token) {
        streamed += token;
    });

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(streamed, "Hello\n\"World\"\t\\Path\\");
    EXPECT_EQ(resp.content, "Hello\n\"World\"\t\\Path\\");
}

TEST(ModelProviderTest, AnthropicFastPathTokenExtraction) {
    std::vector<std::string> chunks = {
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Claude\\n\"}}\n\n",
        "data: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":\"FastPath\"}}\n\n"
    };

    auto mock_http = std::make_shared<MockStreamHttpClient>(chunks);
    AnthropicProvider provider("test-key", "https://api.anthropic.com/v1", mock_http);

    std::string streamed;
    std::vector<Message> msgs = {{"user", "Hi"}};
    ModelResponse resp = provider.chatStream(msgs, [&](const std::string& token) {
        streamed += token;
    });

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(streamed, "Claude\nFastPath");
}

// ============================================================================
// ModelRouter & Fallback Tests
// ============================================================================

TEST(ModelRouterTest, RoutesByAgentRole) {
    ModelRouter router;
    router.initialize();

    auto mock_planner = std::make_shared<MockModelProvider>();
    mock_planner->queueResponse("Planner Model Response");
    router.registerProvider("lm_studio", mock_planner);

    std::vector<Message> msgs = {{"user", "Plan task"}};
    ModelResponse resp = router.route(AgentType::Planner, msgs);

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.content, "Planner Model Response");
}

TEST(ModelRouterTest, FallbackToSecondaryProviderOnFailure) {
    ModelRouter router;
    router.initialize();

    auto failing_provider = std::make_shared<MockModelProvider>();
    failing_provider->queueResponse("", false, "Connection refused: port 1234 offline");

    auto backup_provider = std::make_shared<MockModelProvider>();
    backup_provider->queueResponse("Ollama Backup Response", true, "");

    router.registerProvider("lm_studio", failing_provider);
    router.registerProvider("ollama", backup_provider);
    router.setFallbackChain({"lm_studio", "ollama"});

    std::vector<Message> msgs = {{"user", "Code task"}};
    ModelResponse resp = router.routeWithFallback("lm_studio", "qwen", msgs);

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.content, "Ollama Backup Response");
}

TEST(ModelRouterTest, CircuitBreakerMarksProviderOfflineAfterFailures) {
    ModelRouter router;
    router.initialize();

    auto failing_provider = std::make_shared<MockModelProvider>();
    failing_provider->queueResponse("", false, "Error 1");
    failing_provider->queueResponse("", false, "Error 2");
    failing_provider->queueResponse("", false, "Error 3");

    router.registerProvider("flaky_local", failing_provider);
    router.setFallbackChain({"flaky_local"});

    std::vector<Message> msgs = {{"user", "Test"}};
    router.routeWithFallback("flaky_local", "model", msgs);
    router.routeWithFallback("flaky_local", "model", msgs);
    router.routeWithFallback("flaky_local", "model", msgs);

    ProviderHealth health = router.getProviderHealth("flaky_local");
    EXPECT_EQ(health.status, ProviderStatus::Offline);
    EXPECT_GE(health.consecutive_failures, 3);
}

TEST(ModelRouterTest, TracksMetricsLedgerAccurately) {
    ModelRouter router;
    router.initialize();

    auto mock_provider = std::make_shared<MockModelProvider>();
    mock_provider->setCustomHandler([](const std::vector<Message>&) {
        ModelResponse r;
        r.content = "Response";
        r.success = true;
        r.usage.prompt_tokens = 100;
        r.usage.completion_tokens = 50;
        r.usage.total_tokens = 150;
        r.usage.latency_ms = 250.0;
        return r;
    });

    router.registerProvider("metrics_prov", mock_provider);

    std::vector<Message> msgs = {{"user", "Query 1"}};
    router.routeWithFallback("metrics_prov", "model", msgs);
    router.routeWithFallback("metrics_prov", "model", msgs);

    ProviderMetricsLedger metrics = router.getMetrics("metrics_prov");
    EXPECT_EQ(metrics.total_requests, 2);
    EXPECT_EQ(metrics.successful_requests, 2);
    EXPECT_EQ(metrics.total_prompt_tokens, 200);
    EXPECT_EQ(metrics.total_completion_tokens, 100);
    EXPECT_EQ(metrics.total_tokens, 300);
    EXPECT_GT(metrics.average_latency_ms, 0.0);

    ProviderMetricsLedger global_m = router.getMetrics();
    EXPECT_EQ(global_m.total_requests, 2);
}

TEST(ModelRouterTest, ThreadSafeConcurrentRoutingWithoutModelCorruption) {
    ModelRouter router;
    router.initialize();

    class ModelInspectMockProvider : public ModelProvider {
    public:
        bool initialize() override { return true; }
        ModelResponse chat(const std::vector<Message>& /*messages*/, const std::string& model = "") override {
            ModelResponse r;
            r.success = true;
            r.model = model;
            r.content = "Response from model: " + model;
            return r;
        }
        std::string getName() const override { return "inspect_mock"; }
    };

    auto inspect_provider = std::make_shared<ModelInspectMockProvider>();
    router.registerProvider("inspect_prov", inspect_provider);
    router.setFallbackChain({"inspect_prov"});

    const int num_threads = 20;
    std::vector<std::future<bool>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&router, i]() {
            std::string expected_model = "model_variant_" + std::to_string(i);
            std::vector<Message> msgs = {{"user", "Hello " + std::to_string(i)}};
            auto resp = router.routeWithFallback("inspect_prov", expected_model, msgs);
            return resp.success && (resp.model == expected_model) && 
                   (resp.content == "Response from model: " + expected_model);
        }));
    }

    for (auto& f : futures) {
        EXPECT_TRUE(f.get());
    }
}

TEST(ModelRouterTest, AtomicHalfOpenCircuitBreakerPreventsThunderingHerd) {
    ModelRouter router;
    router.initialize();
    router.setCooldownSeconds(0); // Immediately eligible for half-open probe upon failure

    std::atomic<int> primary_attempts{0};
    std::atomic<int> backup_attempts{0};

    auto primary_provider = std::make_shared<MockModelProvider>();
    // First 3 calls fail to trip circuit breaker
    primary_provider->setCustomHandler([&](const std::vector<Message>&) -> ModelResponse {
        int attempt = ++primary_attempts;
        if (attempt <= 3) {
            return ModelResponse{"", false, "Failure " + std::to_string(attempt)};
        }
        // Probe request: simulate slight latency
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return ModelResponse{"Primary Recovered", true, ""};
    });

    auto backup_provider = std::make_shared<MockModelProvider>();
    backup_provider->setCustomHandler([&](const std::vector<Message>&) -> ModelResponse {
        backup_attempts++;
        return ModelResponse{"Backup Success", true, ""};
    });

    router.registerProvider("primary", primary_provider);
    router.registerProvider("backup", backup_provider);
    router.setFallbackChain({"primary", "backup"});

    std::vector<Message> msgs = {{"user", "trip circuit breaker"}};
    router.routeWithFallback("primary", "m", msgs);
    router.routeWithFallback("primary", "m", msgs);
    router.routeWithFallback("primary", "m", msgs);

    EXPECT_EQ(router.getProviderHealth("primary").status, ProviderStatus::Offline);
    EXPECT_EQ(primary_attempts.load(), 3);

    // Launch 20 concurrent threads. Exactly 1 probe request should go to primary,
    // while the remaining 19 threads are shed to backup.
    const int num_threads = 20;
    std::vector<std::future<ModelResponse>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&router]() {
            std::vector<Message> m = {{"user", "concurrent request"}};
            return router.routeWithFallback("primary", "m", m);
        }));
    }

    int primary_successes = 0;
    int backup_successes = 0;

    for (auto& f : futures) {
        auto r = f.get();
        EXPECT_TRUE(r.success);
        if (r.content == "Primary Recovered") {
            primary_successes++;
        } else if (r.content == "Backup Success") {
            backup_successes++;
        }
    }

    EXPECT_EQ(primary_successes, 1);
    EXPECT_EQ(backup_successes, 19);
    EXPECT_EQ(primary_attempts.load(), 4); // 3 trips + 1 probe
}

TEST(ModelRouterTest, LockFreeMetricsHighConcurrency) {
    ModelRouter router;
    router.initialize();

    auto mock_prov = std::make_shared<MockModelProvider>();
    mock_prov->setCustomHandler([](const std::vector<Message>&) -> ModelResponse {
        ModelResponse r;
        r.success = true;
        r.content = "ok";
        r.usage.prompt_tokens = 10;
        r.usage.completion_tokens = 5;
        r.usage.total_tokens = 15;
        r.usage.latency_ms = 10.0;
        return r;
    });

    router.registerProvider("atomic_prov", mock_prov);
    router.setFallbackChain({"atomic_prov"});

    const int num_threads = 30;
    const int requests_per_thread = 20;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&router, requests_per_thread]() {
            for (int i = 0; i < requests_per_thread; ++i) {
                std::vector<Message> msgs = {{"user", "query"}};
                router.routeWithFallback("atomic_prov", "model", msgs);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    size_t total_expected = num_threads * requests_per_thread;

    auto metrics = router.getMetrics("atomic_prov");
    EXPECT_EQ(metrics.total_requests, total_expected);
    EXPECT_EQ(metrics.successful_requests, total_expected);
    EXPECT_EQ(metrics.failed_requests, 0);
    EXPECT_EQ(metrics.total_prompt_tokens, total_expected * 10);
    EXPECT_EQ(metrics.total_completion_tokens, total_expected * 5);
    EXPECT_EQ(metrics.total_tokens, total_expected * 15);
    EXPECT_GT(metrics.average_latency_ms, 0.0);
    EXPECT_GT(metrics.average_tokens_per_sec, 0.0);

    auto global_m = router.getMetrics();
    EXPECT_EQ(global_m.total_requests, total_expected);
    EXPECT_EQ(global_m.successful_requests, total_expected);
    EXPECT_EQ(global_m.total_tokens, total_expected * 15);
}

TEST(ModelRouterTest, AtomicMetricsLedgerDirectConcurrency) {
    AtomicMetricsLedger ledger;
    const int num_threads = 40;
    const int ops_per_thread = 500;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&ledger, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                if ((t + i) % 3 == 0) {
                    ledger.recordFailure();
                } else {
                    ledger.recordSuccess(10, 5, 15, 2.5);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    auto snapshot = ledger.snapshot();
    EXPECT_EQ(snapshot.total_requests, num_threads * ops_per_thread);
    EXPECT_EQ(snapshot.successful_requests + snapshot.failed_requests, snapshot.total_requests);
    EXPECT_EQ(snapshot.total_prompt_tokens, snapshot.successful_requests * 10);
    EXPECT_EQ(snapshot.total_completion_tokens, snapshot.successful_requests * 5);
    EXPECT_EQ(snapshot.total_tokens, snapshot.successful_requests * 15);
}
