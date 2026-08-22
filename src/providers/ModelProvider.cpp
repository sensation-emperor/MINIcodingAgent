#include "providers/ModelProvider.h"
#include "network/HttpClient.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <string_view>

namespace aios {

namespace {

bool unescapeJsonString(std::string_view raw, std::string& out) {
    out.clear();
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            switch (next) {
                case 'n': out.push_back('\n'); i++; break;
                case 'r': out.push_back('\r'); i++; break;
                case 't': out.push_back('\t'); i++; break;
                case '\"': out.push_back('\"'); i++; break;
                case '\\': out.push_back('\\'); i++; break;
                case 'b': out.push_back('\b'); i++; break;
                case 'f': out.push_back('\f'); i++; break;
                case '/': out.push_back('/'); i++; break;
                case 'u': {
                    if (i + 5 < raw.size()) {
                        unsigned int codepoint = 0;
                        for (int k = 2; k <= 5; ++k) {
                            char c = raw[i + k];
                            codepoint <<= 4;
                            if (c >= '0' && c <= '9') codepoint |= (c - '0');
                            else if (c >= 'a' && c <= 'f') codepoint |= (c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F') codepoint |= (c - 'A' + 10);
                        }
                        if (codepoint < 0x80) {
                            out.push_back(static_cast<char>(codepoint));
                        } else if (codepoint < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
                            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
                            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                        }
                        i += 5;
                    } else {
                        out.push_back('\\');
                    }
                    break;
                }
                default:
                    out.push_back(next);
                    i++;
                    break;
            }
        } else {
            out.push_back(raw[i]);
        }
    }
    return true;
}

bool extractDeltaContent(std::string_view chunk, std::string& out_content) {
    size_t key_pos = chunk.find("\"content\"");
    if (key_pos == std::string_view::npos) return false;

    size_t pos = key_pos + 9;
    while (pos < chunk.size() && (chunk[pos] == ' ' || chunk[pos] == '\t' || chunk[pos] == '\r' || chunk[pos] == '\n')) {
        pos++;
    }
    if (pos >= chunk.size() || chunk[pos] != ':') return false;
    pos++;

    while (pos < chunk.size() && (chunk[pos] == ' ' || chunk[pos] == '\t' || chunk[pos] == '\r' || chunk[pos] == '\n')) {
        pos++;
    }
    if (pos >= chunk.size() || chunk[pos] != '\"') return false;
    pos++;

    size_t val_start = pos;
    bool has_escapes = false;
    while (pos < chunk.size()) {
        if (chunk[pos] == '\\') {
            has_escapes = true;
            pos += 2;
        } else if (chunk[pos] == '\"') {
            break;
        } else {
            pos++;
        }
    }
    if (pos >= chunk.size() || chunk[pos] != '\"') return false;

    std::string_view raw_val = chunk.substr(val_start, pos - val_start);
    if (!has_escapes) {
        out_content = std::string(raw_val);
        return true;
    }
    return unescapeJsonString(raw_val, out_content);
}

bool extractAnthropicTextDelta(std::string_view chunk, std::string& out_content) {
    size_t key_pos = chunk.find("\"text\"");
    if (key_pos == std::string_view::npos) return false;

    size_t pos = key_pos + 6;
    while (pos < chunk.size() && (chunk[pos] == ' ' || chunk[pos] == '\t' || chunk[pos] == '\r' || chunk[pos] == '\n')) {
        pos++;
    }
    if (pos >= chunk.size() || chunk[pos] != ':') return false;
    pos++;

    while (pos < chunk.size() && (chunk[pos] == ' ' || chunk[pos] == '\t' || chunk[pos] == '\r' || chunk[pos] == '\n')) {
        pos++;
    }
    if (pos >= chunk.size() || chunk[pos] != '\"') return false;
    pos++;

    size_t val_start = pos;
    bool has_escapes = false;
    while (pos < chunk.size()) {
        if (chunk[pos] == '\\') {
            has_escapes = true;
            pos += 2;
        } else if (chunk[pos] == '\"') {
            break;
        } else {
            pos++;
        }
    }
    if (pos >= chunk.size() || chunk[pos] != '\"') return false;

    std::string_view raw_val = chunk.substr(val_start, pos - val_start);
    if (!has_escapes) {
        out_content = std::string(raw_val);
        return true;
    }
    return unescapeJsonString(raw_val, out_content);
}

} // anonymous namespace

ModelResponse ModelProvider::chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model) {
    ModelResponse resp = chat(messages, model);
    if (resp.success && on_token && !resp.content.empty()) {
        on_token(resp.content);
    }
    return resp;
}

// ============================================================================
// LMStudioProvider Implementation
// ============================================================================

LMStudioProvider::LMStudioProvider(const std::string& endpoint, std::shared_ptr<HttpClient> http_client)
    : endpoint_(endpoint), http_(http_client ? http_client : std::make_shared<HttpClient>()) {
    default_model_ = "local-model";
}

bool LMStudioProvider::initialize() {
    return isHealthy();
}

bool LMStudioProvider::isHealthy() {
    auto res = http_->get(endpoint_ + "/models", {}, std::chrono::milliseconds(2000));
    return res.success && res.status_code == 200;
}

std::vector<std::string> LMStudioProvider::getAvailableModels() {
    std::vector<std::string> models;
    auto res = http_->get(endpoint_ + "/models");
    if (res.success) {
        try {
            auto j = nlohmann::json::parse(res.body);
            if (j.contains("data") && j["data"].is_array()) {
                for (const auto& item : j["data"]) {
                    if (item.contains("id")) {
                        models.push_back(item["id"].get<std::string>());
                    }
                }
            }
        } catch (...) {}
    }
    return models;
}

ModelResponse LMStudioProvider::chat(const std::vector<Message>& messages, const std::string& model) {
    auto start_time = std::chrono::steady_clock::now();
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"temperature", 0.7},
        {"stream", false}
    };

    auto http_res = http_->postJson(endpoint_ + "/chat/completions", req_body.dump());
    auto end_time = std::chrono::steady_clock::now();
    response.usage.latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (!http_res.success) {
        response.success = false;
        response.error = http_res.error.empty() ? ("HTTP " + std::to_string(http_res.status_code)) : http_res.error;
        return response;
    }

    try {
        auto j = nlohmann::json::parse(http_res.body);
        if (j.contains("choices") && !j["choices"].empty()) {
            auto choice = j["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                response.content = choice["message"]["content"].get<std::string>();
                response.success = true;
            }
        }
        if (j.contains("usage")) {
            response.usage.prompt_tokens = j["usage"].value("prompt_tokens", 0);
            response.usage.completion_tokens = j["usage"].value("completion_tokens", 0);
            response.usage.total_tokens = j["usage"].value("total_tokens", 0);
        }
        if (response.usage.latency_ms > 0 && response.usage.completion_tokens > 0) {
            response.usage.tokens_per_second = (response.usage.completion_tokens / response.usage.latency_ms) * 1000.0;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = std::string("JSON parse error: ") + e.what();
    }

    return response;
}

ModelResponse LMStudioProvider::chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model) {
    auto start_time = std::chrono::steady_clock::now();
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"temperature", 0.7},
        {"stream", true}
    };

    std::string accumulated;
    int token_count = 0;

    auto stream_cb = [&](const std::string& chunk_data) {
        std::string token;
        if (extractDeltaContent(chunk_data, token)) {
            accumulated += token;
            token_count++;
            if (on_token) {
                on_token(token);
            }
            return;
        }

        try {
            auto j = nlohmann::json::parse(chunk_data);
            if (j.contains("choices") && !j["choices"].empty()) {
                auto delta = j["choices"][0].value("delta", nlohmann::json::object());
                if (delta.contains("content")) {
                    token = delta["content"].get<std::string>();
                    accumulated += token;
                    token_count++;
                    if (on_token) {
                        on_token(token);
                    }
                }
            }
        } catch (...) {}
    };

    auto http_res = http_->postStream(endpoint_ + "/chat/completions", req_body.dump(), stream_cb);
    auto end_time = std::chrono::steady_clock::now();
    response.usage.latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (!http_res.success && accumulated.empty()) {
        response.success = false;
        response.error = http_res.error;
        return response;
    }

    response.content = accumulated;
    response.success = true;
    response.usage.completion_tokens = token_count;
    response.usage.total_tokens = token_count;
    if (response.usage.latency_ms > 0 && token_count > 0) {
        response.usage.tokens_per_second = (token_count / response.usage.latency_ms) * 1000.0;
    }

    return response;
}

// ============================================================================
// OllamaProvider Implementation
// ============================================================================

OllamaProvider::OllamaProvider(const std::string& endpoint, std::shared_ptr<HttpClient> http_client)
    : endpoint_(endpoint), http_(http_client ? http_client : std::make_shared<HttpClient>()) {
    default_model_ = "llama3:latest";
}

bool OllamaProvider::initialize() {
    return isHealthy();
}

bool OllamaProvider::isHealthy() {
    auto res = http_->get(endpoint_ + "/api/tags", {}, std::chrono::milliseconds(2000));
    return res.success && res.status_code == 200;
}

std::vector<std::string> OllamaProvider::getAvailableModels() {
    std::vector<std::string> models;
    auto res = http_->get(endpoint_ + "/api/tags");
    if (res.success) {
        try {
            auto j = nlohmann::json::parse(res.body);
            if (j.contains("models") && j["models"].is_array()) {
                for (const auto& item : j["models"]) {
                    if (item.contains("name")) {
                        models.push_back(item["name"].get<std::string>());
                    }
                }
            }
        } catch (...) {}
    }
    return models;
}

ModelResponse OllamaProvider::chat(const std::vector<Message>& messages, const std::string& model) {
    auto start_time = std::chrono::steady_clock::now();
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"stream", false}
    };

    auto http_res = http_->postJson(endpoint_ + "/api/chat", req_body.dump());
    auto end_time = std::chrono::steady_clock::now();
    response.usage.latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (!http_res.success) {
        response.success = false;
        response.error = http_res.error;
        return response;
    }

    try {
        auto j = nlohmann::json::parse(http_res.body);
        if (j.contains("message") && j["message"].contains("content")) {
            response.content = j["message"]["content"].get<std::string>();
            response.success = true;
        }
        response.usage.prompt_tokens = j.value("prompt_eval_count", 0);
        response.usage.completion_tokens = j.value("eval_count", 0);
        response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
    } catch (const std::exception& e) {
        response.success = false;
        response.error = e.what();
    }

    return response;
}

ModelResponse OllamaProvider::chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model) {
    auto start_time = std::chrono::steady_clock::now();
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"stream", true}
    };

    std::string accumulated;
    int token_count = 0;

    auto stream_cb = [&](const std::string& chunk_data) {
        std::string token;
        if (extractDeltaContent(chunk_data, token)) {
            accumulated += token;
            token_count++;
            if (on_token) on_token(token);
            return;
        }

        try {
            auto j = nlohmann::json::parse(chunk_data);
            if (j.contains("message") && j["message"].contains("content")) {
                token = j["message"]["content"].get<std::string>();
                accumulated += token;
                token_count++;
                if (on_token) on_token(token);
            }
        } catch (...) {}
    };

    auto http_res = http_->postStream(endpoint_ + "/api/chat", req_body.dump(), stream_cb);
    auto end_time = std::chrono::steady_clock::now();
    response.usage.latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    response.content = accumulated;
    response.success = !accumulated.empty() || http_res.success;
    response.usage.completion_tokens = token_count;
    return response;
}

// ============================================================================
// OpenAIProvider Implementation
// ============================================================================

OpenAIProvider::OpenAIProvider(const std::string& api_key, const std::string& endpoint, std::shared_ptr<HttpClient> http_client)
    : api_key_(api_key), endpoint_(endpoint), http_(http_client ? http_client : std::make_shared<HttpClient>()) {
    default_model_ = "gpt-4o-mini";
}

bool OpenAIProvider::initialize() {
    return !api_key_.empty();
}

bool OpenAIProvider::isHealthy() {
    return !api_key_.empty();
}

ModelResponse OpenAIProvider::chat(const std::vector<Message>& messages, const std::string& model) {
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    if (api_key_.empty()) {
        response.error = "OpenAI API key is missing";
        response.success = false;
        return response;
    }

    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + api_key_}
    };

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"temperature", 0.7}
    };

    auto http_res = http_->postJson(endpoint_ + "/chat/completions", req_body.dump(), headers);
    if (!http_res.success) {
        response.success = false;
        response.error = http_res.error;
        return response;
    }

    try {
        auto j = nlohmann::json::parse(http_res.body);
        if (j.contains("choices") && !j["choices"].empty()) {
            response.content = j["choices"][0]["message"]["content"].get<std::string>();
            response.success = true;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = e.what();
    }

    return response;
}

ModelResponse OpenAIProvider::chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model) {
    if (api_key_.empty()) {
        ModelResponse r;
        r.error = "OpenAI API key missing";
        return r;
    }

    std::string target_model = model.empty() ? getModel() : model;
    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + api_key_}
    };

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"stream", true}
    };

    std::string accumulated;
    auto stream_cb = [&](const std::string& chunk_data) {
        std::string token;
        if (extractDeltaContent(chunk_data, token)) {
            accumulated += token;
            if (on_token) on_token(token);
            return;
        }

        try {
            auto j = nlohmann::json::parse(chunk_data);
            if (j.contains("choices") && !j["choices"].empty()) {
                auto delta = j["choices"][0].value("delta", nlohmann::json::object());
                if (delta.contains("content")) {
                    token = delta["content"].get<std::string>();
                    accumulated += token;
                    if (on_token) on_token(token);
                }
            }
        } catch (...) {}
    };

    auto http_res = http_->postStream(endpoint_ + "/chat/completions", req_body.dump(), stream_cb, headers);
    ModelResponse response;
    response.content = accumulated;
    response.success = !accumulated.empty() || http_res.success;
    response.provider_name = getName();
    response.model = target_model;
    return response;
}

// ============================================================================
// AnthropicProvider Implementation
// ============================================================================

AnthropicProvider::AnthropicProvider(const std::string& api_key, const std::string& endpoint, std::shared_ptr<HttpClient> http_client)
    : api_key_(api_key), endpoint_(endpoint), http_(http_client ? http_client : std::make_shared<HttpClient>()) {
    default_model_ = "claude-3-5-sonnet-20241022";
}

bool AnthropicProvider::initialize() {
    return !api_key_.empty();
}

bool AnthropicProvider::isHealthy() {
    return !api_key_.empty();
}

ModelResponse AnthropicProvider::chat(const std::vector<Message>& messages, const std::string& model) {
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    if (api_key_.empty()) {
        response.error = "Anthropic API key is missing";
        response.success = false;
        return response;
    }

    std::unordered_map<std::string, std::string> headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"}
    };

    std::string system_prompt;
    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        if (m.role == "system") {
            system_prompt += (system_prompt.empty() ? "" : "\n") + m.content;
        } else {
            j_messages.push_back({{"role", m.role}, {"content", m.content}});
        }
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"max_tokens", 4096}
    };
    if (!system_prompt.empty()) {
        req_body["system"] = system_prompt;
    }

    auto http_res = http_->postJson(endpoint_ + "/messages", req_body.dump(), headers);
    if (!http_res.success) {
        response.success = false;
        response.error = http_res.error;
        return response;
    }

    try {
        auto j = nlohmann::json::parse(http_res.body);
        if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
            for (const auto& c : j["content"]) {
                if (c.value("type", "") == "text") {
                    response.content += c.value("text", "");
                }
            }
            response.success = true;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = e.what();
    }

    return response;
}

ModelResponse AnthropicProvider::chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model) {
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    if (api_key_.empty()) {
        response.error = "Anthropic API key is missing";
        response.success = false;
        return response;
    }

    std::unordered_map<std::string, std::string> headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"}
    };

    std::string system_prompt;
    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        if (m.role == "system") {
            system_prompt += (system_prompt.empty() ? "" : "\n") + m.content;
        } else {
            j_messages.push_back({{"role", m.role}, {"content", m.content}});
        }
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"max_tokens", 4096},
        {"stream", true}
    };
    if (!system_prompt.empty()) req_body["system"] = system_prompt;

    std::string accumulated;
    auto stream_cb = [&](const std::string& chunk_data) {
        std::string token;
        if (extractAnthropicTextDelta(chunk_data, token)) {
            accumulated += token;
            if (on_token) on_token(token);
            return;
        }

        try {
            auto j = nlohmann::json::parse(chunk_data);
            if (j.value("type", "") == "content_block_delta") {
                auto delta = j.value("delta", nlohmann::json::object());
                if (delta.value("type", "") == "text_delta") {
                    token = delta.value("text", "");
                    accumulated += token;
                    if (on_token) on_token(token);
                }
            }
        } catch (...) {}
    };

    auto http_res = http_->postStream(endpoint_ + "/messages", req_body.dump(), stream_cb, headers);
    response.content = accumulated;
    response.success = !accumulated.empty() || http_res.success;
    return response;
}

// ============================================================================
// OpenRouterProvider Implementation
// ============================================================================

OpenRouterProvider::OpenRouterProvider(const std::string& api_key, const std::string& endpoint, std::shared_ptr<HttpClient> http_client)
    : api_key_(api_key), endpoint_(endpoint), http_(http_client ? http_client : std::make_shared<HttpClient>()) {
    default_model_ = "meta-llama/llama-3.3-70b-instruct";
}

bool OpenRouterProvider::initialize() {
    return !api_key_.empty();
}

bool OpenRouterProvider::isHealthy() {
    return !api_key_.empty();
}

ModelResponse OpenRouterProvider::chat(const std::vector<Message>& messages, const std::string& model) {
    ModelResponse response;
    std::string target_model = model.empty() ? getModel() : model;
    response.provider_name = getName();
    response.model = target_model;

    if (api_key_.empty()) {
        response.error = "OpenRouter API key is missing";
        response.success = false;
        return response;
    }

    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + api_key_},
        {"HTTP-Referer", "https://aios.dev"},
        {"X-Title", "AIOS Coding Agent"}
    };

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages}
    };

    auto http_res = http_->postJson(endpoint_ + "/chat/completions", req_body.dump(), headers);
    if (!http_res.success) {
        response.success = false;
        response.error = http_res.error;
        return response;
    }

    try {
        auto j = nlohmann::json::parse(http_res.body);
        if (j.contains("choices") && !j["choices"].empty()) {
            response.content = j["choices"][0]["message"]["content"].get<std::string>();
            response.success = true;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = e.what();
    }

    return response;
}

ModelResponse OpenRouterProvider::chatStream(const std::vector<Message>& messages, TokenCallback on_token, const std::string& model) {
    std::string target_model = model.empty() ? getModel() : model;
    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + api_key_},
        {"HTTP-Referer", "https://aios.dev"},
        {"X-Title", "AIOS Coding Agent"}
    };

    nlohmann::json j_messages = nlohmann::json::array();
    for (const auto& m : messages) {
        j_messages.push_back({{"role", m.role}, {"content", m.content}});
    }

    nlohmann::json req_body = {
        {"model", target_model},
        {"messages", j_messages},
        {"stream", true}
    };

    std::string accumulated;
    auto stream_cb = [&](const std::string& chunk_data) {
        std::string token;
        if (extractDeltaContent(chunk_data, token)) {
            accumulated += token;
            if (on_token) on_token(token);
            return;
        }

        try {
            auto j = nlohmann::json::parse(chunk_data);
            if (j.contains("choices") && !j["choices"].empty()) {
                auto delta = j["choices"][0].value("delta", nlohmann::json::object());
                if (delta.contains("content")) {
                    token = delta["content"].get<std::string>();
                    accumulated += token;
                    if (on_token) on_token(token);
                }
            }
        } catch (...) {}
    };

    auto http_res = http_->postStream(endpoint_ + "/chat/completions", req_body.dump(), stream_cb, headers);
    ModelResponse response;
    response.content = accumulated;
    response.success = !accumulated.empty() || http_res.success;
    response.provider_name = getName();
    response.model = target_model;
    return response;
}

} // namespace aios
