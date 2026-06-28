#pragma once
#include <string>
#include <unordered_map>
#include <any>

namespace aios {

class ConfigManager {
public:
    bool load(const std::string& path = "");
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const;
    void set(const std::string& key, const std::any& value);
    
private:
    std::unordered_map<std::string, std::any> config_;
};

template<typename T>
T ConfigManager::get(const std::string& key, const T& defaultValue) const {
    auto it = config_.find(key);
    if (it != config_.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

} // namespace aios
