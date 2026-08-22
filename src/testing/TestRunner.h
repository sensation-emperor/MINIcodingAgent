#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <filesystem>
#include "TestingTypes.h"

namespace aios::testing {

class TestRunner {
public:
    TestRunner() = default;
    ~TestRunner() = default;

    TestingResult<TestExecutionReport> run(const std::string& test_command_or_executable,
                                          const TestRunOptions& options = {});

    TestExecutionReport parseOutput(const std::string& stdout_str,
                                    const std::string& stderr_str,
                                    int exit_code,
                                    const std::string& framework_hint = "auto") const;

private:
    TestExecutionReport parseGTest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
    TestExecutionReport parsePytest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
    TestExecutionReport parseJest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;
    TestExecutionReport parseCargo(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const;

    std::string stripAnsiCodes(const std::string& input) const;
    std::string sanitizeUtf8(const std::string& input) const;
};

} // namespace aios::testing
