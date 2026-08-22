#include <gtest/gtest.h>
#include "test_e2e_harness.h"

using namespace aios;
using namespace aios::e2e;

class E2ECLITest : public ::testing::Test {
protected:
    void SetUp() override {
        harness_.setUp("cli");
    }

    void TearDown() override {
        harness_.tearDown();
    }

    E2ETestHarness harness_;
};

// ============================================================================
// TIER 1: FEATURE COVERAGE (Features 1 - 8)
// ============================================================================

// ----------------------------------------------------------------------------
// Feature 1: VT100 / ANSI Terminal Initialization & Color Roles
// ----------------------------------------------------------------------------

TEST_F(E2ECLITest, Tier1_TerminalRenderer_BrandColorsANSIEscapeCodes) {
    auto& renderer = harness_.getTerminalRenderer();
    renderer.initialize(true);
    EXPECT_TRUE(renderer.isColorEnabled());

    std::string coral_rose = renderer.colorize("CoralRose", ColorRole::BrandPrimary);
    EXPECT_NE(coral_rose.find("38;2;255;107;157"), std::string::npos);

    std::string sunset_orange = renderer.colorize("SunsetOrange", ColorRole::BrandSecondary);
    EXPECT_NE(sunset_orange.find("38;2;255;154;86"), std::string::npos);

    std::string teal = renderer.colorize("TealSuccess", ColorRole::Success);
    EXPECT_NE(teal.find("38;2;45;212;191"), std::string::npos);

    std::string coral_red = renderer.colorize("CoralRedError", ColorRole::Error);
    EXPECT_NE(coral_red.find("38;2;248;113;113"), std::string::npos);

    std::string amber = renderer.colorize("AmberWarning", ColorRole::Warning);
    EXPECT_NE(amber.find("38;2;251;191;36"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_TerminalRenderer_HexColorConversion) {
    auto& renderer = harness_.getTerminalRenderer();
    std::string colored = renderer.hexColor("CustomColor", "#AABBCC");
    EXPECT_NE(colored.find("38;2;170;187;204"), std::string::npos);
    EXPECT_NE(colored.find("CustomColor"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_TerminalRenderer_Styles_BoldUnderlineDimItalic) {
    auto& renderer = harness_.getTerminalRenderer();
    std::string bold = renderer.colorize("BoldText", ColorRole::TextPrimary, TextStyle::Bold);
    EXPECT_NE(bold.find(";1m"), std::string::npos);

    std::string dim = renderer.colorize("DimText", ColorRole::TextMuted, TextStyle::Dim);
    EXPECT_NE(dim.find(";2m"), std::string::npos);

    std::string italic = renderer.colorize("ItalicText", ColorRole::BrandPrimary, TextStyle::Italic);
    EXPECT_NE(italic.find(";3m"), std::string::npos);

    std::string underline = renderer.colorize("UnderlineText", ColorRole::Success, TextStyle::Underline);
    EXPECT_NE(underline.find(";4m"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_TerminalRenderer_ColorDisabling) {
    auto& renderer = harness_.getTerminalRenderer();
    renderer.setColorEnabled(false);
    EXPECT_FALSE(renderer.isColorEnabled());

    std::string plain = renderer.colorize("PlainText", ColorRole::BrandPrimary);
    EXPECT_EQ(plain, "PlainText");
    renderer.setColorEnabled(true);
}

TEST_F(E2ECLITest, Tier1_TerminalRenderer_Components_CardAndTableFormatting) {
    auto& renderer = harness_.getTerminalRenderer();
    std::string card = renderer.formatCard("Status Title", "Body Content", ColorRole::BrandPrimary);
    EXPECT_NE(card.find("Status Title"), std::string::npos);
    EXPECT_NE(card.find("Body Content"), std::string::npos);

    std::vector<std::string> headers = {"Command", "Description"};
    std::vector<std::vector<std::string>> rows = {
        {"/help", "Display help"},
        {"/run", "Run task"}
    };
    std::string table = renderer.formatTable(headers, rows);
    EXPECT_NE(table.find("/help"), std::string::npos);
    EXPECT_NE(table.find("Display help"), std::string::npos);
}

// ----------------------------------------------------------------------------
// Feature 2: LineReader & Dynamic Prompt
// ----------------------------------------------------------------------------

TEST_F(E2ECLITest, Tier1_CliSession_DynamicPromptGeneration) {
    auto session = harness_.getCliSession();
    session->getState().current_workspace_branch = "feature/e2e";
    session->getState().current_provider = "lm_studio";
    session->getState().current_model = "qwen2.5-coder";

    std::string prompt = session->getPromptString();
    EXPECT_NE(prompt.find("feature/e2e"), std::string::npos);
    EXPECT_NE(prompt.find("lm_studio"), std::string::npos);
    EXPECT_NE(prompt.find("aios"), std::string::npos);

    std::string plain = session->getPlainPromptString();
    EXPECT_EQ(plain, "aios [feature/e2e|lm_studio] > ");
}

TEST_F(E2ECLITest, Tier1_CliSession_SessionStatePersistence) {
    auto session = harness_.getCliSession();
    session->getState().session_id = "session_12345";
    session->getState().variables["key1"] = "val1";

    auto save_path = harness_.getSandboxRoot() / "session.json";
    EXPECT_TRUE(session->saveToFile(save_path.string()));
    EXPECT_TRUE(std::filesystem::exists(save_path));

    CliSession loaded_session;
    EXPECT_TRUE(loaded_session.loadFromFile(save_path.string()));
    EXPECT_EQ(loaded_session.getState().session_id, "session_12345");
    EXPECT_EQ(loaded_session.getState().variables.at("key1"), "val1");
}

TEST_F(E2ECLITest, Tier1_LineReader_HistoryDeduplication) {
    auto hist_path = harness_.getSandboxRoot() / ".aios_history";
    LineReader reader(hist_path.string());
    
    reader.addHistory("/status");
    reader.addHistory("/status"); // Consecutive duplicate
    reader.addHistory("/help");
    reader.addHistory("/help");   // Consecutive duplicate

    auto history = reader.getHistory(10);
    ASSERT_EQ(history.size(), 2);
    EXPECT_EQ(history[0], "/status");
    EXPECT_EQ(history[1], "/help");
}

TEST_F(E2ECLITest, Tier1_LineReader_HistoryFileSaveAndLoad) {
    auto hist_path = harness_.getSandboxRoot() / ".aios_history";
    {
        LineReader reader(hist_path.string());
        reader.addHistory("/task list");
        reader.addHistory("/model switch openai");
        reader.saveHistory();
    }
    EXPECT_TRUE(std::filesystem::exists(hist_path));

    LineReader reader2(hist_path.string());
    reader2.loadHistory();
    auto hist = reader2.getHistory(10);
    ASSERT_EQ(hist.size(), 2);
    EXPECT_EQ(hist[0], "/task list");
    EXPECT_EQ(hist[1], "/model switch openai");
}

TEST_F(E2ECLITest, Tier1_LineReader_MultilineModeToggle) {
    LineReader reader("");
    EXPECT_FALSE(reader.isMultiline());
    reader.setMultiline(true);
    EXPECT_TRUE(reader.isMultiline());
    reader.setMultiline(false);
    EXPECT_FALSE(reader.isMultiline());
}

// ----------------------------------------------------------------------------
// Feature 3: Autocompletion
// ----------------------------------------------------------------------------

TEST_F(E2ECLITest, Tier1_CommandRegistry_Autocomplete_CommandPrefix) {
    auto& registry = harness_.getCommandRegistry();
    auto completions = registry.getCompletions("/m");
    EXPECT_GE(completions.size(), 2); // /model, /memory, /multiline
    
    bool has_model = false, has_memory = false;
    for (const auto& c : completions) {
        if (c == "/model") has_model = true;
        if (c == "/memory") has_memory = true;
    }
    EXPECT_TRUE(has_model);
    EXPECT_TRUE(has_memory);
}

TEST_F(E2ECLITest, Tier1_CommandRegistry_Autocomplete_Subcommands) {
    auto& registry = harness_.getCommandRegistry();
    auto completions = registry.getCompletions("/model ");
    EXPECT_GE(completions.size(), 3);
    
    bool has_list = false, has_switch = false;
    for (const auto& c : completions) {
        if (c == "list") has_list = true;
        if (c == "switch") has_switch = true;
    }
    EXPECT_TRUE(has_list);
    EXPECT_TRUE(has_switch);
}

TEST_F(E2ECLITest, Tier1_CommandRegistry_Autocomplete_WorkspaceSubcommands) {
    auto& registry = harness_.getCommandRegistry();
    auto completions = registry.getCompletions("/workspace ");
    EXPECT_GE(completions.size(), 3);
    
    bool has_list = false, has_create = false, has_switch = false;
    for (const auto& c : completions) {
        if (c == "list") has_list = true;
        if (c == "create") has_create = true;
        if (c == "switch") has_switch = true;
    }
    EXPECT_TRUE(has_list);
    EXPECT_TRUE(has_create);
    EXPECT_TRUE(has_switch);
}

TEST_F(E2ECLITest, Tier1_CommandRegistry_Autocomplete_TaskSubcommands) {
    auto& registry = harness_.getCommandRegistry();
    auto completions = registry.getCompletions("/task ");
    EXPECT_GE(completions.size(), 3);
    
    bool has_graph = false, has_list = false;
    for (const auto& c : completions) {
        if (c == "graph") has_graph = true;
        if (c == "list") has_list = true;
    }
    EXPECT_TRUE(has_graph);
    EXPECT_TRUE(has_list);
}

TEST_F(E2ECLITest, Tier1_CommandRegistry_Autocomplete_TestSubcommands) {
    auto& registry = harness_.getCommandRegistry();
    auto completions = registry.getCompletions("/test ");
    EXPECT_GE(completions.size(), 3);
    
    bool has_run = false, has_gen = false, has_diag = false;
    for (const auto& c : completions) {
        if (c == "run") has_run = true;
        if (c == "gen") has_gen = true;
        if (c == "diag") has_diag = true;
    }
    EXPECT_TRUE(has_run);
    EXPECT_TRUE(has_gen);
    EXPECT_TRUE(has_diag);
}

// ----------------------------------------------------------------------------
// Feature 5 & 6: Slash Command Engine & Builtin Slash Commands (18 Commands)
// ----------------------------------------------------------------------------

TEST_F(E2ECLITest, Tier1_SlashCommands_Help_InventoryAndDetailed) {
    auto res_all = harness_.executeSlashCommand("/help");
    EXPECT_TRUE(res_all.success);
    EXPECT_NE(res_all.output.find("AIOS Slash Commands"), std::string::npos);
    EXPECT_NE(res_all.output.find("/model"), std::string::npos);
    EXPECT_NE(res_all.output.find("/workspace"), std::string::npos);

    auto res_detail = harness_.executeSlashCommand("/help model");
    EXPECT_TRUE(res_detail.success);
    EXPECT_NE(res_detail.output.find("Usage:"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Status_DashboardCard) {
    auto res = harness_.executeSlashCommand("/status");
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("AIOS Global Status"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Model_ListAndSwitch) {
    auto res_list = harness_.executeSlashCommand("/model list");
    EXPECT_TRUE(res_list.success);
    EXPECT_NE(res_list.output.find("Providers"), std::string::npos);

    auto res_switch = harness_.executeSlashCommand("/model switch mock");
    EXPECT_TRUE(res_switch.success);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Memory_StoreAndSearch) {
    auto res_store = harness_.executeSlashCommand("/memory store pref_theme dark");
    EXPECT_TRUE(res_store.success);

    auto res_stats = harness_.executeSlashCommand("/memory stats");
    EXPECT_TRUE(res_stats.success);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Config_GetSetList) {
    auto res_set = harness_.executeSlashCommand("/config set timeout 30");
    EXPECT_TRUE(res_set.success);

    auto res_get = harness_.executeSlashCommand("/config get timeout");
    EXPECT_TRUE(res_get.success);
    EXPECT_NE(res_get.output.find("30"), std::string::npos);

    auto res_list = harness_.executeSlashCommand("/config list");
    EXPECT_TRUE(res_list.success);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Tools_ListAndInfo) {
    auto res_list = harness_.executeSlashCommand("/tools list");
    EXPECT_TRUE(res_list.success);
    EXPECT_NE(res_list.output.find("filesystem"), std::string::npos);

    auto res_info = harness_.executeSlashCommand("/tools info filesystem");
    EXPECT_TRUE(res_info.success);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_History_Display) {
    harness_.getCliSession()->recordCommand("/status", true);
    harness_.getCliSession()->recordCommand("/model list", true);

    auto res = harness_.executeSlashCommand("/history 10");
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("/status"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Multiline_Toggle) {
    auto res1 = harness_.executeSlashCommand("/multiline");
    EXPECT_TRUE(res1.success);
    EXPECT_NE(res1.output.find("enabled"), std::string::npos);

    auto res2 = harness_.executeSlashCommand("/multiline");
    EXPECT_TRUE(res2.success);
    EXPECT_NE(res2.output.find("disabled"), std::string::npos);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Exit_Signal) {
    auto res = harness_.executeSlashCommand("/exit");
    EXPECT_TRUE(res.should_exit);
}

TEST_F(E2ECLITest, Tier1_SlashCommands_Clear_Output) {
    auto res = harness_.executeSlashCommand("/clear");
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.output.find("\033[2J"), std::string::npos);
}

// ----------------------------------------------------------------------------
// Feature 5: Tokenizer & Flag Parsing
// ----------------------------------------------------------------------------

TEST_F(E2ECLITest, Tier1_Tokenizer_PreservesQuotesAndWhitespace) {
    std::string line = "/run \"Refactor the AST Parser for C++23\" --max-cycles=5 --verbose";
    auto tokens = CommandRegistry::tokenize(line);
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "/run");
    EXPECT_EQ(tokens[1], "Refactor the AST Parser for C++23");
    EXPECT_EQ(tokens[2], "--max-cycles=5");
    EXPECT_EQ(tokens[3], "--verbose");

    std::vector<std::string> pos_args;
    std::unordered_map<std::string, std::string> flags;
    CommandRegistry::parseFlags(tokens, pos_args, flags);

    ASSERT_EQ(pos_args.size(), 2);
    EXPECT_EQ(pos_args[0], "/run");
    EXPECT_EQ(pos_args[1], "Refactor the AST Parser for C++23");
    EXPECT_EQ(flags["max-cycles"], "5");
    EXPECT_EQ(flags["verbose"], "true");
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASES (Extreme Inputs, Malformed Inputs)
// ============================================================================

TEST_F(E2ECLITest, Tier2_Tokenizer_UnterminatedQuotesGracefulHandling) {
    std::string line = "/run \"Unclosed string at end of line";
    auto tokens = CommandRegistry::tokenize(line);
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[1], "Unclosed string at end of line");
}

TEST_F(E2ECLITest, Tier2_Tokenizer_EmptyAndWhitespaceStrings) {
    EXPECT_TRUE(CommandRegistry::tokenize("").empty());
    EXPECT_TRUE(CommandRegistry::tokenize("   \t  \n  ").empty());
}

TEST_F(E2ECLITest, Tier2_CommandRegistry_UnknownCommandReturnsError) {
    auto res = harness_.executeSlashCommand("/non_existent_command_xyz");
    EXPECT_FALSE(res.success);
    EXPECT_NE(res.error_message.find("Unknown command"), std::string::npos);
}

TEST_F(E2ECLITest, Tier2_CommandRegistry_ExtremelyLargeInputBuffer) {
    std::string large_task(50000, 'A');
    std::string command = "/memory store large_key " + large_task;
    auto res = harness_.executeSlashCommand(command);
    EXPECT_TRUE(res.success);
}

TEST_F(E2ECLITest, Tier2_CommandRegistry_SpecialCharactersAndEmojis) {
    std::string command = "/memory store emoji_test 🚀🔥🎉_special_chars_!@#$%^&*()";
    auto res = harness_.executeSlashCommand(command);
    EXPECT_TRUE(res.success);
}

TEST_F(E2ECLITest, Tier2_Autocomplete_NoMatchReturnsEmpty) {
    auto& registry = harness_.getCommandRegistry();
    auto completions = registry.getCompletions("/zzzzz_no_match");
    EXPECT_TRUE(completions.empty());
}

TEST_F(E2ECLITest, Tier2_LineReader_MaxCapacityPruning) {
    auto hist_path = harness_.getSandboxRoot() / ".aios_history_large";
    LineReader reader(hist_path.string());

    for (int i = 0; i < 200; ++i) {
        reader.addHistory("/cmd_" + std::to_string(i));
    }

    auto history = reader.getHistory(50);
    EXPECT_EQ(history.size(), 50);
    EXPECT_EQ(history.back(), "/cmd_199");
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATIONS (CLI + Subsystems)
// ============================================================================

TEST_F(E2ECLITest, Tier3_CLI_TaskGraph_Integration) {
    auto res = harness_.executeSlashCommand("/task list");
    EXPECT_TRUE(res.success);
}

TEST_F(E2ECLITest, Tier3_CLI_ModelRouter_Metrics_Integration) {
    auto res_health = harness_.executeSlashCommand("/model health");
    EXPECT_TRUE(res_health.success);

    auto res_metrics = harness_.executeSlashCommand("/model metrics");
    EXPECT_TRUE(res_metrics.success);
}

TEST_F(E2ECLITest, Tier3_CLI_Workspace_Snapshot_Integration) {
    harness_.createSourceFile("src/sample.cpp", "int x = 42;");
    auto res_snap = harness_.executeSlashCommand("/checkpoint InitialCode");
    EXPECT_TRUE(res_snap.success);

    auto res_diff = harness_.executeSlashCommand("/diff");
    EXPECT_TRUE(res_diff.success);
}

TEST_F(E2ECLITest, Tier3_CLI_Test_Diagnostics_Integration) {
    harness_.createSourceFile("src/test_code.cpp", "void foo() { FILE* f = fopen(\"a.txt\", \"r\"); }");
    auto res = harness_.executeSlashCommand("/test diag " + (harness_.getSandboxRoot() / "src/test_code.cpp").string());
    EXPECT_TRUE(res.success);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO
// ============================================================================

TEST_F(E2ECLITest, Tier4_InteractiveDeveloperSession_FullLifecycle) {
    // 1. Session begins, user inspects help
    auto r1 = harness_.executeSlashCommand("/help");
    EXPECT_TRUE(r1.success);

    // 2. User checks status dashboard
    auto r2 = harness_.executeSlashCommand("/status");
    EXPECT_TRUE(r2.success);

    // 3. User configures model
    auto r3 = harness_.executeSlashCommand("/model switch mock");
    EXPECT_TRUE(r3.success);

    // 4. User sets config variable
    auto r4 = harness_.executeSlashCommand("/config set auto_checkpoint true");
    EXPECT_TRUE(r4.success);

    // 5. User writes code file
    harness_.createSourceFile("src/calc.cpp", "int add(int a, int b) { return a + b; }");

    // 6. User creates checkpoint
    auto r5 = harness_.executeSlashCommand("/checkpoint BeforeRefactor");
    EXPECT_TRUE(r5.success);

    // 7. User modifies code
    harness_.createSourceFile("src/calc.cpp", "int add(int a, int b) { return a + b + 1; }");

    // 8. User runs diff
    auto r6 = harness_.executeSlashCommand("/diff");
    EXPECT_TRUE(r6.success);

    // 9. User saves session
    auto sess_path = harness_.getSandboxRoot() / "dev_session.json";
    auto r7 = harness_.executeSlashCommand("/session save " + sess_path.string());
    EXPECT_TRUE(r7.success);
    EXPECT_TRUE(std::filesystem::exists(sess_path));

    // 10. User exits cleanly
    auto r8 = harness_.executeSlashCommand("/exit");
    EXPECT_TRUE(r8.should_exit);
}
