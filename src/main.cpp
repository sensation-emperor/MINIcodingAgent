// AIOS - MINI Coding Agent Operating System
// Main Entry Point

#include "kernel/Kernel.h"
#include "logging/Logger.h"
#include "cli/Repl.h"
#include "cli/NonInteractiveRunner.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>

using namespace aios;

namespace {
    std::atomic<Repl*> g_active_repl{nullptr};
    volatile std::sig_atomic_t g_signal_received = 0;
    
    void signal_handler(int signal) {
        g_signal_received = signal;
        if (signal == SIGINT) {
            Repl* repl = g_active_repl.load();
            if (repl) {
                repl->handleInterrupt();
                return;
            }
        }
        Kernel::instance().stop();
    }
}

void printBanner() {
    std::cout << R"(
     _  _ ___  ____ ____ _  _ ____ ____ 
     |\/| |__] |__| |  | |_/  |___ [__  
     |  | |__] |  | |__| | \_ |___ ___] 
                                        
     MINI Coding Agent - AIOS Full Developer Suite v0.1.0
    )" << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers for graceful shutdown and REPL interruption
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    std::string configPath = "";
    std::string evalCommand = "";
    std::string scriptPath = "";
    bool pipeMode = false;
    bool jsonOutput = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if ((arg == "-e" || arg == "--eval") && i + 1 < argc) {
            evalCommand = argv[++i];
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            scriptPath = argv[++i];
        } else if (arg == "--pipe") {
            pipeMode = true;
        } else if (arg == "--json") {
            jsonOutput = true;
        } else if (arg == "--help" || arg == "-h") {
            printBanner();
            std::cout << "Usage: mini_coding_agent [options]\n"
                      << "Options:\n"
                      << "  -e, --eval <command>   Execute a single slash command or prompt and exit\n"
                      << "  -f, --file <script>    Execute commands from a script file and exit\n"
                      << "  --pipe                 Run in headless pipe streaming mode\n"
                      << "  --json                 Format non-interactive output as JSON\n"
                      << "  --config <path>        Path to custom configuration file\n"
                      << "  -h, --help             Show this help message\n"
                      << std::endl;
            return 0;
        }
    }

    // Only print banner in interactive mode
    if (evalCommand.empty() && scriptPath.empty() && !pipeMode) {
        printBanner();
    }
    
    // Initialize logger
    Logger::initialize("aios", LogLevel::Info);
    LOG_INFO("Starting MINI Coding Agent...");
    
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

    OutputFormat format = jsonOutput ? OutputFormat::Json : OutputFormat::Text;

    // 1. Non-interactive single-command evaluation (-e)
    if (!evalCommand.empty()) {
        NonInteractiveRunner runner(kernel);
        return runner.executeCommand(evalCommand, format);
    }

    // 2. Non-interactive script execution (-f)
    if (!scriptPath.empty()) {
        NonInteractiveRunner runner(kernel);
        return runner.executeScriptFile(scriptPath);
    }

    // 3. Headless pipe streaming mode (--pipe)
    if (pipeMode) {
        NonInteractiveRunner runner(kernel);
        return runner.runPipeMode(format);
    }

    // 4. Interactive Terminal REPL Mode (default)
    Repl repl(kernel);
    g_active_repl.store(&repl);
    int exit_code = repl.run();
    g_active_repl.store(nullptr);

    LOG_INFO("MINI Coding Agent shutdown complete");
    return exit_code;
}
