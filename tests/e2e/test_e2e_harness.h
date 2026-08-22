#pragma once

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>

// Subsystem headers
#include "kernel/Kernel.h"
#include "events/EventBus.h"
#include "tools/ToolRegistry.h"
#include "providers/ModelRouter.h"
#include "providers/MockModelProvider.h"
#include "memory/memory.h"
#include "agents/Orchestrator.h"
#include "agents/SpecializedAgents.h"
#include "agents/AgentToolParser.h"
#include "taskgraph/TaskGraph.h"
#include "parser/ASTParser.h"
#include "repository/RepositoryIndex.h"

// Full Developer Suite Subsystems
#include "cli/SlashCommand.h"
#include "cli/CommandRegistry.h"
#include "cli/CliSession.h"
#include "cli/TerminalRenderer.h"
#include "cli/LineReader.h"

#include "workspace/WorkspaceTypes.h"
#include "workspace/PathContainment.h"
#include "workspace/SnapshotManager.h"
#include "workspace/GitWorktree.h"
#include "workspace/BranchSandbox.h"
#include "workspace/WorkspaceManager.h"
#include "workspace/WorkspaceTools.h"

#include "testing/TestingTypes.h"
#include "testing/DiagnosticsEngine.h"
#include "testing/TestGenerator.h"
#include "testing/TestRunner.h"
#include "testing/CoverageAnalyzer.h"
#include "testing/TestingManager.h"

namespace aios::e2e {

/**
 * @brief Test workspace environment providing isolated sandboxes on disk,
 * mock model provider routing, and unified access to all AIOS subsystems.
 */
class E2ETestHarness {
public:
    E2ETestHarness();
    ~E2ETestHarness();

    // Lifecycle
    void setUp(const std::string& test_name = "e2e_test");
    void tearDown();

    // File sandbox helpers
    std::filesystem::path getSandboxRoot() const { return sandbox_root_; }
    std::filesystem::path createSourceFile(const std::string& relative_path, const std::string& content);
    std::string readSourceFile(const std::string& relative_path) const;
    bool fileExists(const std::string& relative_path) const;
    void removeSourceFile(const std::string& relative_path);

    // Mock Provider configuration
    void setupMockResponses(const std::vector<std::string>& responses);
    void setupToolCallingMock(const std::string& tool_name, const nlohmann::json& params, const std::string& final_text = "TASK_COMPLETE");

    // Subsystem Accessors
    std::shared_ptr<Kernel> getKernel() const { return kernel_; }
    std::shared_ptr<EventBus> getEventBus() const { return event_bus_; }
    std::shared_ptr<ToolRegistry> getToolRegistry() const { return tool_registry_; }
    std::shared_ptr<ModelRouter> getModelRouter() const { return model_router_; }
    std::shared_ptr<MockModelProvider> getMockProvider() const { return mock_provider_; }
    std::shared_ptr<MemoryManager> getMemoryManager() const { return memory_manager_; }
    std::shared_ptr<MultiAgentOrchestrator> getOrchestrator() const { return orchestrator_; }
    std::shared_ptr<ASTParser> getASTParser() const { return ast_parser_; }
    std::shared_ptr<RepositoryIndex> getRepoIndex() const { return repo_index_; }

    // Developer Suite Subsystem Accessors
    std::shared_ptr<workspace::WorkspaceManager> getWorkspaceManager() const { return workspace_manager_; }
    std::shared_ptr<workspace::PathContainment> getPathContainment() const { return path_containment_; }
    std::shared_ptr<workspace::SnapshotManager> getSnapshotManager() const { return snapshot_manager_; }
    std::shared_ptr<workspace::GitWorktree> getWorktreeEngine() const { return worktree_engine_; }
    std::shared_ptr<workspace::BranchSandbox> getBranchSandbox() const { return branch_sandbox_; }

    std::shared_ptr<testing::TestingManager> getTestingManager() const { return testing_manager_; }
    std::shared_ptr<testing::DiagnosticsEngine> getDiagnosticsEngine() const { return diagnostics_engine_; }
    std::shared_ptr<testing::TestGenerator> getTestGenerator() const { return test_generator_; }
    std::shared_ptr<testing::TestRunner> getTestRunner() const { return test_runner_; }
    std::shared_ptr<testing::CoverageAnalyzer> getCoverageAnalyzer() const { return coverage_analyzer_; }

    std::shared_ptr<CliSession> getCliSession() const { return cli_session_; }
    CommandRegistry& getCommandRegistry() { return CommandRegistry::instance(); }
    TerminalRenderer& getTerminalRenderer() { return TerminalRenderer::instance(); }

    // Execution Helpers
    CommandResult executeSlashCommand(const std::string& command_line);
    WorkflowResult executeOrchestratorWorkflow(const std::string& task_prompt);
    TaskGraphExecutionSummary executeTaskGraph(TaskGraph& graph);

private:
    std::filesystem::path sandbox_root_;
    std::shared_ptr<Kernel> kernel_;
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<ToolRegistry> tool_registry_;
    std::shared_ptr<MockModelProvider> mock_provider_;
    std::shared_ptr<ModelRouter> model_router_;
    std::shared_ptr<MemoryManager> memory_manager_;
    std::shared_ptr<MultiAgentOrchestrator> orchestrator_;
    std::shared_ptr<ASTParser> ast_parser_;
    std::shared_ptr<RepositoryIndex> repo_index_;

    std::shared_ptr<workspace::WorkspaceManager> workspace_manager_;
    std::shared_ptr<workspace::PathContainment> path_containment_;
    std::shared_ptr<workspace::SnapshotManager> snapshot_manager_;
    std::shared_ptr<workspace::GitWorktree> worktree_engine_;
    std::shared_ptr<workspace::BranchSandbox> branch_sandbox_;

    std::shared_ptr<testing::TestingManager> testing_manager_;
    std::shared_ptr<testing::DiagnosticsEngine> diagnostics_engine_;
    std::shared_ptr<testing::TestGenerator> test_generator_;
    std::shared_ptr<testing::TestRunner> test_runner_;
    std::shared_ptr<testing::CoverageAnalyzer> coverage_analyzer_;

    std::shared_ptr<CliSession> cli_session_;
};

/**
 * @brief Opaque-box validation helper for asserting contract compliance,
 * JSON schema integrity, ANSI formatting, and diff correctness.
 */
class OpaqueBoxValidator {
public:
    static bool isValidJson(const std::string& input);
    static bool containsAnsiColor(const std::string& input, const std::string& color_code);
    static bool isValidUnifiedDiff(const std::string& diff_content);
    static bool hasMatchingDiagnostic(const std::vector<testing::DiagnosticItem>& items,
                                      const std::string& rule_id,
                                      testing::DiagnosticSeverity severity);
};

} // namespace aios::e2e
