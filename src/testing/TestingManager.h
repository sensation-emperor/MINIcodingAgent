#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <mutex>
#include "TestingTypes.h"
#include "TestGenerator.h"
#include "DiagnosticsEngine.h"
#include "TestRunner.h"
#include "CoverageAnalyzer.h"
#include "repository/RepositoryIndex.h"

namespace aios::testing {

class TestingManager {
public:
    static TestingManager& instance();

    TestingManager() = default;
    virtual ~TestingManager() = default;

    bool initialize(std::shared_ptr<RepositoryIndex> repo_index = nullptr);
    void shutdown();

    // Subsystem Accessors
    std::shared_ptr<TestGenerator> getGenerator() const;
    std::shared_ptr<DiagnosticsEngine> getDiagnostics() const;
    std::shared_ptr<TestRunner> getRunner() const;
    std::shared_ptr<CoverageAnalyzer> getCoverageAnalyzer() const;

    // High-Level Autonomous Test Operations
    TestingResult<GeneratedTestSuite> generateTestsForPath(const std::string& path, const TestGenOptions& opts = {});
    TestingResult<DiagnosticsReport> runStaticDiagnostics(const std::string& path);
    TestingResult<TestExecutionReport> runTestSuite(const std::string& command, const TestRunOptions& opts = {});

    // Closed-Loop Failure Diagnosis & Auto-Repair
    DiagnosticDiagnosisReport diagnoseFailure(const TestCaseResult& failed_case);
    TestingResult<std::string> generateAutoRepairPatch(const TestCaseResult& failed_case);

    // PROJECT.md Contract Compatibility Interface
    virtual std::vector<DiagnosticIssue> runDiagnostics(const std::filesystem::path& path);
    virtual std::expected<std::string, std::string> generateTests(const std::filesystem::path& source_file, const std::string& language);
    virtual std::expected<TestSuiteResult, std::string> runTests(const std::string& test_command, std::chrono::milliseconds timeout = std::chrono::seconds(60));
    virtual std::expected<double, std::string> analyzeCoverage(const std::filesystem::path& coverage_file);

private:
    std::shared_ptr<RepositoryIndex> repo_index_;
    std::shared_ptr<TestGenerator> generator_;
    std::shared_ptr<DiagnosticsEngine> diagnostics_;
    std::shared_ptr<TestRunner> runner_;
    std::shared_ptr<CoverageAnalyzer> coverage_;
    mutable std::mutex mutex_;
};

} // namespace aios::testing
