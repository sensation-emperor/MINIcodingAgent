#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <chrono>
#include "parser/ASTParser.h"

namespace aios {

struct SearchResult {
    std::string file_path;
    int line_number = 1;
    std::string matched_symbol;
    std::string snippet;
    double score = 0.0;
    std::string match_type; // "exact_symbol", "fuzzy_symbol", "bm25_lexical", "hybrid_rrf"
};

struct RepositoryStats {
    size_t total_files_indexed = 0;
    size_t total_symbols = 0;
    size_t total_lines_of_code = 0;
    size_t modified_files_reindexed = 0;
    std::chrono::system_clock::time_point last_index_time;
};

class RepositoryIndex {
public:
    static RepositoryIndex& instance();

    RepositoryIndex();
    ~RepositoryIndex() = default;

    // Indexing
    bool indexFile(const std::string& file_path, const std::string& content);
    size_t indexDirectory(const std::string& root_dir);
    void removeFile(const std::string& file_path);
    void clear();

    // Symbol & Reference Querying
    std::vector<CodeSymbol> findSymbol(const std::string& name, bool exact = false) const;
    std::vector<CodeSymbol> findSymbolsByFile(const std::string& file_path) const;
    std::vector<CodeSymbol> findCallers(const std::string& func_name) const;
    std::vector<std::string> findReferences(const std::string& symbol_name) const;
    std::vector<std::string> getFileImports(const std::string& file_path) const;

    // Hybrid BM25 + Reciprocal Rank Fusion (RRF) Search
    std::vector<SearchResult> searchCode(const std::string& query, size_t top_k = 10) const;

    // Statistics
    RepositoryStats getStats() const;

private:
    void updateInvertedIndex(const std::string& file_path, const std::string& content);
    std::vector<std::string> tokenize(const std::string& text) const;
    double calculateBM25(const std::string& term, const std::string& file_path) const;

    mutable std::mutex mutex_;
    std::unique_ptr<ASTParser> parser_;

    // File registry: file_path -> ParsedFile
    std::unordered_map<std::string, ParsedFile> files_;
    
    // File content cache: file_path -> content
    std::unordered_map<std::string, std::string> file_contents_;

    // Symbol index: lowercase symbol_name -> list of symbol pointers/copies
    std::unordered_map<std::string, std::vector<CodeSymbol>> symbol_table_;

    // Inverted Index for BM25: term -> (file_path -> term_frequency)
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> inverted_index_;
    std::unordered_map<std::string, size_t> file_doc_lengths_;
    double avg_doc_length_ = 0.0;

    RepositoryStats stats_{};
};

} // namespace aios
