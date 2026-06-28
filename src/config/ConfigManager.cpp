#include "config/ConfigManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace aios {

bool ConfigManager::load(const std::string& path) {
    std::string configPath = path.empty() ? "config.json" : path;
    
    std::ifstream file(configPath);
    if (!file.is_open()) {
        // Use default configuration
        return true;
    }
    
    try {
        nlohmann::json json;
        file >> json;
        
        for (auto& [key, value] : json.items()) {
            if (value.is_string()) {
                config_[key] = value.get<std::string>();
            } else if (value.is_number_integer()) {
                config_[key] = value.get<int>();
            } else if (value.is_number_float()) {
                config_[key] = value.get<double>();
            } else if (value.is_boolean()) {
                config_[key] = value.get<bool>();
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

void ConfigManager::set(const std::string& key, const std::any& value) {
    config_[key] = value;
}

} // namespace aios
