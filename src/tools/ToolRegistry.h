// AIOS - MINI Coding Agent Operating System
// Tool Registry - Comprehensive tool system for agent operations

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <optional>
#include <variant>
#include <chrono>
#include <any>

namespace aios {

// Forward declarations
class FileSystem;
class TerminalExecutor;
class GitManager;
class LSPClient;
class ParserManager;
class HTTPClient;
class SandboxManager;

// Tool result types
using ToolResultValue = std::variant<
    std::string,
    int,
    long long,
    double,
    bool,
    std::vector<std::string>,
    std::unordered_map<std::string, std::string>
>;

struct ToolResult {
    bool success;
    std::string output;
    std::string error_message;
    std::unordered_map<std::string, ToolResultValue> data;
    std::chrono::milliseconds execution_time{0};
    int exit_code{0};
    
    static ToolResult ok(const std::string& output = "") {
        return ToolResult{true, output, "", {}, std::chrono::milliseconds{0}, 0};
    }
    
    static ToolResult error(const std::string& message, int code = -1) {
        return ToolResult{false, "", message, {}, std::chrono::milliseconds{0}, code};
    }
};

enum class ToolCategory {
    Filesystem,
    Terminal,
    Git,
    Search,
    LSP,
    Network,
    CodeAnalysis,
    Testing,
    Build,
    Database,
    Memory,
    Context,
    Agent,
    System
};

enum class ToolPermission {
    Read,
    Write,
    Execute,
    Network,
    Dangerous  // Operations that need explicit approval
};

struct ToolDefinition {
    std::string name;
    std::string description;
    ToolCategory category;
    std::vector<ToolPermission> required_permissions;
    std::vector<std::string> parameters;  // Parameter names
    std::unordered_map<std::string, std::string> parameter_descriptions;
    std::unordered_map<std::string, bool> parameter_required;
    std::unordered_map<std::string, std::string> parameter_defaults;
    std::string returns_description;
    std::vector<std::string> examples;
    bool is_dangerous = false;
    std::chrono::milliseconds timeout{30000};
};

struct ToolStats {
    size_t total_calls;
    size_t successful_calls;
    size_t failed_calls;
    std::chrono::milliseconds total_execution_time;
    std::chrono::milliseconds average_execution_time;
    std::chrono::milliseconds min_execution_time;
    std::chrono::milliseconds max_execution_time;
};

/**
 * @brief Base class for all tools
 */
class Tool {
public:
    virtual ~Tool() = default;
    
    // Get tool definition
    virtual ToolDefinition getDefinition() const = 0;
    
    // Execute the tool
    virtual ToolResult execute(const std::unordered_map<std::string, std::string>& params) = 0;
    
    // Get tool statistics
    virtual ToolStats getStats() const = 0;
    
    // Validate parameters
    virtual bool validateParams(const std::unordered_map<std::string, std::string>& params) const;
    
    // Check if tool is available
    virtual bool isAvailable() const { return true; }
    
protected:
    mutable std::mutex mutex_;
    ToolStats stats_{};
    
    void recordCall(bool success, std::chrono::milliseconds duration);
};

/**
 * @brief Filesystem Tools
 */
class FilesystemTools : public Tool {
public:
    explicit FilesystemTools(std::shared_ptr<FileSystem> fs);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<FileSystem> fs_;
    
    ToolResult readFile(const std::unordered_map<std::string, std::string>& params);
    ToolResult writeFile(const std::unordered_map<std::string, std::string>& params);
    ToolResult deleteFile(const std::unordered_map<std::string, std::string>& params);
    ToolResult listDirectory(const std::unordered_map<std::string, std::string>& params);
    ToolResult createDirectory(const std::unordered_map<std::string, std::string>& params);
    ToolResult fileExists(const std::unordered_map<std::string, std::string>& params);
    ToolResult getFileSize(const std::unordered_map<std::string, std::string>& params);
    ToolResult copyFile(const std::unordered_map<std::string, std::string>& params);
    ToolResult moveFile(const std::unordered_map<std::string, std::string>& params);
    ToolResult searchFiles(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Terminal Tools
 */
class TerminalTools : public Tool {
public:
    explicit TerminalTools(std::shared_ptr<TerminalExecutor> executor);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<TerminalExecutor> executor_;
    
    ToolResult runCommand(const std::unordered_map<std::string, std::string>& params);
    ToolResult runScript(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Git Tools
 */
class GitTools : public Tool {
public:
    explicit GitTools(std::shared_ptr<GitManager> git);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<GitManager> git_;
    
    ToolResult getStatus(const std::unordered_map<std::string, std::string>& params);
    ToolResult getDiff(const std::unordered_map<std::string, std::string>& params);
    ToolResult commit(const std::unordered_map<std::string, std::string>& params);
    ToolResult push(const std::unordered_map<std::string, std::string>& params);
    ToolResult pull(const std::unordered_map<std::string, std::string>& params);
    ToolResult checkout(const std::unordered_map<std::string, std::string>& params);
    ToolResult createBranch(const std::unordered_map<std::string, std::string>& params);
    ToolResult mergeBranch(const std::unordered_map<std::string, std::string>& params);
    ToolResult getLog(const std::unordered_map<std::string, std::string>& params);
    ToolResult stash(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Search Tools
 */
class SearchTools : public Tool {
public:
    explicit SearchTools(std::shared_ptr<FileSystem> fs);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<FileSystem> fs_;
    
    ToolResult grepSearch(const std::unordered_map<std::string, std::string>& params);
    ToolResult regexSearch(const std::unordered_map<std::string, std::string>& params);
    ToolResult findFiles(const std::unordered_map<std::string, std::string>& params);
    ToolResult findSymbol(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief LSP Tools
 */
class LSPTools : public Tool {
public:
    explicit LSPTools(std::shared_ptr<LSPClient> lsp);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<LSPClient> lsp_;
    
    ToolResult goToDefinition(const std::unordered_map<std::string, std::string>& params);
    ToolResult findReferences(const std::unordered_map<std::string, std::string>& params);
    ToolResult getHover(const std::unordered_map<std::string, std::string>& params);
    ToolResult getCompletion(const std::unordered_map<std::string, std::string>& params);
    ToolResult getDiagnostics(const std::unordered_map<std::string, std::string>& params);
    ToolResult renameSymbol(const std::unordered_map<std::string, std::string>& params);
    ToolResult formatDocument(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Network Tools
 */
class NetworkTools : public Tool {
public:
    explicit NetworkTools(std::shared_ptr<HTTPClient> http);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<HTTPClient> http_;
    
    ToolResult httpGet(const std::unordered_map<std::string, std::string>& params);
    ToolResult httpPost(const std::unordered_map<std::string, std::string>& params);
    ToolResult downloadFile(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Code Analysis Tools
 */
class CodeAnalysisTools : public Tool {
public:
    explicit CodeAnalysisTools(std::shared_ptr<ParserManager> parser);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<ParserManager> parser_;
    
    ToolResult parseFile(const std::unordered_map<std::string, std::string>& params);
    ToolResult getAST(const std::unordered_map<std::string, std::string>& params);
    ToolResult getSymbols(const std::unordered_map<std::string, std::string>& params);
    ToolResult getCallGraph(const std::unordered_map<std::string, std::string>& params);
    ToolResult getDependencies(const std::unordered_map<std::string, std::string>& params);
    ToolResult analyzeComplexity(const std::unordered_map<std::string, std::string>& params);
};



/**
 * @brief Build Tools
 */
class BuildTools : public Tool {
public:
    explicit BuildTools(std::shared_ptr<TerminalExecutor> executor);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<TerminalExecutor> executor_;
    
    ToolResult build(const std::unordered_map<std::string, std::string>& params);
    ToolResult clean(const std::unordered_map<std::string, std::string>& params);
    ToolResult install(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Memory Tools
 */
class MemoryTools : public Tool {
public:
    explicit MemoryTools(std::shared_ptr<class MemoryManager> memory);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<class MemoryManager> memory_;
    
    ToolResult storeMemory(const std::unordered_map<std::string, std::string>& params);
    ToolResult retrieveMemory(const std::unordered_map<std::string, std::string>& params);
    ToolResult searchMemory(const std::unordered_map<std::string, std::string>& params);
    ToolResult deleteMemory(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Context Tools
 */
class ContextTools : public Tool {
public:
    explicit ContextTools(std::shared_ptr<class ContextEngine> context);
    
    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }
    
private:
    std::shared_ptr<class ContextEngine> context_;
    
    ToolResult addContext(const std::unordered_map<std::string, std::string>& params);
    ToolResult getContext(const std::unordered_map<std::string, std::string>& params);
    ToolResult removeContext(const std::unordered_map<std::string, std::string>& params);
    ToolResult rankFiles(const std::unordered_map<std::string, std::string>& params);
};

/**
 * @brief Tool Registry - Manages all available tools
 */
class ToolRegistry {
public:
    static ToolRegistry& instance();
    
    // Initialize registry
    bool initialize();
    
    // Shutdown registry
    void shutdown();
    
    // Register a tool
    bool registerTool(const std::string& name, std::shared_ptr<Tool> tool);
    
    // Unregister a tool
    bool unregisterTool(const std::string& name);
    
    // Get a tool by name
    std::shared_ptr<Tool> getTool(const std::string& name) const;
    
    // List all tools
    std::vector<std::string> listTools() const;
    
    // List tools by category
    std::vector<std::string> listToolsByCategory(ToolCategory category) const;
    
    // Get tool definition
    std::optional<ToolDefinition> getToolDefinition(const std::string& name) const;
    
    // Execute a tool
    ToolResult executeTool(const std::string& name,
                           const std::unordered_map<std::string, std::string>& params);
    
    // Execute tool with permission check
    ToolResult executeWithPermission(const std::string& name,
                                      const std::unordered_map<std::string, std::string>& params,
                                      const std::vector<ToolPermission>& granted_permissions);
    
    // Get registry statistics
    struct RegistryStats {
        size_t total_tools;
        size_t total_calls;
        size_t successful_calls;
        size_t failed_calls;
        std::unordered_map<std::string, ToolStats> tool_stats;
    };
    RegistryStats getStats() const;
    
    // Set dependencies
    void setFileSystem(std::shared_ptr<FileSystem> fs);
    void setTerminalExecutor(std::shared_ptr<TerminalExecutor> terminal);
    void setGitManager(std::shared_ptr<GitManager> git);
    void setLSPClient(std::shared_ptr<LSPClient> lsp);
    void setParserManager(std::shared_ptr<ParserManager> parser);
    void setHTTPClient(std::shared_ptr<HTTPClient> http);
    void setSandboxManager(std::shared_ptr<SandboxManager> sandbox);
    void setMemoryManager(std::shared_ptr<class MemoryManager> memory);
    void setContextEngine(std::shared_ptr<class ContextEngine> context);
    
    // Create default tools
    void createDefaultTools();
    
private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;
    
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
    
    // Dependencies (initialized later)
    std::shared_ptr<FileSystem> fs_;
    std::shared_ptr<TerminalExecutor> terminal_;
    std::shared_ptr<GitManager> git_;
    std::shared_ptr<LSPClient> lsp_;
    std::shared_ptr<ParserManager> parser_;
    std::shared_ptr<HTTPClient> http_;
    std::shared_ptr<SandboxManager> sandbox_;
    std::shared_ptr<class MemoryManager> memory_;
    std::shared_ptr<class ContextEngine> context_;
    
    RegistryStats stats_{};
};

} // namespace aios
