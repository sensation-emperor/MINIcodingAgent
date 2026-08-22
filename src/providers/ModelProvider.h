#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>

namespace aios {

class HttpClient;

struct Message {
    std::string role;  // "system", "user", "assistant"
    std::string content;
};

struct UsageStats {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
    double latency_ms = 0.0;
    double tokens_per_second = 0.0;
};

struct ModelResponse {
    std::string content;
    bool success = false;
    std::string error;
    UsageStats usage;
    std::string model;
    std::string provider_name;
};

using TokenCallback = std::function<void(const std::string& token_delta)>;

/**
 * @brief Base class for AI Model Providers
 */
class ModelProvider {
public:
    virtual ~ModelProvider() = default;
    virtual bool initialize() = 0;
    virtual bool isHealthy() { return true; }
    virtual ModelResponse chat(const std::vector<Message>& messages, const std::string& model = "") = 0;
    virtual ModelResponse chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model = "");
    virtual std::string getName() const = 0;
    virtual std::vector<std::string> getAvailableModels() { return {}; }
    virtual void setModel(const std::string& model_name) {
        std::lock_guard<std::mutex> lock(model_mutex_);
        default_model_ = model_name;
    }
    virtual std::string getModel() const {
        std::lock_guard<std::mutex> lock(model_mutex_);
        return default_model_;
    }

protected:
    mutable std::mutex model_mutex_;
    std::string default_model_;
};

/**
 * @brief LM Studio Provider (Local OpenAI-compatible endpoint)
 */
class LMStudioProvider : public ModelProvider {
public:
    explicit LMStudioProvider(const std::string& endpoint = "http://localhost:1234/v1",
                              std::shared_ptr<HttpClient> http_client = nullptr);
    bool initialize() override;
    bool isHealthy() override;
    ModelResponse chat(const std::vector<Message>& messages, const std::string& model = "") override;
    ModelResponse chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model = "") override;
    std::string getName() const override { return "lm_studio"; }
    std::vector<std::string> getAvailableModels() override;

private:
    std::string endpoint_;
    std::shared_ptr<HttpClient> http_;
};

/**
 * @brief Ollama Provider (Local Ollama REST API)
 */
class OllamaProvider : public ModelProvider {
public:
    explicit OllamaProvider(const std::string& endpoint = "http://localhost:11434",
                            std::shared_ptr<HttpClient> http_client = nullptr);
    bool initialize() override;
    bool isHealthy() override;
    ModelResponse chat(const std::vector<Message>& messages, const std::string& model = "") override;
    ModelResponse chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model = "") override;
    std::string getName() const override { return "ollama"; }
    std::vector<std::string> getAvailableModels() override;

private:
    std::string endpoint_;
    std::shared_ptr<HttpClient> http_;
};

/**
 * @brief OpenAI Provider
 */
class OpenAIProvider : public ModelProvider {
public:
    explicit OpenAIProvider(const std::string& api_key = "",
                            const std::string& endpoint = "https://api.openai.com/v1",
                            std::shared_ptr<HttpClient> http_client = nullptr);
    bool initialize() override;
    bool isHealthy() override;
    ModelResponse chat(const std::vector<Message>& messages, const std::string& model = "") override;
    ModelResponse chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model = "") override;
    std::string getName() const override { return "openai"; }
    void setApiKey(const std::string& key) { api_key_ = key; }

private:
    std::string api_key_;
    std::string endpoint_;
    std::shared_ptr<HttpClient> http_;
};

/**
 * @brief Anthropic Provider (Claude API)
 */
class AnthropicProvider : public ModelProvider {
public:
    explicit AnthropicProvider(const std::string& api_key = "",
                               const std::string& endpoint = "https://api.anthropic.com/v1",
                               std::shared_ptr<HttpClient> http_client = nullptr);
    bool initialize() override;
    bool isHealthy() override;
    ModelResponse chat(const std::vector<Message>& messages, const std::string& model = "") override;
    ModelResponse chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model = "") override;
    std::string getName() const override { return "anthropic"; }
    void setApiKey(const std::string& key) { api_key_ = key; }

private:
    std::string api_key_;
    std::string endpoint_;
    std::shared_ptr<HttpClient> http_;
};

/**
 * @brief OpenRouter Provider (Multi-model cloud aggregator)
 */
class OpenRouterProvider : public ModelProvider {
public:
    explicit OpenRouterProvider(const std::string& api_key = "",
                                const std::string& endpoint = "https://openrouter.ai/api/v1",
                                std::shared_ptr<HttpClient> http_client = nullptr);
    bool initialize() override;
    bool isHealthy() override;
    ModelResponse chat(const std::vector<Message>& messages, const std::string& model = "") override;
    ModelResponse chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model = "") override;
    std::string getName() const override { return "openrouter"; }
    void setApiKey(const std::string& key) { api_key_ = key; }

private:
    std::string api_key_;
    std::string endpoint_;
    std::shared_ptr<HttpClient> http_;
};

} // namespace aios
