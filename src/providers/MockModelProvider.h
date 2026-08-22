#pragma once

#include "providers/ModelProvider.h"
#include <queue>
#include <mutex>
#include <functional>

namespace aios {

class MockModelProvider : public ModelProvider {
public:
    using ResponseHandler = std::function<ModelResponse(const std::vector<Message>&)>;

    bool initialize() override { return true; }

    void queueResponse(const std::string& content, bool success = true, const std::string& error = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        response_queue_.push(ModelResponse{content, success, error});
    }

    void setCustomHandler(ResponseHandler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        custom_handler_ = std::move(handler);
    }

    ModelResponse chat(const std::vector<Message>& messages, const std::string& /*model*/ = "") override {
        std::lock_guard<std::mutex> lock(mutex_);
        recorded_calls_.push_back(messages);

        if (custom_handler_) {
            return custom_handler_(messages);
        }

        if (!response_queue_.empty()) {
            auto resp = response_queue_.front();
            response_queue_.pop();
            return resp;
        }

        return ModelResponse{"TASK_COMPLETE: Default mock response", true, ""};
    }

    std::string getName() const override { return "mock"; }

    std::vector<std::vector<Message>> getRecordedCalls() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return recorded_calls_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!response_queue_.empty()) response_queue_.pop();
        recorded_calls_.clear();
        custom_handler_ = nullptr;
    }

private:
    mutable std::mutex mutex_;
    std::queue<ModelResponse> response_queue_;
    std::vector<std::vector<Message>> recorded_calls_;
    ResponseHandler custom_handler_;
};

} // namespace aios
