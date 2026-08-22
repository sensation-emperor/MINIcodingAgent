#include "repository/RepositoryIndex.h"
#include "logging/Logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace aios {

namespace fs = std::filesystem;

RepositoryIndex& RepositoryIndex::instance() {
    static RepositoryIndex instance;
    return instance;
}

RepositoryIndex::RepositoryIndex() 
    : parser_(std::make_unique<ASTParser>()) {}

std::vector<std::string> RepositoryIndex::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (!current.empty()) {
            if (current.size() >= 2) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }
    if (!current.empty() && current.size() >= 2) {
        tokens.push_back(current);
    }
    return tokens;
}

bool RepositoryIndex::indexFile(const std::string& file_path, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string checksum = ASTParser::computeChecksum(content);

    // Incremental check: if checksum matches, file has not changed
    auto existing_it = files_.find(file_path);
    if (existing_it != files_.end() && existing_it->second.checksum == checksum) {
        return false; // Skipped, already up to date
    }

    bool is_reindex = (existing_it != files_.end());
    if (is_reindex) {
        // Remove old symbols and inverted index entries
        for (const auto& sym : existing_it->second.symbols) {
            std::string lower_name = sym.name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            auto& sym_list = symbol_table_[lower_name];
            sym_list.erase(std::remove_if(sym_list.begin(), sym_list.end(), 
                [&](const CodeSymbol& s) { return s.file_path == file_path; }), sym_list.end());
        }
        stats_.modified_files_reindexed++;
    }

    // Parse AST
    ParsedFile pf = parser_->parseFile(file_path, content);
    pf.checksum = checksum;

    // Register symbols
    for (const auto& sym : pf.symbols) {
        std::string lower_name = sym.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        symbol_table_[lower_name].push_back(sym);
    }

    // Update BM25 Inverted Index
    updateInvertedIndex(file_path, content);

    files_[file_path] = pf;
    file_contents_[file_path] = content;

    // Update Stats
    stats_.total_files_indexed = files_.size();
    stats_.total_symbols = 0;
    stats_.total_lines_of_code = 0;
    for (const auto& [_, f] : files_) {
        stats_.total_symbols += f.symbols.size();
        stats_.total_lines_of_code += f.line_count;
    }
    stats_.last_index_time = std::chrono::system_clock::now();

    return true;
}

void RepositoryIndex::updateInvertedIndex(const std::string& file_path, const std::string& content) {
    auto tokens = tokenize(content);
    file_doc_lengths_[file_path] = tokens.size();

    // Calculate term frequencies
    std::unordered_map<std::string, size_t> tf;
    for (const auto& t : tokens) {
        tf[t]++;
    }

    for (const auto& [term, count] : tf) {
        inverted_index_[term][file_path] = count;
    }

    // Recalculate average document length
    size_t total_len = 0;
    for (const auto& [_, len] : file_doc_lengths_) {
        total_len += len;
    }
    avg_doc_length_ = files_.empty() ? 0.0 : (static_cast<double>(total_len) / files_.size());
}

size_t RepositoryIndex::indexDirectory(const std::string& root_dir) {
    size_t indexed_count = 0;
    std::unordered_set<std::string> seen_files;

    try {
        if (!fs::exists(root_dir) || !fs::is_directory(root_dir)) {
            return 0;
        }

        for (const auto& entry : fs::recursive_directory_iterator(root_dir, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            std::string path_str = entry.path().lexically_normal().string();
            
            // Ignore hidden dirs, build dirs, node_modules
            if (path_str.find("/.git") != std::string::npos || path_str.find("\\.git") != std::string::npos ||
                path_str.find("/build") != std::string::npos || path_str.find("\\build") != std::string::npos ||
                path_str.find("node_modules") != std::string::npos || path_str.find(".vs") != std::string::npos ||
                path_str.find("target") != std::string::npos || path_str.find(".gemini") != std::string::npos) {
                continue;
            }

            // Check if extension is supported
            std::string lang = ASTParser::detectLanguage(path_str);
            if (lang == "unknown") continue;

            std::ifstream file(path_str);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                if (indexFile(path_str, buffer.str())) {
                    indexed_count++;
                }
                seen_files.insert(path_str);
            }
        }

        // Invalidate deleted files
        std::vector<std::string> to_remove;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [fpath, _] : files_) {
                if (fpath.rfind(root_dir, 0) == 0 && seen_files.find(fpath) == seen_files.end()) {
                    to_remove.push_back(fpath);
                }
            }
        }
        for (const auto& f : to_remove) {
            removeFile(f);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Repository indexDirectory error: {}", e.what());
    }

    return indexed_count;
}

void RepositoryIndex::removeFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(file_path);
    if (it == files_.end()) return;

    for (const auto& sym : it->second.symbols) {
        std::string lower_name = sym.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        auto& sym_list = symbol_table_[lower_name];
        sym_list.erase(std::remove_if(sym_list.begin(), sym_list.end(), 
            [&](const CodeSymbol& s) { return s.file_path == file_path; }), sym_list.end());
    }

    files_.erase(it);
    file_contents_.erase(file_path);
    file_doc_lengths_.erase(file_path);
}

void RepositoryIndex::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    files_.clear();
    file_contents_.clear();
    symbol_table_.clear();
    inverted_index_.clear();
    file_doc_lengths_.clear();
    avg_doc_length_ = 0.0;
    stats_ = RepositoryStats{};
}

std::vector<CodeSymbol> RepositoryIndex::findSymbol(const std::string& name, bool exact) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CodeSymbol> results;

    std::string lower_target = name;
    std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(), ::tolower);

    if (exact) {
        auto it = symbol_table_.find(lower_target);
        if (it != symbol_table_.end()) {
            results = it->second;
        }
    } else {
        for (const auto& [sym_name, list] : symbol_table_) {
            if (sym_name.find(lower_target) != std::string::npos) {
                for (const auto& s : list) {
                    results.push_back(s);
                }
            }
        }
    }
    return results;
}

std::vector<CodeSymbol> RepositoryIndex::findSymbolsByFile(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(file_path);
    if (it != files_.end()) {
        return it->second.symbols;
    }
    return {};
}

std::vector<CodeSymbol> RepositoryIndex::findCallers(const std::string& func_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CodeSymbol> callers;

    for (const auto& [_, file] : files_) {
        for (const auto& sym : file.symbols) {
            if (std::find(sym.callees.begin(), sym.callees.end(), func_name) != sym.callees.end()) {
                callers.push_back(sym);
            }
        }
    }
    return callers;
}

std::vector<std::string> RepositoryIndex::findReferences(const std::string& symbol_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> refs;

    for (const auto& [fpath, content] : file_contents_) {
        if (content.find(symbol_name) != std::string::npos) {
            refs.push_back(fpath);
        }
    }
    return refs;
}

std::vector<std::string> RepositoryIndex::getFileImports(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(file_path);
    if (it != files_.end()) {
        return it->second.imports;
    }
    return {};
}

double RepositoryIndex::calculateBM25(const std::string& term, const std::string& file_path) const {
    auto t_it = inverted_index_.find(term);
    if (t_it == inverted_index_.end()) return 0.0;

    auto doc_it = t_it->second.find(file_path);
    if (doc_it == t_it->second.end()) return 0.0;

    size_t tf = doc_it->second;
    size_t df = t_it->second.size();
    size_t N = files_.size();

    // IDF calculation
    double idf = std::log((N - df + 0.5) / (df + 0.5) + 1.0);

    // BM25 parameters
    const double k1 = 1.2;
    const double b = 0.75;

    auto len_it = file_doc_lengths_.find(file_path);
    size_t doc_len = (len_it != file_doc_lengths_.end()) ? len_it->second : 100;
    double len_norm = (avg_doc_length_ > 0) ? (doc_len / avg_doc_length_) : 1.0;

    double tf_comp = (tf * (k1 + 1.0)) / (tf + k1 * (1.0 - b + b * len_norm));
    return idf * tf_comp;
}

std::vector<SearchResult> RepositoryIndex::searchCode(const std::string& query, size_t top_k) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (files_.empty()) return {};

    auto query_tokens = tokenize(query);

    // 1. BM25 Ranking
    std::unordered_map<std::string, double> bm25_scores;
    for (const auto& token : query_tokens) {
        auto t_it = inverted_index_.find(token);
        if (t_it != inverted_index_.end()) {
            for (const auto& [fpath, _] : t_it->second) {
                bm25_scores[fpath] += calculateBM25(token, fpath);
            }
        }
    }

    std::vector<std::pair<std::string, double>> bm25_ranked(bm25_scores.begin(), bm25_scores.end());
    std::sort(bm25_ranked.begin(), bm25_ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    // 2. Symbol Ranking
    std::unordered_map<std::string, double> symbol_scores;
    std::unordered_map<std::string, std::string> top_symbol_match;

    for (const auto& token : query_tokens) {
        for (const auto& [sym_name, list] : symbol_table_) {
            if (sym_name == token) {
                for (const auto& sym : list) {
                    symbol_scores[sym.file_path] += 10.0;
                    top_symbol_match[sym.file_path] = sym.name;
                }
            } else if (sym_name.find(token) != std::string::npos) {
                for (const auto& sym : list) {
                    symbol_scores[sym.file_path] += 3.0;
                    if (top_symbol_match.find(sym.file_path) == top_symbol_match.end()) {
                        top_symbol_match[sym.file_path] = sym.name;
                    }
                }
            }
        }
    }

    std::vector<std::pair<std::string, double>> symbol_ranked(symbol_scores.begin(), symbol_scores.end());
    std::sort(symbol_ranked.begin(), symbol_ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    // 3. Reciprocal Rank Fusion (RRF)
    const double k_rrf = 60.0;
    std::unordered_map<std::string, double> rrf_scores;

    for (size_t r = 0; r < bm25_ranked.size(); ++r) {
        rrf_scores[bm25_ranked[r].first] += 1.0 / (k_rrf + r + 1.0);
    }
    for (size_t r = 0; r < symbol_ranked.size(); ++r) {
        rrf_scores[symbol_ranked[r].first] += 1.5 / (k_rrf + r + 1.0); // Boost symbol match
    }

    std::vector<std::pair<std::string, double>> final_ranked(rrf_scores.begin(), rrf_scores.end());
    std::sort(final_ranked.begin(), final_ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    // Form search results
    std::vector<SearchResult> results;
    size_t count = std::min(top_k, final_ranked.size());

    for (size_t i = 0; i < count; ++i) {
        SearchResult res;
        res.file_path = final_ranked[i].first;
        res.score = final_ranked[i].second;
        res.match_type = "hybrid_rrf";

        auto sym_it = top_symbol_match.find(res.file_path);
        if (sym_it != top_symbol_match.end()) {
            res.matched_symbol = sym_it->second;
        }

        // Get snippet from file content
        auto c_it = file_contents_.find(res.file_path);
        if (c_it != file_contents_.end()) {
            res.snippet = c_it->second.substr(0, std::min<size_t>(200, c_it->second.size()));
        }

        results.push_back(res);
    }

    return results;
}

RepositoryStats RepositoryIndex::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace aios
