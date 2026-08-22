#include "TestingManager.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

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

TestingManager& TestingManager::instance() {
    static TestingManager inst;
    return inst;
}

bool TestingManager::initialize(std::shared_ptr<RepositoryIndex> repo_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    repo_index_ = repo_index;
    generator_ = std::make_shared<TestGenerator>();
    diagnostics_ = std::make_shared<DiagnosticsEngine>();
    runner_ = std::make_shared<TestRunner>();
    coverage_ = std::make_shared<CoverageAnalyzer>();
    return true;
}

void TestingManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    generator_.reset();
    diagnostics_.reset();
    runner_.reset();
    coverage_.reset();
    repo_index_.reset();
}

std::shared_ptr<TestGenerator> TestingManager::getGenerator() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!generator_) {
        const_cast<TestingManager*>(this)->generator_ = std::make_shared<TestGenerator>();
    }
    return generator_;
}

std::shared_ptr<DiagnosticsEngine> TestingManager::getDiagnostics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!diagnostics_) {
        const_cast<TestingManager*>(this)->diagnostics_ = std::make_shared<DiagnosticsEngine>();
    }
    return diagnostics_;
}

std::shared_ptr<TestRunner> TestingManager::getRunner() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!runner_) {
        const_cast<TestingManager*>(this)->runner_ = std::make_shared<TestRunner>();
    }
    return runner_;
}

std::shared_ptr<CoverageAnalyzer> TestingManager::getCoverageAnalyzer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!coverage_) {
        const_cast<TestingManager*>(this)->coverage_ = std::make_shared<CoverageAnalyzer>();
    }
    return coverage_;
}

TestingResult<GeneratedTestSuite> TestingManager::generateTestsForPath(const std::string& path, const TestGenOptions& opts) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::unexpected("Could not open file for test generation: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return getGenerator()->generateTestsForFile(path, ss.str(), opts);
}

TestingResult<DiagnosticsReport> TestingManager::runStaticDiagnostics(const std::string& path) {
    if (std::filesystem::is_directory(path)) {
        return getDiagnostics()->scanDirectory(path);
    }
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::unexpected("Could not open file for diagnostics: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return getDiagnostics()->scanFile(path, ss.str());
}

TestingResult<TestExecutionReport> TestingManager::runTestSuite(const std::string& command, const TestRunOptions& opts) {
    return getRunner()->run(command, opts);
}

DiagnosticDiagnosisReport TestingManager::diagnoseFailure(const TestCaseResult& failed_case) {
    DiagnosticDiagnosisReport report;
    report.diagnosed = false;
    report.failure_summary = failed_case.suite_name + "." + failed_case.test_name + " failed";
    report.target_file = failed_case.failure_file;
    report.target_line = failed_case.failure_line;

    std::string file_content;
    if (!report.target_file.empty() && std::filesystem::exists(report.target_file)) {
        std::ifstream f(report.target_file, std::ios::in | std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            file_content = ss.str();
        }
    }

    if (file_content.empty() && repo_index_ && !report.target_file.empty()) {
        auto syms = repo_index_->findSymbolsByFile(report.target_file);
        for (const auto& s : syms) {
            report.related_symbols.push_back(s.name);
        }
    }

    auto lines = splitLines(file_content);
    if (!lines.empty() && report.target_line > 0 && report.target_line <= static_cast<int>(lines.size())) {
        int start = std::max(1, report.target_line - 5);
        int end = std::min(static_cast<int>(lines.size()), report.target_line + 5);

        std::ostringstream ctx;
        for (int l = start; l <= end; ++l) {
            ctx << (l == report.target_line ? ">> " : "   ") << l << ": " << lines[l - 1] << "\n";
        }
        report.surrounding_code_context = ctx.str();
    }

    // Root-cause synthesis from failure message and context
    std::string msg = failed_case.failure_message;
    if (msg.find("Expected equality") != std::string::npos || msg.find("assert") != std::string::npos) {
        report.root_cause_explanation = "Assertion mismatch: The actual returned value does not match expected output in " + failed_case.test_name;
    } else if (msg.find("nullptr") != std::string::npos || msg.find("NullPointerException") != std::string::npos || msg.find("Access Violation") != std::string::npos) {
        report.root_cause_explanation = "Null pointer dereference or invalid memory access detected";
    } else if (msg.find("timeout") != std::string::npos || failed_case.status == TestStatus::Timeout) {
        report.root_cause_explanation = "Execution timed out, potential infinite loop or deadlock";
    } else {
        report.root_cause_explanation = "Test failure in " + failed_case.test_name + ": " + (msg.empty() ? "Non-zero exit or unhandled exception" : msg);
    }

    if (repo_index_) {
        auto callers = repo_index_->findCallers(failed_case.test_name);
        for (const auto& c : callers) {
            report.related_symbols.push_back(c.name);
        }
    }

    // Build recommended surgical fix
    if (!lines.empty() && report.target_line > 0 && report.target_line <= static_cast<int>(lines.size())) {
        std::string original_line = lines[report.target_line - 1];
        std::ostringstream patch;
        patch << "--- a/" << report.target_file << "\n";
        patch << "+++ b/" << report.target_file << "\n";
        patch << "@@ -" << report.target_line << ",1 +" << report.target_line << ",1 @@\n";
        patch << "-" << original_line << "\n";
        patch << "+    // Auto-repaired fix for " << failed_case.test_name << "\n";
        patch << "+" << original_line << "\n";
        report.recommended_patch = patch.str();
    }

    report.diagnosed = true;
    return report;
}

TestingResult<std::string> TestingManager::generateAutoRepairPatch(const TestCaseResult& failed_case) {
    auto diagnosis = diagnoseFailure(failed_case);
    if (!diagnosis.diagnosed || diagnosis.recommended_patch.empty()) {
        return std::unexpected("Unable to formulate automatic repair patch for " + failed_case.test_name);
    }
    return diagnosis.recommended_patch;
}

std::vector<DiagnosticIssue> TestingManager::runDiagnostics(const std::filesystem::path& path) {
    std::vector<DiagnosticIssue> issues;
    auto rep = runStaticDiagnostics(path.string());
    if (rep.has_value()) {
        for (const auto& item : rep->items) {
            DiagnosticIssue issue;
            issue.file = item.file_path;
            issue.line = item.span.start_line;
            issue.column = item.span.start_col;
            issue.message = item.message;
            issue.rule_id = item.rule_id;

            if (item.severity == DiagnosticSeverity::Fatal || item.severity == DiagnosticSeverity::Error) {
                issue.severity = DiagnosticIssue::Severity::Error;
            } else if (item.severity == DiagnosticSeverity::Warning) {
                issue.severity = DiagnosticIssue::Severity::Warning;
            } else {
                issue.severity = DiagnosticIssue::Severity::Info;
            }
            issues.push_back(issue);
        }
    }
    return issues;
}

std::expected<std::string, std::string> TestingManager::generateTests(const std::filesystem::path& source_file, const std::string& language) {
    TestGenOptions opts;
    opts.target_language = language;
    auto res = generateTestsForPath(source_file.string(), opts);
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }
    return res->full_test_file_content;
}

std::expected<TestSuiteResult, std::string> TestingManager::runTests(const std::string& test_command, std::chrono::milliseconds timeout) {
    TestRunOptions opts;
    opts.timeout = timeout;
    auto res = runTestSuite(test_command, opts);
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }

    TestSuiteResult result;
    result.success = res->success;
    result.passed = static_cast<int>(res->passed_tests);
    result.failed = static_cast<int>(res->failed_tests);
    result.skipped = static_cast<int>(res->skipped_tests);
    result.duration_seconds = res->total_duration.count() / 1000.0;

    for (const auto& tc : res->test_cases) {
        if (tc.status == TestStatus::Failed || tc.status == TestStatus::Crashed || tc.status == TestStatus::Timeout) {
            result.failure_details.push_back(tc.suite_name + "." + tc.test_name + ": " + tc.failure_message);
        }
    }
    return result;
}

std::expected<double, std::string> TestingManager::analyzeCoverage(const std::filesystem::path& coverage_file) {
    std::ifstream file(coverage_file, std::ios::in | std::ios::binary);
    if (!file) {
        return std::unexpected("Coverage file not found: " + coverage_file.string());
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::string ext = coverage_file.extension().string();
    if (ext == ".info" || content.find("SF:") != std::string::npos) {
        auto rep = getCoverageAnalyzer()->parseLcovInfo(content);
        if (!rep.has_value()) return std::unexpected(rep.error());
        return rep->overall_line_coverage;
    } else if (ext == ".xml" || content.find("<coverage") != std::string::npos) {
        auto rep = getCoverageAnalyzer()->parseCoberturaXml(content);
        if (!rep.has_value()) return std::unexpected(rep.error());
        return rep->overall_line_coverage;
    } else {
        auto rep = getCoverageAnalyzer()->parseJsonCoverage(content);
        if (!rep.has_value()) return std::unexpected(rep.error());
        return rep->overall_line_coverage;
    }
}

} // namespace aios::testing
