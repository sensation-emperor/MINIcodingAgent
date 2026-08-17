// AIOS - MINI Coding Agent Operating System
// Context Engine - Manages conversation context, file ranking, and prompt building

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <optional>
#include <chrono>
#include <set>
#include <map>

namespace aios {

// Forward declarations
class MemoryManager;
class VectorDatabase;
class RepositoryIndex;

enum class ContextType {
    Conversation,
    File,
    Symbol,
    Dependency,
    GitHistory,
    TerminalOutput,
    Error,
    Documentation,
    UserPreference
};

struct ContextItem {
    std::string id;
    ContextType type;
    std::string content;
    std::string source;  // File path, conversation ID, etc.
    double relevance_score;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point accessed_at;
    size_t access_count;
    size_t token_count;
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> metadata;
    bool pinned;  // If true, won't be evicted
};

struct ConversationState {
    std::string conversation_id;
    std::string current_goal;
    std::vector<std::string> active_tasks;
    std::vector<std::string> open_questions;
    std::vector<std::string> constraints;
    std::vector<std::string> assumptions;
    std::vector<std::string> todo_list;
    std::vector<std::string> running_agents;
    std::string current_directory;
    std::string current_git_branch;
    std::unordered_map<std::string, std::string> user_preferences;
    std::string project_summary;
    std::vector<std::string> important_files;
    std::vector<std::string> recent_edits;
    std::vector<std::string> failed_attempts;
    std::string current_objective;
    double confidence_estimate;
    std::chrono::milliseconds time_spent;
    std::chrono::milliseconds remaining_budget;
    std::vector<std::pair<std::string, std::string>> active_tool_outputs;
};

struct ContextStats {
    size_t total_items;
    size_t total_tokens;
    size_t conversation_items;
    size_t file_items;
    size_t symbol_items;
    size_t dependency_items;
    double average_relevance_score;
    size_t cache_hits;
    size_t cache_misses;
    size_t evictions;
};

struct FileRanking {
    std::string file_path;
    double score;
    std::vector<std::string> reasons;  // Why this file is relevant
    size_t edit_distance;  // How recently edited
    size_t reference_count;  // How many other files reference it
    bool is_dependency;
    bool is_test;
};

struct SymbolContext {
    std::string symbol_name;
    std::string symbol_type;  // class, function, variable, etc.
    std::string file_path;
    std::string definition;
    std::vector<std::string> usages;
    std::vector<std::string> callers;
    std::vector<std::string> callees;
    std::string documentation;
};

/**
 * @brief Context Engine - Builds optimal context for LLM prompts
 * 
 * The context engine is responsible for:
 * - Managing conversation state
 * - Ranking files by relevance
 * - Building dependency graphs
 * - Tracking recent changes
 * - Finding relevant symbols
 * - Constructing optimized prompts
 */
class ContextEngine {
public:
    ContextEngine();
    ~ContextEngine();
    
    // Initialize context engine
    bool initialize();
    
    // Shutdown context engine
    void shutdown();
    
    // Stop operations gracefully
    void stop();
    
    // ===== Conversation Management =====
    
    // Start a new conversation
    std::string startConversation(const std::string& initial_goal = "");
    
    // End a conversation
    bool endConversation(const std::string& conversation_id);
    
    // Get current conversation state
    std::optional<ConversationState> getConversationState(const std::string& conversation_id) const;
    
    // Update conversation state
    bool updateConversationState(const std::string& conversation_id, const ConversationState& state);
    
    // Add message to conversation
    bool addMessage(const std::string& conversation_id, 
                    const std::string& role,
                    const std::string& content);
    
    // Get conversation history
    std::vector<std::pair<std::string, std::string>> getConversationHistory(
        const std::string& conversation_id, size_t max_messages = 100) const;
    
    // ===== Context Item Management =====
    
    // Add a context item
    std::string addContextItem(const std::string& conversation_id,
                               ContextType type,
                               const std::string& content,
                               const std::string& source = "",
                               const std::vector<std::string>& tags = {});
    
    // Get a context item
    std::optional<ContextItem> getContextItem(const std::string& item_id) const;
    
    // Remove a context item
    bool removeContextItem(const std::string& item_id);
    
    // Update context item relevance
    bool updateRelevance(const std::string& item_id, double score);
    
    // Pin/unpin context item
    bool pinContextItem(const std::string& item_id, bool pinned);
    
    // ===== File Ranking =====
    
    // Rank files by relevance to a query
    std::vector<FileRanking> rankFiles(const std::string& query,
                                       const std::string& conversation_id = "",
                                       size_t max_results = 50);
    
    // Get files related to a symbol
    std::vector<FileRanking> getFilesRelatedToSymbol(const std::string& symbol_name,
                                                      size_t max_results = 20);
    
    // Get recently modified files
    std::vector<FileRanking> getRecentlyModifiedFiles(size_t count = 20);
    
    // Get files in dependency chain
    std::vector<FileRanking> getDependencyChain(const std::string& file_path,
                                                 size_t depth = 3);
    
    // ===== Symbol Context =====
    
    // Find symbol context
    std::optional<SymbolContext> findSymbol(const std::string& symbol_name,
                                            const std::string& file_path = "") const;
    
    // Get all symbols in a file
    std::vector<SymbolContext> getSymbolsInFile(const std::string& file_path) const;
    
    // Get symbol callers
    std::vector<SymbolContext> getSymbolCallers(const std::string& symbol_name) const;
    
    // Get symbol callees
    std::vector<SymbolContext> getSymbolCallees(const std::string& symbol_name) const;
    
    // ===== Context Building =====
    
    // Build context for a specific task
    std::string buildContext(const std::string& conversation_id,
                             const std::string& task,
                             size_t max_tokens = 8000);
    
    // Build prompt from context
    std::string buildPrompt(const std::string& conversation_id,
                            const std::string& system_prompt,
                            const std::string& task,
                            size_t max_tokens = 8000);
    
    // Get relevant context items
    std::vector<ContextItem> getRelevantContext(const std::string& conversation_id,
                                                 const std::string& query,
                                                 size_t max_items = 20);
    
    // ===== Token Management =====
    
    // Count tokens in content
    size_t countTokens(const std::string& content) const;
    
    // Get total tokens in conversation context
    size_t getTotalTokens(const std::string& conversation_id) const;
    
    // Truncate context to fit token limit
    std::string truncateToTokenLimit(const std::string& content, size_t max_tokens);
    
    // ===== Cache Management =====
    
    // Clear context cache
    void clearCache();
    
    // Get context statistics
    ContextStats getStats() const;
    
    // Set token budget
    void setTokenBudget(size_t tokens);
    
    // Get current token usage
    size_t getTokenUsage() const;
    
    // ===== Event Subscription =====
    
    // Subscribe to context changes
    using ContextChangeCallback = std::function<void(const std::string& conversation_id, 
                                                      const std::string& change_type)>;
    void subscribeToChanges(ContextChangeCallback callback);
    
    // Notify context change
    void notifyChange(const std::string& conversation_id, const std::string& change_type);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    
    // Internal methods
    std::string generateId() const;
    double calculateFileRelevance(const std::string& file_path, 
                                   const std::string& query,
                                   const std::string& conversation_id);
    void updateAccessTime(ContextItem& item);
    void evictIfNeeded(const std::string& conversation_id);
    std::vector<ContextItem> selectContextItems(const std::string& conversation_id,
                                                 size_t max_tokens);
};

} // namespace aios
