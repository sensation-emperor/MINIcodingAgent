// AIOS - MINI Coding Agent Operating System
// Context Engine Implementation

#include "ContextEngine.h"
#include "memory/memory.h"
#include "vector/vector.h"
#include "repository/repository.h"
#include "logging/Logger.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <numeric>

namespace aios {

struct ContextEngine::Impl {
    // Conversation states
    std::unordered_map<std::string, ConversationState> conversations;
    
    // Context items by conversation
    std::unordered_map<std::string, std::unordered_map<std::string, ContextItem>> context_items;
    
    // Context items by ID for quick lookup
    std::unordered_map<std::string, ContextItem> all_context_items;
    
    // File rankings cache
    std::unordered_map<std::string, std::vector<FileRanking>> file_rankings_cache;
    
    // Symbol cache
    std::unordered_map<std::string, SymbolContext> symbol_cache;
    
    // Change subscribers
    std::vector<ContextEngine::ContextChangeCallback> change_subscribers;
    
    // Statistics
    ContextStats stats{};
    
    // Token budget
    size_t token_budget{8000};
    
    // Current token usage
    std::unordered_map<std::string, size_t> conversation_tokens;
    
    // ID counter
    std::atomic<size_t> id_counter{0};
    
    // Dependencies (simplified)
    std::unordered_map<std::string, std::set<std::string>> file_dependencies;
    std::unordered_map<std::string, std::set<std::string>> file_dependents;
    
    // Recently modified files
    std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> recent_files;
    
    // Logger
    Logger logger{"ContextEngine"};
};

ContextEngine::ContextEngine() : impl_(std::make_unique<Impl>()) {}

ContextEngine::~ContextEngine() {
    shutdown();
}

bool ContextEngine::initialize() {
    if (running_) {
        return true;
    }
    
    impl_->logger.info("Initializing Context Engine");
    
    // Initialize statistics
    impl_->stats = ContextStats{};
    
    // Load recent files from repository
    // In production, this would scan the filesystem
    
    running_ = true;
    impl_->logger.info("Context Engine initialized successfully");
    
    return true;
}

void ContextEngine::shutdown() {
    stopping_ = true;
    
    impl_->logger.info("Shutting down Context Engine");
    
    // Persist important context
    // In production, save to database
    
    running_ = false;
    impl_->logger.info("Context Engine shut down complete");
}

void ContextEngine::stop() {
    stopping_ = true;
    impl_->logger.debug("Context Engine stop requested");
}

std::string ContextEngine::startConversation(const std::string& initial_goal) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string conv_id = generateId();
    
    ConversationState state;
    state.conversation_id = conv_id;
    state.current_goal = initial_goal;
    state.created_at = std::chrono::system_clock::now();
    state.confidence_estimate = 1.0;
    
    impl_->conversations[conv_id] = state;
    impl_->context_items[conv_id] = {};
    impl_->conversation_tokens[conv_id] = 0;
    
    impl_->logger.info("Started conversation: {}", conv_id);
    
    notifyChange(conv_id, "started");
    
    return conv_id;
}

bool ContextEngine::endConversation(const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->conversations.find(conversation_id);
    if (it == impl_->conversations.end()) {
        impl_->logger.warn("Conversation not found: {}", conversation_id);
        return false;
    }
    
    // Remove conversation and its context items
    for (const auto& [item_id, item] : impl_->context_items[conversation_id]) {
        impl_->all_context_items.erase(item_id);
    }
    
    impl_->context_items.erase(conversation_id);
    impl_->conversations.erase(conversation_id);
    impl_->conversation_tokens.erase(conversation_id);
    
    impl_->logger.info("Ended conversation: {}", conversation_id);
    
    notifyChange(conversation_id, "ended");
    
    return true;
}

std::optional<ConversationState> ContextEngine::getConversationState(const std::string& conversation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->conversations.find(conversation_id);
    if (it == impl_->conversations.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

bool ContextEngine::updateConversationState(const std::string& conversation_id, 
                                             const ConversationState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->conversations.find(conversation_id);
    if (it == impl_->conversations.end()) {
        impl_->logger.warn("Conversation not found: {}", conversation_id);
        return false;
    }
    
    it->second = state;
    it->second.accessed_at = std::chrono::system_clock::now();
    
    notifyChange(conversation_id, "state_updated");
    
    return true;
}

bool ContextEngine::addMessage(const std::string& conversation_id,
                                const std::string& role,
                                const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto conv_it = impl_->conversations.find(conversation_id);
    if (conv_it == impl_->conversations.end()) {
        impl_->logger.warn("Conversation not found: {}", conversation_id);
        return false;
    }
    
    // Add as context item
    ContextItem item;
    item.id = generateId();
    item.type = ContextType::Conversation;
    item.content = content;
    item.source = conversation_id;
    item.metadata["role"] = role;
    item.created_at = std::chrono::system_clock::now();
    item.accessed_at = item.created_at;
    item.access_count = 1;
    item.token_count = countTokens(content);
    item.pinned = false;
    item.relevance_score = 1.0;
    
    impl_->context_items[conversation_id][item.id] = item;
    impl_->all_context_items[item.id] = item;
    impl_->conversation_tokens[conversation_id] += item.token_count;
    
    // Update stats
    impl_->stats.total_items++;
    impl_->stats.conversation_items++;
    impl_->stats.total_tokens += item.token_count;
    
    notifyChange(conversation_id, "message_added");
    
    return true;
}

std::vector<std::pair<std::string, std::string>> ContextEngine::getConversationHistory(
    const std::string& conversation_id, size_t max_messages) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::pair<std::string, std::string>> history;
    
    auto conv_it = impl_->conversations.find(conversation_id);
    if (conv_it == impl_->conversations.end()) {
        return history;
    }
    
    auto ctx_it = impl_->context_items.find(conversation_id);
    if (ctx_it == impl_->context_items.end()) {
        return history;
    }
    
    // Collect conversation items
    std::vector<ContextItem> messages;
    for (const auto& [id, item] : ctx_it->second) {
        if (item.type == ContextType::Conversation) {
            messages.push_back(item);
        }
    }
    
    // Sort by creation time
    std::sort(messages.begin(), messages.end(), 
              [](const ContextItem& a, const ContextItem& b) {
                  return a.created_at < b.created_at;
              });
    
    // Take last N messages
    if (messages.size() > max_messages) {
        messages = std::vector<ContextItem>(messages.end() - max_messages, messages.end());
    }
    
    for (const auto& msg : messages) {
        auto role_it = msg.metadata.find("role");
        std::string role = (role_it != msg.metadata.end()) ? role_it->second : "unknown";
        history.emplace_back(role, msg.content);
    }
    
    return history;
}

std::string ContextEngine::addContextItem(const std::string& conversation_id,
                                           ContextType type,
                                           const std::string& content,
                                           const std::string& source,
                                           const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ContextItem item;
    item.id = generateId();
    item.type = type;
    item.content = content;
    item.source = source;
    item.tags = tags;
    item.created_at = std::chrono::system_clock::now();
    item.accessed_at = item.created_at;
    item.access_count = 1;
    item.token_count = countTokens(content);
    item.pinned = false;
    item.relevance_score = 0.5;  // Default relevance
    
    // Calculate initial relevance based on type
    switch (type) {
        case ContextType::File:
            item.relevance_score = calculateFileRelevance(source, "", conversation_id);
            break;
        case ContextType::Symbol:
            item.relevance_score = 0.8;
            break;
        case ContextType::Dependency:
            item.relevance_score = 0.7;
            break;
        default:
            item.relevance_score = 0.5;
    }
    
    impl_->context_items[conversation_id][item.id] = item;
    impl_->all_context_items[item.id] = item;
    impl_->conversation_tokens[conversation_id] += item.token_count;
    
    // Update stats
    impl_->stats.total_items++;
    impl_->stats.total_tokens += item.token_count;
    
    switch (type) {
        case ContextType::File:
            impl_->stats.file_items++;
            break;
        case ContextType::Symbol:
            impl_->stats.symbol_items++;
            break;
        case ContextType::Dependency:
            impl_->stats.dependency_items++;
            break;
        default:
            break;
    }
    
    notifyChange(conversation_id, "context_added");
    
    return item.id;
}

std::optional<ContextItem> ContextEngine::getContextItem(const std::string& item_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->all_context_items.find(item_id);
    if (it == impl_->all_context_items.end()) {
        impl_->stats.cache_misses++;
        return std::nullopt;
    }
    
    impl_->stats.cache_hits++;
    return it->second;
}

bool ContextEngine::removeContextItem(const std::string& item_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->all_context_items.find(item_id);
    if (it == impl_->all_context_items.end()) {
        return false;
    }
    
    const auto& item = it->second;
    
    // Find conversation and remove
    for (auto& [conv_id, items] : impl_->context_items) {
        auto item_it = items.find(item_id);
        if (item_it != items.end()) {
            impl_->conversation_tokens[conv_id] -= item.token_count;
            impl_->stats.total_tokens -= item.token_count;
            items.erase(item_it);
            break;
        }
    }
    
    impl_->all_context_items.erase(it);
    impl_->stats.total_items--;
    
    return true;
}

bool ContextEngine::updateRelevance(const std::string& item_id, double score) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->all_context_items.find(item_id);
    if (it == impl_->all_context_items.end()) {
        return false;
    }
    
    it->second.relevance_score = std::max(0.0, std::min(1.0, score));
    return true;
}

bool ContextEngine::pinContextItem(const std::string& item_id, bool pinned) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->all_context_items.find(item_id);
    if (it == impl_->all_context_items.end()) {
        return false;
    }
    
    it->second.pinned = pinned;
    return true;
}

std::vector<FileRanking> ContextEngine::rankFiles(const std::string& query,
                                                   const std::string& conversation_id,
                                                   size_t max_results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<FileRanking> rankings;
    
    // In production, this would use:
    // - BM25 text search
    // - Embedding similarity
    // - AST analysis
    // - Dependency graph traversal
    
    // Simplified implementation
    auto conv_it = impl_->conversations.find(conversation_id);
    
    // Get files from context
    if (conv_it != impl_->conversations.end()) {
        auto ctx_it = impl_->context_items.find(conversation_id);
        if (ctx_it != impl_->context_items.end()) {
            for (const auto& [id, item] : ctx_it->second) {
                if (item.type == ContextType::File) {
                    FileRanking ranking;
                    ranking.file_path = item.source;
                    ranking.score = item.relevance_score;
                    ranking.is_dependency = false;
                    ranking.is_test = item.source.find("test") != std::string::npos ||
                                     item.source.find("Test") != std::string::npos;
                    ranking.reference_count = 1;
                    ranking.edit_distance = 0;
                    
                    // Check if query matches filename
                    if (item.source.find(query) != std::string::npos) {
                        ranking.score += 0.3;
                        ranking.reasons.push_back("filename_match");
                    }
                    
                    rankings.push_back(ranking);
                }
            }
        }
    }
    
    // Sort by score descending
    std::sort(rankings.begin(), rankings.end(),
              [](const FileRanking& a, const FileRanking& b) {
                  return a.score > b.score;
              });
    
    // Limit results
    if (rankings.size() > max_results) {
        rankings.resize(max_results);
    }
    
    return rankings;
}

std::vector<FileRanking> ContextEngine::getFilesRelatedToSymbol(const std::string& symbol_name,
                                                                 size_t max_results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<FileRanking> rankings;
    
    // Find symbol in cache
    auto sym_it = impl_->symbol_cache.find(symbol_name);
    if (sym_it != impl_->symbol_cache.end()) {
        FileRanking ranking;
        ranking.file_path = sym_it->second.file_path;
        ranking.score = 0.9;
        ranking.reasons.push_back("contains_symbol");
        ranking.is_dependency = false;
        ranking.is_test = false;
        ranking.reference_count = sym_it->second.usages.size();
        rankings.push_back(ranking);
        
        // Add files where symbol is used
        for (const auto& usage : sym_it->second.usages) {
            FileRanking usage_ranking;
            usage_ranking.file_path = usage;
            usage_ranking.score = 0.7;
            usage_ranking.reasons.push_back("uses_symbol");
            usage_ranking.is_dependency = false;
            usage_ranking.is_test = usage.find("test") != std::string::npos;
            usage_ranking.reference_count = 1;
            rankings.push_back(usage_ranking);
        }
    }
    
    // Sort and limit
    std::sort(rankings.begin(), rankings.end(),
              [](const FileRanking& a, const FileRanking& b) {
                  return a.score > b.score;
              });
    
    if (rankings.size() > max_results) {
        rankings.resize(max_results);
    }
    
    return rankings;
}

std::vector<FileRanking> ContextEngine::getRecentlyModifiedFiles(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<FileRanking> rankings;
    
    // Sort recent files by time
    auto sorted_files = impl_->recent_files;
    std::sort(sorted_files.begin(), sorted_files.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });
    
    for (size_t i = 0; i < std::min(count, sorted_files.size()); ++i) {
        FileRanking ranking;
        ranking.file_path = sorted_files[i].second;
        ranking.score = 1.0 - (static_cast<double>(i) / count);
        ranking.reasons.push_back("recently_modified");
        ranking.edit_distance = i;
        ranking.is_dependency = false;
        ranking.is_test = ranking.file_path.find("test") != std::string::npos;
        ranking.reference_count = 0;
        rankings.push_back(ranking);
    }
    
    return rankings;
}

std::vector<FileRanking> ContextEngine::getDependencyChain(const std::string& file_path,
                                                            size_t depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<FileRanking> rankings;
    std::set<std::string> visited;
    std::vector<std::pair<std::string, size_t>> to_visit;
    
    to_visit.emplace_back(file_path, 0);
    
    while (!to_visit.empty()) {
        auto [current, current_depth] = to_visit.back();
        to_visit.pop_back();
        
        if (visited.count(current) || current_depth > depth) {
            continue;
        }
        
        visited.insert(current);
        
        FileRanking ranking;
        ranking.file_path = current;
        ranking.score = 1.0 - (static_cast<double>(current_depth) / (depth + 1));
        ranking.reasons.push_back("dependency_chain");
        ranking.edit_distance = current_depth;
        ranking.is_dependency = current != file_path;
        ranking.is_test = current.find("test") != std::string::npos;
        ranking.reference_count = impl_->file_dependents[current].size();
        
        rankings.push_back(ranking);
        
        // Add dependencies to visit
        if (current_depth < depth) {
            for (const auto& dep : impl_->file_dependencies[current]) {
                to_visit.emplace_back(dep, current_depth + 1);
            }
        }
    }
    
    return rankings;
}

std::optional<SymbolContext> ContextEngine::findSymbol(const std::string& symbol_name,
                                                        const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check cache first
    auto it = impl_->symbol_cache.find(symbol_name);
    if (it != impl_->symbol_cache.end()) {
        if (file_path.empty() || it->second.file_path == file_path) {
            impl_->stats.cache_hits++;
            return it->second;
        }
    }
    
    impl_->stats.cache_misses++;
    return std::nullopt;
}

std::vector<SymbolContext> ContextEngine::getSymbolsInFile(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SymbolContext> symbols;
    
    for (const auto& [name, symbol] : impl_->symbol_cache) {
        if (symbol.file_path == file_path) {
            symbols.push_back(symbol);
        }
    }
    
    return symbols;
}

std::vector<SymbolContext> ContextEngine::getSymbolCallers(const std::string& symbol_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SymbolContext> callers;
    
    auto it = impl_->symbol_cache.find(symbol_name);
    if (it != impl_->symbol_cache.end()) {
        for (const auto& caller_name : it->second.callers) {
            auto caller_it = impl_->symbol_cache.find(caller_name);
            if (caller_it != impl_->symbol_cache.end()) {
                callers.push_back(caller_it->second);
            }
        }
    }
    
    return callers;
}

std::vector<SymbolContext> ContextEngine::getSymbolCallees(const std::string& symbol_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SymbolContext> callees;
    
    auto it = impl_->symbol_cache.find(symbol_name);
    if (it != impl_->symbol_cache.end()) {
        for (const auto& callee_name : it->second.callees) {
            auto callee_it = impl_->symbol_cache.find(callee_name);
            if (callee_it != impl_->symbol_cache.end()) {
                callees.push_back(callee_it->second);
            }
        }
    }
    
    return callees;
}

std::string ContextEngine::buildContext(const std::string& conversation_id,
                                         const std::string& task,
                                         size_t max_tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream context;
    
    // Get relevant context items
    auto items = selectContextItems(conversation_id, max_tokens);
    
    context << "=== CONTEXT ===\n\n";
    
    // Add conversation state
    auto conv_it = impl_->conversations.find(conversation_id);
    if (conv_it != impl_->conversations.end()) {
        const auto& state = conv_it->second;
        
        if (!state.current_goal.empty()) {
            context << "Current Goal: " << state.current_goal << "\n\n";
        }
        
        if (!state.current_objective.empty()) {
            context << "Current Objective: " << state.current_objective << "\n\n";
        }
        
        if (!state.todo_list.empty()) {
            context << "TODO List:\n";
            for (const auto& todo : state.todo_list) {
                context << "- " << todo << "\n";
            }
            context << "\n";
        }
        
        if (!state.important_files.empty()) {
            context << "Important Files:\n";
            for (const auto& file : state.important_files) {
                context << "- " << file << "\n";
            }
            context << "\n";
        }
    }
    
    // Add context items sorted by relevance
    context << "Relevant Information:\n\n";
    for (const auto& item : items) {
        context << "[" << static_cast<int>(item.type) << "] ";
        if (!item.source.empty()) {
            context << item.source << ": ";
        }
        context << item.content << "\n\n";
    }
    
    context << "=== END CONTEXT ===\n";
    
    return context.str();
}

std::string ContextEngine::buildPrompt(const std::string& conversation_id,
                                        const std::string& system_prompt,
                                        const std::string& task,
                                        size_t max_tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream prompt;
    
    // Add system prompt
    prompt << system_prompt << "\n\n";
    
    // Add context
    prompt << buildContext(conversation_id, task, max_tokens / 2) << "\n\n";
    
    // Add task
    prompt << "=== TASK ===\n" << task << "\n=== END TASK ===\n";
    
    return prompt.str();
}

std::vector<ContextItem> ContextEngine::getRelevantContext(const std::string& conversation_id,
                                                            const std::string& query,
                                                            size_t max_items) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ContextItem> relevant;
    
    auto ctx_it = impl_->context_items.find(conversation_id);
    if (ctx_it == impl_->context_items.end()) {
        return relevant;
    }
    
    // Score items by relevance to query
    for (const auto& [id, item] : ctx_it->second) {
        double score = item.relevance_score;
        
        // Boost score if query matches content
        if (item.content.find(query) != std::string::npos) {
            score += 0.3;
        }
        
        // Boost score if query matches source
        if (item.source.find(query) != std::string::npos) {
            score += 0.2;
        }
        
        // Boost score if query matches tags
        for (const auto& tag : item.tags) {
            if (tag.find(query) != std::string::npos) {
                score += 0.1;
            }
        }
        
        ContextItem scored_item = item;
        scored_item.relevance_score = std::min(1.0, score);
        relevant.push_back(scored_item);
    }
    
    // Sort by relevance
    std::sort(relevant.begin(), relevant.end(),
              [](const ContextItem& a, const ContextItem& b) {
                  return a.relevance_score > b.relevance_score;
              });
    
    // Limit results
    if (relevant.size() > max_items) {
        relevant.resize(max_items);
    }
    
    return relevant;
}

size_t ContextEngine::countTokens(const std::string& content) const {
    // Simple token counting: approximately 4 characters per token
    // In production, use actual tokenizer from the model provider
    if (content.empty()) {
        return 0;
    }
    return content.size() / 4 + 1;
}

size_t ContextEngine::getTotalTokens(const std::string& conversation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = impl_->conversation_tokens.find(conversation_id);
    if (it == impl_->conversation_tokens.end()) {
        return 0;
    }
    
    return it->second;
}

std::string ContextEngine::truncateToTokenLimit(const std::string& content, size_t max_tokens) {
    size_t max_chars = max_tokens * 4;
    
    if (content.size() <= max_chars) {
        return content;
    }
    
    return content.substr(0, max_chars);
}

void ContextEngine::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    impl_->symbol_cache.clear();
    impl_->file_rankings_cache.clear();
    impl_->stats.cache_hits = 0;
    impl_->stats.cache_misses = 0;
    
    impl_->logger.debug("Context cache cleared");
}

ContextStats ContextEngine::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->stats;
}

void ContextEngine::setTokenBudget(size_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->token_budget = tokens;
    impl_->logger.debug("Token budget set to: {}", tokens);
}

size_t ContextEngine::getTokenUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t total = 0;
    for (const auto& [conv_id, tokens] : impl_->conversation_tokens) {
        total += tokens;
    }
    return total;
}

void ContextEngine::subscribeToChanges(ContextChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->change_subscribers.push_back(callback);
}

void ContextEngine::notifyChange(const std::string& conversation_id, const std::string& change_type) {
    // Don't hold lock while calling callbacks
    std::vector<ContextChangeCallback> subscribers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers = impl_->change_subscribers;
    }
    
    for (const auto& callback : subscribers) {
        try {
            callback(conversation_id, change_type);
        } catch (const std::exception& e) {
            impl_->logger.error("Context change callback failed: {}", e.what());
        }
    }
}

std::string ContextEngine::generateId() const {
    size_t id = ++impl_->id_counter;
    return "ctx_" + std::to_string(id) + "_" + 
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count());
}

double ContextEngine::calculateFileRelevance(const std::string& file_path,
                                              const std::string& query,
                                              const std::string& conversation_id) {
    double score = 0.5;  // Base score
    
    // Check if file is in important files list
    auto conv_it = impl_->conversations.find(conversation_id);
    if (conv_it != impl_->conversations.end()) {
        for (const auto& important : conv_it->second.important_files) {
            if (important == file_path) {
                score += 0.3;
            }
        }
    }
    
    // Check if file was recently edited
    for (const auto& [time, path] : impl_->recent_files) {
        if (path == file_path) {
            score += 0.2;
            break;
        }
    }
    
    // Check if file is a dependency of important files
    auto dep_it = impl_->file_dependents.find(file_path);
    if (dep_it != impl_->file_dependents.end() && !dep_it->second.empty()) {
        score += 0.1;
    }
    
    return std::min(1.0, score);
}

void ContextEngine::updateAccessTime(ContextItem& item) {
    item.accessed_at = std::chrono::system_clock::now();
    item.access_count++;
}

void ContextEngine::evictIfNeeded(const std::string& conversation_id) {
    auto tokens_it = impl_->conversation_tokens.find(conversation_id);
    if (tokens_it == impl_->conversation_tokens.end()) {
        return;
    }
    
    size_t current_tokens = tokens_it->second;
    if (current_tokens <= impl_->token_budget) {
        return;
    }
    
    auto ctx_it = impl_->context_items.find(conversation_id);
    if (ctx_it == impl_->context_items.end()) {
        return;
    }
    
    // Find non-pinned items sorted by relevance (lowest first)
    std::vector<std::pair<std::string, double>> candidates;
    for (const auto& [id, item] : ctx_it->second) {
        if (!item.pinned) {
            candidates.emplace_back(id, item.relevance_score);
        }
    }
    
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });
    
    // Evict until under budget
    for (const auto& [id, score] : candidates) {
        if (tokens_it->second <= impl_->token_budget * 0.9) {  // Evict to 90% of budget
            break;
        }
        
        auto item_it = ctx_it->second.find(id);
        if (item_it != ctx_it->second.end()) {
            tokens_it->second -= item_it->second.token_count;
            impl_->stats.total_tokens -= item_it->second.token_count;
            impl_->all_context_items.erase(id);
            ctx_it->second.erase(item_it);
            impl_->stats.evictions++;
            
            impl_->logger.debug("Evicted context item: {} (score: {})", id, score);
        }
    }
}

std::vector<ContextItem> ContextEngine::selectContextItems(const std::string& conversation_id,
                                                            size_t max_tokens) {
    std::vector<ContextItem> selected;
    
    auto ctx_it = impl_->context_items.find(conversation_id);
    if (ctx_it == impl_->context_items.end()) {
        return selected;
    }
    
    // Get all items
    std::vector<ContextItem> items;
    for (const auto& [id, item] : ctx_it->second) {
        items.push_back(item);
    }
    
    // Sort by relevance (pinned items first, then by score)
    std::sort(items.begin(), items.end(),
              [](const ContextItem& a, const ContextItem& b) {
                  if (a.pinned != b.pinned) {
                      return a.pinned;
                  }
                  return a.relevance_score > b.relevance_score;
              });
    
    // Select items until token limit
    size_t current_tokens = 0;
    for (const auto& item : items) {
        if (current_tokens + item.token_count <= max_tokens) {
            selected.push_back(item);
            current_tokens += item.token_count;
        } else if (item.pinned) {
            // Always include pinned items even over limit
            selected.push_back(item);
        }
    }
    
    return selected;
}

} // namespace aios
