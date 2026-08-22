#include <gtest/gtest.h>
#include "taskgraph/TaskGraph.h"
#include "planner/planner.h"
#include "providers/MockModelProvider.h"
#include "events/EventBus.h"
#include <atomic>
#include <thread>
#include <chrono>

using namespace aios;

// ============================================================================
// TaskGraph Structure & Topological Validation Tests
// ============================================================================

TEST(TaskGraphTest, BuildsGraphAndValidatesDependencies) {
    TaskGraph graph("Implement Feature");

    TaskNode n1; n1.id = "n1"; n1.title = "Research";
    TaskNode n2; n2.id = "n2"; n2.title = "Frontend"; n2.dependencies = {"n1"};
    TaskNode n3; n3.id = "n3"; n3.title = "Backend"; n3.dependencies = {"n1"};
    TaskNode n4; n4.id = "n4"; n4.title = "Integration Tests"; n4.dependencies = {"n2", "n3"};

    EXPECT_TRUE(graph.addNode(n1));
    EXPECT_TRUE(graph.addNode(n2));
    EXPECT_TRUE(graph.addNode(n3));
    EXPECT_TRUE(graph.addNode(n4));

    EXPECT_EQ(graph.size(), 4);
    EXPECT_FALSE(graph.hasCycle());

    auto order = graph.getTopologicalOrder();
    ASSERT_EQ(order.size(), 4);
    EXPECT_EQ(order[0], "n1");
    EXPECT_EQ(order[3], "n4");

    auto levels = graph.getExecutionLevels();
    ASSERT_EQ(levels.size(), 3);
    EXPECT_EQ(levels[0].size(), 1); // n1
    EXPECT_EQ(levels[1].size(), 2); // n2, n3
    EXPECT_EQ(levels[2].size(), 1); // n4
}

TEST(TaskGraphTest, DetectsCyclesInGraph) {
    TaskGraph graph("Cyclic Graph");

    TaskNode n1; n1.id = "n1"; n1.title = "Task 1"; n1.dependencies = {"n3"};
    TaskNode n2; n2.id = "n2"; n2.title = "Task 2"; n2.dependencies = {"n1"};
    TaskNode n3; n3.id = "n3"; n3.title = "Task 3"; n3.dependencies = {"n2"};

    graph.addNode(n1);
    graph.addNode(n2);
    graph.addNode(n3);

    EXPECT_TRUE(graph.hasCycle());
}

TEST(TaskGraphTest, JsonSerializationRoundTrip) {
    TaskGraph graph("Test Goal");
    TaskNode n1; n1.id = "step1"; n1.title = "Setup";
    TaskNode n2; n2.id = "step2"; n2.title = "Build"; n2.dependencies = {"step1"};
    graph.addNode(n1);
    graph.addNode(n2);

    std::string json_str = graph.toJsonString();
    TaskGraph reconstructed = TaskGraph::fromJsonString(json_str);

    EXPECT_EQ(reconstructed.getGoal(), "Test Goal");
    EXPECT_EQ(reconstructed.size(), 2);
    auto n2_opt = reconstructed.getNode("step2");
    ASSERT_TRUE(n2_opt.has_value());
    ASSERT_EQ(n2_opt->dependencies.size(), 1);
    EXPECT_EQ(n2_opt->dependencies[0], "step1");
}

// ============================================================================
// Concurrent TaskGraphExecutor Tests
// ============================================================================

TEST(TaskGraphTest, DetectsSelfLoopCycle) {
    TaskGraph graph("Self Loop");
    TaskNode n1; n1.id = "n1"; n1.title = "Self dependent"; n1.dependencies = {"n1"};
    graph.addNode(n1);
    EXPECT_TRUE(graph.hasCycle());
    EXPECT_TRUE(graph.hasCycles());
}

TEST(TaskGraphTest, DetectsIndirectCycleInComplexGraph) {
    TaskGraph graph("Complex Cycle");
    TaskNode n1{"n1", "Start"};
    TaskNode n2{"n2", "Step 2", "", AgentType::Generic, "", {"n1"}};
    TaskNode n3{"n3", "Step 3", "", AgentType::Generic, "", {"n2"}};
    TaskNode n4{"n4", "Step 4", "", AgentType::Generic, "", {"n3"}};
    TaskNode n5{"n5", "Independent"};

    graph.addNode(n1);
    graph.addNode(n2);
    graph.addNode(n3);
    graph.addNode(n4);
    graph.addNode(n5);

    EXPECT_FALSE(graph.hasCycle());

    // Introduce back-edge n4 -> n2 creating cycle n2 -> n3 -> n4 -> n2
    graph.addDependency("n4", "n2");
    EXPECT_TRUE(graph.hasCycle());
}

TEST(TaskGraphTest, DisconnectedComponentsCycle) {
    TaskGraph graph("Disconnected Components");
    // Component 1: DAG
    TaskNode a1{"a1", "A1"};
    TaskNode a2{"a2", "A2", "", AgentType::Generic, "", {"a1"}};
    graph.addNode(a1);
    graph.addNode(a2);

    // Component 2: Cycle
    TaskNode b1{"b1", "B1"};
    TaskNode b2{"b2", "B2", "", AgentType::Generic, "", {"b1"}};
    graph.addNode(b1);
    graph.addNode(b2);
    graph.addDependency("b2", "b1");

    EXPECT_TRUE(graph.hasCycle());
}

TEST(TaskGraphTest, LargeDAGWithoutCycles) {
    TaskGraph graph("Large Layered DAG");
    const int LAYERS = 10;
    const int NODES_PER_LAYER = 10;

    for (int l = 0; l < LAYERS; ++l) {
        for (int i = 0; i < NODES_PER_LAYER; ++i) {
            std::string id = "node_" + std::to_string(l) + "_" + std::to_string(i);
            TaskNode node{id, "Node " + id};
            if (l > 0) {
                // Depend on all nodes from previous layer
                for (int p = 0; p < NODES_PER_LAYER; ++p) {
                    node.dependencies.push_back("node_" + std::to_string(l - 1) + "_" + std::to_string(p));
                }
            }
            graph.addNode(node);
        }
    }

    EXPECT_EQ(graph.size(), LAYERS * NODES_PER_LAYER);
    EXPECT_FALSE(graph.hasCycle());

    auto order = graph.getTopologicalOrder();
    EXPECT_EQ(order.size(), LAYERS * NODES_PER_LAYER);

    auto levels = graph.getExecutionLevels();
    EXPECT_EQ(levels.size(), LAYERS);
}

// ============================================================================
// Concurrent TaskGraphExecutor Tests
// ============================================================================

TEST(TaskGraphExecutorTest, ExecutesDiamondDAGInParallel) {
    TaskGraph graph("Diamond Parallel Execution");

    TaskNode root; root.id = "root"; root.title = "Root";
    TaskNode branchA; branchA.id = "branchA"; branchA.title = "Branch A"; branchA.dependencies = {"root"};
    TaskNode branchB; branchB.id = "branchB"; branchB.title = "Branch B"; branchB.dependencies = {"root"};
    TaskNode join; join.id = "join"; join.title = "Join"; join.dependencies = {"branchA", "branchB"};

    graph.addNode(root);
    graph.addNode(branchA);
    graph.addNode(branchB);
    graph.addNode(join);

    std::atomic<int> running_concurrently{0};
    std::atomic<int> max_concurrent{0};

    TaskGraphExecutor executor;
    executor.setNodeHandler([&](TaskNode& node) -> TaskExecutionResult {
        int current = ++running_concurrently;
        int prev_max = max_concurrent.load();
        while (current > prev_max && !max_concurrent.compare_exchange_weak(prev_max, current)) {}

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        --running_concurrently;
        return {true, "Done: " + node.id, ""};
    });

    auto summary = executor.execute(graph);

    EXPECT_TRUE(summary.success);
    EXPECT_EQ(summary.completed_nodes, 4);
    EXPECT_EQ(summary.failed_nodes, 0);
    EXPECT_GE(max_concurrent.load(), 2); // branchA and branchB executed concurrently!
}

TEST(TaskGraphExecutorTest, SkipsDownstreamNodesOnFailure) {
    TaskGraph graph("Failure Invalidation Test");

    TaskNode n1; n1.id = "n1"; n1.title = "Task 1 (Fails)";
    TaskNode n2; n2.id = "n2"; n2.title = "Task 2 (Dependent on 1)"; n2.dependencies = {"n1"};
    TaskNode n3; n3.id = "n3"; n3.title = "Task 3 (Independent)";

    graph.addNode(n1);
    graph.addNode(n2);
    graph.addNode(n3);

    TaskGraphExecutor executor;
    executor.setNodeHandler([&](TaskNode& node) -> TaskExecutionResult {
        if (node.id == "n1") {
            return {false, "", "Compilation error"};
        }
        return {true, "Success", ""};
    });

    auto summary = executor.execute(graph);

    EXPECT_FALSE(summary.success);
    EXPECT_EQ(summary.failed_nodes, 1);
    EXPECT_EQ(summary.completed_nodes, 1); // n3 completed
    EXPECT_EQ(summary.skipped_nodes, 1);   // n2 skipped

    auto n2_node = graph.getNode("n2");
    ASSERT_TRUE(n2_node.has_value());
    EXPECT_EQ(n2_node->state, TaskNodeState::Skipped);
}

TEST(TaskGraphExecutorTest, HandlesHighConcurrencyLayeredMesh) {
    TaskGraph graph("150-Node Mesh");
    const int LAYERS = 10;
    const int NODES_PER_LAYER = 15;

    for (int l = 0; l < LAYERS; ++l) {
        for (int i = 0; i < NODES_PER_LAYER; ++i) {
            std::string id = "m_" + std::to_string(l) + "_" + std::to_string(i);
            TaskNode node{id, "Mesh Node " + id};
            if (l > 0) {
                // Connect to 2 nodes in previous layer
                node.dependencies.push_back("m_" + std::to_string(l - 1) + "_" + std::to_string(i % NODES_PER_LAYER));
                node.dependencies.push_back("m_" + std::to_string(l - 1) + "_" + std::to_string((i + 1) % NODES_PER_LAYER));
            }
            graph.addNode(node);
        }
    }

    EXPECT_EQ(graph.size(), 150);

    TaskGraphExecutorConfig cfg;
    cfg.max_concurrency = 16;
    TaskGraphExecutor executor(cfg);

    std::atomic<size_t> executed_count{0};
    executor.setNodeHandler([&](TaskNode& /*node*/) -> TaskExecutionResult {
        executed_count++;
        return {true, "OK", ""};
    });

    auto summary = executor.execute(graph);

    EXPECT_TRUE(summary.success);
    EXPECT_EQ(summary.completed_nodes, 150);
    EXPECT_EQ(summary.failed_nodes, 0);
    EXPECT_EQ(summary.skipped_nodes, 0);
    EXPECT_EQ(executed_count.load(), 150);
}

TEST(TaskGraphExecutorTest, HandlesRetriesSuccessfully) {
    TaskGraph graph("Retry Test");

    TaskNode n1{"n1", "Flaky Task"};
    n1.max_retries = 2;
    TaskNode n2{"n2", "Dependent Task", "", AgentType::Generic, "", {"n1"}};

    graph.addNode(n1);
    graph.addNode(n2);

    std::atomic<int> n1_attempts{0};

    TaskGraphExecutor executor;
    executor.setNodeHandler([&](TaskNode& node) -> TaskExecutionResult {
        if (node.id == "n1") {
            int attempt = ++n1_attempts;
            if (attempt <= 2) {
                return {false, "", "Temporary network failure"};
            }
            return {true, "Recovered on attempt 3", ""};
        }
        return {true, "Dependent done", ""};
    });

    auto summary = executor.execute(graph);

    EXPECT_TRUE(summary.success);
    EXPECT_EQ(summary.completed_nodes, 2);
    EXPECT_EQ(summary.failed_nodes, 0);
    EXPECT_EQ(n1_attempts.load(), 3);

    auto n1_node = graph.getNode("n1");
    ASSERT_TRUE(n1_node.has_value());
    EXPECT_EQ(n1_node->state, TaskNodeState::Completed);
    EXPECT_EQ(n1_node->retry_count, 2);
}

TEST(TaskGraphExecutorTest, SupportsCancellation) {
    TaskGraph graph("Cancellation Test");
    for (int i = 0; i < 20; ++i) {
        TaskNode n{"t_" + std::to_string(i), "Task " + std::to_string(i)};
        graph.addNode(n);
    }

    TaskGraphExecutorConfig cfg;
    cfg.max_concurrency = 4;
    TaskGraphExecutor executor(cfg);

    executor.setNodeHandler([&](TaskNode& /*node*/) -> TaskExecutionResult {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return {true, "Done", ""};
    });

    std::thread cancel_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        executor.cancel();
    });

    auto summary = executor.execute(graph);
    if (cancel_thread.joinable()) {
        cancel_thread.join();
    }

    // Executor should terminate promptly without hanging
    EXPECT_FALSE(executor.isRunning());
}

TEST(TaskGraphExecutorTest, ReusesExecutorInstance) {
    TaskGraphExecutorConfig cfg;
    cfg.max_concurrency = 4;
    TaskGraphExecutor executor(cfg);

    executor.setNodeHandler([](TaskNode& node) -> TaskExecutionResult {
        return {true, "Handled " + node.id, ""};
    });

    for (int run = 0; run < 3; ++run) {
        TaskGraph graph("Run " + std::to_string(run));
        TaskNode n1{"r" + std::to_string(run) + "_1", "Task 1"};
        TaskNode n2{"r" + std::to_string(run) + "_2", "Task 2", "", AgentType::Generic, "", {n1.id}};
        graph.addNode(n1);
        graph.addNode(n2);

        auto summary = executor.execute(graph);
        EXPECT_TRUE(summary.success);
        EXPECT_EQ(summary.completed_nodes, 2);
    }
}


// ============================================================================
// Planner Multi-Strategy Tests
// ============================================================================

TEST(PlannerTest, CreatesChainOfThoughtPlan) {
    Planner planner;
    planner.initialize();

    auto mock_provider = std::make_shared<MockModelProvider>();
    mock_provider->queueResponse(R"(```json
{
  "nodes": [
    {"id": "s1", "title": "Analyze Code", "description": "find symbols", "assigned_agent_type": 1, "dependencies": []},
    {"id": "s2", "title": "Implement Feature", "description": "write code", "assigned_agent_type": 2, "dependencies": ["s1"]},
    {"id": "s3", "title": "Validate", "description": "run tests", "assigned_agent_type": 3, "dependencies": ["s2"]}
  ]
}
```)");
    planner.setModelProvider(mock_provider);

    TaskGraph g = planner.createPlan("Add logging feature", PlanStrategyType::ChainOfThought);

    EXPECT_EQ(g.size(), 3);
    EXPECT_FALSE(g.hasCycle());
    auto nodes = g.getAllNodes();
    EXPECT_EQ(nodes.size(), 3);
}

TEST(PlannerTest, SelectsOptimalTreeOfThoughtBranch) {
    Planner planner;
    planner.initialize();

    auto mock_provider = std::make_shared<MockModelProvider>();

    // Candidate Branch 1 (Minimal)
    mock_provider->queueResponse(R"(```json
{
  "nodes": [
    {"id": "b1_1", "title": "Edit file", "assigned_agent_type": 2, "dependencies": []}
  ]
}
```)");

    // Candidate Branch 2 (Modular - has Researcher and Tester)
    mock_provider->queueResponse(R"(```json
{
  "nodes": [
    {"id": "b2_1", "title": "Research symbols", "assigned_agent_type": 1, "dependencies": []},
    {"id": "b2_2", "title": "Write module", "assigned_agent_type": 2, "dependencies": ["b2_1"]},
    {"id": "b2_3", "title": "Run tests", "assigned_agent_type": 3, "dependencies": ["b2_2"]}
  ]
}
```)");

    // Candidate Branch 3 (Defensive)
    mock_provider->queueResponse(R"(```json
{
  "nodes": [
    {"id": "b3_1", "title": "Quick Patch", "assigned_agent_type": 2, "dependencies": []}
  ]
}
```)");

    planner.setModelProvider(mock_provider);

    TaskGraph optimal_graph = planner.createPlan("Refactor Database Layer", PlanStrategyType::TreeOfThought);

    // Branch 2 had Researcher and Tester, yielding highest score
    EXPECT_EQ(optimal_graph.size(), 3);
    EXPECT_TRUE(optimal_graph.getNode("b2_1").has_value());
    EXPECT_TRUE(optimal_graph.getNode("b2_3").has_value());
}

TEST(PlannerTest, ExecutesPlanGraphEndToEnd) {
    Planner planner;
    planner.initialize();

    TaskGraph graph("Test Plan Execution");
    TaskNode n1; n1.id = "p1"; n1.title = "Step 1";
    TaskNode n2; n2.id = "p2"; n2.title = "Step 2"; n2.dependencies = {"p1"};
    graph.addNode(n1);
    graph.addNode(n2);

    auto summary = planner.executePlan(graph, [](TaskNode& node) -> TaskExecutionResult {
        return {true, "Executed " + node.id, ""};
    });

    EXPECT_TRUE(summary.success);
    EXPECT_EQ(summary.completed_nodes, 2);

    PlannerStats stats = planner.getStats();
    EXPECT_EQ(stats.executed_graphs, 1);
    EXPECT_EQ(stats.successful_graphs, 1);
}
