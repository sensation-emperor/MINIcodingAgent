// AIOS - MINI Coding Agent Operating System
// Main Entry Point

#include "kernel/Kernel.h"
#include "logging/Logger.h"
#include <iostream>
#include <csignal>

using namespace aios;

namespace {
    volatile std::sig_atomic_t g_signal_received = 0;
    
    void signal_handler(int signal) {
        g_signal_received = signal;
    }
}

void printBanner() {
    std::cout << R"(
     _  _ ___  ____ ____ _  _ ____ ____ 
     |\/| |__] |__| |  | |_/  |___ [__  
     |  | |__] |  | |__| | \_ |___ ___] 
                                        
    MINI Coding Agent - AIOS v0.1.0
    )" << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    printBanner();
    
    // Initialize logger first
    Logger::initialize("aios", LogLevel::Info);
    
    LOG_INFO("Starting MINI Coding Agent...");
    
    // Parse command line arguments
    std::string configPath = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: mini_coding_agent [options]\n"
                      << "Options:\n"
                      << "  --config <path>  Path to configuration file\n"
                      << "  --help, -h       Show this help message\n"
                      << std::endl;
            return 0;
        }
    }
    
    // Get kernel instance and initialize
    auto& kernel = Kernel::instance();
    
    if (!kernel.initialize(configPath)) {
        LOG_ERROR("Failed to initialize kernel");
        return 1;
    }
    
    // Register shutdown handler
    kernel.onShutdown([]() {
        LOG_INFO("Shutdown hook executed");
    });
    
    // Start the kernel in a separate thread so we can handle signals
    std::thread kernelThread([&kernel]() {
        kernel.run();
    });
    
    // Wait for signal or stop condition
    while (g_signal_received == 0 && kernel.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Handle signal
    if (g_signal_received != 0) {
        LOG_INFO("Received signal {}, initiating shutdown...", g_signal_received);
    }
    
    // Stop the kernel
    kernel.stop();
    
    // Wait for kernel thread to finish
    if (kernelThread.joinable()) {
        kernelThread.join();
    }
    
    LOG_INFO("MINI Coding Agent shutdown complete");
    
    return 0;
}
