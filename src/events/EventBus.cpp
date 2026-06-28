#include "events/EventBus.h"
#include <algorithm>

namespace aios {

bool EventBus::initialize() {
    return true;
}

void EventBus::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.clear();
    pendingEvents_.clear();
}

void EventBus::subscribe(const std::string& event, EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_[event].push_back(std::move(callback));
}

void EventBus::unsubscribe(const std::string& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.erase(event);
}

void EventBus::publish(const std::string& event, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingEvents_.push_back({event, data});
}

void EventBus::processEvents() {
    std::vector<EventData> eventsToProcess;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        eventsToProcess = std::move(pendingEvents_);
        pendingEvents_.clear();
    }
    
    for (const auto& eventData : eventsToProcess) {
        auto it = subscribers_.find(eventData.name);
        if (it != subscribers_.end()) {
            for (auto& callback : it->second) {
                try {
                    callback(eventData.data);
                } catch (...) {
                    // Log error but continue processing
                }
            }
        }
    }
}

} // namespace aios
