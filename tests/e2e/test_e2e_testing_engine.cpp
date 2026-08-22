#include <gtest/gtest.h>
#include "test_e2e_harness.h"

using namespace aios;
using namespace aios::testing;
using namespace aios::e2e;

class E2ETestingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        harness_.setUp("testing_engine");
    }

    void TearDown() override {
        harness_.tearDown();
    }

    E2ETestHarness harness_;
};

// ============================================================================
// TIER 1: FEATURE COVERAGE (Features 15 - 22)
// ============================================================================

// ----------------------------------------------------------------------------
// Feature 15: Polyglot Test Synthesis
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_CppGTestSynthesis) {
    auto generator = harness_.getTestGenerator();
    std::string cpp_code = R"(
int add(int a, int b) { return a + b; }
class Calculator {
public:
    int multiply(int x, int y) { return x * y; }
};
)";

    TestGenOptions opts;
    opts.target_language = "cpp";
    opts.framework = "gtest";

    auto result = generator->generateTestsForFile("src/math.cpp", cpp_code, opts);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->language, "cpp");
    EXPECT_EQ(result->framework, "gtest");
    EXPECT_NE(result->full_test_file_content.find("#include <gtest/gtest.h>"), std::string::npos);
    EXPECT_NE(result->full_test_file_content.find("TEST("), std::string::npos);
    EXPECT_GE(result->test_cases.size(), 2);
}

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_PythonPytestSynthesis) {
    auto generator = harness_.getTestGenerator();
    std::string py_code = R"(
def divide(a: float, b: float) -> float:
    if b == 0:
        raise ValueError("division by zero")
    return a / b

class UserService:
    def get_user(self, user_id: int):
        return {"id": user_id, "name": "Alice"}
)";

    TestGenOptions opts;
    opts.target_language = "python";
    opts.framework = "pytest";

    auto result = generator->generateTestsForFile("src/service.py", py_code, opts);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->language, "python");
    EXPECT_EQ(result->framework, "pytest");
    EXPECT_NE(result->full_test_file_content.find("import pytest"), std::string::npos);
    EXPECT_NE(result->full_test_file_content.find("def test_"), std::string::npos);
}

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_TypeScriptJestSynthesis) {
    auto generator = harness_.getTestGenerator();
    std::string ts_code = R"(
export function formatName(first: string, last: string): string {
    return `${first} ${last}`.trim();
}
)";

    TestGenOptions opts;
    opts.target_language = "typescript";
    opts.framework = "jest";

    auto result = generator->generateTestsForFile("src/formatter.ts", ts_code, opts);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->framework, "jest");
    EXPECT_NE(result->full_test_file_content.find("describe("), std::string::npos);
    EXPECT_NE(result->full_test_file_content.find("expect("), std::string::npos);
}

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_RustCargoSynthesis) {
    auto generator = harness_.getTestGenerator();
    std::string rust_code = R"(
pub fn is_even(n: i32) -> bool {
    n % 2 == 0
}
)";

    TestGenOptions opts;
    opts.target_language = "rust";
    opts.framework = "cargo";

    auto result = generator->generateTestsForFile("src/lib.rs", rust_code, opts);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_NE(result->full_test_file_content.find("#[test]"), std::string::npos);
    EXPECT_NE(result->full_test_file_content.find("assert!("), std::string::npos);
}

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_GoTestSynthesis) {
    auto generator = harness_.getTestGenerator();
    std::string go_code = R"(
package utils

func Max(a, b int) int {
    if a > b {
        return a
    }
    return b
}
)";

    TestGenOptions opts;
    opts.target_language = "go";
    opts.framework = "go_test";

    auto result = generator->generateTestsForFile("src/utils.go", go_code, opts);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_NE(result->full_test_file_content.find("import \"testing\""), std::string::npos);
    EXPECT_NE(result->full_test_file_content.find("func Test"), std::string::npos);
}

// ----------------------------------------------------------------------------
// Feature 16: Boundary Value & Mock Generation
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_BoundaryValuesForTypes) {
    auto generator = harness_.getTestGenerator();
    
    auto int_bounds = generator->generateBoundaryValuesForType("int");
    EXPECT_GE(int_bounds.size(), 4);

    auto double_bounds = generator->generateBoundaryValuesForType("double");
    EXPECT_GE(double_bounds.size(), 4);

    auto string_bounds = generator->generateBoundaryValuesForType("std::string");
    EXPECT_GE(string_bounds.size(), 3);

    auto ptr_bounds = generator->generateBoundaryValuesForType("void*");
    EXPECT_GE(ptr_bounds.size(), 1);
    EXPECT_EQ(ptr_bounds[0], "nullptr");
}

TEST_F(E2ETestingEngineTest, Tier1_TestGenerator_MockClassGeneration) {
    auto generator = harness_.getTestGenerator();
    CodeSymbol class_sym;
    class_sym.name = "DatabaseConnection";
    class_sym.type = SymbolType::Class;
    
    CodeSymbol m1;
    m1.name = "query";
    m1.signature = "std::string query(const std::string& sql)";
    class_sym.children.push_back(m1);

    std::string gmock_code = generator->generateMockClass(class_sym, "gtest");
    EXPECT_NE(gmock_code.find("MockDatabaseConnection"), std::string::npos);
    EXPECT_NE(gmock_code.find("MOCK_METHOD"), std::string::npos);
}

// ----------------------------------------------------------------------------
// Feature 17 & 18: Static Code Diagnostics & Code Smell Scanners
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_DiagnosticsEngine_DetectsResourceLeaks) {
    auto diagnostics = harness_.getDiagnosticsEngine();
    std::string leaky_code = R"(
void processFile() {
    FILE* fp = fopen("data.txt", "r");
    // Missing fclose(fp)
}
)";

    auto report = diagnostics->scanFile("src/leak.cpp", leaky_code);
    ASSERT_TRUE(report.has_value());
    EXPECT_FALSE(report->passed);
    EXPECT_GT(report->total_errors, 0);
    
    bool found_leak_rule = false;
    for (const auto& item : report->items) {
        if (item.category == DiagnosticCategory::ResourceLeak || item.rule_id.find("LEAK") != std::string::npos) {
            found_leak_rule = true;
        }
    }
    EXPECT_TRUE(found_leak_rule);
}

TEST_F(E2ETestingEngineTest, Tier1_DiagnosticsEngine_DetectsSecuritySmells) {
    auto diagnostics = harness_.getDiagnosticsEngine();
    std::string insecure_code = R"(
void runCommand(const char* input) {
    char buf[128];
    sprintf(buf, "ls %s", input);
    system(buf);
}
)";

    auto report = diagnostics->scanFile("src/sec.cpp", insecure_code);
    ASSERT_TRUE(report.has_value());
    EXPECT_FALSE(report->passed);
    
    bool found_sec_smell = false;
    for (const auto& item : report->items) {
        if (item.category == DiagnosticCategory::Security || item.rule_id.find("SEC") != std::string::npos) {
            found_sec_smell = true;
        }
    }
    EXPECT_TRUE(found_sec_smell);
}

TEST_F(E2ETestingEngineTest, Tier1_DiagnosticsEngine_ParsesCompilerErrors) {
    auto diagnostics = harness_.getDiagnosticsEngine();
    
    // MSVC flavor
    std::string msvc_err = "c:\\src\\main.cpp(42,10): error C2065: 'undefined_var': undeclared identifier";
    auto msvc_items = diagnostics->parseCompilerOutput(msvc_err, "msvc");
    ASSERT_EQ(msvc_items.size(), 1);
    EXPECT_EQ(msvc_items[0].span.start_line, 42);
    EXPECT_EQ(msvc_items[0].span.start_col, 10);
    EXPECT_EQ(msvc_items[0].severity, DiagnosticSeverity::Error);

    // GCC / Clang flavor
    std::string gcc_err = "src/main.cpp:55:12: error: expected ';' before '}' token";
    auto gcc_items = diagnostics->parseCompilerOutput(gcc_err, "gcc");
    ASSERT_EQ(gcc_items.size(), 1);
    EXPECT_EQ(gcc_items[0].span.start_line, 55);
    EXPECT_EQ(gcc_items[0].span.start_col, 12);
}

TEST_F(E2ETestingEngineTest, Tier1_DiagnosticsEngine_ParsesLinterOutputs) {
    auto diagnostics = harness_.getDiagnosticsEngine();
    
    std::string clang_tidy_json = R"({
  "Diagnostics": [
    {
      "DiagnosticName": "modernize-use-nullptr",
      "FilePath": "src/main.cpp",
      "FileOffset": 100,
      "Message": "use nullptr"
    }
  ]
})";
    auto tidy_items = diagnostics->parseClangTidyJson(clang_tidy_json);
    ASSERT_EQ(tidy_items.size(), 1);
    EXPECT_EQ(tidy_items[0].rule_id, "modernize-use-nullptr");
}

// ----------------------------------------------------------------------------
// Feature 19: Sandboxed Test Execution & Output Normalization
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_TestRunner_ParsesGoogleTestOutput) {
    auto runner = harness_.getTestRunner();
    std::string gtest_out = R"(
[==========] Running 2 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 2 tests from MathTest
[ RUN      ] MathTest.Addition
[       OK ] MathTest.Addition (2 ms)
[ RUN      ] MathTest.Division
src/test.cpp:25: Failure
Expected equality of these values:
  divide(10, 2)
    Which is: 4
  5
[  FAILED  ] MathTest.Division (5 ms)
[----------] 2 tests from MathTest (7 ms total)
[----------] Global test environment tear-down
[==========] 2 tests from 1 test suite ran. (7 ms total)
[  PASSED  ] 1 test.
[  FAILED  ] 1 test, listed below:
[  FAILED  ] MathTest.Division

 1 FAILED TEST
)";

    auto report = runner->parseOutput(gtest_out, "", 1, "gtest");
    EXPECT_FALSE(report.success);
    EXPECT_EQ(report.total_tests, 2);
    EXPECT_EQ(report.passed_tests, 1);
    EXPECT_EQ(report.failed_tests, 1);
    ASSERT_EQ(report.test_cases.size(), 2);
    EXPECT_EQ(report.test_cases[0].status, TestStatus::Passed);
    EXPECT_EQ(report.test_cases[1].status, TestStatus::Failed);
}

TEST_F(E2ETestingEngineTest, Tier1_TestRunner_ParsesPytestOutput) {
    auto runner = harness_.getTestRunner();
    std::string pytest_out = R"(
============================= test session starts =============================
collected 3 items

tests/test_calc.py .F.                                                   [100%]

================================== FAILURES ===================================
________________________________ test_subtract ________________________________
    def test_subtract():
>       assert subtract(5, 3) == 3
E       assert 2 == 3

tests/test_calc.py:10: AssertionError
=========================== short test summary info ===========================
FAILED tests/test_calc.py::test_subtract - assert 2 == 3
========================= 1 failed, 2 passed in 0.12s =========================
)";

    auto report = runner->parseOutput(pytest_out, "", 1, "pytest");
    EXPECT_FALSE(report.success);
    EXPECT_EQ(report.total_tests, 3);
    EXPECT_EQ(report.passed_tests, 2);
    EXPECT_EQ(report.failed_tests, 1);
}

// ----------------------------------------------------------------------------
// Feature 20: Code Coverage Engine
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_CoverageAnalyzer_ParsesLcovInfo) {
    auto analyzer = harness_.getCoverageAnalyzer();
    std::string lcov_data = R"(
TN:
SF:src/math.cpp
FN:5,add
FNF:1
FNH:1
DA:5,10
DA:6,10
DA:7,0
LF:3
LH:2
BRDA:6,0,0,5
BRDA:6,0,1,0
BRF:2
BRH:1
end_of_record
)";

    auto res = analyzer->parseLcovInfo(lcov_data);
    ASSERT_TRUE(res.has_value());
    EXPECT_NEAR(res->overall_line_coverage, 66.66, 1.0);
    EXPECT_NEAR(res->overall_branch_coverage, 50.0, 1.0);
    EXPECT_EQ(res->total_lines, 3);
    EXPECT_EQ(res->covered_lines, 2);
}

TEST_F(E2ETestingEngineTest, Tier1_CoverageAnalyzer_ChecksThresholds) {
    auto analyzer = harness_.getCoverageAnalyzer();
    CoverageReport report;
    report.overall_line_coverage = 85.0;
    report.overall_branch_coverage = 75.0;

    auto pass_gate = analyzer->checkThresholds(report, 80.0, 70.0);
    EXPECT_TRUE(pass_gate.passed);

    auto fail_gate = analyzer->checkThresholds(report, 90.0, 70.0);
    EXPECT_FALSE(fail_gate.passed);
}

// ----------------------------------------------------------------------------
// Feature 21: Closed-Loop Failure Diagnosis & Auto-Repair
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_TestingManager_DiagnosesTestFailure) {
    auto testing_mgr = harness_.getTestingManager();
    
    TestCaseResult failed_case;
    failed_case.suite_name = "AuthTest";
    failed_case.test_name = "LoginValidatesToken";
    failed_case.status = TestStatus::Failed;
    failed_case.failure_message = "Expected token != nullptr but was null";
    failed_case.failure_file = "src/auth.cpp";
    failed_case.failure_line = 45;

    auto diagnosis = testing_mgr->diagnoseFailure(failed_case);
    EXPECT_TRUE(diagnosis.diagnosed);
    EXPECT_EQ(diagnosis.target_file, "src/auth.cpp");
    EXPECT_EQ(diagnosis.target_line, 45);
    EXPECT_FALSE(diagnosis.root_cause_explanation.empty());

    auto patch = testing_mgr->generateAutoRepairPatch(failed_case);
    ASSERT_TRUE(patch.has_value());
    EXPECT_FALSE(patch->empty());
}

// ----------------------------------------------------------------------------
// Feature 22: Testing Tools & ToolRegistry Integration
// ----------------------------------------------------------------------------

TEST_F(E2ETestingEngineTest, Tier1_TestingTools_ToolRegistryExecution) {
    auto tool_registry = harness_.getToolRegistry();
    EXPECT_TRUE(tool_registry->hasTool("testing_generate_tests"));
    EXPECT_TRUE(tool_registry->hasTool("testing_run_diagnostics"));
    EXPECT_TRUE(tool_registry->hasTool("testing_run_tests"));
    EXPECT_TRUE(tool_registry->hasTool("testing_analyze_coverage"));
    EXPECT_TRUE(tool_registry->hasTool("testing_auto_repair"));

    harness_.createSourceFile("src/diag_me.cpp", "int cleanFunction() { return 1; }");
    
    nlohmann::json diag_params;
    diag_params["path"] = (harness_.getSandboxRoot() / "src/diag_me.cpp").string();

    auto res = tool_registry->executeTool("testing_run_diagnostics", diag_params.dump());
    EXPECT_TRUE(res.success);
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASES
// ============================================================================

TEST_F(E2ETestingEngineTest, Tier2_TestGenerator_EmptyAndMalformedSourceFile) {
    auto generator = harness_.getTestGenerator();
    auto res_empty = generator->generateTestsForFile("src/empty.cpp", "", {});
    ASSERT_TRUE(res_empty.has_value());
    EXPECT_TRUE(res_empty->test_cases.empty());
}

TEST_F(E2ETestingEngineTest, Tier2_DiagnosticsEngine_ExtremeFileSizeAndComplexity) {
    auto diagnostics = harness_.getDiagnosticsEngine();
    std::string massive_file = "int global_v = 0;\n";
    for (int i = 0; i < 3000; ++i) {
        massive_file += "int func_" + std::to_string(i) + "() { return " + std::to_string(i) + "; }\n";
    }

    auto report = diagnostics->scanFile("src/massive.cpp", massive_file);
    ASSERT_TRUE(report.has_value());
}

TEST_F(E2ETestingEngineTest, Tier2_CoverageAnalyzer_CorruptedLcovData) {
    auto analyzer = harness_.getCoverageAnalyzer();
    std::string corrupted_lcov = "SF:broken\nINVALID_KEY:NaN\nDA:abc,xyz\nend_of_record";
    auto res = analyzer->parseLcovInfo(corrupted_lcov);
    // Should parse gracefully without crashing
    ASSERT_TRUE(res.has_value());
}

TEST_F(E2ETestingEngineTest, Tier2_TestRunner_StripAnsiCodesSanitization) {
    auto runner = harness_.getTestRunner();
    std::string ansi_output = "\033[32m[  PASSED  ]\033[0m 1 test.\n";
    auto report = runner->parseOutput(ansi_output, "", 0, "gtest");
    EXPECT_TRUE(report.success);
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATIONS
// ============================================================================

TEST_F(E2ETestingEngineTest, Tier3_ProjectContract_VirtualMethods) {
    auto testing_mgr = harness_.getTestingManager();
    auto test_file = harness_.createSourceFile("src/contract.cpp", "int val() { return 10; }");

    auto diag_issues = testing_mgr->runDiagnostics(test_file);
    EXPECT_TRUE(diag_issues.empty()); // Clean file

    auto gen_res = testing_mgr->generateTests(test_file, "cpp");
    ASSERT_TRUE(gen_res.has_value());
    EXPECT_FALSE(gen_res->empty());
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO
// ============================================================================

TEST_F(E2ETestingEngineTest, Tier4_ClosedLoopAutoRemediationWorkflow) {
    auto testing_mgr = harness_.getTestingManager();
    auto generator = harness_.getTestGenerator();
    auto diagnostics = harness_.getDiagnosticsEngine();

    // 1. CoderAgent creates code with potential defect
    std::string source_code = R"(
int computeFactorial(int n) {
    if (n <= 1) return 1;
    return n * computeFactorial(n - 1);
}
)";
    harness_.createSourceFile("src/factorial.cpp", source_code);

    // 2. Diagnostics checks static rules
    auto diag_report = diagnostics->scanFile("src/factorial.cpp", source_code);
    ASSERT_TRUE(diag_report.has_value());
    EXPECT_TRUE(diag_report->passed);

    // 3. TestGenerator synthesizes test suite
    TestGenOptions opts;
    opts.target_language = "cpp";
    opts.framework = "gtest";
    auto test_suite = generator->generateTestsForFile("src/factorial.cpp", source_code, opts);
    ASSERT_TRUE(test_suite.has_value());
    EXPECT_GE(test_suite->test_cases.size(), 1);

    // 4. Simulate a test failure scenario
    TestCaseResult fail_case;
    fail_case.suite_name = "FactorialTest";
    fail_case.test_name = "HandlesNegativeInput";
    fail_case.status = TestStatus::Failed;
    fail_case.failure_message = "Stack overflow on negative input";
    fail_case.failure_file = "src/factorial.cpp";
    fail_case.failure_line = 3;

    // 5. Closed-loop diagnosis & repair patch generation
    auto diagnosis = testing_mgr->diagnoseFailure(fail_case);
    EXPECT_TRUE(diagnosis.diagnosed);
    
    auto patch = testing_mgr->generateAutoRepairPatch(fail_case);
    ASSERT_TRUE(patch.has_value());
    EXPECT_FALSE(patch->empty());
}
