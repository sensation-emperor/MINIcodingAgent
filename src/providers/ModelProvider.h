#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace aios {

struct Message {
    std::string role;  // "system", "user", "assistant"
    std::string content;
};

struct ModelResponse {
    std::string content;
    bool success;
    std::string error;
};

class ModelProvider {
public:
    virtual ~ModelProvider() = default;
    virtual bool initialize() = 0;
    virtual ModelResponse chat(const std::vector<Message>& messages) = 0;
    virtual std::string getName() const = 0;
};

class AnthropicProvider : public ModelProvider {
public:
    bool initialize() override { return true; }
    ModelResponse chat(const std::vector<Message>& messages) override;
    std::string getName() const override { return "anthropic"; }
};

class OpenAIProvider : public ModelProvider {
public:
    bool initialize() override { return true; }
    ModelResponse chat(const std::vector<Message>& messages) override;
    std::string getName() const override { return "openai"; }
};

class OllamaProvider : public ModelProvider {
public:
    bool initialize() override { return true; }
    ModelResponse chat(const std::vector<Message>& messages) override;
    std::string getName() const override { return "ollama"; }
};

} // namespace aios
