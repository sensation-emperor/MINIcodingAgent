#include "cli/NonInteractiveRunner.h"
#include "cli/CommandRegistry.h"
#include "cli/TerminalRenderer.h"
#include "cli/CliSession.h"
#include "kernel/Kernel.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace aios {

namespace {
std::string trimLine(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}
} // namespace

NonInteractiveRunner::NonInteractiveRunner(Kernel& kernel)
    : kernel_(kernel) {
    CommandRegistry::instance().registerBuiltinCommands();
}

int NonInteractiveRunner::executeCommand(const std::string& command_line, OutputFormat format) {
    std::string trimmed = trimLine(command_line);
    if (trimmed.empty()) {
        return 0;
    }

    bool use_color = (format == OutputFormat::Text);
    TerminalRenderer::instance().setColorEnabled(use_color);

    auto session = std::make_shared<CliSession>();
    CommandContext ctx{
        trimmed,
        "",
        {},
        {},
        session,
        kernel_,
        TerminalRenderer::instance(),
        false // non-interactive
    };

    CommandResult res = CommandRegistry::instance().dispatch(trimmed, ctx);

    if (format == OutputFormat::Json) {
        nlohmann::json j;
        j["success"] = res.success;
        j["output"] = res.output;
        j["error"] = res.error_message;
        j["exit_code"] = res.exit_code;
        j["command"] = trimmed;
        std::cout << j.dump(2) << std::endl;
    } else {
        if (!res.output.empty()) {
            std::cout << res.output;
            if (res.output.back() != '\n') {
                std::cout << "\n";
            }
        }
        if (!res.error_message.empty()) {
            std::cerr << "Error: " << res.error_message << std::endl;
        }
    }

    if (!res.success) {
        return (res.exit_code != 0) ? res.exit_code : 1;
    }
    return 0;
}

int NonInteractiveRunner::executeScriptFile(const std::string& script_path, bool continue_on_error) {
    std::ifstream file(script_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open script file: " << script_path << std::endl;
        return 1;
    }

    std::string line;
    int line_number = 0;
    int last_exit_code = 0;

    while (std::getline(file, line)) {
        ++line_number;
        std::string trimmed = trimLine(line);
        if (trimmed.empty() || trimmed.starts_with("#") || trimmed.starts_with("//")) {
            continue; // Skip comments and empty lines
        }

        int exit_code = executeCommand(trimmed, OutputFormat::Text);
        if (exit_code != 0) {
            last_exit_code = exit_code;
            if (!continue_on_error) {
                std::cerr << "Script execution halted at line " << line_number << " with error code " << exit_code << std::endl;
                return exit_code;
            }
        }
    }

    return last_exit_code;
}

int NonInteractiveRunner::runPipeMode(OutputFormat format) {
    std::string line;
    int last_exit_code = 0;

    while (std::getline(std::cin, line)) {
        std::string trimmed = trimLine(line);
        if (trimmed.empty()) {
            continue;
        }
        int code = executeCommand(trimmed, format);
        if (code != 0) {
            last_exit_code = code;
        }
    }

    return last_exit_code;
}

} // namespace aios
