#include "providers/ModelRouter.h"
#include "events/EventBus.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace aios {

ModelRouter& ModelRouter::instance() {
    static ModelRouter instance;
    return instance;
}

ModelRouter::ModelRouter() {
    global_metrics_ = std::make_shared<AtomicMetricsLedger>();
    fallback_chain_ = {"lm_studio", "ollama", "openrouter", "openai", "anthropic"};
}

bool ModelRouter::initialize() {
    std::unique_lock<std::shared_mutex> lock(provider_mutex_);
    LOG_INFO("Initializing ModelRouter with default role routes...");

    // Setup default role routing table prioritizing local models
    role_routes_[AgentType::Planner]    = {"lm_studio", "qwen2.5-coder:14b", 0.3, 4096};
    role_routes_[AgentType::Debugger]   = {"lm_studio", "qwen2.5-coder:14b", 0.2, 4096};
    role_routes_[AgentType::Coder]      = {"lm_studio", "qwen2.5-coder:7b",  0.4, 4096};
    role_routes_[AgentType::Tester]     = {"lm_studio", "qwen2.5-coder:7b",  0.2, 2048};
    role_routes_[AgentType::Reviewer]   = {"lm_studio", "qwen2.5-coder:14b", 0.2, 4096};
    role_routes_[AgentType::Researcher] = {"lm_studio", "llama3:latest",      0.5, 2048};
    role_routes_[AgentType::Generic]    = {"lm_studio", "local-model",         0.7, 4096};

    // Register default providers if not already registered
    if (providers_.find("lm_studio") == providers_.end())
        providers_["lm_studio"] = std::make_shared<LMStudioProvider>();
    if (providers_.find("ollama") == providers_.end())
        providers_["ollama"] = std::make_shared<OllamaProvider>();
    if (providers_.find("openai") == providers_.end())
        providers_["openai"] = std::make_shared<OpenAIProvider>();
    if (providers_.find("anthropic") == providers_.end())
        providers_["anthropic"] = std::make_shared<AnthropicProvider>();
    if (providers_.find("openrouter") == providers_.end())
        providers_["openrouter"] = std::make_shared<OpenRouterProvider>();

    LOG_INFO("ModelRouter initialized successfully");
    return true;
}

void ModelRouter::setEventBus(std::shared_ptr<EventBus> event_bus) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    event_bus_ = event_bus;
}

void ModelRouter::registerProvider(const std::string& name, std::shared_ptr<ModelProvider> provider) {
    std::unique_lock<std::shared_mutex> lock(provider_mutex_);
    providers_[name] = provider;

    std::lock_guard<std::mutex> hlock(health_mutex_);
    health_map_[name] = ProviderHealth{ProviderStatus::Healthy, 0, {}, {}, ""};
    LOG_INFO("Registered model provider: {}", name);
}

std::shared_ptr<ModelProvider> ModelRouter::getProvider(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(provider_mutex_);
    auto it = providers_.find(name);
    return (it != providers_.end()) ? it->second : nullptr;
}

std::vector<std::string> ModelRouter::listProviders() const {
    std::shared_lock<std::shared_mutex> lock(provider_mutex_);
    std::vector<std::string> list;
    list.reserve(providers_.size());
    for (const auto& [name, _] : providers_) {
        list.push_back(name);
    }
    return list;
}

void ModelRouter::setRoleRoute(AgentType type, const ModelRouteConfig& config) {
    std::unique_lock<std::shared_mutex> lock(provider_mutex_);
    role_routes_[type] = config;
}

ModelRouteConfig ModelRouter::getRoleRoute(AgentType type) const {
    std::shared_lock<std::shared_mutex> lock(provider_mutex_);
    auto it = role_routes_.find(type);
    if (it != role_routes_.end()) {
        return it->second;
    }
    return {"lm_studio", "local-model", 0.7, 4096};
}

void ModelRouter::setFallbackChain(const std::vector<std::string>& provider_names) {
    std::unique_lock<std::shared_mutex> lock(provider_mutex_);
    fallback_chain_ = provider_names;
}

std::vector<std::string> ModelRouter::getFallbackChain() const {
    std::shared_lock<std::shared_mutex> lock(provider_mutex_);
    return fallback_chain_;
}

// Circuit breaker — called while health_mutex_ is held by the caller
bool ModelRouter::shouldAttempt(const std::string& provider_name) {
    auto it = health_map_.find(provider_name);
    if (it == health_map_.end()) return true;

    auto& h = it->second;

    if (h.status == ProviderStatus::Offline) {
        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - h.last_failure_time).count();
        if (diff >= cooldown_seconds_.load(std::memory_order_relaxed)) {
            if (!h.probe_in_flight) {
                h.probe_in_flight = true;
                h.status = ProviderStatus::HalfOpen;
                LOG_INFO("Circuit Breaker: Provider [{}] cooldown elapsed — Half-Open probe allowed.", provider_name);
                return true;
            }
        }
        return false;
    }

    if (h.status == ProviderStatus::HalfOpen) {
        return false;
    }

    return true;
}

void ModelRouter::recordSuccess(const std::string& provider_name, const ModelResponse& resp) {
    // Update health state under health_mutex_
    {
        std::lock_guard<std::mutex> hlock(health_mutex_);
        auto& h = health_map_[provider_name];
        h.status = ProviderStatus::Healthy;
        h.consecutive_failures = 0;
        h.probe_in_flight = false;
        h.last_success_time = std::chrono::system_clock::now();
    }

    // Atomic metrics ledger update — lock-free
    getOrCreateMetrics(provider_name)->recordSuccess(
        resp.usage.prompt_tokens,
        resp.usage.completion_tokens,
        resp.usage.total_tokens,
        resp.usage.latency_ms
    );
    global_metrics_->recordSuccess(
        resp.usage.prompt_tokens,
        resp.usage.completion_tokens,
        resp.usage.total_tokens,
        resp.usage.latency_ms
    );

    // Publish event — read event_bus_ ref under metrics_mutex_
    std::shared_ptr<EventBus> bus;
    {
        std::lock_guard<std::mutex> mlock(metrics_mutex_);
        bus = event_bus_;
    }
    if (bus) {
        nlohmann::json j;
        j["provider"]           = provider_name;
        j["model"]              = resp.model;
        j["prompt_tokens"]      = resp.usage.prompt_tokens;
        j["completion_tokens"]  = resp.usage.completion_tokens;
        j["latency_ms"]         = resp.usage.latency_ms;
        bus->publish("model.tokens_used", j.dump());
    }
}

void ModelRouter::recordFailure(const std::string& provider_name, const std::string& error) {
    ProviderStatus new_status;
    {
        std::lock_guard<std::mutex> hlock(health_mutex_);
        auto& h = health_map_[provider_name];
        h.consecutive_failures++;
        h.last_failure_time = std::chrono::system_clock::now();
        h.last_error = error;
        h.probe_in_flight = false;

        if (h.consecutive_failures >= 3) {
            h.status = ProviderStatus::Offline;
            LOG_WARN("Provider {} marked OFFLINE ({} consecutive failures): {}",
                     provider_name, h.consecutive_failures, error);
        } else {
            h.status = ProviderStatus::Degraded;
        }
        new_status = h.status;
    }

    // Atomic metrics — no mutex required
    getOrCreateMetrics(provider_name)->recordFailure();
    global_metrics_->recordFailure();

    std::shared_ptr<EventBus> bus;
    {
        std::lock_guard<std::mutex> mlock(metrics_mutex_);
        bus = event_bus_;
    }
    if (bus) {
        nlohmann::json j;
        j["provider"] = provider_name;
        j["error"]    = error;
        j["status"]   = (new_status == ProviderStatus::Offline ? "offline" : "degraded");
        bus->publish("model.provider_health_changed", j.dump());
    }
}

ModelResponse ModelRouter::route(AgentType type, 
                                const std::vector<Message>& messages, 
                                TokenCallback on_token) {
    ModelRouteConfig target_route = getRoleRoute(type);
    return routeWithFallback(target_route.provider_name, target_route.model_name, messages, on_token);
}

ModelResponse ModelRouter::routeWithFallback(const std::string& initial_provider,
                                            const std::string& model,
                                            const std::vector<Message>& messages,
                                            TokenCallback on_token) {
    // Build fallback chain with initial provider first — shared read lock
    std::vector<std::string> chain;
    {
        std::shared_lock<std::shared_mutex> lock(provider_mutex_);
        chain.reserve(fallback_chain_.size() + 1);
        chain.push_back(initial_provider);
        for (const auto& p : fallback_chain_) {
            if (p != initial_provider) {
                chain.push_back(p);
            }
        }
    }

    std::string last_error;

    for (const auto& provider_name : chain) {
        // Check circuit breaker — health_mutex_ scope
        bool should_attempt = false;
        {
            std::lock_guard<std::mutex> hlock(health_mutex_);
            should_attempt = shouldAttempt(provider_name);
        }
        if (!should_attempt) continue;

        // Look up provider with shared (non-blocking) reader lock
        std::shared_ptr<ModelProvider> provider;
        {
            std::shared_lock<std::shared_mutex> rlock(provider_mutex_);
            auto it = providers_.find(provider_name);
            if (it != providers_.end()) {
                provider = it->second;
            }
        }
        if (!provider) continue;

        LOG_INFO("ModelRouter: Dispatching to provider [{}] model [{}]...", provider_name, model);

        ModelResponse resp;
        if (on_token) {
            resp = provider->chatStream(messages, on_token, model);
        } else {
            resp = provider->chat(messages, model);
        }

        if (resp.success) {
            recordSuccess(provider_name, resp);
            return resp;
        }

        last_error = resp.error;
        recordFailure(provider_name, resp.error);
        LOG_WARN("Provider [{}] failed: {}. Trying next fallback...", provider_name, resp.error);

        std::shared_ptr<EventBus> bus;
        {
            std::lock_guard<std::mutex> mlock(metrics_mutex_);
            bus = event_bus_;
        }
        if (bus) {
            nlohmann::json j;
            j["failed_provider"] = provider_name;
            j["error"] = resp.error;
            bus->publish("model.fallback_triggered", j.dump());
        }
    }

    ModelResponse final_err;
    final_err.success = false;
    final_err.error = "All providers in fallback chain exhausted. Last error: " + last_error;
    return final_err;
}

ProviderHealth ModelRouter::getProviderHealth(const std::string& provider_name) const {
    std::lock_guard<std::mutex> lock(health_mutex_);
    auto it = health_map_.find(provider_name);
    return (it != health_map_.end()) ? it->second : ProviderHealth{};
}

void ModelRouter::checkHealthAll() {
    // Snapshot provider list with shared lock (non-blocking)
    std::unordered_map<std::string, std::shared_ptr<ModelProvider>> copy;
    {
        std::shared_lock<std::shared_mutex> rlock(provider_mutex_);
        copy = providers_;
    }

    for (const auto& [name, prov] : copy) {
        if (!prov) continue;
        bool healthy = prov->isHealthy();

        std::lock_guard<std::mutex> hlock(health_mutex_);
        auto& h = health_map_[name];
        if (healthy) {
            h.status = ProviderStatus::Healthy;
            h.consecutive_failures = 0;
            h.probe_in_flight = false;
        } else {
            h.status = ProviderStatus::Degraded;
        }
    }
}

void ModelRouter::resetHealth(const std::string& provider_name) {
    std::lock_guard<std::mutex> hlock(health_mutex_);
    health_map_[provider_name] = ProviderHealth{ProviderStatus::Healthy, 0, {}, {}, ""};
}

ProviderMetricsLedger ModelRouter::getMetrics(const std::string& provider_name) const {
    if (provider_name.empty()) {
        return global_metrics_->snapshot();
    }
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    auto it = metrics_map_.find(provider_name);
    if (it != metrics_map_.end()) {
        return it->second->snapshot();
    }
    return ProviderMetricsLedger{};
}

std::unordered_map<std::string, ProviderMetricsLedger> ModelRouter::getAllMetrics() const {
    std::unordered_map<std::string, ProviderMetricsLedger> result;
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    for (const auto& [name, ledger] : metrics_map_) {
        result[name] = ledger->snapshot();
    }
    return result;
}

std::shared_ptr<AtomicMetricsLedger> ModelRouter::getOrCreateMetrics(const std::string& provider_name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    auto& entry = metrics_map_[provider_name];
    if (!entry) {
        entry = std::make_shared<AtomicMetricsLedger>();
    }
    return entry;
}

} // namespace aios
