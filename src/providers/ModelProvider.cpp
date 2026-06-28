#include "providers/ModelProvider.h"
#include "logging/Logger.h"

namespace aios {

ModelResponse AnthropicProvider::chat(const std::vector<Message>& messages) {
    LOG_INFO("AnthropicProvider: {} messages", messages.size());
    return {"", false, "Not implemented"};
}

ModelResponse OpenAIProvider::chat(const std::vector<Message>& messages) {
    LOG_INFO("OpenAIProvider: {} messages", messages.size());
    return {"", false, "Not implemented"};
}

ModelResponse OllamaProvider::chat(const std::vector<Message>& messages) {
    LOG_INFO("OllamaProvider: {} messages", messages.size());
    return {"", false, "Not implemented"};
}

} // namespace aios
