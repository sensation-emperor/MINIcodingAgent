#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace aios {

using EventCallback = std::function<void(const std::string&)>;

class EventBus {
public:
    bool initialize();
    void shutdown();
    
    void subscribe(const std::string& event, EventCallback callback);
    void unsubscribe(const std::string& event);
    void publish(const std::string& event, const std::string& data = "");
    
    void processEvents();
    
private:
    struct EventData {
        std::string name;
        std::string data;
    };
    
    std::unordered_map<std::string, std::vector<EventCallback>> subscribers_;
    std::vector<EventData> pendingEvents_;
    std::mutex mutex_;
};

} // namespace aios
