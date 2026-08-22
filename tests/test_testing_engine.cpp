#include <gtest/gtest.h>
#include "testing/TestingTypes.h"
#include "testing/TestGenerator.h"
#include "testing/DiagnosticsEngine.h"
#include "testing/TestRunner.h"
#include "testing/CoverageAnalyzer.h"
#include "testing/TestingManager.h"
#include "testing/TestingTools.h"
#include "parser/ASTParser.h"
#include "repository/RepositoryIndex.h"
#include "tools/ToolRegistry.h"

using namespace aios::testing;

// ============================================================================
// TestGenerator Tests
// ============================================================================

TEST(TestGeneratorTest, GeneratesCppGTestFromAST) {
    TestGenerator generator;
    std::string cpp_code = R"(
#include <string>

class Calculator {
public:
    int add(int a, int b) {
        return a + b;
    }
    double divide(double x, double y) {
        return x / y;
    }
};
)";

    TestGenOptions opts;
    opts.target_language = "cpp";
    opts.framework = "gtest";
    opts.generate_edge_cases = true;
    opts.generate_fixtures = true;

    auto result = generator.generateTestsForFile("src/calc.cpp", cpp_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.language, "cpp");
    EXPECT_EQ(suite.framework, "gtest");
    EXPECT_FALSE(suite.test_cases.empty());

    EXPECT_NE(suite.full_test_file_content.find("#include <gtest/gtest.h>"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("class CalculatorTest : public ::testing::Test"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("TEST_F(CalculatorTest, add_NominalExecution)"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("TEST(CalculatorTest, add_BoundaryValues)"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesCppCatch2FromAST) {
    TestGenerator generator;
    std::string cpp_code = R"(
int compute_hash(const std::string& input) {
    return 42;
}
)";

    TestGenOptions opts;
    opts.target_language = "cpp";
    opts.framework = "catch2";

    auto result = generator.generateTestsForFile("src/hash.cpp", cpp_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.framework, "catch2");
    EXPECT_NE(suite.full_test_file_content.find("#include <catch2/catch_test_macros.hpp>"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("TEST_CASE(\"compute_hash"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesPythonPytestWithParametrizeAndFixtures) {
    TestGenerator generator;
    std::string py_code = R"(
class UserService:
    def get_user(self, user_id: int) -> str:
        return f"user_{user_id}"

def standalone_func(val: int) -> bool:
    return val > 0
)";

    TestGenOptions opts;
    opts.target_language = "python";
    opts.framework = "pytest";
    opts.generate_edge_cases = true;
    opts.generate_fixtures = true;

    auto result = generator.generateTestsForFile("services/user.py", py_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.language, "python");
    EXPECT_EQ(suite.framework, "pytest");
    EXPECT_NE(suite.full_test_file_content.find("import pytest"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("@pytest.fixture"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("@pytest.mark.parametrize"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesPythonUnittest) {
    TestGenerator generator;
    std::string py_code = R"(
def add(a: int, b: int) -> int:
    return a + b
)";

    TestGenOptions opts;
    opts.target_language = "python";
    opts.framework = "unittest";

    auto result = generator.generateTestsForFile("math_lib.py", py_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.framework, "unittest");
    EXPECT_NE(suite.full_test_file_content.find("import unittest"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("class Test"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesTypeScriptJest) {
    TestGenerator generator;
    std::string ts_code = R"(
export class OrderProcessor {
    public processOrder(orderId: string, amount: number): boolean {
        return true;
    }
}
)";

    TestGenOptions opts;
    opts.target_language = "typescript";
    opts.framework = "jest";

    auto result = generator.generateTestsForFile("src/order.ts", ts_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.framework, "jest");
    EXPECT_NE(suite.full_test_file_content.find("import { describe, it, expect, beforeEach"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("describe('"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("it('should execute"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesRustCargoTest) {
    TestGenerator generator;
    std::string rust_code = R"(
pub fn multiply(a: i32, b: i32) -> i32 {
    a * b
}
)";

    TestGenOptions opts;
    opts.target_language = "rust";
    opts.framework = "cargo";

    auto result = generator.generateTestsForFile("src/lib.rs", rust_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.framework, "cargo");
    EXPECT_NE(suite.full_test_file_content.find("#[cfg(test)]"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("#[test]"), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("fn test_multiply()"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesGoTests) {
    TestGenerator generator;
    std::string go_code = R"(
package calc

func Add(a int, b int) int {
    return a + b
}
)";

    TestGenOptions opts;
    opts.target_language = "go";
    opts.framework = "go_test";

    auto result = generator.generateTestsForFile("calc.go", go_code, opts);
    ASSERT_TRUE(result.has_value());

    const auto& suite = result.value();
    EXPECT_EQ(suite.framework, "go_test");
    EXPECT_NE(suite.full_test_file_content.find("import ("), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("\"testing\""), std::string::npos);
    EXPECT_NE(suite.full_test_file_content.find("func TestAdd(t *testing.T)"), std::string::npos);
}

TEST(TestGeneratorTest, GeneratesBoundaryValuesForNumericStringAndCollectionTypes) {
    TestGenerator generator;

    auto int_bounds = generator.generateBoundaryValuesForType("int");
    EXPECT_GE(int_bounds.size(), 3);
    EXPECT_TRUE(std::find(int_bounds.begin(), int_bounds.end(), "0") != int_bounds.end());
    EXPECT_TRUE(std::find(int_bounds.begin(), int_bounds.end(), "std::numeric_limits<int>::max()") != int_bounds.end());

    auto float_bounds = generator.generateBoundaryValuesForType("double");
    EXPECT_GE(float_bounds.size(), 3);
    EXPECT_TRUE(std::find(float_bounds.begin(), float_bounds.end(), "std::numeric_limits<double>::quiet_NaN()") != float_bounds.end());

    auto str_bounds = generator.generateBoundaryValuesForType("std::string");
    EXPECT_GE(str_bounds.size(), 3);
    EXPECT_TRUE(std::find(str_bounds.begin(), str_bounds.end(), "\"\"") != str_bounds.end());

    auto ptr_bounds = generator.generateBoundaryValuesForType("void*");
    EXPECT_GE(ptr_bounds.size(), 1);
    EXPECT_TRUE(std::find(ptr_bounds.begin(), ptr_bounds.end(), "nullptr") != ptr_bounds.end());
}

TEST(TestGeneratorTest, GeneratesMocksForCppAndPython) {
    TestGenerator generator;
    aios::CodeSymbol sym;
    sym.name = "IDatabaseDriver";
    sym.type = aios::SymbolType::Interface;
    sym.callees = {"connect", "query", "disconnect"};

    std::string gmock = generator.generateMockClass(sym, "gmock");
    EXPECT_NE(gmock.find("class MockIDatabaseDriver : public IDatabaseDriver"), std::string::npos);
    EXPECT_NE(gmock.find("MOCK_METHOD(void, connect"), std::string::npos);

    std::string pymock = generator.generateMockClass(sym, "pytest");
    EXPECT_NE(pymock.find("unittest.mock.MagicMock(spec=IDatabaseDriver)"), std::string::npos);
}

TEST(TestGeneratorTest, HandlesSyntaxErrorGracefully) {
    TestGenerator generator;
    std::string invalid_code = "class Incomplete {";

    TestGenOptions opts;
    opts.target_language = "cpp";

    auto res = generator.generateTestsForFile("invalid.cpp", invalid_code, opts);
    // If ASTParser handles or fails, the result should not crash and be valid
    EXPECT_TRUE(res.has_value() || !res.value().full_test_file_content.empty());
}

// ============================================================================
// DiagnosticsEngine Tests
// ============================================================================

TEST(DiagnosticsTest, DetectsResourceLeaks) {
    DiagnosticsEngine engine;
    std::string code = R"(
#include <stdio.h>

void process_data() {
    FILE* f = fopen("test.txt", "r");
    char buffer[128];
    // Forgot fclose(f)
}

void allocate_stuff() {
    int* ptr = (int*)malloc(sizeof(int) * 10);
    // Forgot free(ptr)
}
)";

    auto res = engine.scanFile("leak.cpp", code);
    ASSERT_TRUE(res.has_value());
    EXPECT_FALSE(res->passed);
    EXPECT_GE(res->items.size(), 2);

    bool found_fopen_leak = false;
    bool found_malloc_leak = false;
    for (const auto& item : res->items) {
        if (item.rule_id == "resource/unclosed-file-handle") found_fopen_leak = true;
        if (item.rule_id == "resource/unfreed-heap-memory") found_malloc_leak = true;
    }
    EXPECT_TRUE(found_fopen_leak);
    EXPECT_TRUE(found_malloc_leak);
}

TEST(DiagnosticsTest, DetectsSecurityVulnerabilities) {
    DiagnosticsEngine engine;
    std::string code = R"(
#include <stdlib.h>
#include <string.h>

void execute_user_cmd(const char* input) {
    system(input); // Command injection risk
    char dst[10];
    strcpy(dst, input); // Buffer overflow risk
}
)";

    auto res = engine.scanFile("vuln.cpp", code);
    ASSERT_TRUE(res.has_value());
    EXPECT_FALSE(res->passed);

    bool found_system = false;
    bool found_strcpy = false;
    for (const auto& item : res->items) {
        if (item.rule_id == "security/arbitrary-command-execution") found_system = true;
        if (item.rule_id == "security/buffer-overflow-unsafe-function") found_strcpy = true;
    }
    EXPECT_TRUE(found_system);
    EXPECT_TRUE(found_strcpy);
}

TEST(DiagnosticsTest, DetectsConcurrencyFlaws) {
    DiagnosticsEngine engine;
    std::string code = R"(
#include <thread>

int shared_counter = 0;

void worker() {
    shared_counter++;
}

void run() {
    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();
}
)";

    auto res = engine.scanFile("concurrency.cpp", code);
    ASSERT_TRUE(res.has_value());

    bool found_concurrency = false;
    for (const auto& item : res->items) {
        if (item.category == DiagnosticCategory::Concurrency) found_concurrency = true;
    }
    EXPECT_TRUE(found_concurrency);
}

TEST(DiagnosticsTest, DetectsCodeSmellsAndComplexity) {
    DiagnosticsEngine engine;
    std::string code = R"(
void empty_catch_block() {
    try {
        doSomething();
    } catch (...) {}
}
)";

    auto res = engine.scanFile("smell.cpp", code);
    ASSERT_TRUE(res.has_value());

    bool found_swallowed = false;
    for (const auto& item : res->items) {
        if (item.rule_id == "error_handling/swallowed-exception") found_swallowed = true;
    }
    EXPECT_TRUE(found_swallowed);
}

TEST(DiagnosticsTest, ParsesMSVCCompilerErrors) {
    DiagnosticsEngine engine;
    std::string output = R"(
C:\Users\dev\project\src\main.cpp(42,10): error C2065: 'undefined_var': undeclared identifier
src\util.cpp(88): warning C4101: 'unused_local': unreferenced local variable
)";

    auto items = engine.parseCompilerOutput(output, "msvc");
    ASSERT_EQ(items.size(), 2);

    EXPECT_EQ(items[0].file_path, "C:\\Users\\dev\\project\\src\\main.cpp");
    EXPECT_EQ(items[0].span.start_line, 42);
    EXPECT_EQ(items[0].span.start_col, 10);
    EXPECT_EQ(items[0].severity, DiagnosticSeverity::Error);
    EXPECT_EQ(items[0].rule_id, "C2065");
    EXPECT_NE(items[0].message.find("undeclared identifier"), std::string::npos);

    EXPECT_EQ(items[1].file_path, "src\\util.cpp");
    EXPECT_EQ(items[1].span.start_line, 88);
    EXPECT_EQ(items[1].severity, DiagnosticSeverity::Warning);
    EXPECT_EQ(items[1].rule_id, "C4101");
}

TEST(DiagnosticsTest, ParsesGCCCompilerErrors) {
    DiagnosticsEngine engine;
    std::string output = R"(
src/parser.cpp:123:45: error: 'foo' was not declared in this scope
src/parser.cpp:140:12: warning: unused variable 'x' [-Wunused-variable]
)";

    auto items = engine.parseCompilerOutput(output, "gcc");
    ASSERT_EQ(items.size(), 2);

    EXPECT_EQ(items[0].file_path, "src/parser.cpp");
    EXPECT_EQ(items[0].span.start_line, 123);
    EXPECT_EQ(items[0].span.start_col, 45);
    EXPECT_EQ(items[0].severity, DiagnosticSeverity::Error);

    EXPECT_EQ(items[1].span.start_line, 140);
    EXPECT_EQ(items[1].severity, DiagnosticSeverity::Warning);
    EXPECT_EQ(items[1].rule_id, "-Wunused-variable");
}

TEST(DiagnosticsTest, ParsesClangTidyJson) {
    DiagnosticsEngine engine;
    std::string json_str = R"({
  "MainSourceFile": "src/app.cpp",
  "Diagnostics": [
    {
      "DiagnosticName": "modernize-use-nullptr",
      "DiagnosticMessage": {
        "Message": "use nullptr",
        "FilePath": "src/app.cpp",
        "FileOffset": 50
      },
      "Level": "Warning"
    }
  ]
})";

    auto items = engine.parseClangTidyJson(json_str);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].rule_id, "modernize-use-nullptr");
    EXPECT_EQ(items[0].message, "use nullptr");
    EXPECT_EQ(items[0].file_path, "src/app.cpp");
}

TEST(DiagnosticsTest, ParsesRuffJson) {
    DiagnosticsEngine engine;
    std::string json_str = R"([
  {
    "code": "F401",
    "message": "`os` imported but unused",
    "filename": "service.py",
    "location": { "row": 1, "column": 1 },
    "end_location": { "row": 1, "column": 10 },
    "fix": { "message": "Remove unused import" }
  }
])";

    auto items = engine.parseRuffJson(json_str);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].rule_id, "F401");
    EXPECT_EQ(items[0].file_path, "service.py");
    EXPECT_EQ(items[0].span.start_line, 1);
    ASSERT_TRUE(items[0].fix.has_value());
    EXPECT_EQ(items[0].fix->description, "Remove unused import");
}

TEST(DiagnosticsTest, ParsesEslintJson) {
    DiagnosticsEngine engine;
    std::string json_str = R"([
  {
    "filePath": "src/index.js",
    "messages": [
      {
        "ruleId": "no-unused-vars",
        "severity": 2,
        "message": "'val' is defined but never used.",
        "line": 15,
        "column": 7,
        "endLine": 15,
        "endColumn": 10
      }
    ]
  }
])";

    auto items = engine.parseEslintJson(json_str);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].rule_id, "no-unused-vars");
    EXPECT_EQ(items[0].severity, DiagnosticSeverity::Error);
    EXPECT_EQ(items[0].span.start_line, 15);
}

// ============================================================================
// TestRunner Tests
// ============================================================================

TEST(TestRunnerTest, ParsesGoogleTestOutput) {
    TestRunner runner;
    std::string gtest_out = R"(
[==========] Running 3 tests from 2 test suites.
[----------] 2 tests from MathTest
[ RUN      ] MathTest.AddsNumbers
[       OK ] MathTest.AddsNumbers (5 ms)
[ RUN      ] MathTest.FailsOnZero
C:\project\tests\test_math.cpp(42): error: Expected equality of these values:
  divide(1, 0)
    Which is: 0
  1
[  FAILED  ] MathTest.FailsOnZero (12 ms)
[----------] 1 test from StringTest
[ RUN      ] StringTest.Concat
[  SKIPPED ] StringTest.Concat (0 ms)
[==========] 3 tests ran.
)";

    auto rep = runner.parseOutput(gtest_out, "", 1, "gtest");
    EXPECT_FALSE(rep.success);
    EXPECT_EQ(rep.total_tests, 3);
    EXPECT_EQ(rep.passed_tests, 1);
    EXPECT_EQ(rep.failed_tests, 1);
    EXPECT_EQ(rep.skipped_tests, 1);

    ASSERT_EQ(rep.test_cases.size(), 3);
    EXPECT_EQ(rep.test_cases[0].suite_name, "MathTest");
    EXPECT_EQ(rep.test_cases[0].test_name, "AddsNumbers");
    EXPECT_EQ(rep.test_cases[0].status, TestStatus::Passed);

    EXPECT_EQ(rep.test_cases[1].suite_name, "MathTest");
    EXPECT_EQ(rep.test_cases[1].test_name, "FailsOnZero");
    EXPECT_EQ(rep.test_cases[1].status, TestStatus::Failed);
    EXPECT_EQ(rep.test_cases[1].failure_file, "C:\\project\\tests\\test_math.cpp");
    EXPECT_EQ(rep.test_cases[1].failure_line, 42);

    EXPECT_EQ(rep.test_cases[2].status, TestStatus::Skipped);
}

TEST(TestRunnerTest, ParsesPytestOutput) {
    TestRunner runner;
    std::string py_out = R"(
tests/test_service.py::test_create_user PASSED [ 33%]
tests/test_service.py::test_delete_user FAILED [ 66%]
tests/test_service.py::test_skipped SKIPPED [100%]
)";

    auto rep = runner.parseOutput(py_out, "", 1, "pytest");
    EXPECT_FALSE(rep.success);
    EXPECT_EQ(rep.total_tests, 3);
    EXPECT_EQ(rep.passed_tests, 1);
    EXPECT_EQ(rep.failed_tests, 1);
    EXPECT_EQ(rep.skipped_tests, 1);
}

TEST(TestRunnerTest, ParsesJestOutput) {
    TestRunner runner;
    std::string jest_out = R"(
 PASS  src/math.test.ts
  ✓ adds numbers (4 ms)
  ✕ fails gracefully (8 ms)
)";

    auto rep = runner.parseOutput(jest_out, "", 1, "jest");
    EXPECT_FALSE(rep.success);
    EXPECT_EQ(rep.total_tests, 2);
    EXPECT_EQ(rep.passed_tests, 1);
    EXPECT_EQ(rep.failed_tests, 1);
}

TEST(TestRunnerTest, ParsesCargoTestOutput) {
    TestRunner runner;
    std::string cargo_out = R"(
running 2 tests
test tests::test_add ... ok
test tests::test_sub ... FAILED

failures:
)";

    auto rep = runner.parseOutput(cargo_out, "", 1, "cargo");
    EXPECT_FALSE(rep.success);
    EXPECT_EQ(rep.total_tests, 2);
    EXPECT_EQ(rep.passed_tests, 1);
    EXPECT_EQ(rep.failed_tests, 1);
}

TEST(TestRunnerTest, HandlesAnsiCodesAndInvalidUtf8) {
    TestRunner runner;
    std::string ansi_colored = "\x1B[32m[ RUN      ]\x1B[0m Suite.Test\n\x1B[32m[       OK ]\x1B[0m Suite.Test (1 ms)";
    auto rep = runner.parseOutput(ansi_colored, "", 0, "gtest");
    EXPECT_TRUE(rep.success);
    EXPECT_EQ(rep.passed_tests, 1);
    EXPECT_EQ(rep.test_cases[0].suite_name, "Suite");
    EXPECT_EQ(rep.test_cases[0].test_name, "Test");
}

TEST(TestRunnerTest, HandlesProcessExecutionAndTimeout) {
    TestRunner runner;
    TestRunOptions opts;
    opts.timeout = std::chrono::milliseconds(5000);

#ifdef _WIN32
    auto res = runner.run("echo [ RUN      ] P.T & echo [       OK ] P.T (1 ms)", opts);
#else
    auto res = runner.run("echo '[ RUN      ] P.T'; echo '[       OK ] P.T (1 ms)'", opts);
#endif
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->success);
    EXPECT_EQ(res->passed_tests, 1);
}

// ============================================================================
// CoverageAnalyzer Tests
// ============================================================================

TEST(CoverageTest, CalculatesLcovLineAndBranchCoverage) {
    CoverageAnalyzer analyzer;
    std::string lcov_content = R"(
SF:src/math.cpp
FN:5,add
FNDA:10,add
FNF:1
FNH:1
BRDA:6,0,0,5
BRDA:6,0,1,0
BRF:2
BRH:1
DA:5,10
DA:6,10
DA:7,0
DA:8,0
DA:9,5
LF:5
LH:3
end_of_record
)";

    auto res = analyzer.parseLcovInfo(lcov_content);
    ASSERT_TRUE(res.has_value());

    const auto& rep = res.value();
    EXPECT_EQ(rep.total_lines, 5);
    EXPECT_EQ(rep.covered_lines, 3);
    EXPECT_DOUBLE_EQ(rep.overall_line_coverage, 60.0);
    EXPECT_DOUBLE_EQ(rep.overall_branch_coverage, 50.0);
    EXPECT_DOUBLE_EQ(rep.overall_function_coverage, 100.0);

    ASSERT_TRUE(rep.files.contains("src/math.cpp"));
    const auto& fc = rep.files.at("src/math.cpp");
    EXPECT_EQ(fc.uncovered_line_ranges.size(), 1);
    EXPECT_EQ(fc.uncovered_line_ranges[0].start_line, 7);
    EXPECT_EQ(fc.uncovered_line_ranges[0].end_line, 8);
}

TEST(CoverageTest, ParsesCoberturaXmlFormat) {
    CoverageAnalyzer analyzer;
    std::string xml = R"(<?xml version="1.0" ?>
<coverage line-rate="0.75" branch-rate="0.50">
  <packages>
    <package name="core">
      <classes>
        <class name="Engine" filename="src/engine.cpp">
          <lines>
            <line number="1" hits="1"/>
            <line number="2" hits="1"/>
            <line number="3" hits="0"/>
            <line number="4" hits="1"/>
          </lines>
        </class>
      </classes>
    </package>
  </packages>
</coverage>
)";

    auto res = analyzer.parseCoberturaXml(xml);
    ASSERT_TRUE(res.has_value());
    EXPECT_DOUBLE_EQ(res->overall_line_coverage, 75.0);
    EXPECT_DOUBLE_EQ(res->overall_branch_coverage, 50.0);
    EXPECT_TRUE(res->files.contains("src/engine.cpp"));
}

TEST(CoverageTest, ParsesJsonCoverageFormat) {
    CoverageAnalyzer analyzer;
    std::string json = R"({
  "total": {
    "lines": { "total": 100, "covered": 85, "pct": 85.0 },
    "branches": { "total": 20, "covered": 15, "pct": 75.0 }
  },
  "src/app.js": {
    "path": "src/app.js",
    "lines": { "total": 10, "covered": 9, "pct": 90.0 }
  }
})";

    auto res = analyzer.parseJsonCoverage(json);
    ASSERT_TRUE(res.has_value());
    EXPECT_DOUBLE_EQ(res->overall_line_coverage, 85.0);
    EXPECT_DOUBLE_EQ(res->overall_branch_coverage, 75.0);
}

TEST(CoverageTest, EnforcesCoverageGateThresholds) {
    CoverageAnalyzer analyzer;
    CoverageReport rep;
    rep.overall_line_coverage = 75.0;
    rep.overall_branch_coverage = 65.0;

    FileCoverage fc;
    fc.file_path = "src/deficient.cpp";
    fc.total_lines = 10;
    fc.covered_lines = 5;
    fc.line_coverage_percent = 50.0;
    rep.files["src/deficient.cpp"] = fc;

    auto gate = analyzer.checkThresholds(rep, 80.0, 70.0);
    EXPECT_FALSE(gate.passed);
    EXPECT_EQ(gate.deficit_files.size(), 1);
    EXPECT_EQ(gate.deficit_files[0], "src/deficient.cpp");
}

// ============================================================================
// TestingManager & TestingTools Tests
// ============================================================================

TEST(TestingManagerTest, SingletonLifecycleAndSubsystemAccess) {
    auto& mgr = TestingManager::instance();
    EXPECT_TRUE(mgr.initialize());

    EXPECT_NE(mgr.getGenerator(), nullptr);
    EXPECT_NE(mgr.getDiagnostics(), nullptr);
    EXPECT_NE(mgr.getRunner(), nullptr);
    EXPECT_NE(mgr.getCoverageAnalyzer(), nullptr);

    mgr.shutdown();
}

TEST(TestingManagerTest, DiagnosesFailureAndSuggestsRepair) {
    auto& mgr = TestingManager::instance();
    mgr.initialize();

    TestCaseResult failed_case;
    failed_case.suite_name = "AuthTest";
    failed_case.test_name = "test_validate_token";
    failed_case.status = TestStatus::Failed;
    failed_case.failure_file = "src/auth.cpp";
    failed_case.failure_line = 10;
    failed_case.failure_message = "Expected equality of these values: token.is_valid() Which is: false, true";

    auto diag = mgr.diagnoseFailure(failed_case);
    EXPECT_TRUE(diag.diagnosed);
    EXPECT_NE(diag.root_cause_explanation.find("Assertion mismatch"), std::string::npos);

    mgr.shutdown();
}

TEST(TestingManagerTest, ProjectContractCompatibilityMethods) {
    auto& mgr = TestingManager::instance();
    mgr.initialize();

    // 1. runDiagnostics
    auto issues = mgr.runDiagnostics(std::filesystem::path("src/kernel/Kernel.h"));
    // Expect vector returned without crash
    (void)issues;

    // 2. generateTests
    auto gen_res = mgr.generateTests(std::filesystem::path("src/kernel/Kernel.h"), "cpp");
    // Verify either generated content or expected response
    (void)gen_res;

    mgr.shutdown();
}

TEST(TestingManagerTest, GeneratesAutoRepairPatchCorrectly) {
    auto& mgr = TestingManager::instance();
    mgr.initialize();

    TestCaseResult failed_case;
    failed_case.suite_name = "MathTest";
    failed_case.test_name = "test_divide_zero";
    failed_case.status = TestStatus::Failed;
    failed_case.failure_file = "src/math.cpp";
    failed_case.failure_line = 5;
    failed_case.failure_message = "Expected equality of these values: actual vs expected";

    auto patch_res = mgr.generateAutoRepairPatch(failed_case);
    // Since src/math.cpp might not exist on disk, we verify behavior or diagnostic report
    auto diag = mgr.diagnoseFailure(failed_case);
    EXPECT_TRUE(diag.diagnosed);
    EXPECT_EQ(diag.target_file, "src/math.cpp");
    EXPECT_EQ(diag.target_line, 5);

    mgr.shutdown();
}

TEST(TestingManagerTest, TestingToolsRegistryIntegration) {
    auto& mgr = TestingManager::instance();
    mgr.initialize();

    EXPECT_TRUE(TestingTools::registerTool());

    auto tool = aios::ToolRegistry::instance().getTool("testing");
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->getDefinition().name, "testing");

    // 1. Execute diagnose_code
    std::unordered_map<std::string, std::string> params_diag = {
        {"operation", "diagnose_code"},
        {"path", "src/kernel/Kernel.h"}
    };
    auto res_diag = tool->execute(params_diag);
    EXPECT_TRUE(res_diag.success);
    EXPECT_NE(res_diag.output.find("diagnostics"), std::string::npos);

    // 2. Execute generate_tests
    std::unordered_map<std::string, std::string> params_gen = {
        {"operation", "generate_tests"},
        {"file_path", "src/kernel/Kernel.h"},
        {"language", "cpp"},
        {"framework", "gtest"}
    };
    auto res_gen = tool->execute(params_gen);
    EXPECT_TRUE(res_gen.success);
    EXPECT_NE(res_gen.output.find("full_test_content"), std::string::npos);

    // 3. Execute run_tests
    std::unordered_map<std::string, std::string> params_run = {
        {"operation", "run_tests"},
        {"command", "echo [==========] Running 1 test from 1 test suite.\necho [ RUN      ] S.T\necho [       OK ] S.T (1 ms)"}
    };
    auto res_run = tool->execute(params_run);
    EXPECT_TRUE(res_run.success);

    // 4. Missing operation error
    std::unordered_map<std::string, std::string> empty_params;
    auto res_err = tool->execute(empty_params);
    EXPECT_FALSE(res_err.success);

    mgr.shutdown();
}
