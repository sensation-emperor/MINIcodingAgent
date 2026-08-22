#pragma once

#include "cli/LineReader.h"
#include "cli/TerminalRenderer.h"
#include "cli/CommandRegistry.h"
#include "cli/CliSession.h"
#include <memory>
#include <atomic>

namespace aios {

class Kernel;

class Repl {
public:
    explicit Repl(Kernel& kernel);
    ~Repl();

    bool initialize();
    int run();
    void stop();
    void handleInterrupt(); // SIGINT handler

    bool isRunning() const { return running_; }
    std::shared_ptr<CliSession> getSession() const { return session_; }
    LineReader* getLineReader() const { return line_reader_.get(); }

private:
    Kernel& kernel_;
    std::unique_ptr<LineReader> line_reader_;
    std::shared_ptr<CliSession> session_;
    std::atomic<bool> running_{false};
    std::atomic<bool> in_execution_{false};
};

} // namespace aios
