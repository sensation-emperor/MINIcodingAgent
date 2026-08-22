#include "cli/Repl.h"
#include "kernel/Kernel.h"
#include <iostream>
#include <sstream>

namespace aios {

namespace {
std::string trimString(const std::string& str) {
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

Repl::Repl(Kernel& kernel)
    : kernel_(kernel),
      session_(std::make_shared<CliSession>()) {
}

Repl::~Repl() {
    stop();
}

bool Repl::initialize() {
    TerminalRenderer::instance().initialize(true);
    CommandRegistry::instance().registerBuiltinCommands();

    line_reader_ = std::make_unique<LineReader>();
    line_reader_->setCompletionHandler([](const std::string& prefix) {
        return CommandRegistry::instance().getCompletions(prefix);
    });

    return true;
}

int Repl::run() {
    if (!line_reader_) {
        if (!initialize()) {
            return 1;
        }
    }

    running_ = true;
    TerminalRenderer::instance().printBanner();

    while (running_) {
        std::string prompt = session_->getPromptString();
        std::string line = line_reader_->readLine(prompt);

        if (line.empty() && !std::cin.good()) {
            break;
        }

        std::string trimmed = trimString(line);
        if (trimmed.empty()) {
            continue;
        }

        line_reader_->addHistory(line);

        // Special handling for /multiline toggle
        if (trimmed == "/multiline" || trimmed == "/multi") {
            bool current = line_reader_->isMultiline();
            line_reader_->setMultiline(!current);
            std::cout << TerminalRenderer::instance().colorize(
                std::string("Multiline mode ") + (!current ? "ENABLED" : "DISABLED"),
                ColorRole::BrandSecondary) << "\n";
            continue;
        }

        in_execution_ = true;
        CommandContext ctx{
            line,
            "",
            {},
            {},
            session_,
            kernel_,
            TerminalRenderer::instance(),
            true
        };

        CommandResult res = CommandRegistry::instance().dispatch(line, ctx);
        in_execution_ = false;

        session_->recordCommand(line, res.success);

        if (!res.output.empty()) {
            std::cout << res.output;
            if (res.output.back() != '\n') {
                std::cout << "\n";
            }
        }

        if (!res.error_message.empty()) {
            TerminalRenderer::instance().printCard("Error", res.error_message, ColorRole::Error);
        }

        if (res.should_exit) {
            running_ = false;
            break;
        }
    }

    if (line_reader_) {
        line_reader_->saveHistory();
    }

    std::cout << TerminalRenderer::instance().colorize("\nGracefully exiting AIOS. Goodbye!\n", ColorRole::BrandPrimary) << std::endl;
    return 0;
}

void Repl::stop() {
    running_ = false;
}

void Repl::handleInterrupt() {
    if (in_execution_) {
        std::cout << "\n" << TerminalRenderer::instance().colorize("[Interrupted by user. Active operation cancelled.]", ColorRole::Warning) << "\n";
        in_execution_ = false;
    } else {
        std::cout << "\n(To exit AIOS, press Ctrl+D or type /exit)\n";
    }
}

} // namespace aios
