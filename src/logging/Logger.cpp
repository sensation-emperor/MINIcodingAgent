#include "logging/Logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <fmt/format.h>

namespace aios {

struct Logger::Impl {
    std::shared_ptr<spdlog::logger> logger;
};

void Logger::initialize(const std::string& name, LogLevel level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>(name, console_sink);
    
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    switch (level) {
        case LogLevel::Trace: logger->set_level(spdlog::level::trace); break;
        case LogLevel::Debug: logger->set_level(spdlog::level::debug); break;
        case LogLevel::Info: logger->set_level(spdlog::level::info); break;
        case LogLevel::Warn: logger->set_level(spdlog::level::warn); break;
        case LogLevel::Error: logger->set_level(spdlog::level::err); break;
        case LogLevel::Critical: logger->set_level(spdlog::level::critical); break;
    }
    
    spdlog::register_logger(logger);
    
    instance().impl_ = std::make_unique<Impl>();
    instance().impl_->logger = logger;
}

void Logger::shutdown() {
    spdlog::shutdown();
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!impl_ || !impl_->logger) {
        return;
    }
    
    switch (level) {
        case LogLevel::Trace: impl_->logger->trace(message); break;
        case LogLevel::Debug: impl_->logger->debug(message); break;
        case LogLevel::Info: impl_->logger->info(message); break;
        case LogLevel::Warn: impl_->logger->warn(message); break;
        case LogLevel::Error: impl_->logger->error(message); break;
        case LogLevel::Critical: impl_->logger->critical(message); break;
    }
}

void Logger::setLevel(LogLevel level) {
    if (impl_ && impl_->logger) {
        switch (level) {
            case LogLevel::Trace: impl_->logger->set_level(spdlog::level::trace); break;
            case LogLevel::Debug: impl_->logger->set_level(spdlog::level::debug); break;
            case LogLevel::Info: impl_->logger->set_level(spdlog::level::info); break;
            case LogLevel::Warn: impl_->logger->set_level(spdlog::level::warn); break;
            case LogLevel::Error: impl_->logger->set_level(spdlog::level::err); break;
            case LogLevel::Critical: impl_->logger->set_level(spdlog::level::critical); break;
        }
    }
}

} // namespace aios
