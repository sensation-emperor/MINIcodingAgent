#pragma once

#include <string>
#include <memory>
#include <fmt/core.h>
#include <fmt/format.h>

namespace aios {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

class Logger {
public:
    static void initialize(const std::string& name, LogLevel level = LogLevel::Info);
    static void shutdown();
    static Logger& instance();
    
    void log(LogLevel level, const std::string& message);
    void setLevel(LogLevel level);
    
private:
    Logger() = default;
    ~Logger() = default;
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Logging macros
#define LOG_TRACE(msg, ...) aios::Logger::instance().log(aios::LogLevel::Trace, fmt::format(msg, ##__VA_ARGS__))
#define LOG_DEBUG(msg, ...) aios::Logger::instance().log(aios::LogLevel::Debug, fmt::format(msg, ##__VA_ARGS__))
#define LOG_INFO(msg, ...) aios::Logger::instance().log(aios::LogLevel::Info, fmt::format(msg, ##__VA_ARGS__))
#define LOG_WARN(msg, ...) aios::Logger::instance().log(aios::LogLevel::Warn, fmt::format(msg, ##__VA_ARGS__))
#define LOG_ERROR(msg, ...) aios::Logger::instance().log(aios::LogLevel::Error, fmt::format(msg, ##__VA_ARGS__))
#define LOG_CRITICAL(msg, ...) aios::Logger::instance().log(aios::LogLevel::Critical, fmt::format(msg, ##__VA_ARGS__))

} // namespace aios
