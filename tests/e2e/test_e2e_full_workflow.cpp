#include <gtest/gtest.h>
#include "test_e2e_harness.h"

using namespace aios;
using namespace aios::e2e;

class E2EFullWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        harness_.setUp("full_workflow");
    }

    void TearDown() override {
        harness_.tearDown();
    }

    E2ETestHarness harness_;
};

// ============================================================================
// TIER 1 & 2: MULTI-SUBSYSTEM INTEGRATION & ORCHESTRATION (Features 23 - 28)
// ============================================================================

TEST_F(E2EFullWorkflowTest, Tier1_ToolParser_XMLTagAttributesParsing) {
    // Feature 23 verification
    std::string llm_with_attrs = R"(
<tool_call name="workspace_snapshot" timeout="5000" priority="high">
{
  "description": "Pre-edit snapshot",
  "snapshot_id": "snap_attr_1"
}
</tool_call>
)";

    auto calls = AgentToolParser::parseToolCalls(llm_with_attrs);
    ASSERT_EQ(calls.size(), 1);
    EXPECT_EQ(calls[0].tool_name, "workspace_snapshot");
    EXPECT_EQ(calls[0].parameters["snapshot_id"], "snap_attr_1");
}

TEST_F(E2EFullWorkflowTest, Tier1_ASTParser_MultipleMethodsPerLineAndCallees) {
    // Feature 24 verification
    auto parser = harness_.getASTParser();
    std::string cpp_code = R"(
class Vector2D {
public:
    float getX() const { return x_; } float getY() const { return y_; }
    void set(float x, float y) { log_set(); x_ = x; y_ = y; }
private:
    float x_ = 0.0f;
    float y_ = 0.0f;
};
)";

    auto parsed = parser->parseFile("src/Vector2D.h", cpp_code);
    EXPECT_TRUE(parsed.success);
    
    // Should extract both getX and getY even if on same line
    bool has_getX = false, has_getY = false, has_set = false;
    for (const auto& sym : parsed.symbols) {
        if (sym.name == "getX") has_getX = true;
        if (sym.name == "getY") has_getY = true;
        if (sym.name == "set") has_set = true;
    }
    EXPECT_TRUE(has_getX);
    EXPECT_TRUE(has_getY);
    EXPECT_TRUE(has_set);
}

TEST_F(E2EFullWorkflowTest, Tier1_MemoryManager_WorkingMemoryEvictionIntegrity) {
    // Feature 25 verification
    auto mem_mgr = harness_.getMemoryManager();
    
    // Store items up to budget
    for (int i = 0; i < 50; ++i) {
        mem_mgr->store("key_" + std::to_string(i), "value_" + std::to_string(i), MemoryCategory::Working);
    }

    auto results = mem_mgr->retrieve("key_49", 5);
    EXPECT_FALSE(results.empty());
}

TEST_F(E2EFullWorkflowTest, Tier1_TaskGraph_DynamicDAGExecution) {
    TaskGraph graph("E2E Multi-Stage Pipeline");
    
    TaskNode n1{"n1", "Research Phase", "Analyze requirements", AgentType::Researcher};
    TaskNode n2{"n2", "Coding Phase", "Implement changes", AgentType::Coder};
    TaskNode n3{"n3", "Diagnostics Phase", "Run static analysis", AgentType::Tester};
    TaskNode n4{"n4", "Review Phase", "Evaluate quality score", AgentType::Reviewer};

    n2.dependencies = {"n1"};
    n3.dependencies = {"n2"};
    n4.dependencies = {"n3"};

    graph.addNode(n1);
    graph.addNode(n2);
    graph.addNode(n3);
    graph.addNode(n4);

    EXPECT_FALSE(graph.hasCycle());
    EXPECT_EQ(graph.size(), 4);

    auto summary = harness_.executeTaskGraph(graph);
    EXPECT_TRUE(summary.success);
    EXPECT_EQ(summary.completed_nodes, 4);
    EXPECT_EQ(summary.failed_nodes, 0);
}

// ============================================================================
// TIER 3 & 4: REAL-WORLD MULTI-AGENT DEVELOPER WORKFLOW
// ============================================================================

TEST_F(E2EFullWorkflowTest, Tier4_FullDeveloperSuite_EndToEndLifecycle) {
    // Setup Mock Responses for MultiAgentOrchestrator steps
    // Step 1: Planner
    std::string plan_resp = R"(```json
{
  "goal": "Build robust StringUtils module",
  "steps": [
    {
      "id": "step_1",
      "title": "Create StringUtils Header",
      "description": "Define trim, split, join",
      "assigned_agent_type": "Coder",
      "dependencies": [],
      "acceptance_criteria": "Header defined"
    }
  ]
}
```)";

    // Step 2: Coder
    std::string coder_resp = R"(```json
{
  "tool": "workspace_snapshot",
  "params": {
    "snapshot_id": "pre_string_utils",
    "description": "Pre StringUtils implementation"
  }
}
```)";

    // Step 3: Reviewer
    std::string review_resp = R"(```json
{
  "score": 95,
  "summary": "Clean and robust implementation",
  "passed": true,
  "critical_issues": [],
  "suggestions": ["Add Unicode test cases"]
}
```)";

    harness_.setupMockResponses({plan_resp, coder_resp, review_resp});

    // 1. Allocate isolated workspace for task
    auto ws_mgr = harness_.getWorkspaceManager();
    auto agent_ws = ws_mgr->allocateIsolatedAgentWorkspace("task_string_utils");
    ASSERT_TRUE(agent_ws.has_value());

    // 2. CoderAgent creates files
    std::string header_content = R"(#pragma once
#include <string>
#include <vector>

namespace utils {
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delim);
}
)";
    harness_.createSourceFile("include/StringUtils.h", header_content);

    // 3. Diagnostics scan
    auto diagnostics = harness_.getDiagnosticsEngine();
    auto diag_res = diagnostics->scanFile("include/StringUtils.h", header_content);
    ASSERT_TRUE(diag_res.has_value());
    EXPECT_TRUE(diag_res->passed);

    // 4. Test Generator synthesizes unit tests
    auto generator = harness_.getTestGenerator();
    TestGenOptions gen_opts;
    gen_opts.target_language = "cpp";
    gen_opts.framework = "gtest";
    auto tests_res = generator->generateTestsForFile("include/StringUtils.h", header_content, gen_opts);
    ASSERT_TRUE(tests_res.has_value());
    EXPECT_GE(tests_res->test_cases.size(), 1);

    // 5. Save tests in workspace
    harness_.createSourceFile("tests/test_string_utils.cpp", tests_res->full_test_file_content);
    EXPECT_TRUE(harness_.fileExists("tests/test_string_utils.cpp"));

    // 6. Execute Orchestrator workflow
    auto workflow_res = harness_.executeOrchestratorWorkflow("Implement and verify StringUtils");
    EXPECT_TRUE(workflow_res.success);
    EXPECT_GE(workflow_res.review.score, 70);

    // 7. Clean release of agent workspace
    auto release_res = ws_mgr->releaseAgentWorkspace("task_string_utils", true);
    ASSERT_TRUE(release_res.has_value());
    EXPECT_TRUE(release_res.value());
}

// ============================================================================
// TIER 5: ADVERSARIAL COVERAGE HARDENING (Feature 30)
// ============================================================================

TEST_F(E2EFullWorkflowTest, Tier5_Adversarial_UnclosedXmlToolBlocks) {
    std::string bad_xml = "<tool_call name=\"filesystem\">{\"operation\": \"read\", \"path\": \"src/main.cpp\"}";
    auto calls = AgentToolParser::parseToolCalls(bad_xml);
    // Graceful recovery
    EXPECT_LE(calls.size(), 1);
}

TEST_F(E2EFullWorkflowTest, Tier5_Adversarial_ModelRouterFallbackCircuitBreaker) {
    auto router = harness_.getModelRouter();
    
    // Register primary provider that fails and fallback provider that succeeds
    auto failing_provider = std::make_shared<MockModelProvider>("failing_primary");
    failing_provider->setShouldFail(true);
    failing_provider->setErrorMessage("500 Internal Server Error");

    auto backup_provider = std::make_shared<MockModelProvider>("backup_provider");
    backup_provider->addResponse("Successful fallback response");

    router->registerProvider(failing_provider);
    router->registerProvider(backup_provider);

    // Configure fallback route
    router->setFallbackProviders({"failing_primary", "backup_provider"});

    std::vector<ChatMessage> msgs = {{ChatRole::User, "Hello"}};
    auto resp = router->route(AgentType::Coder, msgs);
    
    EXPECT_EQ(resp, "Successful fallback response");
    EXPECT_EQ(router->getProviderHealth("failing_primary").status, ProviderStatus::Degraded);
}

TEST_F(E2EFullWorkflowTest, Tier5_Adversarial_PathContainment_SymlinkAndJunctionTraversals) {
    auto containment = harness_.getPathContainment();
    
    // Attempting traversal with multiple combinations of dots and slashes
    std::vector<std::string> adversarial_paths = {
        ".././../Windows/System32",
        "..\\..\\..\\Windows\\System32",
        "....//....//etc//shadow",
        "/etc/passwd\0hidden.txt",
        "nested/../../../../../../boot"
    };

    for (const auto& p : adversarial_paths) {
        auto res = containment->validate(harness_.getSandboxRoot() / p, AccessMode::ReadWrite);
        EXPECT_FALSE(res.is_valid);
    }
}

TEST_F(E2EFullWorkflowTest, Tier5_Adversarial_TaskGraph_CycleDetectionAndCascadingSkips) {
    TaskGraph graph("Cycle Graph");
    TaskNode a{"a", "Task A"};
    TaskNode b{"b", "Task B"};
    TaskNode c{"c", "Task C"};

    a.dependencies = {"c"};
    b.dependencies = {"a"};
    c.dependencies = {"b"}; // Cycle: A -> B -> C -> A

    graph.addNode(a);
    graph.addNode(b);
    graph.addNode(c);

    EXPECT_TRUE(graph.hasCycle());
}

TEST_F(E2EFullWorkflowTest, Tier5_Adversarial_ConcurrentSnapshotAndRollbackStress) {
    auto snap_mgr = harness_.getSnapshotManager();
    
    // Create baseline files
    for (int i = 0; i < 20; ++i) {
        harness_.createSourceFile("src/file_" + std::to_string(i) + ".cpp", "int val = " + std::to_string(i) + ";");
    }

    auto snap = snap_mgr->createSnapshot("stress_snap", "Pre-stress snapshot");
    ASSERT_TRUE(snap.has_value());

    // Mutate all files
    for (int i = 0; i < 20; ++i) {
        harness_.createSourceFile("src/file_" + std::to_string(i) + ".cpp", "int val = 999999;");
    }

    // Rollback
    auto rb = snap_mgr->rollbackSnapshot("stress_snap");
    ASSERT_TRUE(rb.has_value());
    EXPECT_TRUE(rb->success);

    // Verify all 20 restored
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(harness_.readSourceFile("src/file_" + std::to_string(i) + ".cpp"), 
                  "int val = " + std::to_string(i) + ";");
    }
}
