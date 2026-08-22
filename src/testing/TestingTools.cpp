#include "TestingTools.h"
#include <sstream>
#include <nlohmann/json.hpp>

namespace aios::testing {

TestingTools::TestingTools(std::shared_ptr<TestingManager> manager)
    : manager_(manager ? manager : std::shared_ptr<TestingManager>(&TestingManager::instance(), [](TestingManager*){})) {
}

ToolDefinition TestingTools::getDefinition() const {
    ToolDefinition def;
    def.name = "testing";
    def.description = "Autonomous test generation, code diagnostics, test execution, coverage analysis, and auto-repair.";
    def.category = ToolCategory::Testing;
    def.required_permissions = {ToolPermission::Read, ToolPermission::Execute};
    def.parameters = {"operation", "file_path", "path", "command", "language", "framework", "timeout_ms", "min_line_pct", "failure_file", "failure_line", "failure_message", "test_name"};
    def.parameter_descriptions = {
        {"operation", "Operation to execute: generate_tests, run_tests, diagnose_code, analyze_coverage, auto_repair"},
        {"file_path", "Path to source file for test generation"},
        {"path", "Path to source file or directory for diagnostics scan"},
        {"command", "Test command or executable to run"},
        {"language", "Programming language (cpp, python, typescript, javascript, rust, go)"},
        {"framework", "Testing framework (gtest, catch2, pytest, unittest, jest, cargo, go_test)"},
        {"timeout_ms", "Execution timeout in milliseconds"},
        {"min_line_pct", "Minimum line coverage percentage gate"},
        {"failure_file", "Failing test source file path for auto-repair"},
        {"failure_line", "Failing test line number for auto-repair"},
        {"failure_message", "Failing test assertion message for auto-repair"},
        {"test_name", "Failing test name for auto-repair"}
    };
    def.parameter_required = {
        {"operation", true}
    };
    def.parameter_defaults = {
        {"timeout_ms", "60000"},
        {"min_line_pct", "80.0"}
    };
    def.returns_description = "Structured JSON containing test code, diagnostic issues, test execution metrics, coverage rates, or repair patches.";
    def.examples = {
        "testing(operation='generate_tests', file_path='src/math.cpp', language='cpp', framework='gtest')",
        "testing(operation='run_tests', command='ctest --output-on-failure')",
        "testing(operation='diagnose_code', path='src/')",
        "testing(operation='analyze_coverage', path='coverage.info')",
        "testing(operation='auto_repair', failure_file='src/math.cpp', failure_line='42', failure_message='Expected 10 got 0')"
    };
    return def;
}

bool TestingTools::registerTool(std::shared_ptr<TestingManager> manager) {
    auto tool = std::make_shared<TestingTools>(manager);
    return ToolRegistry::instance().registerTool("testing", tool);
}

ToolResult TestingTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start_time = std::chrono::steady_clock::now();

    auto it = params.find("operation");
    if (it == params.end()) {
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        recordCall(false, dur);
        return ToolResult::error("Missing mandatory 'operation' parameter.");
    }

    std::string op = it->second;
    ToolResult res;

    if (op == "generate_tests") {
        res = handleGenerateTests(params);
    } else if (op == "run_tests") {
        res = handleRunTests(params);
    } else if (op == "diagnose_code") {
        res = handleDiagnoseCode(params);
    } else if (op == "analyze_coverage") {
        res = handleAnalyzeCoverage(params);
    } else if (op == "auto_repair") {
        res = handleAutoRepair(params);
    } else {
        res = ToolResult::error("Unknown operation: " + op);
    }

    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    res.execution_time = dur;
    recordCall(res.success, dur);
    return res;
}

ToolResult TestingTools::handleGenerateTests(const std::unordered_map<std::string, std::string>& params) {
    std::string file_path;
    if (params.count("file_path")) file_path = params.at("file_path");
    else if (params.count("path")) file_path = params.at("path");
    else return ToolResult::error("Missing 'file_path' parameter for generate_tests.");

    TestGenOptions opts;
    if (params.count("language")) opts.target_language = params.at("language");
    if (params.count("framework")) opts.framework = params.at("framework");

    auto result = manager_->generateTestsForPath(file_path, opts);
    if (!result.has_value()) {
        return ToolResult::error("Test generation failed: " + result.error());
    }

    nlohmann::json j;
    j["success"] = true;
    j["file_path"] = result->file_path;
    j["language"] = result->language;
    j["framework"] = result->framework;
    j["test_cases_count"] = result->test_cases.size();
    j["full_test_content"] = result->full_test_file_content;

    return ToolResult::ok(j.dump(2));
}

ToolResult TestingTools::handleRunTests(const std::unordered_map<std::string, std::string>& params) {
    if (!params.count("command")) {
        return ToolResult::error("Missing 'command' parameter for run_tests.");
    }
    std::string cmd = params.at("command");

    TestRunOptions opts;
    if (params.count("timeout_ms")) {
        try {
            opts.timeout = std::chrono::milliseconds(std::stoull(params.at("timeout_ms")));
        } catch (...) {}
    }
    if (params.count("framework")) {
        opts.framework_hint = params.at("framework");
    }

    auto result = manager_->runTestSuite(cmd, opts);
    if (!result.has_value()) {
        return ToolResult::error("Test execution failed: " + result.error());
    }

    nlohmann::json j;
    j["success"] = result->success;
    j["total_tests"] = result->total_tests;
    j["passed_tests"] = result->passed_tests;
    j["failed_tests"] = result->failed_tests;
    j["skipped_tests"] = result->skipped_tests;
    j["duration_ms"] = result->total_duration.count();
    j["summary"] = result->summary_string;

    nlohmann::json cases = nlohmann::json::array();
    for (const auto& tc : result->test_cases) {
        nlohmann::json c;
        c["suite"] = tc.suite_name;
        c["name"] = tc.test_name;
        c["status"] = (tc.status == TestStatus::Passed) ? "Passed" : (tc.status == TestStatus::Failed ? "Failed" : "Skipped");
        c["duration_ms"] = tc.duration.count();
        if (!tc.failure_message.empty()) c["failure"] = tc.failure_message;
        cases.push_back(c);
    }
    j["test_cases"] = cases;

    return ToolResult::ok(j.dump(2));
}

ToolResult TestingTools::handleDiagnoseCode(const std::unordered_map<std::string, std::string>& params) {
    std::string path;
    if (params.count("path")) path = params.at("path");
    else if (params.count("file_path")) path = params.at("file_path");
    else return ToolResult::error("Missing 'path' parameter for diagnose_code.");

    auto result = manager_->runStaticDiagnostics(path);
    if (!result.has_value()) {
        return ToolResult::error("Diagnostics failed: " + result.error());
    }

    nlohmann::json j;
    j["passed"] = result->passed;
    j["total_errors"] = result->total_errors;
    j["total_warnings"] = result->total_warnings;
    j["total_hints"] = result->total_hints;

    nlohmann::json items = nlohmann::json::array();
    for (const auto& item : result->items) {
        nlohmann::json itm;
        itm["file"] = item.file_path;
        itm["line"] = item.span.start_line;
        itm["column"] = item.span.start_col;
        itm["rule_id"] = item.rule_id;
        itm["message"] = item.message;
        itm["severity"] = (item.severity == DiagnosticSeverity::Fatal ? "Fatal" : (item.severity == DiagnosticSeverity::Error ? "Error" : "Warning"));
        if (item.fix.has_value()) {
            itm["suggested_fix"] = item.fix->description;
        }
        items.push_back(itm);
    }
    j["diagnostics"] = items;

    return ToolResult::ok(j.dump(2));
}

ToolResult TestingTools::handleAnalyzeCoverage(const std::unordered_map<std::string, std::string>& params) {
    std::string path;
    if (params.count("path")) path = params.at("path");
    else if (params.count("file_path")) path = params.at("file_path");
    else return ToolResult::error("Missing 'path' parameter for analyze_coverage.");

    auto cov_res = manager_->analyzeCoverage(path);
    if (!cov_res.has_value()) {
        return ToolResult::error("Coverage analysis failed: " + cov_res.error());
    }

    nlohmann::json j;
    j["overall_line_coverage"] = cov_res.value();
    double min_line_pct = 80.0;
    if (params.count("min_line_pct")) {
        try { min_line_pct = std::stod(params.at("min_line_pct")); } catch (...) {}
    }
    j["gate_passed"] = (cov_res.value() >= min_line_pct);
    j["required_min_line_pct"] = min_line_pct;

    return ToolResult::ok(j.dump(2));
}

ToolResult TestingTools::handleAutoRepair(const std::unordered_map<std::string, std::string>& params) {
    TestCaseResult tc;
    if (params.count("failure_file")) tc.failure_file = params.at("failure_file");
    if (params.count("failure_line")) {
        try { tc.failure_line = std::stoi(params.at("failure_line")); } catch (...) {}
    }
    if (params.count("failure_message")) tc.failure_message = params.at("failure_message");
    if (params.count("test_name")) tc.test_name = params.at("test_name");
    tc.status = TestStatus::Failed;

    auto patch_res = manager_->generateAutoRepairPatch(tc);
    if (!patch_res.has_value()) {
        return ToolResult::error("Auto-repair patch generation failed: " + patch_res.error());
    }

    nlohmann::json j;
    j["success"] = true;
    j["patch"] = patch_res.value();
    return ToolResult::ok(j.dump(2));
}

} // namespace aios::testing
