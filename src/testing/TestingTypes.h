#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <variant>
#include <expected>

namespace aios::testing {

// Standard result alias for the testing subsystem using C++23 std::expected
template <typename T>
using TestingResult = std::expected<T, std::string>;

// ============================================================================
// Diagnostics Types
// ============================================================================

enum class DiagnosticSeverity {
    Fatal,
    Error,
    Warning,
    Information,
    Hint
};

enum class DiagnosticCategory {
    Syntax,
    Compilation,
    TypeCheck,
    Security,
    ResourceLeak,
    Concurrency,
    CodeSmell,
    Performance
};

struct SourceSpan {
    int start_line = 1;
    int start_col = 1;
    int end_line = 1;
    int end_col = 1;
};

struct SuggestedFix {
    std::string description;
    SourceSpan replacement_span;
    std::string replacement_text;
};

struct DiagnosticItem {
    std::string file_path;
    SourceSpan span;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticCategory category = DiagnosticCategory::Compilation;
    std::string rule_id;
    std::string message;
    std::string snippet;
    std::optional<SuggestedFix> fix;
};

struct DiagnosticsReport {
    std::string target_path;
    size_t total_errors = 0;
    size_t total_warnings = 0;
    size_t total_hints = 0;
    std::vector<DiagnosticItem> items;
    bool passed = true; // true if total_errors == 0
};

// PROJECT.md interface contract struct
struct DiagnosticIssue {
    enum class Severity { Info, Warning, Error, Critical };
    Severity severity = Severity::Error;
    std::string file;
    int line = 1;
    int column = 1;
    std::string message;
    std::string rule_id;
};

// ============================================================================
// Test Generator Types
// ============================================================================

struct TestGenOptions {
    std::string target_language;  // "cpp", "python", "javascript", "typescript", "rust", "go"
    std::string framework;        // "gtest", "catch2", "pytest", "unittest", "jest", "vitest", "cargo", "go_test"
    bool generate_edge_cases = true;
    bool generate_mocks = true;
    bool generate_fixtures = true;
    size_t max_tests_per_function = 5;
    std::string custom_prompt_guidance;
};

struct GeneratedTestCase {
    std::string test_name;
    std::string test_category; // "happy_path", "boundary_value", "error_handling", "concurrency", "performance"
    std::string description;
    std::string source_code;
};

struct GeneratedTestSuite {
    std::string file_path;
    std::string target_source_file;
    std::string language;
    std::string framework;
    std::string full_test_file_content;
    std::vector<GeneratedTestCase> test_cases;
    std::vector<std::string> mocked_symbols;
    bool success = true;
    std::string error_message;
};

// ============================================================================
// Test Runner Types
// ============================================================================

enum class TestStatus {
    Passed,
    Failed,
    Skipped,
    Timeout,
    Crashed
};

struct TestCaseResult {
    std::string suite_name;
    std::string test_name;
    TestStatus status = TestStatus::Passed;
    std::chrono::milliseconds duration{0};
    std::string failure_message;
    std::string failure_file;
    int failure_line = 0;
    std::string stack_trace;
    std::string stdout_output;
};

struct TestExecutionReport {
    std::string command;
    std::filesystem::path working_directory;
    bool success = false;
    size_t total_tests = 0;
    size_t passed_tests = 0;
    size_t failed_tests = 0;
    size_t skipped_tests = 0;
    std::chrono::milliseconds total_duration{0};
    std::vector<TestCaseResult> test_cases;
    std::string raw_stdout;
    std::string raw_stderr;
    int exit_code = 0;
    std::string summary_string;
};

struct TestRunOptions {
    std::filesystem::path working_directory;
    std::chrono::milliseconds timeout{60000};
    std::string filter_expression;
    std::unordered_map<std::string, std::string> environment_vars;
    std::string framework_hint = "auto"; // "gtest", "pytest", "jest", "cargo", "go_test", "auto"
};

// PROJECT.md interface contract struct
struct TestSuiteResult {
    bool success = false;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    double duration_seconds = 0.0;
    std::vector<std::string> failure_details;
};

// ============================================================================
// Coverage Analyzer Types
// ============================================================================

struct LineRange {
    int start_line = 0;
    int end_line = 0;
};

struct FileCoverage {
    std::string file_path;
    size_t total_lines = 0;
    size_t covered_lines = 0;
    double line_coverage_percent = 0.0;
    size_t total_branches = 0;
    size_t covered_branches = 0;
    double branch_coverage_percent = 0.0;
    size_t total_functions = 0;
    size_t covered_functions = 0;
    double function_coverage_percent = 0.0;
    std::vector<LineRange> uncovered_line_ranges;
};

struct CoverageReport {
    double overall_line_coverage = 0.0;
    double overall_branch_coverage = 0.0;
    double overall_function_coverage = 0.0;
    size_t total_lines = 0;
    size_t covered_lines = 0;
    std::unordered_map<std::string, FileCoverage> files;
    std::string format;
};

struct CoverageGateResult {
    bool passed = false;
    double required_line_percent = 80.0;
    double actual_line_percent = 0.0;
    double required_branch_percent = 70.0;
    double actual_branch_percent = 0.0;
    std::vector<std::string> deficit_files;
};

// ============================================================================
// Diagnosis & Auto-Repair Types
// ============================================================================

struct DiagnosticDiagnosisReport {
    bool diagnosed = false;
    std::string failure_summary;
    std::string root_cause_explanation;
    std::string target_file;
    int target_line = 0;
    std::string surrounding_code_context;
    std::string recommended_patch;
    std::vector<std::string> related_symbols;
};

} // namespace aios::testing
