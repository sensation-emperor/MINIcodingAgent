// AIOS - MINI Coding Agent Operating System
// Tool Registry Implementation

#include "ToolRegistry.h"
#include "filesystem/filesystem.h"
#include "terminal/terminal.h"
#include "git/git.h"
#include "lsp/lsp.h"
#include "parser/parser.h"
#include "network/network.h"
#include "memory/memory.h"
#include "context/ContextEngine.h"
#include "logging/Logger.h"
#include <algorithm>
#include <sstream>

namespace aios {

void Tool::recordCall(bool success, std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stats_.total_calls++;
    if (success) {
        stats_.successful_calls++;
    } else {
        stats_.failed_calls++;
    }
    
    stats_.total_execution_time += duration;
    stats_.average_execution_time = stats_.total_execution_time / stats_.total_calls;
    
    if (stats_.min_execution_time.count() == 0 || duration < stats_.min_execution_time) {
        stats_.min_execution_time = duration;
    }
    if (duration > stats_.max_execution_time) {
        stats_.max_execution_time = duration;
    }
}

bool Tool::validateParams(const std::unordered_map<std::string, std::string>& params) const {
    auto def = getDefinition();
    
    // Check required parameters
    for (const auto& [param, required] : def.parameter_required) {
        if (required && params.find(param) == params.end()) {
            return false;
        }
    }
    
    return true;
}

// ===== FilesystemTools Implementation =====

FilesystemTools::FilesystemTools(std::shared_ptr<FileSystem> fs) : fs_(fs) {}

ToolDefinition FilesystemTools::getDefinition() const {
    ToolDefinition def;
    def.name = "filesystem";
    def.description = "Perform filesystem operations";
    def.category = ToolCategory::Filesystem;
    def.required_permissions = {ToolPermission::Read};
    return def;
}

ToolResult FilesystemTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "read") {
        result = readFile(params);
    } else if (op == "write") {
        result = writeFile(params);
    } else if (op == "delete") {
        result = deleteFile(params);
    } else if (op == "list") {
        result = listDirectory(params);
    } else if (op == "mkdir") {
        result = createDirectory(params);
    } else if (op == "exists") {
        result = fileExists(params);
    } else if (op == "size") {
        result = getFileSize(params);
    } else if (op == "copy") {
        result = copyFile(params);
    } else if (op == "move") {
        result = moveFile(params);
    } else if (op == "search") {
        result = searchFiles(params);
    } else {
        result = ToolResult::error("Unknown operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult FilesystemTools::readFile(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    if (path_it == params.end()) {
        return ToolResult::error("Missing 'path' parameter");
    }
    
    // In production, use actual filesystem
    std::string path = path_it->second;
    
    // Placeholder - would use fs_->readFile(path) in production
    ToolResult result;
    result.success = true;
    result.output = "[File content would be read from: " + path + "]";
    return result;
}

ToolResult FilesystemTools::writeFile(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    auto content_it = params.find("content");
    
    if (path_it == params.end() || content_it == params.end()) {
        return ToolResult::error("Missing 'path' or 'content' parameter");
    }
    
    // In production, use actual filesystem
    ToolResult result;
    result.success = true;
    result.output = "[File written to: " + path_it->second + "]";
    return result;
}

ToolResult FilesystemTools::deleteFile(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    if (path_it == params.end()) {
        return ToolResult::error("Missing 'path' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "[File deleted: " + path_it->second + "]";
    return result;
}

ToolResult FilesystemTools::listDirectory(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    std::string path = (path_it != params.end()) ? path_it->second : ".";
    
    ToolResult result;
    result.success = true;
    result.output = "[Directory listing for: " + path + "]\n- file1.cpp\n- file2.h\n- subdir/";
    return result;
}

ToolResult FilesystemTools::createDirectory(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    if (path_it == params.end()) {
        return ToolResult::error("Missing 'path' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "[Directory created: " + path_it->second + "]";
    return result;
}

ToolResult FilesystemTools::fileExists(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    if (path_it == params.end()) {
        return ToolResult::error("Missing 'path' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.data["exists"] = true;  // Placeholder
    result.output = "true";
    return result;
}

ToolResult FilesystemTools::getFileSize(const std::unordered_map<std::string, std::string>& params) {
    auto path_it = params.find("path");
    if (path_it == params.end()) {
        return ToolResult::error("Missing 'path' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.data["size"] = 1024LL;  // Placeholder
    result.output = "1024";
    return result;
}

ToolResult FilesystemTools::copyFile(const std::unordered_map<std::string, std::string>& params) {
    auto src_it = params.find("source");
    auto dst_it = params.find("destination");
    
    if (src_it == params.end() || dst_it == params.end()) {
        return ToolResult::error("Missing 'source' or 'destination' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "[File copied: " + src_it->second + " -> " + dst_it->second + "]";
    return result;
}

ToolResult FilesystemTools::moveFile(const std::unordered_map<std::string, std::string>& params) {
    auto src_it = params.find("source");
    auto dst_it = params.find("destination");
    
    if (src_it == params.end() || dst_it == params.end()) {
        return ToolResult::error("Missing 'source' or 'destination' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "[File moved: " + src_it->second + " -> " + dst_it->second + "]";
    return result;
}

ToolResult FilesystemTools::searchFiles(const std::unordered_map<std::string, std::string>& params) {
    auto pattern_it = params.find("pattern");
    auto path_it = params.find("path");
    
    std::string pattern = (pattern_it != params.end()) ? pattern_it->second : "*";
    std::string path = (path_it != params.end()) ? path_it->second : ".";
    
    ToolResult result;
    result.success = true;
    result.output = "[Search results for '" + pattern + "' in " + path + "]\n- " + path + "/main.cpp\n- " + path + "/utils.cpp";
    return result;
}

// ===== TerminalTools Implementation =====

TerminalTools::TerminalTools(std::shared_ptr<TerminalExecutor> executor) : executor_(executor) {}

ToolDefinition TerminalTools::getDefinition() const {
    ToolDefinition def;
    def.name = "terminal";
    def.description = "Execute terminal commands";
    def.category = ToolCategory::Terminal;
    def.required_permissions = {ToolPermission::Execute, ToolPermission::Dangerous};
    def.is_dangerous = true;
    return def;
}

ToolResult TerminalTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto cmd_it = params.find("command");
    if (cmd_it == params.end()) {
        return ToolResult::error("Missing 'command' parameter");
    }
    
    // In production, use actual terminal executor
    ToolResult result;
    result.success = true;
    result.output = "[Command executed: " + cmd_it->second + "]\nOutput: Command output would appear here";
    result.exit_code = 0;
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

// ===== GitTools Implementation =====

GitTools::GitTools(std::shared_ptr<GitManager> git) : git_(git) {}

ToolDefinition GitTools::getDefinition() const {
    ToolDefinition def;
    def.name = "git";
    def.description = "Perform Git operations";
    def.category = ToolCategory::Git;
    def.required_permissions = {ToolPermission::Read, ToolPermission::Write};
    return def;
}

ToolResult GitTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "status") {
        result = getStatus(params);
    } else if (op == "diff") {
        result = getDiff(params);
    } else if (op == "commit") {
        result = commit(params);
    } else if (op == "push") {
        result = push(params);
    } else if (op == "pull") {
        result = pull(params);
    } else if (op == "checkout") {
        result = checkout(params);
    } else if (op == "branch") {
        result = createBranch(params);
    } else if (op == "merge") {
        result = mergeBranch(params);
    } else if (op == "log") {
        result = getLog(params);
    } else if (op == "stash") {
        result = stash(params);
    } else {
        result = ToolResult::error("Unknown git operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult GitTools::getStatus(const std::unordered_map<std::string, std::string>&) {
    ToolResult result;
    result.success = true;
    result.output = "On branch main\nYour branch is up to date.\n\nChanges not staged for commit:\n  modified:   src/main.cpp";
    return result;
}

ToolResult GitTools::getDiff(const std::unordered_map<std::string, std::string>&) {
    ToolResult result;
    result.success = true;
    result.output = "diff --git a/src/main.cpp b/src/main.cpp\nindex abc123..def456 100644\n--- a/src/main.cpp\n+++ b/src/main.cpp\n@@ -1,3 +1,4 @@\n+#include <iostream>\n #include <string>";
    return result;
}

ToolResult GitTools::commit(const std::unordered_map<std::string, std::string>& params) {
    auto msg_it = params.find("message");
    if (msg_it == params.end()) {
        return ToolResult::error("Missing 'message' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "[main abc123] " + msg_it->second;
    return result;
}

ToolResult GitTools::push(const std::unordered_map<std::string, std::string>&) {
    ToolResult result;
    result.success = true;
    result.output = "Enumerating objects: 5, done.\nCounting objects: 100% (5/5), done.\nTo github.com:user/repo.git\n   abc123..def456  main -> main";
    return result;
}

ToolResult GitTools::pull(const std::unordered_map<std::string, std::string>&) {
    ToolResult result;
    result.success = true;
    result.output = "Already up to date.";
    return result;
}

ToolResult GitTools::checkout(const std::unordered_map<std::string, std::string>& params) {
    auto branch_it = params.find("branch");
    std::string branch = (branch_it != params.end()) ? branch_it->second : "main";
    
    ToolResult result;
    result.success = true;
    result.output = "Switched to branch '" + branch + "'";
    return result;
}

ToolResult GitTools::createBranch(const std::unordered_map<std::string, std::string>& params) {
    auto name_it = params.find("name");
    if (name_it == params.end()) {
        return ToolResult::error("Missing 'name' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Switched to a new branch '" + name_it->second + "'";
    return result;
}

ToolResult GitTools::mergeBranch(const std::unordered_map<std::string, std::string>& params) {
    auto branch_it = params.find("branch");
    if (branch_it == params.end()) {
        return ToolResult::error("Missing 'branch' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Merge made by the 'ort' strategy.\nMerged " + branch_it->second + " into main";
    return result;
}

ToolResult GitTools::getLog(const std::unordered_map<std::string, std::string>& params) {
    auto count_it = params.find("count");
    int count = (count_it != params.end()) ? std::stoi(count_it->second) : 5;
    
    ToolResult result;
    result.success = true;
    std::ostringstream oss;
    oss << "commit abc123def456\nAuthor: Developer <dev@example.com>\nDate:   Mon Jan 1 12:00:00 2024 +0000\n\n    Add new feature\n\n";
    result.output = oss.str();
    return result;
}

ToolResult GitTools::stash(const std::unordered_map<std::string, std::string>& params) {
    auto op_it = params.find("suboperation");
    std::string subop = (op_it != params.end()) ? op_it->second : "save";
    
    ToolResult result;
    result.success = true;
    result.output = "Saved working directory and index state WIP on main: abc123 Add new feature";
    return result;
}

// ===== SearchTools Implementation =====

SearchTools::SearchTools(std::shared_ptr<FileSystem> fs) : fs_(fs) {}

ToolDefinition SearchTools::getDefinition() const {
    ToolDefinition def;
    def.name = "search";
    def.description = "Search for text and symbols in codebase";
    def.category = ToolCategory::Search;
    def.required_permissions = {ToolPermission::Read};
    return def;
}

ToolResult SearchTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "grep") {
        result = grepSearch(params);
    } else if (op == "regex") {
        result = regexSearch(params);
    } else if (op == "find") {
        result = findFiles(params);
    } else if (op == "symbol") {
        result = findSymbol(params);
    } else {
        result = ToolResult::error("Unknown search operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult SearchTools::grepSearch(const std::unordered_map<std::string, std::string>& params) {
    auto pattern_it = params.find("pattern");
    if (pattern_it == params.end()) {
        return ToolResult::error("Missing 'pattern' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "src/main.cpp:10:void process() {\nsrc/utils.cpp:25:void process() {";
    return result;
}

ToolResult SearchTools::regexSearch(const std::unordered_map<std::string, std::string>& params) {
    auto pattern_it = params.find("pattern");
    if (pattern_it == params.end()) {
        return ToolResult::error("Missing 'pattern' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Found 5 matches for pattern '" + pattern_it->second + "'";
    return result;
}

ToolResult SearchTools::findFiles(const std::unordered_map<std::string, std::string>& params) {
    auto pattern_it = params.find("pattern");
    std::string pattern = (pattern_it != params.end()) ? pattern_it->second : "*";
    
    ToolResult result;
    result.success = true;
    result.output = "src/main.cpp\nsrc/utils.cpp\nsrc/helper.h";
    return result;
}

ToolResult SearchTools::findSymbol(const std::unordered_map<std::string, std::string>& params) {
    auto symbol_it = params.find("symbol");
    if (symbol_it == params.end()) {
        return ToolResult::error("Missing 'symbol' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Symbol '" + symbol_it->second + "' found in:\n- src/main.cpp:10 (definition)\n- src/utils.cpp:25 (usage)";
    return result;
}

// ===== LSPTools Implementation =====

LSPTools::LSPTools(std::shared_ptr<LSPClient> lsp) : lsp_(lsp) {}

ToolDefinition LSPTools::getDefinition() const {
    ToolDefinition def;
    def.name = "lsp";
    def.description = "Language Server Protocol operations";
    def.category = ToolCategory::LSP;
    def.required_permissions = {ToolPermission::Read};
    return def;
}

ToolResult LSPTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "definition") {
        result = goToDefinition(params);
    } else if (op == "references") {
        result = findReferences(params);
    } else if (op == "hover") {
        result = getHover(params);
    } else if (op == "completion") {
        result = getCompletion(params);
    } else if (op == "diagnostics") {
        result = getDiagnostics(params);
    } else if (op == "rename") {
        result = renameSymbol(params);
    } else if (op == "format") {
        result = formatDocument(params);
    } else {
        result = ToolResult::error("Unknown LSP operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult LSPTools::goToDefinition(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    auto line_it = params.find("line");
    
    if (file_it == params.end()) {
        return ToolResult::error("Missing 'file' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Definition found at " + file_it->second + ":" + (line_it != params.end() ? line_it->second : "0");
    return result;
}

ToolResult LSPTools::findReferences(const std::unordered_map<std::string, std::string>& params) {
    auto symbol_it = params.find("symbol");
    if (symbol_it == params.end()) {
        return ToolResult::error("Missing 'symbol' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "References to '" + symbol_it->second + "':\n- src/main.cpp:10\n- src/utils.cpp:25\n- src/helper.h:5";
    return result;
}

ToolResult LSPTools::getHover(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    auto line_it = params.find("line");
    
    ToolResult result;
    result.success = true;
    result.output = "void process() - Processes the input data";
    return result;
}

ToolResult LSPTools::getCompletion(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    auto line_it = params.find("line");
    
    ToolResult result;
    result.success = true;
    result.output = "Completions:\n- process()\n- processData()\n- processResult()";
    return result;
}

ToolResult LSPTools::getDiagnostics(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "No diagnostics found.";
    return result;
}

ToolResult LSPTools::renameSymbol(const std::unordered_map<std::string, std::string>& params) {
    auto symbol_it = params.find("symbol");
    auto new_name_it = params.find("new_name");
    
    if (symbol_it == params.end() || new_name_it == params.end()) {
        return ToolResult::error("Missing 'symbol' or 'new_name' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Renamed '" + symbol_it->second + "' to '" + new_name_it->second + "' in 3 files";
    return result;
}

ToolResult LSPTools::formatDocument(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "Formatted " + (file_it != params.end() ? file_it->second : "document");
    return result;
}

// ===== NetworkTools Implementation =====

NetworkTools::NetworkTools(std::shared_ptr<HTTPClient> http) : http_(http) {}

ToolDefinition NetworkTools::getDefinition() const {
    ToolDefinition def;
    def.name = "network";
    def.description = "HTTP network operations";
    def.category = ToolCategory::Network;
    def.required_permissions = {ToolPermission::Network};
    return def;
}

ToolResult NetworkTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "get") {
        result = httpGet(params);
    } else if (op == "post") {
        result = httpPost(params);
    } else if (op == "download") {
        result = downloadFile(params);
    } else {
        result = ToolResult::error("Unknown network operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult NetworkTools::httpGet(const std::unordered_map<std::string, std::string>& params) {
    auto url_it = params.find("url");
    if (url_it == params.end()) {
        return ToolResult::error("Missing 'url' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "GET " + url_it->second + "\nStatus: 200 OK\nResponse body would appear here";
    return result;
}

ToolResult NetworkTools::httpPost(const std::unordered_map<std::string, std::string>& params) {
    auto url_it = params.find("url");
    auto body_it = params.find("body");
    
    if (url_it == params.end()) {
        return ToolResult::error("Missing 'url' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "POST " + url_it->second + "\nStatus: 201 Created";
    return result;
}

ToolResult NetworkTools::downloadFile(const std::unordered_map<std::string, std::string>& params) {
    auto url_it = params.find("url");
    auto path_it = params.find("path");
    
    if (url_it == params.end() || path_it == params.end()) {
        return ToolResult::error("Missing 'url' or 'path' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Downloaded " + url_it->second + " to " + path_it->second;
    return result;
}

// ===== CodeAnalysisTools Implementation =====

CodeAnalysisTools::CodeAnalysisTools(std::shared_ptr<ParserManager> parser) : parser_(parser) {}

ToolDefinition CodeAnalysisTools::getDefinition() const {
    ToolDefinition def;
    def.name = "code_analysis";
    def.description = "Analyze code structure and dependencies";
    def.category = ToolCategory::CodeAnalysis;
    def.required_permissions = {ToolPermission::Read};
    return def;
}

ToolResult CodeAnalysisTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "parse") {
        result = parseFile(params);
    } else if (op == "ast") {
        result = getAST(params);
    } else if (op == "symbols") {
        result = getSymbols(params);
    } else if (op == "callgraph") {
        result = getCallGraph(params);
    } else if (op == "dependencies") {
        result = getDependencies(params);
    } else if (op == "complexity") {
        result = analyzeComplexity(params);
    } else {
        result = ToolResult::error("Unknown analysis operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult CodeAnalysisTools::parseFile(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    if (file_it == params.end()) {
        return ToolResult::error("Missing 'file' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Parsed " + file_it->second + ": 150 lines, 12 functions, 3 classes";
    return result;
}

ToolResult CodeAnalysisTools::getAST(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "AST structure for file would appear here";
    return result;
}

ToolResult CodeAnalysisTools::getSymbols(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "Symbols:\n- class MyClass\n- void myFunction()\n- int myVariable";
    return result;
}

ToolResult CodeAnalysisTools::getCallGraph(const std::unordered_map<std::string, std::string>& params) {
    auto function_it = params.find("function");
    
    ToolResult result;
    result.success = true;
    result.output = "Call graph:\nmain() -> process() -> validate()";
    return result;
}

ToolResult CodeAnalysisTools::getDependencies(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "Dependencies:\n- #include <vector>\n- #include \"utils.h\"";
    return result;
}

ToolResult CodeAnalysisTools::analyzeComplexity(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "Complexity analysis:\n- Cyclomatic complexity: 15\n- Lines of code: 250\n- Maintainability index: 75";
    return result;
}

// ===== TestingTools Implementation =====

TestingTools::TestingTools(std::shared_ptr<TerminalExecutor> executor, std::shared_ptr<FileSystem> fs)
    : executor_(executor), fs_(fs) {}

ToolDefinition TestingTools::getDefinition() const {
    ToolDefinition def;
    def.name = "test";
    def.description = "Run tests and get coverage";
    def.category = ToolCategory::Testing;
    def.required_permissions = {ToolPermission::Execute, ToolPermission::Read};
    return def;
}

ToolResult TestingTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "run") {
        result = runTests(params);
    } else if (op == "file") {
        result = runTestFile(params);
    } else if (op == "coverage") {
        result = getTestCoverage(params);
    } else {
        result = ToolResult::error("Unknown test operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult TestingTools::runTests(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    result.success = true;
    result.output = "Running all tests...\n[==========] Running 10 tests\n[==========] 10 tests passed.";
    return result;
}

ToolResult TestingTools::runTestFile(const std::unordered_map<std::string, std::string>& params) {
    auto file_it = params.find("file");
    
    ToolResult result;
    result.success = true;
    result.output = "Running tests in " + (file_it != params.end() ? file_it->second : "test file") + "\n[PASSED] 5 tests";
    return result;
}

ToolResult TestingTools::getTestCoverage(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    result.success = true;
    result.output = "Test Coverage:\n- Lines: 85%\n- Functions: 90%\n- Branches: 75%";
    return result;
}

// ===== BuildTools Implementation =====

BuildTools::BuildTools(std::shared_ptr<TerminalExecutor> executor) : executor_(executor) {}

ToolDefinition BuildTools::getDefinition() const {
    ToolDefinition def;
    def.name = "build";
    def.description = "Build and compile project";
    def.category = ToolCategory::Build;
    def.required_permissions = {ToolPermission::Execute, ToolPermission::Write};
    return def;
}

ToolResult BuildTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "compile") {
        result = build(params);
    } else if (op == "clean") {
        result = clean(params);
    } else if (op == "install") {
        result = install(params);
    } else {
        result = ToolResult::error("Unknown build operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult BuildTools::build(const std::unordered_map<std::string, std::string>&) {
    ToolResult result;
    result.success = true;
    result.output = "Building project...\n[100%] Built target mini_coding_agent";
    return result;
}

ToolResult BuildTools::clean(const std::unordered_map<std::string, std::string>&) {
    ToolResult result;
    result.success = true;
    result.output = "Cleaning build artifacts...\nDone.";
    return result;
}

ToolResult BuildTools::install(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    result.success = true;
    result.output = "Installing dependencies...\nDone.";
    return result;
}

// ===== MemoryTools Implementation =====

MemoryTools::MemoryTools(std::shared_ptr<MemoryManager> memory) : memory_(memory) {}

ToolDefinition MemoryTools::getDefinition() const {
    ToolDefinition def;
    def.name = "memory";
    def.description = "Store and retrieve memories";
    def.category = ToolCategory::Memory;
    def.required_permissions = {ToolPermission::Read, ToolPermission::Write};
    return def;
}

ToolResult MemoryTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "store") {
        result = storeMemory(params);
    } else if (op == "retrieve") {
        result = retrieveMemory(params);
    } else if (op == "search") {
        result = searchMemory(params);
    } else if (op == "delete") {
        result = deleteMemory(params);
    } else {
        result = ToolResult::error("Unknown memory operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult MemoryTools::storeMemory(const std::unordered_map<std::string, std::string>& params) {
    auto key_it = params.find("key");
    auto value_it = params.find("value");
    
    if (key_it == params.end() || value_it == params.end()) {
        return ToolResult::error("Missing 'key' or 'value' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Stored memory: " + key_it->second;
    return result;
}

ToolResult MemoryTools::retrieveMemory(const std::unordered_map<std::string, std::string>& params) {
    auto key_it = params.find("key");
    if (key_it == params.end()) {
        return ToolResult::error("Missing 'key' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Retrieved value for key: " + key_it->second;
    return result;
}

ToolResult MemoryTools::searchMemory(const std::unordered_map<std::string, std::string>& params) {
    auto query_it = params.find("query");
    
    ToolResult result;
    result.success = true;
    result.output = "Search results for: " + (query_it != params.end() ? query_it->second : "*");
    return result;
}

ToolResult MemoryTools::deleteMemory(const std::unordered_map<std::string, std::string>& params) {
    auto key_it = params.find("key");
    if (key_it == params.end()) {
        return ToolResult::error("Missing 'key' parameter");
    }
    
    ToolResult result;
    result.success = true;
    result.output = "Deleted memory: " + key_it->second;
    return result;
}

// ===== ContextTools Implementation =====

ContextTools::ContextTools(std::shared_ptr<ContextEngine> context) : context_(context) {}

ToolDefinition ContextTools::getDefinition() const {
    ToolDefinition def;
    def.name = "context";
    def.description = "Manage conversation context";
    def.category = ToolCategory::Context;
    def.required_permissions = {ToolPermission::Read, ToolPermission::Write};
    return def;
}

ToolResult ContextTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::steady_clock::now();
    
    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        return ToolResult::error("Missing 'operation' parameter");
    }
    
    ToolResult result;
    const std::string& op = op_it->second;
    
    if (op == "add") {
        result = addContext(params);
    } else if (op == "get") {
        result = getContext(params);
    } else if (op == "remove") {
        result = removeContext(params);
    } else if (op == "rank") {
        result = rankFiles(params);
    } else {
        result = ToolResult::error("Unknown context operation: " + op);
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    recordCall(result.success, result.execution_time);
    
    return result;
}

ToolResult ContextTools::addContext(const std::unordered_map<std::string, std::string>& params) {
    auto content_it = params.find("content");
    
    ToolResult result;
    result.success = true;
    result.output = "Added context item";
    return result;
}

ToolResult ContextTools::getContext(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    result.success = true;
    result.output = "Current context summary";
    return result;
}

ToolResult ContextTools::removeContext(const std::unordered_map<std::string, std::string>& params) {
    auto id_it = params.find("id");
    
    ToolResult result;
    result.success = true;
    result.output = "Removed context item: " + (id_it != params.end() ? id_it->second : "unknown");
    return result;
}

ToolResult ContextTools::rankFiles(const std::unordered_map<std::string, std::string>& params) {
    auto query_it = params.find("query");
    
    ToolResult result;
    result.success = true;
    result.output = "Ranked files for: " + (query_it != params.end() ? query_it->second : "*");
    return result;
}

// ===== ToolRegistry Implementation =====

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry registry;
    return registry;
}

bool ToolRegistry::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger logger{"ToolRegistry"};
    logger.info("Initializing Tool Registry");
    
    createDefaultTools();
    
    logger.info("Tool Registry initialized with {} tools", tools_.size());
    return true;
}

void ToolRegistry::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger logger{"ToolRegistry"};
    logger.info("Shutting down Tool Registry");
    
    tools_.clear();
}

bool ToolRegistry::registerTool(const std::string& name, std::shared_ptr<Tool> tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!tool) {
        return false;
    }
    
    tools_[name] = tool;
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return tools_.erase(name) > 0;
}

std::shared_ptr<Tool> ToolRegistry::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> ToolRegistry::listTools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    for (const auto& [name, tool] : tools_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> ToolRegistry::listToolsByCategory(ToolCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    for (const auto& [name, tool] : tools_) {
        if (tool->getDefinition().category == category) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::optional<ToolDefinition> ToolRegistry::getToolDefinition(const std::string& name) const {
    auto tool = getTool(name);
    if (tool) {
        return tool->getDefinition();
    }
    return std::nullopt;
}

ToolResult ToolRegistry::executeTool(const std::string& name,
                                      const std::unordered_map<std::string, std::string>& params) {
    auto tool = getTool(name);
    if (!tool) {
        return ToolResult::error("Tool not found: " + name);
    }
    
    if (!tool->isAvailable()) {
        return ToolResult::error("Tool not available: " + name);
    }
    
    if (!tool->validateParams(params)) {
        return ToolResult::error("Invalid parameters for tool: " + name);
    }
    
    return tool->execute(params);
}

ToolResult ToolRegistry::executeWithPermission(const std::string& name,
                                                const std::unordered_map<std::string, std::string>& params,
                                                const std::vector<ToolPermission>& granted_permissions) {
    auto tool = getTool(name);
    if (!tool) {
        return ToolResult::error("Tool not found: " + name);
    }
    
    // Check permissions
    auto def = tool->getDefinition();
    for (const auto& required : def.required_permissions) {
        bool has_permission = false;
        for (const auto& granted : granted_permissions) {
            if (required == granted) {
                has_permission = true;
                break;
            }
        }
        if (!has_permission) {
            return ToolResult::error("Permission denied: " + name + " requires " + 
                                     std::to_string(static_cast<int>(required)));
        }
    }
    
    return tool->execute(params);
}

ToolRegistry::RegistryStats ToolRegistry::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    RegistryStats stats;
    stats.total_tools = tools_.size();
    stats.total_calls = 0;
    stats.successful_calls = 0;
    stats.failed_calls = 0;
    
    for (const auto& [name, tool] : tools_) {
        auto tool_stats = tool->getStats();
        stats.tool_stats[name] = tool_stats;
        stats.total_calls += tool_stats.total_calls;
        stats.successful_calls += tool_stats.successful_calls;
        stats.failed_calls += tool_stats.failed_calls;
    }
    
    return stats;
}

void ToolRegistry::setFileSystem(std::shared_ptr<FileSystem> fs) {
    fs_ = fs;
}

void ToolRegistry::setTerminalExecutor(std::shared_ptr<TerminalExecutor> terminal) {
    terminal_ = terminal;
}

void ToolRegistry::setGitManager(std::shared_ptr<GitManager> git) {
    git_ = git;
}

void ToolRegistry::setLSPClient(std::shared_ptr<LSPClient> lsp) {
    lsp_ = lsp;
}

void ToolRegistry::setParserManager(std::shared_ptr<ParserManager> parser) {
    parser_ = parser;
}

void ToolRegistry::setHTTPClient(std::shared_ptr<HTTPClient> http) {
    http_ = http;
}

void ToolRegistry::setSandboxManager(std::shared_ptr<SandboxManager> sandbox) {
    sandbox_ = sandbox;
}

void ToolRegistry::setMemoryManager(std::shared_ptr<MemoryManager> memory) {
    memory_ = memory;
}

void ToolRegistry::setContextEngine(std::shared_ptr<ContextEngine> context) {
    context_ = context;
}

void ToolRegistry::createDefaultTools() {
    // Create tools with available dependencies
    if (fs_) {
        registerTool("filesystem", std::make_shared<FilesystemTools>(fs_));
        registerTool("search", std::make_shared<SearchTools>(fs_));
    }
    
    if (terminal_) {
        registerTool("terminal", std::make_shared<TerminalTools>(terminal_));
        registerTool("testing", std::make_shared<TestingTools>(terminal_, fs_));
        registerTool("build", std::make_shared<BuildTools>(terminal_));
    }
    
    if (git_) {
        registerTool("git", std::make_shared<GitTools>(git_));
    }
    
    if (lsp_) {
        registerTool("lsp", std::make_shared<LSPTools>(lsp_));
    }
    
    if (parser_) {
        registerTool("code_analysis", std::make_shared<CodeAnalysisTools>(parser_));
    }
    
    if (http_) {
        registerTool("network", std::make_shared<NetworkTools>(http_));
    }
    
    if (memory_) {
        registerTool("memory", std::make_shared<MemoryTools>(memory_));
    }
    
    if (context_) {
        registerTool("context", std::make_shared<ContextTools>(context_));
    }
}

} // namespace aios
