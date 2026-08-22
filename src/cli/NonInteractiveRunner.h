#pragma once

#include <string>
#include <vector>

namespace aios {

class Kernel;

enum class OutputFormat {
    Text,
    Json
};

class NonInteractiveRunner {
public:
    explicit NonInteractiveRunner(Kernel& kernel);

    int executeCommand(const std::string& command_line, OutputFormat format = OutputFormat::Text);
    int executeScriptFile(const std::string& script_path, bool continue_on_error = false);
    int runPipeMode(OutputFormat format = OutputFormat::Text);

private:
    Kernel& kernel_;
};

} // namespace aios
