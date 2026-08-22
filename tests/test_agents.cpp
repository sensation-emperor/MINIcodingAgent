#include <gtest/gtest.h>
#include "agents/agents.h"
#include "agents/SpecializedAgents.h"
#include "agents/Orchestrator.h"
#include "agents/AgentToolParser.h"
#include "providers/MockModelProvider.h"
#include "tools/ToolRegistry.h"
#include "events/EventBus.h"

using namespace aios;

// ============================================================================
// AgentToolParser Tests
// ============================================================================

TEST(AgentToolParserTest, ParsesJsonMarkdownCodeBlocks) {
    std::string llm_output = R"(I will read the source file now.
```json
{
  "tool": "filesystem",
  "params": {
    "operation": "read",
    "path": "src/main.cpp"
  }
}
```
Let me know if you need more details.)";

    auto calls = AgentToolParser::parseToolCalls(llm_output);
    ASSERT_EQ(calls.size(), 1);
    EXPECT_EQ(calls[0].tool_name, "filesystem");
    EXPECT_EQ(calls[0].parameters["operation"], "read");
    EXPECT_EQ(calls[0].parameters["path"], "src/main.cpp");
    EXPECT_TRUE(calls[0].is_valid);

    std::string thought = AgentToolParser::extractThought(llm_output);
    EXPECT_NE(thought.find("I will read the source file now."), std::string::npos);
}

TEST(AgentToolParserTest, ParsesXmlTagToolCalls) {
    std::string llm_output = R"(Executing search:
<tool_call name="search">
{
  "operation": "grep",
  "query": "AgentManager"
}
</tool_call>)";

    auto calls = AgentToolParser::parseToolCalls(llm_output);
    ASSERT_EQ(calls.size(), 1);
    EXPECT_EQ(calls[0].tool_name, "search");
    EXPECT_EQ(calls[0].parameters["operation"], "grep");
    EXPECT_EQ(calls[0].parameters["query"], "AgentManager");
}

TEST(AgentToolParserTest, ParsesReActActionStyle) {
    std::string llm_output = "Thought: I need to check file list\nAction: filesystem(operation=\"list\", path=\"src\")\n";
    auto calls = AgentToolParser::parseToolCalls(llm_output);
    ASSERT_EQ(calls.size(), 1);
    EXPECT_EQ(calls[0].tool_name, "filesystem");
    EXPECT_EQ(calls[0].parameters["operation"], "list");
    EXPECT_EQ(calls[0].parameters["path"], "src");
}

TEST(AgentToolParserTest, DetectsTaskCompletion) {
    EXPECT_TRUE(AgentToolParser::isTaskComplete("Everything is done. TASK_COMPLETE"));
    EXPECT_TRUE(AgentToolParser::isTaskComplete("<task_completed/>"));
    EXPECT_TRUE(AgentToolParser::isTaskComplete("{\"status\": \"completed\"}"));
    EXPECT_FALSE(AgentToolParser::isTaskComplete("```json\n{\"tool\": \"read\"}\n```"));
}

// ============================================================================
// Specialized Agents Tests
// ============================================================================

TEST(SpecializedAgentsTest, PlannerAgentGeneratesStructuredPlan) {
    AgentConfig config;
    config.id = "planner_test";
    config.name = "Test Planner";
    
    PlannerAgent planner(config);
    auto mock_provider = std::make_shared<MockModelProvider>();
    
    std::string mock_plan_json = R"(```json
{
  "goal": "Add unit tests for agents",
  "steps": [
    {
      "id": "step_1",
      "title": "Analyze test framework",
      "description": "Inspect existing tests",
      "assigned_agent_type": "Researcher",
      "dependencies": [],
      "acceptance_criteria": "Found test structure"
    },
    {
      "id": "step_2",
      "title": "Write test cases",
      "description": "Implement test_agents.cpp",
      "assigned_agent_type": "Coder",
      "dependencies": ["step_1"],
      "acceptance_criteria": "Tests compile"
    }
  ]
}
```)";
    mock_provider->queueResponse(mock_plan_json);
    planner.setModelProvider(mock_provider);

    AgentPlanResult plan = planner.generatePlan("Add unit tests for agents");
    EXPECT_TRUE(plan.is_valid);
    EXPECT_EQ(plan.goal, "Add unit tests for agents");
    ASSERT_EQ(plan.steps.size(), 2);
    EXPECT_EQ(plan.steps[0].assigned_agent_type, "Researcher");
    EXPECT_EQ(plan.steps[1].assigned_agent_type, "Coder");
}

TEST(SpecializedAgentsTest, ReviewerAgentEvaluatesChanges) {
    AgentConfig config;
    config.id = "reviewer_test";
    config.name = "Test Reviewer";

    ReviewerAgent reviewer(config);
    auto mock_provider = std::make_shared<MockModelProvider>();

    std::string mock_review_json = R"(```json
{
  "overall_score": 92,
  "passed": true,
  "critical_issues": [],
  "suggestions": ["Add more assertions"],
  "comments": "Great implementation"
}
```)";
    mock_provider->queueResponse(mock_review_json);
    reviewer.setModelProvider(mock_provider);

    ReviewScore score = reviewer.evaluateChanges("Implement feature X", "+ int x = 42;");
    EXPECT_TRUE(score.passed);
    EXPECT_EQ(score.overall_score, 92);
    EXPECT_EQ(score.comments, "Great implementation");
    ASSERT_EQ(score.suggestions.size(), 1);
    EXPECT_EQ(score.suggestions[0], "Add more assertions");
}

TEST(SpecializedAgentsTest, DebuggerAgentDiagnosesAndFixes) {
    AgentConfig config;
    config.id = "debugger_test";
    config.name = "Test Debugger";

    DebuggerAgent debugger(config);
    auto mock_provider = std::make_shared<MockModelProvider>();
    mock_provider->queueResponse("TASK_COMPLETE: Fixed missing semicolon at line 42");
    debugger.setModelProvider(mock_provider);

    std::string fix = debugger.diagnoseAndFix("Fix compile error", "error: expected ';' at end of member declaration", "int a");
    EXPECT_NE(fix.find("Fixed missing semicolon"), std::string::npos);
}

// ============================================================================
// MultiAgentOrchestrator Tests
// ============================================================================

TEST(OrchestratorTest, RunsFullWorkflowSuccessfully) {
    auto mock_provider = std::make_shared<MockModelProvider>();

    // Step 1: Planner response
    mock_provider->queueResponse(R"json(```json
{
  "goal": "Build math utility",
  "steps": [
    {"id": "s1", "title": "Research", "description": "find math files", "assigned_agent_type": "Researcher", "dependencies": [], "acceptance_criteria": "done"},
    {"id": "s2", "title": "Code", "description": "implement add()", "assigned_agent_type": "Coder", "dependencies": ["s1"], "acceptance_criteria": "done"}
  ]
}
```)json");

    // Step 2: Researcher response
    mock_provider->queueResponse("TASK_COMPLETE: Found math header in src/math.h");

    // Step 3: Coder response
    mock_provider->queueResponse("TASK_COMPLETE: Implemented int add(int a, int b) { return a + b; }");

    // Step 4: Tester response
    mock_provider->queueResponse("TASK_COMPLETE: [PASSED] 10/10 tests passed");

    // Step 5: Reviewer response
    mock_provider->queueResponse(R"(```json
{
  "overall_score": 95,
  "passed": true,
  "critical_issues": [],
  "suggestions": [],
  "comments": "Flawless code"
}
```)");

    auto event_bus = std::make_shared<EventBus>();
    event_bus->initialize();

    std::vector<std::string> event_log;
    event_bus->subscribe("orchestrator.step_progress", [&event_log](const std::string& data) {
        event_log.push_back(data);
    });

    OrchestratorConfig config;
    config.auto_git_checkpoint = false;
    MultiAgentOrchestrator orchestrator(config);
    orchestrator.setModelProvider(mock_provider);
    orchestrator.setEventBus(event_bus);

    std::vector<std::string> progress_steps;
    orchestrator.onProgress([&progress_steps](const std::string& step, const std::string& agent, const std::string& status, const std::string&) {
        progress_steps.push_back(step + ":" + agent + ":" + status);
    });

    WorkflowResult res = orchestrator.runWorkflow("Build math utility");

    event_bus->processEvents();

    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.review.overall_score, 95);
    EXPECT_TRUE(res.review.passed);
    EXPECT_GT(progress_steps.size(), 4);
    EXPECT_GT(event_log.size(), 0);
}

TEST(OrchestratorTest, TriggersDebugLoopOnTestFailure) {
    auto mock_provider = std::make_shared<MockModelProvider>();

    // Step 1: Planner
    mock_provider->queueResponse(R"(```json
{
  "goal": "Fix bug",
  "steps": [{"id": "s1", "title": "Fix", "description": "Fix bug", "assigned_agent_type": "Coder", "dependencies": [], "acceptance_criteria": "done"}]
}
```)");

    // Step 2: Researcher
    mock_provider->queueResponse("TASK_COMPLETE: Located bug in parser");

    // Step 3: Coder first attempt
    mock_provider->queueResponse("TASK_COMPLETE: Modified parser.cpp");

    // Step 4: Tester fails on attempt 0
    mock_provider->queueResponse("FAIL: assertion failed in parser_test.cpp:45");

    // Step 5: Debugger diagnoses failure
    mock_provider->queueResponse("TASK_COMPLETE: Found off-by-one error, patch index <= len to index < len");

    // Step 6: Coder applies patch
    mock_provider->queueResponse("TASK_COMPLETE: Applied patch to parser.cpp");

    // Step 7: Tester passes on retry cycle 1
    mock_provider->queueResponse("TASK_COMPLETE: [PASSED] All tests passed");

    // Step 8: Reviewer passes
    mock_provider->queueResponse(R"(```json
{
  "overall_score": 88,
  "passed": true,
  "critical_issues": [],
  "suggestions": [],
  "comments": "Fixed correctly"
}
```)");

    OrchestratorConfig config;
    config.auto_git_checkpoint = false;
    config.max_repair_cycles = 2;

    MultiAgentOrchestrator orchestrator(config);
    orchestrator.setModelProvider(mock_provider);

    WorkflowResult res = orchestrator.runWorkflow("Fix bug in parser");

    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.repair_cycles_used, 1);
}

// ============================================================================
// AgentManager Tests
// ============================================================================

TEST(AgentManagerTest, CreatesAndExecutesSpecializedAgents) {
    auto& manager = AgentManager::instance();
    manager.initialize();

    auto mock_provider = std::make_shared<MockModelProvider>();
    mock_provider->queueResponse("TASK_COMPLETE: Executed specialized coder task");
    manager.setModelProvider(mock_provider);

    AgentConfig config;
    config.type = AgentType::Coder;
    config.name = "My Coder";

    std::string agent_id = manager.createAgent(config);
    EXPECT_FALSE(agent_id.empty());

    auto agent = manager.getAgent(agent_id);
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(agent->getType(), AgentType::Coder);

    AgentResult res = manager.executeTask(agent_id, "Write code");
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.content.find("Executed specialized coder task"), std::string::npos);

    EXPECT_TRUE(manager.removeAgent(agent_id));
    EXPECT_EQ(manager.getAgent(agent_id), nullptr);
}
