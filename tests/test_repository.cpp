#include <gtest/gtest.h>
#include "parser/ASTParser.h"
#include "repository/RepositoryIndex.h"

using namespace aios;

// ============================================================================
// ASTParser Multi-Language Tests
// ============================================================================

TEST(ASTParserTest, ParsesCppSymbolsAndIncludes) {
    ASTParser parser;
    std::string cpp_code = R"(
#include <iostream>
#include "kernel/Kernel.h"

// Math utility class
class MathHelper {
public:
    // Computes sum
    int add(int a, int b) {
        log_operation("add");
        return a + b;
    }
};

struct Point {
    int x;
    int y;
};

void global_calculate() {
    MathHelper helper;
    helper.add(1, 2);
}
)";

    ParsedFile pf = parser.parseFile("src/math.cpp", cpp_code);

    EXPECT_EQ(pf.language, "cpp");
    ASSERT_EQ(pf.imports.size(), 2);
    EXPECT_EQ(pf.imports[0], "<iostream>");
    EXPECT_EQ(pf.imports[1], "\"kernel/Kernel.h\"");

    EXPECT_GE(pf.symbols.size(), 3);

    // Verify class symbol
    auto class_it = std::find_if(pf.symbols.begin(), pf.symbols.end(), [](const CodeSymbol& s) { return s.name == "MathHelper"; });
    ASSERT_NE(class_it, pf.symbols.end());
    EXPECT_EQ(class_it->type, SymbolType::Class);
    EXPECT_NE(class_it->docstring.find("Math utility class"), std::string::npos);

    // Verify method symbol & callees
    auto add_it = std::find_if(pf.symbols.begin(), pf.symbols.end(), [](const CodeSymbol& s) { return s.name == "add"; });
    ASSERT_NE(add_it, pf.symbols.end());
    EXPECT_EQ(add_it->type, SymbolType::Method);
    EXPECT_EQ(add_it->parent_symbol, "MathHelper");
    EXPECT_NE(add_it->docstring.find("Computes sum"), std::string::npos);
}

TEST(ASTParserTest, ParsesPythonFunctionsAndClasses) {
    ASTParser parser;
    std::string py_code = R"(
import os
from typing import List

# Service orchestrator class
class OrchestratorService:
    """Manages workflows"""
    
    # Executes task
    async def process_task(self, task_name: str) -> bool:
        self.log_start(task_name)
        return True

def standalone_helper():
    pass
)";

    ParsedFile pf = parser.parseFile("services/orchestrator.py", py_code);

    EXPECT_EQ(pf.language, "python");
    ASSERT_GE(pf.imports.size(), 2);

    auto class_it = std::find_if(pf.symbols.begin(), pf.symbols.end(), [](const CodeSymbol& s) { return s.name == "OrchestratorService"; });
    ASSERT_NE(class_it, pf.symbols.end());
    EXPECT_EQ(class_it->type, SymbolType::Class);

    auto method_it = std::find_if(pf.symbols.begin(), pf.symbols.end(), [](const CodeSymbol& s) { return s.name == "process_task"; });
    ASSERT_NE(method_it, pf.symbols.end());
    EXPECT_EQ(method_it->type, SymbolType::Method);
    EXPECT_EQ(method_it->parent_symbol, "OrchestratorService");
}

TEST(ASTParserTest, ParsesTypeScriptAndRustSymbols) {
    ASTParser parser;
    std::string ts_code = R"(
import { Component } from 'react';

export class AppManager {
    public initialize(): void {
        console.log("ready");
    }
}
)";

    ParsedFile pf_ts = parser.parseFile("src/App.ts", ts_code);
    EXPECT_EQ(pf_ts.language, "typescript");
    EXPECT_FALSE(pf_ts.symbols.empty());
    EXPECT_EQ(pf_ts.symbols[0].name, "AppManager");

    std::string rust_code = R"(
use std::collections::HashMap;

pub struct AgentWorker {
    pub id: String,
}

pub fn execute_job(worker: &AgentWorker) {
    println!("Job started");
}
)";

    ParsedFile pf_rs = parser.parseFile("src/worker.rs", rust_code);
    EXPECT_EQ(pf_rs.language, "rust");
    EXPECT_GE(pf_rs.symbols.size(), 2);
}

// ============================================================================
// RepositoryIndex & Incremental Indexing Tests
// ============================================================================

TEST(RepositoryIndexTest, IndexesFilesIncrementally) {
    RepositoryIndex index;
    index.clear();

    std::string code_v1 = "class UserManager { void createUser() {} };";
    bool first_index = index.indexFile("src/user.cpp", code_v1);
    EXPECT_TRUE(first_index); // Newly indexed

    // Index same content again -> should skip
    bool duplicate_index = index.indexFile("src/user.cpp", code_v1);
    EXPECT_FALSE(duplicate_index); // Skipped because checksum matched

    // Modify file
    std::string code_v2 = "class UserManager { void createUser() {} void deleteUser() {} };";
    bool modified_index = index.indexFile("src/user.cpp", code_v2);
    EXPECT_TRUE(modified_index); // Re-indexed

    RepositoryStats stats = index.getStats();
    EXPECT_EQ(stats.total_files_indexed, 1);
    EXPECT_EQ(stats.modified_files_reindexed, 1);
    EXPECT_EQ(stats.total_symbols, 3); // UserManager, createUser, deleteUser
}

TEST(RepositoryIndexTest, FindsSymbolsAndCallers) {
    RepositoryIndex index;
    index.clear();

    std::string file1 = R"(
class AuthController {
    void login() {
        validateToken();
    }
};
)";

    std::string file2 = R"(
void validateToken() {
    verifySignature();
}
)";

    index.indexFile("src/auth.cpp", file1);
    index.indexFile("src/token.cpp", file2);

    // Exact symbol lookup
    auto syms = index.findSymbol("AuthController", true);
    ASSERT_EQ(syms.size(), 1);
    EXPECT_EQ(syms[0].name, "AuthController");
    EXPECT_EQ(syms[0].file_path, "src/auth.cpp");

    // Fuzzy symbol lookup
    auto fuzzy = index.findSymbol("Token", false);
    EXPECT_GE(fuzzy.size(), 1);

    // Find Callers of validateToken
    auto callers = index.findCallers("validateToken");
    ASSERT_EQ(callers.size(), 1);
    EXPECT_EQ(callers[0].name, "login");
    EXPECT_EQ(callers[0].parent_symbol, "AuthController");
}

TEST(RepositoryIndexTest, PerformsHybridBM25Search) {
    RepositoryIndex index;
    index.clear();

    index.indexFile("src/parser.cpp", "class JsonParser { void parseJsonPayload() { processTokens(); } };");
    index.indexFile("src/database.cpp", "class SqliteStorage { void executeSqlQuery() { sqlite3_exec(); } };");
    index.indexFile("src/network.cpp", "class HttpClient { void sendHttpRequest() { connectSocket(); } };");

    auto results = index.searchCode("JsonParser parse", 5);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].file_path, "src/parser.cpp");
    EXPECT_GT(results[0].score, 0.0);
    EXPECT_EQ(results[0].match_type, "hybrid_rrf");

    auto net_results = index.searchCode("HttpClient socket", 5);
    ASSERT_FALSE(net_results.empty());
    EXPECT_EQ(net_results[0].file_path, "src/network.cpp");
}
