#include "test_e2e_harness.h"
#include <random>
#include <sstream>
#include <regex>

namespace aios::e2e {

E2ETestHarness::E2ETestHarness() = default;

E2ETestHarness::~E2ETestHarness() {
    tearDown();
}

void E2ETestHarness::setUp(const std::string& test_name) {
    // 1. Create a unique temporary directory for the test sandbox
    auto temp_base = std::filesystem::temp_directory_path();
    uint64_t nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    sandbox_root_ = temp_base / ("aios_e2e_" + test_name + "_" + std::to_string(nonce));
    std::filesystem::create_directories(sandbox_root_);

    // 2. Initialize EventBus & Logger
    event_bus_ = std::make_shared<EventBus>();

    // 3. Initialize ToolRegistry
    tool_registry_ = std::make_shared<ToolRegistry>();
    tool_registry_->registerDefaultTools();

    // 4. Initialize Mock Model Provider & Router
    mock_provider_ = std::make_shared<MockModelProvider>();
    model_router_ = std::make_shared<ModelRouter>();
    model_router_->registerProvider(mock_provider_);
    model_router_->setDefaultProvider("mock");

    // 5. Initialize Memory & Knowledge Graph
    MemoryConfig mem_cfg;
    mem_cfg.max_working_memory_items = 100;
    mem_cfg.working_memory_byte_limit = 1024 * 1024;
    memory_manager_ = std::make_shared<MemoryManager>(mem_cfg);

    // 6. Initialize AST Parser & Repository Index
    ast_parser_ = std::make_shared<ASTParser>();
    repo_index_ = std::make_shared<RepositoryIndex>();
    repo_index_->setRepositoryPath(sandbox_root_);

    // 7. Initialize Workspace Subsystem
    path_containment_ = std::make_shared<workspace::PathContainment>(sandbox_root_);
    snapshot_manager_ = std::make_shared<workspace::SnapshotManager>(sandbox_root_, path_containment_);
    worktree_engine_ = std::make_shared<workspace::GitWorktree>(sandbox_root_);
    branch_sandbox_ = std::make_shared<workspace::BranchSandbox>(sandbox_root_);
    
    workspace_manager_ = workspace::WorkspaceManager::create(sandbox_root_);
    workspace_manager_->initialize(sandbox_root_);
    workspace::registerWorkspaceTools(*tool_registry_, workspace_manager_);

    // 8. Initialize Testing Subsystem
    diagnostics_engine_ = std::make_shared<testing::DiagnosticsEngine>();
    test_generator_ = std::make_shared<testing::TestGenerator>();
    test_runner_ = std::make_shared<testing::TestRunner>();
    coverage_analyzer_ = std::make_shared<testing::CoverageAnalyzer>();
    testing_manager_ = std::make_shared<testing::TestingManager>();
    testing_manager_->initialize(repo_index_);

    // 9. Initialize MultiAgentOrchestrator
    OrchestratorConfig orch_cfg;
    orch_cfg.max_repair_cycles = 2;
    orch_cfg.review_pass_score = 70;
    orch_cfg.auto_git_checkpoint = false;
    orch_cfg.rollback_on_unrecoverable_failure = false;
    orchestrator_ = std::make_shared<MultiAgentOrchestrator>(orch_cfg);
    orchestrator_->setModelProvider(mock_provider_);
    orchestrator_->setToolRegistry(tool_registry_);
    orchestrator_->setEventBus(event_bus_);

    // 10. Initialize CLI Subsystem
    cli_session_ = std::make_shared<CliSession>();
    cli_session_->getState().current_workspace_path = sandbox_root_.string();
    CommandRegistry::instance().registerBuiltinCommands();
    TerminalRenderer::instance().initialize(true);
}

void E2ETestHarness::tearDown() {
    if (workspace_manager_) {
        workspace_manager_->shutdown();
    }
    if (testing_manager_) {
        testing_manager_->shutdown();
    }
    if (!sandbox_root_.empty() && std::filesystem::exists(sandbox_root_)) {
        std::error_code ec;
        std::filesystem::remove_all(sandbox_root_, ec);
    }
}

std::filesystem::path E2ETestHarness::createSourceFile(const std::string& relative_path, const std::string& content) {
    auto full_path = sandbox_root_ / relative_path;
    std::filesystem::create_directories(full_path.parent_path());
    std::ofstream out(full_path, std::ios::binary);
    out << content;
    out.close();
    return full_path;
}

std::string E2ETestHarness::readSourceFile(const std::string& relative_path) const {
    auto full_path = sandbox_root_ / relative_path;
    if (!std::filesystem::exists(full_path)) {
        return "";
    }
    std::ifstream in(full_path, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool E2ETestHarness::fileExists(const std::string& relative_path) const {
    return std::filesystem::exists(sandbox_root_ / relative_path);
}

void E2ETestHarness::removeSourceFile(const std::string& relative_path) {
    auto full_path = sandbox_root_ / relative_path;
    std::error_code ec;
    std::filesystem::remove(full_path, ec);
}

void E2ETestHarness::setupMockResponses(const std::vector<std::string>& responses) {
    if (!mock_provider_) return;
    for (const auto& resp : responses) {
        mock_provider_->addResponse(resp);
    }
}

void E2ETestHarness::setupToolCallingMock(const std::string& tool_name, const nlohmann::json& params, const std::string& final_text) {
    if (!mock_provider_) return;
    std::string tool_call_block = "```json\n{\n  \"tool\": \"" + tool_name + "\",\n  \"params\": " + params.dump(2) + "\n}\n```";
    mock_provider_->addResponse(tool_call_block);
    if (!final_text.empty()) {
        mock_provider_->addResponse(final_text);
    }
}

CommandResult E2ETestHarness::executeSlashCommand(const std::string& command_line) {
    CommandContext ctx{
        .raw_line = command_line,
        .command_name = "",
        .args = {},
        .flags = {},
        .session = cli_session_,
        .kernel = *kernel_,
        .renderer = TerminalRenderer::instance(),
        .is_interactive = false
    };
    return CommandRegistry::instance().dispatch(command_line, ctx);
}

WorkflowResult E2ETestHarness::executeOrchestratorWorkflow(const std::string& task_prompt) {
    if (!orchestrator_) return WorkflowResult{};
    return orchestrator_->runWorkflow(task_prompt);
}

TaskGraphExecutionSummary E2ETestHarness::executeTaskGraph(TaskGraph& graph) {
    TaskGraphExecutorConfig config;
    config.max_concurrency = 2;
    TaskGraphExecutor executor(config);
    executor.setEventBus(event_bus_);
    executor.setNodeHandler([](TaskNode& node) -> TaskExecutionResult {
        return TaskExecutionResult{.success = true, .output = "Executed " + node.id, .error = ""};
    });
    return executor.execute(graph);
}

// ============================================================================
// OpaqueBoxValidator Implementations
// ============================================================================

bool OpaqueBoxValidator::isValidJson(const std::string& input) {
    try {
        auto parsed = nlohmann::json::parse(input);
        return !parsed.is_discarded();
    } catch (...) {
        return false;
    }
}

bool OpaqueBoxValidator::containsAnsiColor(const std::string& input, const std::string& color_code) {
    return input.find(color_code) != std::string::npos;
}

bool OpaqueBoxValidator::isValidUnifiedDiff(const std::string& diff_content) {
    if (diff_content.empty()) return false;
    bool has_header = (diff_content.find("--- ") != std::string::npos && 
                       diff_content.find("+++ ") != std::string::npos);
    bool has_hunk = (diff_content.find("@@ ") != std::string::npos);
    return has_header && has_hunk;
}

bool OpaqueBoxValidator::hasMatchingDiagnostic(const std::vector<testing::DiagnosticItem>& items,
                                              const std::string& rule_id,
                                              testing::DiagnosticSeverity severity) {
    for (const auto& item : items) {
        if (item.rule_id == rule_id && item.severity == severity) {
            return true;
        }
    }
    return false;
}

} // namespace aios::e2e
