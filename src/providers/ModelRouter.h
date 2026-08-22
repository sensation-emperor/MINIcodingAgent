#pragma once

#include "providers/ModelProvider.h"
#include "agents/agents.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>

namespace aios {

class EventBus;

enum class ProviderStatus {
    Healthy,
    Degraded,
    HalfOpen,
    Offline
};

struct ProviderHealth {
    ProviderStatus status = ProviderStatus::Healthy;
    size_t consecutive_failures = 0;
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point last_success_time;
    std::string last_error;
    bool probe_in_flight = false;
};

struct ProviderMetricsLedger {
    size_t total_requests = 0;
    size_t successful_requests = 0;
    size_t failed_requests = 0;
    size_t total_prompt_tokens = 0;
    size_t total_completion_tokens = 0;
    size_t total_tokens = 0;
    double total_latency_ms = 0.0;
    double average_latency_ms = 0.0;
    double average_tokens_per_sec = 0.0;
};

struct AtomicMetricsLedger {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> total_prompt_tokens{0};
    std::atomic<uint64_t> total_completion_tokens{0};
    std::atomic<uint64_t> total_tokens{0};
    std::atomic<double> total_latency_ms{0.0};

    void recordSuccess(size_t prompt_tokens, size_t completion_tokens, size_t total_toks, double latency_ms) {
        total_requests.fetch_add(1, std::memory_order_relaxed);
        successful_requests.fetch_add(1, std::memory_order_relaxed);
        total_prompt_tokens.fetch_add(prompt_tokens, std::memory_order_relaxed);
        total_completion_tokens.fetch_add(completion_tokens, std::memory_order_relaxed);
        total_tokens.fetch_add(total_toks, std::memory_order_relaxed);

        double cur = total_latency_ms.load(std::memory_order_relaxed);
        while (!total_latency_ms.compare_exchange_weak(cur, cur + latency_ms, 
                                                       std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }

    void recordFailure() {
        total_requests.fetch_add(1, std::memory_order_relaxed);
        failed_requests.fetch_add(1, std::memory_order_relaxed);
    }

    ProviderMetricsLedger snapshot() const {
        ProviderMetricsLedger m;
        m.total_requests = static_cast<size_t>(total_requests.load(std::memory_order_relaxed));
        m.successful_requests = static_cast<size_t>(successful_requests.load(std::memory_order_relaxed));
        m.failed_requests = static_cast<size_t>(failed_requests.load(std::memory_order_relaxed));
        m.total_prompt_tokens = static_cast<size_t>(total_prompt_tokens.load(std::memory_order_relaxed));
        m.total_completion_tokens = static_cast<size_t>(total_completion_tokens.load(std::memory_order_relaxed));
        m.total_tokens = static_cast<size_t>(total_tokens.load(std::memory_order_relaxed));
        m.total_latency_ms = total_latency_ms.load(std::memory_order_relaxed);
        if (m.successful_requests > 0) {
            m.average_latency_ms = m.total_latency_ms / m.successful_requests;
            if (m.total_latency_ms > 0) {
                m.average_tokens_per_sec = (static_cast<double>(m.total_completion_tokens) / m.total_latency_ms) * 1000.0;
            }
        }
        return m;
    }
};

struct ModelRouteConfig {
    std::string provider_name;
    std::string model_name;
    double temperature = 0.7;
    int max_tokens = 4096;
};

class ModelRouter {
public:
    static ModelRouter& instance();

    ModelRouter();
    ~ModelRouter() = default;

    bool initialize();
    void setEventBus(std::shared_ptr<EventBus> event_bus);

    // Provider registration
    void registerProvider(const std::string& name, std::shared_ptr<ModelProvider> provider);
    std::shared_ptr<ModelProvider> getProvider(const std::string& name) const;
    std::vector<std::string> listProviders() const;

    // Routing configuration
    void setRoleRoute(AgentType type, const ModelRouteConfig& config);
    ModelRouteConfig getRoleRoute(AgentType type) const;
    void setFallbackChain(const std::vector<std::string>& provider_names);
    std::vector<std::string> getFallbackChain() const;

    // Execution with automatic fallback and circuit breaker
    ModelResponse route(AgentType type, 
                        const std::vector<Message>& messages, 
                        TokenCallback on_token = nullptr);

    ModelResponse routeWithFallback(const std::string& initial_provider,
                                    const std::string& model,
                                    const std::vector<Message>& messages,
                                    TokenCallback on_token = nullptr);

    // Health & Circuit Breaker
    ProviderHealth getProviderHealth(const std::string& provider_name) const;
    void checkHealthAll();
    void resetHealth(const std::string& provider_name);
    void setCooldownSeconds(int seconds) { cooldown_seconds_ = seconds; }
    int getCooldownSeconds() const { return cooldown_seconds_.load(); }

    // Metrics Ledger
    ProviderMetricsLedger getMetrics(const std::string& provider_name = "") const;
    std::unordered_map<std::string, ProviderMetricsLedger> getAllMetrics() const;

private:
    void recordSuccess(const std::string& provider_name, const ModelResponse& resp);
    void recordFailure(const std::string& provider_name, const std::string& error);
    bool shouldAttempt(const std::string& provider_name);
    std::shared_ptr<AtomicMetricsLedger> getOrCreateMetrics(const std::string& provider_name);

    // Readers-writer lock for provider registry (hot concurrent read path)
    mutable std::shared_mutex provider_mutex_;
    // Separate fine-grained mutex for health/circuit-breaker state writes
    mutable std::mutex health_mutex_;
    // Coarse mutex for metrics ledger updates and event bus ref
    mutable std::mutex metrics_mutex_;

    std::shared_ptr<EventBus> event_bus_;
    std::unordered_map<std::string, std::shared_ptr<ModelProvider>> providers_;
    std::unordered_map<AgentType, ModelRouteConfig> role_routes_;
    std::vector<std::string> fallback_chain_;
    std::unordered_map<std::string, ProviderHealth> health_map_;
    std::unordered_map<std::string, std::shared_ptr<AtomicMetricsLedger>> metrics_map_;
    std::shared_ptr<AtomicMetricsLedger> global_metrics_;
    std::atomic<int> cooldown_seconds_{30};
};

} // namespace aios
