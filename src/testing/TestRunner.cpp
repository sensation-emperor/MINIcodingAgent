#include "TestRunner.h"
#include <sstream>
#include <regex>
#include <algorithm>
#include <thread>
#include <future>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

namespace aios::testing {

static std::vector<std::string> splitLines(const std::string& str) {
    std::vector<std::string> lines;
    std::istringstream stream(str);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::string TestRunner::stripAnsiCodes(const std::string& input) const {
    static const std::regex ansi_regex(R"(\x1B\[[0-9;]*[a-zA-Z])");
    return std::regex_replace(input, ansi_regex, "");
}

std::string TestRunner::sanitizeUtf8(const std::string& input) const {
    std::string clean;
    clean.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) {
            clean += static_cast<char>(c);
        } else if ((c & 0xE0) == 0xC0 && i + 1 < input.size() && (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80) {
            clean += input[i];
            clean += input[++i];
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size() &&
                   (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80 &&
                   (static_cast<unsigned char>(input[i + 2]) & 0xC0) == 0x80) {
            clean += input[i];
            clean += input[++i];
            clean += input[++i];
        } else if ((c & 0xF8) == 0xF0 && i + 3 < input.size() &&
                   (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80 &&
                   (static_cast<unsigned char>(input[i + 2]) & 0xC0) == 0x80 &&
                   (static_cast<unsigned char>(input[i + 3]) & 0xC0) == 0x80) {
            clean += input[i];
            clean += input[++i];
            clean += input[++i];
            clean += input[++i];
        } else {
            clean += "?";
        }
    }
    return clean;
}

TestingResult<TestExecutionReport> TestRunner::run(const std::string& test_command_or_executable,
                                                  const TestRunOptions& options) {
    std::string cmd = test_command_or_executable;
    if (!options.filter_expression.empty()) {
        if (options.framework_hint == "gtest" || cmd.find("gtest") != std::string::npos) {
            cmd += " --gtest_filter=" + options.filter_expression;
        } else if (options.framework_hint == "pytest" || cmd.find("pytest") != std::string::npos) {
            cmd += " -k " + options.filter_expression;
        } else if (options.framework_hint == "jest" || cmd.find("jest") != std::string::npos) {
            cmd += " -t " + options.filter_expression;
        } else if (options.framework_hint == "cargo" || cmd.find("cargo") != std::string::npos) {
            cmd += " " + options.filter_expression;
        }
    }

    auto start_time = std::chrono::steady_clock::now();

#ifdef _WIN32
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        return std::unexpected("Failed to create stdout pipe");
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return std::unexpected("Failed to create stderr pipe");
    }
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::string full_cmd = "cmd.exe /c " + cmd;
    std::vector<char> cmd_buf(full_cmd.begin(), full_cmd.end());
    cmd_buf.push_back('\0');

    std::string work_dir_str = options.working_directory.empty() ? "" : options.working_directory.string();
    const char* pWorkDir = work_dir_str.empty() ? NULL : work_dir_str.c_str();

    BOOL success = CreateProcessA(
        NULL,
        cmd_buf.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        pWorkDir,
        &si,
        &pi
    );

    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    if (!success) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        return std::unexpected("Failed to execute process: " + cmd);
    }

    // Read asynchronously
    auto readPipe = [](HANDLE hPipe) -> std::string {
        std::string result;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result.append(buffer, bytesRead);
        }
        CloseHandle(hPipe);
        return result;
    };

    auto future_stdout = std::async(std::launch::async, readPipe, hStdoutRead);
    auto future_stderr = std::async(std::launch::async, readPipe, hStderrRead);

    DWORD wait_timeout_ms = static_cast<DWORD>(options.timeout.count());
    DWORD wait_res = WaitForSingleObject(pi.hProcess, wait_timeout_ms);

    bool timed_out = false;
    DWORD exit_code = 0;

    if (wait_res == WAIT_TIMEOUT) {
        timed_out = true;
        TerminateProcess(pi.hProcess, 101);
        WaitForSingleObject(pi.hProcess, 1000);
        exit_code = 101;
    } else {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::string raw_stdout = future_stdout.get();
    std::string raw_stderr = future_stderr.get();

#else
    // POSIX fallback
    std::string raw_stdout;
    std::string raw_stderr;
    int exit_code = 0;
    bool timed_out = false;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::unexpected("Failed to open pipe for command: " + cmd);
    }
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        raw_stdout += buffer;
    }
    int status = pclose(pipe);
    exit_code = WEXITSTATUS(status);
#endif

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    TestExecutionReport report = parseOutput(raw_stdout, raw_stderr, static_cast<int>(exit_code), options.framework_hint);
    report.command = cmd;
    report.working_directory = options.working_directory;
    report.total_duration = elapsed;

    if (timed_out) {
        report.success = false;
        report.summary_string = "Execution timed out after " + std::to_string(options.timeout.count()) + " ms";
        for (auto& tc : report.test_cases) {
            if (tc.status == TestStatus::Passed) tc.status = TestStatus::Timeout;
        }
    }

    return report;
}

TestExecutionReport TestRunner::parseOutput(const std::string& stdout_str,
                                            const std::string& stderr_str,
                                            int exit_code,
                                            const std::string& framework_hint) const {
    std::string clean_stdout = sanitizeUtf8(stripAnsiCodes(stdout_str));
    std::string clean_stderr = sanitizeUtf8(stripAnsiCodes(stderr_str));

    std::string hint = framework_hint;
    std::transform(hint.begin(), hint.end(), hint.begin(), [](unsigned char c) { return std::tolower(c); });

    if (hint == "gtest" || clean_stdout.find("[==========] Running") != std::string::npos || clean_stdout.find("[ RUN      ]") != std::string::npos) {
        return parseGTest(clean_stdout, clean_stderr, exit_code);
    } else if (hint == "pytest" || clean_stdout.find("pytest") != std::string::npos || clean_stdout.find("==== FAILURES ====") != std::string::npos || clean_stdout.find("PASSED [") != std::string::npos) {
        return parsePytest(clean_stdout, clean_stderr, exit_code);
    } else if (hint == "jest" || clean_stdout.find("PASS ") != std::string::npos || clean_stdout.find("FAIL ") != std::string::npos || clean_stdout.find("Test Suites:") != std::string::npos) {
        return parseJest(clean_stdout, clean_stderr, exit_code);
    } else if (hint == "cargo" || clean_stdout.find("running ") != std::string::npos && clean_stdout.find("test result:") != std::string::npos) {
        return parseCargo(clean_stdout, clean_stderr, exit_code);
    }

    // Default fallback parser
    TestExecutionReport rep;
    rep.raw_stdout = clean_stdout;
    rep.raw_stderr = clean_stderr;
    rep.exit_code = exit_code;
    rep.success = (exit_code == 0);
    rep.total_tests = 1;
    rep.passed_tests = (exit_code == 0) ? 1 : 0;
    rep.failed_tests = (exit_code == 0) ? 0 : 1;

    TestCaseResult tc;
    tc.suite_name = "DefaultSuite";
    tc.test_name = "Execution";
    tc.status = (exit_code == 0) ? TestStatus::Passed : TestStatus::Failed;
    if (exit_code != 0) {
        tc.failure_message = clean_stderr.empty() ? clean_stdout : clean_stderr;
    }
    rep.test_cases.push_back(tc);
    return rep;
}

TestExecutionReport TestRunner::parseGTest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const {
    TestExecutionReport rep;
    rep.raw_stdout = stdout_str;
    rep.raw_stderr = stderr_str;
    rep.exit_code = exit_code;

    auto lines = splitLines(stdout_str);
    TestCaseResult current_tc;
    bool in_test = false;
    std::string current_failure_block;

    // Pattern: C:\path\file.cpp(123): error: ...
    static const std::regex error_line_regex(R"(^(.*?)\((\d+)\):\s*error:\s*(.*)$)");

    for (const auto& line : lines) {
        if (line.starts_with("[ RUN      ]")) {
            if (in_test) {
                // Save previous if somehow not closed
                rep.test_cases.push_back(current_tc);
            }
            in_test = true;
            current_failure_block.clear();
            std::string name_part = line.substr(12);
            while (!name_part.empty() && (name_part.front() == ' ' || name_part.front() == '\t')) name_part.erase(0, 1);
            auto dot_pos = name_part.find('.');
            if (dot_pos != std::string::npos) {
                current_tc.suite_name = name_part.substr(0, dot_pos);
                current_tc.test_name = name_part.substr(dot_pos + 1);
            } else {
                current_tc.suite_name = "GTestSuite";
                current_tc.test_name = name_part;
            }
            current_tc.status = TestStatus::Passed;
            current_tc.duration = std::chrono::milliseconds(0);
            current_tc.failure_message.clear();
            current_tc.failure_file.clear();
            current_tc.failure_line = 0;
            current_tc.stack_trace.clear();
            current_tc.stdout_output.clear();
        } else if (line.starts_with("[       OK ]")) {
            if (in_test) {
                current_tc.status = TestStatus::Passed;
                // Parse duration: (12 ms)
                auto open_p = line.rfind('(');
                auto close_p = line.rfind(')');
                if (open_p != std::string::npos && close_p != std::string::npos && close_p > open_p) {
                    std::string ms_str = line.substr(open_p + 1, close_p - open_p - 1);
                    try {
                        current_tc.duration = std::chrono::milliseconds(std::stoi(ms_str));
                    } catch (...) {}
                }
                rep.test_cases.push_back(current_tc);
                rep.passed_tests++;
                in_test = false;
            }
        } else if (line.starts_with("[  FAILED  ]")) {
            if (in_test) {
                current_tc.status = TestStatus::Failed;
                current_tc.failure_message = current_failure_block;
                rep.test_cases.push_back(current_tc);
                rep.failed_tests++;
                in_test = false;
            }
        } else if (line.starts_with("[  SKIPPED ]")) {
            if (in_test) {
                current_tc.status = TestStatus::Skipped;
                rep.test_cases.push_back(current_tc);
                rep.skipped_tests++;
                in_test = false;
            }
        } else if (in_test) {
            current_failure_block += line + "\n";
            std::smatch m;
            if (std::regex_search(line, m, error_line_regex)) {
                if (current_tc.failure_file.empty()) {
                    current_tc.failure_file = m[1].str();
                    try { current_tc.failure_line = std::stoi(m[2].str()); } catch (...) {}
                }
            }
        }
    }

    rep.total_tests = rep.test_cases.size();
    rep.success = (rep.failed_tests == 0 && exit_code == 0);
    rep.summary_string = "Passed: " + std::to_string(rep.passed_tests) + ", Failed: " +
                         std::to_string(rep.failed_tests) + ", Skipped: " + std::to_string(rep.skipped_tests);
    return rep;
}

TestExecutionReport TestRunner::parsePytest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const {
    TestExecutionReport rep;
    rep.raw_stdout = stdout_str;
    rep.raw_stderr = stderr_str;
    rep.exit_code = exit_code;

    auto lines = splitLines(stdout_str);
    for (const auto& line : lines) {
        auto dcolon = line.find("::");
        if (dcolon != std::string::npos) {
            TestCaseResult tc;
            tc.suite_name = line.substr(0, dcolon);
            std::string rest = line.substr(dcolon + 2);
            auto space = rest.find(' ');
            if (space != std::string::npos) {
                tc.test_name = rest.substr(0, space);
                std::string status_part = rest.substr(space);
                if (status_part.find("PASSED") != std::string::npos) {
                    tc.status = TestStatus::Passed;
                    rep.passed_tests++;
                } else if (status_part.find("FAILED") != std::string::npos) {
                    tc.status = TestStatus::Failed;
                    rep.failed_tests++;
                } else if (status_part.find("SKIPPED") != std::string::npos) {
                    tc.status = TestStatus::Skipped;
                    rep.skipped_tests++;
                }
            } else {
                tc.test_name = rest;
                tc.status = TestStatus::Passed;
                rep.passed_tests++;
            }
            rep.test_cases.push_back(tc);
        }
    }

    rep.total_tests = rep.test_cases.size();
    rep.success = (rep.failed_tests == 0 && exit_code == 0);
    rep.summary_string = "Pytest: " + std::to_string(rep.passed_tests) + " passed, " +
                         std::to_string(rep.failed_tests) + " failed";
    return rep;
}

TestExecutionReport TestRunner::parseJest(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const {
    TestExecutionReport rep;
    rep.raw_stdout = stdout_str;
    rep.raw_stderr = stderr_str;
    rep.exit_code = exit_code;

    auto lines = splitLines(stdout_str + "\n" + stderr_str);
    for (const auto& line : lines) {
        if (line.find("✓") != std::string::npos || line.find("√") != std::string::npos) {
            TestCaseResult tc;
            tc.suite_name = "JestSuite";
            tc.test_name = line;
            tc.status = TestStatus::Passed;
            rep.passed_tests++;
            rep.test_cases.push_back(tc);
        } else if (line.find("✕") != std::string::npos || line.find("×") != std::string::npos) {
            TestCaseResult tc;
            tc.suite_name = "JestSuite";
            tc.test_name = line;
            tc.status = TestStatus::Failed;
            tc.failure_message = line;
            rep.failed_tests++;
            rep.test_cases.push_back(tc);
        }
    }

    rep.total_tests = rep.test_cases.size();
    rep.success = (rep.failed_tests == 0 && exit_code == 0);
    rep.summary_string = "Jest: " + std::to_string(rep.passed_tests) + " passed, " +
                         std::to_string(rep.failed_tests) + " failed";
    return rep;
}

TestExecutionReport TestRunner::parseCargo(const std::string& stdout_str, const std::string& stderr_str, int exit_code) const {
    TestExecutionReport rep;
    rep.raw_stdout = stdout_str;
    rep.raw_stderr = stderr_str;
    rep.exit_code = exit_code;

    auto lines = splitLines(stdout_str);
    for (const auto& line : lines) {
        if (line.starts_with("test ") && (line.ends_with("... ok") || line.ends_with("... FAILED") || line.ends_with("... ignored"))) {
            TestCaseResult tc;
            tc.suite_name = "CargoTests";
            auto space_idx = line.find(' ', 5);
            if (space_idx != std::string::npos) {
                tc.test_name = line.substr(5, space_idx - 5);
            } else {
                tc.test_name = line.substr(5);
            }

            if (line.ends_with("... ok")) {
                tc.status = TestStatus::Passed;
                rep.passed_tests++;
            } else if (line.ends_with("... FAILED")) {
                tc.status = TestStatus::Failed;
                rep.failed_tests++;
            } else {
                tc.status = TestStatus::Skipped;
                rep.skipped_tests++;
            }
            rep.test_cases.push_back(tc);
        }
    }

    rep.total_tests = rep.test_cases.size();
    rep.success = (rep.failed_tests == 0 && exit_code == 0);
    rep.summary_string = "Cargo: " + std::to_string(rep.passed_tests) + " passed, " +
                         std::to_string(rep.failed_tests) + " failed";
    return rep;
}

} // namespace aios::testing
