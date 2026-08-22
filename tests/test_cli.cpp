#include <gtest/gtest.h>
#include "cli/SlashCommand.h"
#include "cli/CliSession.h"
#include "cli/TerminalRenderer.h"
#include "cli/LineReader.h"
#include "cli/CommandRegistry.h"
#include "cli/Repl.h"
#include "cli/NonInteractiveRunner.h"
#include "kernel/Kernel.h"
#include "memory/memory.h"
#include "providers/ModelRouter.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace aios;

// ============================================================================
// TerminalRenderer Tests
// ============================================================================

TEST(TerminalRendererTest, ColorizesTextWithAllRoles) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(true);

    std::string primary = renderer.colorize("AIOS", ColorRole::BrandPrimary);
    EXPECT_NE(primary.find("255;107;157"), std::string::npos);

    std::string secondary = renderer.colorize("Branch", ColorRole::BrandSecondary);
    EXPECT_NE(secondary.find("255;154;86"), std::string::npos);

    std::string success = renderer.colorize("Passed", ColorRole::Success);
    EXPECT_NE(success.find("45;212;191"), std::string::npos);

    std::string error = renderer.colorize("Failed", ColorRole::Error);
    EXPECT_NE(error.find("248;113;113"), std::string::npos);

    std::string warning = renderer.colorize("Alert", ColorRole::Warning);
    EXPECT_NE(warning.find("251;191;36"), std::string::npos);
}

TEST(TerminalRendererTest, AppliesTextStyles) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(true);

    std::string bold = renderer.colorize("BoldText", ColorRole::BrandPrimary, TextStyle::Bold);
    EXPECT_NE(bold.find(";1m"), std::string::npos);

    std::string dim = renderer.colorize("DimText", ColorRole::TextMuted, TextStyle::Dim);
    EXPECT_NE(dim.find(";2m"), std::string::npos);

    std::string italic = renderer.colorize("ItalicText", ColorRole::TextPrimary, TextStyle::Italic);
    EXPECT_NE(italic.find(";3m"), std::string::npos);

    std::string underline = renderer.colorize("UnderlineText", ColorRole::Success, TextStyle::Underline);
    EXPECT_NE(underline.find(";4m"), std::string::npos);
}

TEST(TerminalRendererTest, ParsesHexColors) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(true);

    std::string custom = renderer.hexColor("CustomText", "#FF6B9D");
    EXPECT_NE(custom.find("255;107;157"), std::string::npos);

    std::string custom_no_hash = renderer.hexColor("CustomText", "2DD4BF");
    EXPECT_NE(custom_no_hash.find("45;212;191"), std::string::npos);
}

TEST(TerminalRendererTest, PlainTextWhenColorDisabled) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(false);

    std::string out = renderer.colorize("Plain Text", ColorRole::BrandPrimary, TextStyle::Bold);
    EXPECT_EQ(out, "Plain Text");

    std::string hex_out = renderer.hexColor("Plain Hex", "#FF6B9D");
    EXPECT_EQ(hex_out, "Plain Hex");

    renderer.setColorEnabled(true); // Reset
}

TEST(TerminalRendererTest, FormatsCardWithRoundedCorners) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(false);

    std::string card = renderer.formatCard("Task Status", "Execution in progress...\nAll nodes active.");
    EXPECT_NE(card.find("Task Status"), std::string::npos);
    EXPECT_NE(card.find("Execution in progress..."), std::string::npos);
    EXPECT_NE(card.find("All nodes active."), std::string::npos);
    EXPECT_NE(card.find("╭─"), std::string::npos);
    EXPECT_NE(card.find("╰"), std::string::npos);

    renderer.setColorEnabled(true);
}

TEST(TerminalRendererTest, FormatsTableWithHeadersAndBorders) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(false);

    std::vector<std::string> headers = {"Command", "Description", "Category"};
    std::vector<std::vector<std::string>> rows = {
        {"/help", "Show help inventory", "Core"},
        {"/model", "Manage providers and routing", "Model"},
        {"/run", "Execute autonomous agent task", "Workflow"}
    };

    std::string table = renderer.formatTable(headers, rows);
    EXPECT_NE(table.find("Command"), std::string::npos);
    EXPECT_NE(table.find("/help"), std::string::npos);
    EXPECT_NE(table.find("/model"), std::string::npos);
    EXPECT_NE(table.find("/run"), std::string::npos);
    EXPECT_NE(table.find("┌"), std::string::npos);
    EXPECT_NE(table.find("├"), std::string::npos);
    EXPECT_NE(table.find("└"), std::string::npos);

    renderer.setColorEnabled(true);
}

TEST(TerminalRendererTest, FormatsDiffCorrectly) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(true);

    std::string diff_input = "diff --git a/test.cpp b/test.cpp\n"
                             "--- a/test.cpp\n"
                             "+++ b/test.cpp\n"
                             "@@ -1,3 +1,4 @@\n"
                             "-old_function();\n"
                             "+new_function();\n"
                             " unchanged();\n";

    std::string formatted = renderer.formatDiff(diff_input);
    EXPECT_NE(formatted.find("new_function"), std::string::npos);
    EXPECT_NE(formatted.find("old_function"), std::string::npos);
}

TEST(TerminalRendererTest, FormatsMarkdownSyntax) {
    auto& renderer = TerminalRenderer::instance();
    renderer.setColorEnabled(true);

    std::string md_input = "# Header 1\n"
                           "## Header 2\n"
                           "- Bullet point\n"
                           "```cpp\n"
                           "int main() { return 0; }\n"
                           "```\n";

    std::string formatted = renderer.formatMarkdown(md_input);
    EXPECT_NE(formatted.find("Header 1"), std::string::npos);
    EXPECT_NE(formatted.find("Header 2"), std::string::npos);
    EXPECT_NE(formatted.find("Bullet point"), std::string::npos);
    EXPECT_NE(formatted.find("int main()"), std::string::npos);
}

// ============================================================================
// LineReader Tests
// ============================================================================

TEST(LineReaderTest, ManagesHistoryAndDeduplication) {
    std::string test_hist_file = "test_aios_history_temp.txt";
    if (std::filesystem::exists(test_hist_file)) {
        std::filesystem::remove(test_hist_file);
    }

    {
        LineReader reader(test_hist_file);
        reader.addHistory("/help");
        reader.addHistory("/help"); // Consecutive duplicate should be ignored
        reader.addHistory("/status");
        reader.addHistory("/model list");
        reader.addHistory("/model list"); // Consecutive duplicate ignored
        reader.addHistory(""); // Empty line ignored

        auto hist = reader.getHistory(10);
        ASSERT_EQ(hist.size(), 3);
        EXPECT_EQ(hist[0], "/help");
        EXPECT_EQ(hist[1], "/status");
        EXPECT_EQ(hist[2], "/model list");

        reader.saveHistory();
    }

    // Reload from file
    {
        LineReader reader2(test_hist_file);
        auto hist = reader2.getHistory(10);
        ASSERT_EQ(hist.size(), 3);
        EXPECT_EQ(hist[0], "/help");
        EXPECT_EQ(hist[1], "/status");
        EXPECT_EQ(hist[2], "/model list");

        reader2.clearHistory();
        EXPECT_EQ(reader2.getHistory(10).size(), 0);
    }

    if (std::filesystem::exists(test_hist_file)) {
        std::filesystem::remove(test_hist_file);
    }
}

TEST(LineReaderTest, AutocompletionCallbackTriggered) {
    LineReader reader("");
    bool handler_called = false;

    reader.setCompletionHandler([&](const std::string& prefix) -> std::vector<std::string> {
        handler_called = true;
        if (prefix == "/m") {
            return {"/model", "/memory", "/multiline"};
        }
        return {};
    });

    EXPECT_FALSE(reader.isMultiline());
    reader.setMultiline(true);
    EXPECT_TRUE(reader.isMultiline());
}

// ============================================================================
// CommandRegistry Tests
// ============================================================================

TEST(CommandRegistryTest, TokenizationHandlesQuotesAndEscapes) {
    std::string line = R"(/run "Implement binary search in C++" --branch=feat/algo 'arg with spaces' \"escaped\")";
    auto tokens = CommandRegistry::tokenize(line);

    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0], "/run");
    EXPECT_EQ(tokens[1], "Implement binary search in C++");
    EXPECT_EQ(tokens[2], "--branch=feat/algo");
    EXPECT_EQ(tokens[3], "arg with spaces");
    EXPECT_EQ(tokens[4], "\"escaped\"");
}

TEST(CommandRegistryTest, FlagParsingExtractsKeyValues) {
    std::vector<std::string> tokens = {
        "build", "all",
        "--timeout=30",
        "--verbose",
        "-c", "Release",
        "-f"
    };

    std::vector<std::string> pos_args;
    std::unordered_map<std::string, std::string> flags;
    CommandRegistry::parseFlags(tokens, pos_args, flags);

    ASSERT_EQ(pos_args.size(), 2);
    EXPECT_EQ(pos_args[0], "build");
    EXPECT_EQ(pos_args[1], "all");

    EXPECT_EQ(flags["timeout"], "30");
    EXPECT_EQ(flags["verbose"], "true");
    EXPECT_EQ(flags["c"], "Release");
    EXPECT_EQ(flags["f"], "true");
}

TEST(CommandRegistryTest, DispatchesBuiltinCommandsAndAliases) {
    auto& reg = CommandRegistry::instance();
    reg.registerBuiltinCommands();

    auto session = std::make_shared<CliSession>();
    CommandContext ctx{"/help", "", {}, {}, session, Kernel::instance(), TerminalRenderer::instance(), false};

    // Test /help
    auto res_help = reg.dispatch("/help", ctx);
    EXPECT_TRUE(res_help.success);
    EXPECT_FALSE(res_help.output.empty());

    // Test alias /h
    auto res_h = reg.dispatch("/h", ctx);
    EXPECT_TRUE(res_h.success);

    // Test unknown command
    auto res_unknown = reg.dispatch("/unknown_command_xyz", ctx);
    EXPECT_FALSE(res_unknown.success);
    EXPECT_NE(res_unknown.error_message.find("Unknown command"), std::string::npos);
}

TEST(CommandRegistryTest, DispatchesNaturalLanguageToRun) {
    auto& reg = CommandRegistry::instance();
    reg.registerBuiltinCommands();

    auto session = std::make_shared<CliSession>();
    CommandContext ctx{"Write a unit test for ASTParser", "", {}, {}, session, Kernel::instance(), TerminalRenderer::instance(), false};

    auto res = reg.dispatch("Write a unit test for ASTParser", ctx);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("Task Result Summary"), std::string::npos);
}

TEST(CommandRegistryTest, AutocompletionProvidesCandidates) {
    auto& reg = CommandRegistry::instance();
    reg.registerBuiltinCommands();

    auto comps = reg.getCompletions("/m");
    EXPECT_GE(comps.size(), 3); // /model, /memory, /multiline

    bool found_model = false;
    for (const auto& c : comps) {
        if (c == "/model") found_model = true;
    }
    EXPECT_TRUE(found_model);

    auto sub_comps = reg.getCompletions("/model ");
    EXPECT_GE(sub_comps.size(), 4); // switch, list, health, metrics, role
}

// ============================================================================
// Builtin Commands Tests (All 18 Commands)
// ============================================================================

class BuiltinCommandsTest : public ::testing::Test {
protected:
    void SetUp() override {
        CommandRegistry::instance().registerBuiltinCommands();
        session = std::make_shared<CliSession>();
        ctx = std::make_unique<CommandContext>(CommandContext{
            "",
            "",
            {},
            {},
            session,
            Kernel::instance(),
            TerminalRenderer::instance(),
            false
        });
    }

    std::shared_ptr<CliSession> session;
    std::unique_ptr<CommandContext> ctx;
};

TEST_F(BuiltinCommandsTest, Command1_Help) {
    auto res1 = CommandRegistry::instance().dispatch("/help", *ctx);
    EXPECT_TRUE(res1.success);
    EXPECT_NE(res1.output.find("AIOS Slash Commands"), std::string::npos);

    auto res2 = CommandRegistry::instance().dispatch("/help model", *ctx);
    EXPECT_TRUE(res2.success);
    EXPECT_NE(res2.output.find("Help: /model"), std::string::npos);

    auto res3 = CommandRegistry::instance().dispatch("/help invalid_cmd", *ctx);
    EXPECT_FALSE(res3.success);
}

TEST_F(BuiltinCommandsTest, Command2_Run) {
    auto res1 = CommandRegistry::instance().dispatch("/run Implement Dijkstra algorithm", *ctx);
    EXPECT_TRUE(res1.success);
    EXPECT_NE(res1.output.find("Task Result Summary"), std::string::npos);

    auto res2 = CommandRegistry::instance().dispatch("/run", *ctx);
    EXPECT_FALSE(res2.success);
}

TEST_F(BuiltinCommandsTest, Command3_Task) {
    auto res_graph = CommandRegistry::instance().dispatch("/task graph Build REST API", *ctx);
    EXPECT_TRUE(res_graph.success);
    EXPECT_NE(res_graph.output.find("DAG Execution Summary"), std::string::npos);

    auto res_list = CommandRegistry::instance().dispatch("/task list", *ctx);
    EXPECT_TRUE(res_list.success);

    auto res_cancel = CommandRegistry::instance().dispatch("/task cancel", *ctx);
    EXPECT_TRUE(res_cancel.success);

    auto res_retry = CommandRegistry::instance().dispatch("/task retry node_1", *ctx);
    EXPECT_TRUE(res_retry.success);
}

TEST_F(BuiltinCommandsTest, Command4_Model) {
    auto res_list = CommandRegistry::instance().dispatch("/model list", *ctx);
    EXPECT_TRUE(res_list.success);

    auto res_switch = CommandRegistry::instance().dispatch("/model switch lm_studio local-coder-llm", *ctx);
    EXPECT_TRUE(res_switch.success);
    EXPECT_EQ(session->getState().current_provider, "lm_studio");
    EXPECT_EQ(session->getState().current_model, "local-coder-llm");

    auto res_role = CommandRegistry::instance().dispatch("/model role Coder lm_studio local-coder", *ctx);
    EXPECT_TRUE(res_role.success);

    auto res_health = CommandRegistry::instance().dispatch("/model health", *ctx);
    EXPECT_TRUE(res_health.success);

    auto res_metrics = CommandRegistry::instance().dispatch("/model metrics", *ctx);
    EXPECT_TRUE(res_metrics.success);
}

TEST_F(BuiltinCommandsTest, Command5_Memory) {
    auto res_store = CommandRegistry::instance().dispatch("/memory store user_lang C++", *ctx);
    EXPECT_TRUE(res_store.success);

    auto res_search = CommandRegistry::instance().dispatch("/memory search C++", *ctx);
    EXPECT_TRUE(res_search.success);

    auto res_stats = CommandRegistry::instance().dispatch("/memory stats", *ctx);
    EXPECT_TRUE(res_stats.success);

    auto res_clear = CommandRegistry::instance().dispatch("/memory clear session", *ctx);
    EXPECT_TRUE(res_clear.success);

    auto res_kg = CommandRegistry::instance().dispatch("/memory kg root", *ctx);
    EXPECT_TRUE(res_kg.success);
}

TEST_F(BuiltinCommandsTest, Command6_Workspace) {
    auto res_list = CommandRegistry::instance().dispatch("/workspace list", *ctx);
    EXPECT_TRUE(res_list.success);

    auto res_create = CommandRegistry::instance().dispatch("/workspace create feat/branch_test", *ctx);
    EXPECT_TRUE(res_create.success);
    EXPECT_EQ(session->getState().current_workspace_branch, "feat/branch_test");

    auto res_switch = CommandRegistry::instance().dispatch("/workspace switch main", *ctx);
    EXPECT_TRUE(res_switch.success);
    EXPECT_EQ(session->getState().current_workspace_branch, "main");

    auto res_clean = CommandRegistry::instance().dispatch("/workspace clean", *ctx);
    EXPECT_TRUE(res_clean.success);

    auto res_diff = CommandRegistry::instance().dispatch("/workspace diff", *ctx);
    EXPECT_TRUE(res_diff.success);
}

TEST_F(BuiltinCommandsTest, Command7_Test) {
    auto res_run = CommandRegistry::instance().dispatch("/test run CliTest.*", *ctx);
    EXPECT_TRUE(res_run.success);

    auto res_gen = CommandRegistry::instance().dispatch("/test gen src/cli/Repl.cpp", *ctx);
    EXPECT_TRUE(res_gen.success);

    auto res_diag = CommandRegistry::instance().dispatch("/test diag src/cli/LineReader.cpp", *ctx);
    EXPECT_TRUE(res_diag.success);
}

TEST_F(BuiltinCommandsTest, Command8_Status) {
    auto res = CommandRegistry::instance().dispatch("/status", *ctx);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("AIOS Global Status"), std::string::npos);
}

TEST_F(BuiltinCommandsTest, Command9_Checkpoint) {
    auto res = CommandRegistry::instance().dispatch("/checkpoint Initial state", *ctx);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("Created snapshot checkpoint: cp_"), std::string::npos);
}

TEST_F(BuiltinCommandsTest, Command10_Rollback) {
    auto res = CommandRegistry::instance().dispatch("/rollback cp_12345", *ctx);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("Successfully rolled back"), std::string::npos);
}

TEST_F(BuiltinCommandsTest, Command11_Diff) {
    auto res = CommandRegistry::instance().dispatch("/diff src/main.cpp", *ctx);
    EXPECT_TRUE(res.success);
}

TEST_F(BuiltinCommandsTest, Command12_History) {
    session->recordCommand("/status", true);
    session->recordCommand("/model list", true);

    auto res = CommandRegistry::instance().dispatch("/history 10", *ctx);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("/status"), std::string::npos);
    EXPECT_NE(res.output.find("/model list"), std::string::npos);
}

TEST_F(BuiltinCommandsTest, Command13_Config) {
    auto res_list = CommandRegistry::instance().dispatch("/config list", *ctx);
    EXPECT_TRUE(res_list.success);

    auto res_set = CommandRegistry::instance().dispatch("/config set timeout 45", *ctx);
    EXPECT_TRUE(res_set.success);

    auto res_get = CommandRegistry::instance().dispatch("/config get timeout", *ctx);
    EXPECT_TRUE(res_get.success);

    auto res_reload = CommandRegistry::instance().dispatch("/config reload", *ctx);
    EXPECT_TRUE(res_reload.success);
}

TEST_F(BuiltinCommandsTest, Command14_Tools) {
    auto res_list = CommandRegistry::instance().dispatch("/tools list", *ctx);
    EXPECT_TRUE(res_list.success);

    auto res_stats = CommandRegistry::instance().dispatch("/tools stats", *ctx);
    EXPECT_TRUE(res_stats.success);
}

TEST_F(BuiltinCommandsTest, Command15_Clear) {
    auto res = CommandRegistry::instance().dispatch("/clear", *ctx);
    EXPECT_TRUE(res.success);
}

TEST_F(BuiltinCommandsTest, Command16_Multiline) {
    auto res = CommandRegistry::instance().dispatch("/multiline", *ctx);
    EXPECT_TRUE(res.success);
}

TEST_F(BuiltinCommandsTest, Command17_Session) {
    std::string test_sess_file = "test_session_output.json";
    if (std::filesystem::exists(test_sess_file)) {
        std::filesystem::remove(test_sess_file);
    }

    auto res_save = CommandRegistry::instance().dispatch("/session save " + test_sess_file, *ctx);
    EXPECT_TRUE(res_save.success);
    EXPECT_TRUE(std::filesystem::exists(test_sess_file));

    auto res_load = CommandRegistry::instance().dispatch("/session load " + test_sess_file, *ctx);
    EXPECT_TRUE(res_load.success);

    auto res_new = CommandRegistry::instance().dispatch("/session new", *ctx);
    EXPECT_TRUE(res_new.success);

    if (std::filesystem::exists(test_sess_file)) {
        std::filesystem::remove(test_sess_file);
    }
}

TEST_F(BuiltinCommandsTest, Command18_Exit) {
    auto res_exit = CommandRegistry::instance().dispatch("/exit", *ctx);
    EXPECT_TRUE(res_exit.success);
    EXPECT_TRUE(res_exit.should_exit);

    auto res_quit = CommandRegistry::instance().dispatch("/quit", *ctx);
    EXPECT_TRUE(res_quit.success);
    EXPECT_TRUE(res_quit.should_exit);
}

// ============================================================================
// CliSession Tests
// ============================================================================

TEST(CliSessionTest, ManagesSessionStateAndPrompts) {
    CliSession session;
    EXPECT_FALSE(session.getState().session_id.empty());
    EXPECT_EQ(session.getState().current_workspace_branch, "main");
    EXPECT_EQ(session.getState().current_provider, "lm_studio");

    std::string plain_prompt = session.getPlainPromptString();
    EXPECT_EQ(plain_prompt, "aios [main|lm_studio] > ");

    session.getState().current_workspace_branch = "feat/repl";
    session.getState().current_provider = "ollama";
    session.getState().current_model = "codellama";

    std::string updated_prompt = session.getPlainPromptString();
    EXPECT_EQ(updated_prompt, "aios [feat/repl|ollama] > ");

    session.recordCommand("/help", true);
    session.recordCommand("/invalid", false);

    const auto& hist = session.getCommandHistory();
    ASSERT_EQ(hist.size(), 2);
    EXPECT_EQ(hist[0].first, "/help");
    EXPECT_TRUE(hist[0].second);
    EXPECT_EQ(hist[1].first, "/invalid");
    EXPECT_FALSE(hist[1].second);
}

// ============================================================================
// Repl Lifecycle Tests
// ============================================================================

TEST(ReplLifecycleTest, InitializesAndHandlesInterrupt) {
    Repl repl(Kernel::instance());
    EXPECT_FALSE(repl.isRunning());

    EXPECT_TRUE(repl.initialize());
    EXPECT_NE(repl.getSession(), nullptr);
    EXPECT_NE(repl.getLineReader(), nullptr);

    repl.handleInterrupt(); // Should not crash
    repl.stop();
    EXPECT_FALSE(repl.isRunning());
}

// ============================================================================
// NonInteractiveRunner Tests
// ============================================================================

TEST(NonInteractiveRunnerTest, ExecutesCommandsInTextAndJsonFormats) {
    NonInteractiveRunner runner(Kernel::instance());

    // Text format one-shot
    int code_text = runner.executeCommand("/status", OutputFormat::Text);
    EXPECT_EQ(code_text, 0);

    // JSON format one-shot
    int code_json = runner.executeCommand("/model list", OutputFormat::Json);
    EXPECT_EQ(code_json, 0);

    // Invalid command
    int code_err = runner.executeCommand("/invalid_cmd_123", OutputFormat::Text);
    EXPECT_NE(code_err, 0);
}

TEST(NonInteractiveRunnerTest, ExecutesScriptFile) {
    std::string test_script = "test_script.aios";
    {
        std::ofstream file(test_script);
        file << "# AIOS Test Script\n"
             << "/status\n"
             << "/model list\n"
             << "/memory stats\n";
    }

    NonInteractiveRunner runner(Kernel::instance());
    int code = runner.executeScriptFile(test_script, false);
    EXPECT_EQ(code, 0);

    if (std::filesystem::exists(test_script)) {
        std::filesystem::remove(test_script);
    }
}
