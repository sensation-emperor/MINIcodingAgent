#include "DiagnosticsEngine.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace aios::testing {

static std::vector<std::string> splitLines(const std::string& str) {
    std::vector<std::string> lines;
    std::istringstream stream(str);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

TestingResult<DiagnosticsReport> DiagnosticsEngine::scanFile(const std::string& file_path, const std::string& content) {
    ASTParser parser;
    ParsedFile pf = parser.parseFile(file_path, content);

    DiagnosticsReport report;
    report.target_path = file_path;

    if (!pf.success) {
        DiagnosticItem item;
        item.file_path = file_path;
        item.span = SourceSpan{1, 1, 1, 1};
        item.severity = DiagnosticSeverity::Error;
        item.category = DiagnosticCategory::Syntax;
        item.rule_id = "syntax/ast-parse-failure";
        item.message = "Failed to parse AST: " + pf.error;
        report.items.push_back(item);
        report.total_errors++;
        report.passed = false;
        return report;
    }

    report.items = runAstRuleChecks(pf, content);

    for (const auto& item : report.items) {
        if (item.severity == DiagnosticSeverity::Fatal || item.severity == DiagnosticSeverity::Error) {
            report.total_errors++;
        } else if (item.severity == DiagnosticSeverity::Warning) {
            report.total_warnings++;
        } else {
            report.total_hints++;
        }
    }

    report.passed = (report.total_errors == 0);
    return report;
}

TestingResult<DiagnosticsReport> DiagnosticsEngine::scanDirectory(const std::string& dir_path, const std::vector<std::string>& file_patterns) {
    DiagnosticsReport combined;
    combined.target_path = dir_path;

    std::filesystem::path root(dir_path);
    if (!std::filesystem::exists(root)) {
        return std::unexpected("Directory does not exist: " + dir_path);
    }

    auto matchesPattern = [&](const std::string& filename) {
        if (file_patterns.empty()) {
            static const std::vector<std::string> default_exts = {
                ".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hxx",
                ".py", ".js", ".ts", ".jsx", ".tsx", ".rs", ".go", ".java"
            };
            for (const auto& ext : default_exts) {
                if (filename.ends_with(ext)) return true;
            }
            return false;
        }
        for (const auto& pat : file_patterns) {
            if (pat.starts_with("*.") && filename.ends_with(pat.substr(1))) return true;
            if (filename.find(pat) != std::string::npos) return true;
        }
        return false;
    };

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) {
                std::string path_str = entry.path().string();
                if (matchesPattern(entry.path().filename().string())) {
                    std::ifstream file(entry.path(), std::ios::in | std::ios::binary);
                    if (file) {
                        std::ostringstream ss;
                        ss << file.rdbuf();
                        auto res = scanFile(path_str, ss.str());
                        if (res.has_value()) {
                            combined.items.insert(combined.items.end(), res->items.begin(), res->items.end());
                            combined.total_errors += res->total_errors;
                            combined.total_warnings += res->total_warnings;
                            combined.total_hints += res->total_hints;
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Directory scan error: ") + e.what());
    }

    combined.passed = (combined.total_errors == 0);
    return combined;
}

std::vector<DiagnosticItem> DiagnosticsEngine::runAstRuleChecks(const ParsedFile& pf, const std::string& content) const {
    std::vector<DiagnosticItem> items;
    checkSyntaxAndStructure(pf, content, items);
    checkResourceLeaks(pf, content, items);
    checkSecuritySmells(pf, content, items);
    checkConcurrencySmells(pf, content, items);
    checkComplexityAndLength(pf, content, items);
    return items;
}

void DiagnosticsEngine::checkSyntaxAndStructure(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const {
    auto lines = splitLines(content);
    int open_braces = 0;
    int open_parens = 0;
    int open_brackets = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        // Strip string literals and comments for simple bracket check
        bool in_str = false;
        char quote = 0;
        for (size_t c = 0; c < line.size(); ++c) {
            char ch = line[c];
            if (in_str) {
                if (ch == quote && (c == 0 || line[c - 1] != '\\')) in_str = false;
                continue;
            }
            if (ch == '"' || ch == '\'') {
                in_str = true;
                quote = ch;
                continue;
            }
            if (ch == '/' && c + 1 < line.size() && line[c + 1] == '/') break;
            if (ch == '{') open_braces++;
            else if (ch == '}') open_braces--;
            else if (ch == '(') open_parens++;
            else if (ch == ')') open_parens--;
            else if (ch == '[') open_brackets++;
            else if (ch == ']') open_brackets--;
        }
    }

    if (open_braces != 0) {
        DiagnosticItem item;
        item.file_path = pf.file_path;
        item.span = SourceSpan{static_cast<int>(lines.size()), 1, static_cast<int>(lines.size()), 1};
        item.severity = DiagnosticSeverity::Error;
        item.category = DiagnosticCategory::Syntax;
        item.rule_id = "syntax/unmatched-braces";
        item.message = "Mismatched curly braces detected in file";
        out.push_back(item);
    }
    if (open_parens != 0) {
        DiagnosticItem item;
        item.file_path = pf.file_path;
        item.span = SourceSpan{static_cast<int>(lines.size()), 1, static_cast<int>(lines.size()), 1};
        item.severity = DiagnosticSeverity::Error;
        item.category = DiagnosticCategory::Syntax;
        item.rule_id = "syntax/unmatched-parentheses";
        item.message = "Mismatched parentheses detected in file";
        out.push_back(item);
    }
}

static std::string stripComments(const std::string& line) {
    auto pos = line.find("//");
    if (pos != std::string::npos) {
        return line.substr(0, pos);
    }
    return line;
}

void DiagnosticsEngine::checkResourceLeaks(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const {
    auto lines = splitLines(content);

    // 1. C/C++ fopen without fclose
    bool has_fopen = false;
    bool has_fclose = false;
    int fopen_line = 1;

    // 2. Raw malloc/new without free/delete or smart pointers
    bool has_malloc = false;
    bool has_free = false;
    int malloc_line = 1;

    // 3. Raw new without delete / unique_ptr
    bool has_raw_new = false;
    bool has_delete = false;
    int new_line = 1;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        std::string code_line = stripComments(line);

        if (code_line.find("fopen(") != std::string::npos || code_line.find("fopen (") != std::string::npos) {
            has_fopen = true;
            fopen_line = static_cast<int>(i + 1);
        }
        if (code_line.find("fclose(") != std::string::npos || code_line.find("fclose (") != std::string::npos) {
            has_fclose = true;
        }

        if (code_line.find("malloc(") != std::string::npos) {
            has_malloc = true;
            malloc_line = static_cast<int>(i + 1);
        }
        if (code_line.find("free(") != std::string::npos) {
            has_free = true;
        }

        if (code_line.find("new ") != std::string::npos &&
            code_line.find("std::make_unique") == std::string::npos &&
            code_line.find("std::make_shared") == std::string::npos &&
            code_line.find("new (std::nothrow)") == std::string::npos &&
            code_line.find("new (") == std::string::npos) {
            has_raw_new = true;
            new_line = static_cast<int>(i + 1);
        }
        if (code_line.find("delete ") != std::string::npos || code_line.find("delete[]") != std::string::npos) {
            has_delete = true;
        }

        // Python open without with
        if (pf.language == "python") {
            if (code_line.find("open(") != std::string::npos && code_line.find("with open(") == std::string::npos) {
                DiagnosticItem item;
                item.file_path = pf.file_path;
                item.span = SourceSpan{static_cast<int>(i + 1), 1, static_cast<int>(i + 1), static_cast<int>(line.size())};
                item.severity = DiagnosticSeverity::Warning;
                item.category = DiagnosticCategory::ResourceLeak;
                item.rule_id = "resource/unmanaged-file-handle";
                item.message = "File opened without 'with' context manager in Python";
                item.snippet = line;
                SuggestedFix fix;
                fix.description = "Wrap file opening in 'with open(...) as f:' block";
                fix.replacement_span = item.span;
                fix.replacement_text = "with " + line;
                item.fix = fix;
                out.push_back(item);
            }
        }
    }

    if (has_fopen && !has_fclose) {
        DiagnosticItem item;
        item.file_path = pf.file_path;
        item.span = SourceSpan{fopen_line, 1, fopen_line, 1};
        item.severity = DiagnosticSeverity::Error;
        item.category = DiagnosticCategory::ResourceLeak;
        item.rule_id = "resource/unclosed-file-handle";
        item.message = "Resource leak: file opened with fopen() is never closed with fclose()";
        if (fopen_line <= static_cast<int>(lines.size())) {
            item.snippet = lines[fopen_line - 1];
        }
        SuggestedFix fix;
        fix.description = "Ensure fclose(handle) is invoked or use RAII wrapper";
        fix.replacement_span = item.span;
        item.fix = fix;
        out.push_back(item);
    }

    if (has_malloc && !has_free) {
        DiagnosticItem item;
        item.file_path = pf.file_path;
        item.span = SourceSpan{malloc_line, 1, malloc_line, 1};
        item.severity = DiagnosticSeverity::Warning;
        item.category = DiagnosticCategory::ResourceLeak;
        item.rule_id = "resource/unfreed-heap-memory";
        item.message = "Resource leak: memory allocated with malloc() is not freed with free()";
        if (malloc_line <= static_cast<int>(lines.size())) {
            item.snippet = lines[malloc_line - 1];
        }
        out.push_back(item);
    }

    if (has_raw_new && !has_delete && pf.language == "cpp") {
        DiagnosticItem item;
        item.file_path = pf.file_path;
        item.span = SourceSpan{new_line, 1, new_line, 1};
        item.severity = DiagnosticSeverity::Warning;
        item.category = DiagnosticCategory::ResourceLeak;
        item.rule_id = "resource/raw-new-without-delete";
        item.message = "Raw 'new' used without matching 'delete'; consider std::make_unique or std::make_shared";
        if (new_line <= static_cast<int>(lines.size())) {
            item.snippet = lines[new_line - 1];
        }
        out.push_back(item);
    }
}

void DiagnosticsEngine::checkSecuritySmells(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const {
    auto lines = splitLines(content);

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // 1. Insecure system/exec invocation
        if (line.find("system(") != std::string::npos || line.find("popen(") != std::string::npos ||
            (line.find("exec(") != std::string::npos && pf.language == "python") ||
            (line.find("eval(") != std::string::npos && (pf.language == "javascript" || pf.language == "python"))) {
            DiagnosticItem item;
            item.file_path = pf.file_path;
            item.span = SourceSpan{static_cast<int>(i + 1), 1, static_cast<int>(i + 1), static_cast<int>(line.size())};
            item.severity = DiagnosticSeverity::Error;
            item.category = DiagnosticCategory::Security;
            item.rule_id = "security/arbitrary-command-execution";
            item.message = "Insecure execution call (system/popen/exec/eval) detected";
            item.snippet = line;
            out.push_back(item);
        }

        // 2. Insecure string operations in C/C++
        if (pf.language == "cpp" || pf.language == "c") {
            if (line.find("strcpy(") != std::string::npos || line.find("strcat(") != std::string::npos || line.find("gets(") != std::string::npos) {
                DiagnosticItem item;
                item.file_path = pf.file_path;
                item.span = SourceSpan{static_cast<int>(i + 1), 1, static_cast<int>(i + 1), static_cast<int>(line.size())};
                item.severity = DiagnosticSeverity::Error;
                item.category = DiagnosticCategory::Security;
                item.rule_id = "security/buffer-overflow-unsafe-function";
                item.message = "Unbounded string copy/concatenation function (strcpy/strcat/gets) is susceptible to buffer overflows";
                item.snippet = line;
                SuggestedFix fix;
                fix.description = "Replace with safe bounded alternatives (e.g. strncpy_s, std::string)";
                fix.replacement_span = item.span;
                item.fix = fix;
                out.push_back(item);
            }
        }

        // 3. Hardcoded credentials / private keys
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if ((lower.find("password = \"") != std::string::npos || lower.find("api_key = \"") != std::string::npos ||
             lower.find("secret_key = \"") != std::string::npos || lower.find("private_key = \"") != std::string::npos) &&
            line.find("\"\"") == std::string::npos) {
            DiagnosticItem item;
            item.file_path = pf.file_path;
            item.span = SourceSpan{static_cast<int>(i + 1), 1, static_cast<int>(i + 1), static_cast<int>(line.size())};
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::Security;
            item.rule_id = "security/hardcoded-secret";
            item.message = "Potential hardcoded credential or secret detected in source code";
            item.snippet = line;
            out.push_back(item);
        }

        // 4. SQL Injection via string concatenation
        if ((lower.find("select ") != std::string::npos || lower.find("insert into ") != std::string::npos || lower.find("delete from ") != std::string::npos) &&
            (line.find(" + ") != std::string::npos || line.find("%s") != std::string::npos || line.find("f\"") != std::string::npos)) {
            DiagnosticItem item;
            item.file_path = pf.file_path;
            item.span = SourceSpan{static_cast<int>(i + 1), 1, static_cast<int>(i + 1), static_cast<int>(line.size())};
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::Security;
            item.rule_id = "security/sql-injection-risk";
            item.message = "Dynamic SQL query formed via string concatenation; use parameterized queries";
            item.snippet = line;
            out.push_back(item);
        }
    }
}

void DiagnosticsEngine::checkConcurrencySmells(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const {
    auto lines = splitLines(content);
    bool has_threads = false;
    bool has_mutex_or_atomic = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (line.find("std::thread") != std::string::npos || line.find("std::async") != std::string::npos ||
            line.find("threading.Thread") != std::string::npos || line.find("go ") != std::string::npos) {
            has_threads = true;
        }
        if (line.find("std::mutex") != std::string::npos || line.find("std::lock_guard") != std::string::npos ||
            line.find("std::unique_lock") != std::string::npos || line.find("std::atomic") != std::string::npos ||
            line.find("threading.Lock") != std::string::npos || line.find("sync.Mutex") != std::string::npos) {
            has_mutex_or_atomic = true;
        }
    }

    if (has_threads && !has_mutex_or_atomic) {
        DiagnosticItem item;
        item.file_path = pf.file_path;
        item.span = SourceSpan{1, 1, 1, 1};
        item.severity = DiagnosticSeverity::Warning;
        item.category = DiagnosticCategory::Concurrency;
        item.rule_id = "concurrency/unguarded-multithreading";
        item.message = "Multi-threaded execution detected without synchronization primitives (mutex/lock/atomic)";
        out.push_back(item);
    }
}

void DiagnosticsEngine::checkComplexityAndLength(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const {
    auto lines = splitLines(content);

    for (const auto& sym : pf.symbols) {
        if (sym.type != SymbolType::Function && sym.type != SymbolType::Method) continue;

        int func_lines = sym.end_line >= sym.start_line ? (sym.end_line - sym.start_line + 1) : 0;
        if (func_lines > 100) {
            DiagnosticItem item;
            item.file_path = pf.file_path;
            item.span = SourceSpan{sym.start_line, 1, sym.end_line, 1};
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::CodeSmell;
            item.rule_id = "maintainability/function-too-long";
            item.message = "Function '" + sym.name + "' exceeds 100 lines (" + std::to_string(func_lines) + " lines)";
            out.push_back(item);
        }

        // Count branching statements for cyclomatic complexity
        int complexity = 1;
        if (sym.start_line > 0 && sym.end_line <= static_cast<int>(lines.size())) {
            for (int l = sym.start_line - 1; l < sym.end_line; ++l) {
                const auto& line = lines[l];
                if (line.find("if ") != std::string::npos || line.find("if(") != std::string::npos) complexity++;
                if (line.find("else if") != std::string::npos) complexity++;
                if (line.find("for ") != std::string::npos || line.find("for(") != std::string::npos) complexity++;
                if (line.find("while ") != std::string::npos || line.find("while(") != std::string::npos) complexity++;
                if (line.find("case ") != std::string::npos) complexity++;
                if (line.find("&&") != std::string::npos) complexity++;
                if (line.find("||") != std::string::npos) complexity++;
                if (line.find("catch ") != std::string::npos || line.find("catch(") != std::string::npos) complexity++;
            }
        }

        if (complexity > 15) {
            DiagnosticItem item;
            item.file_path = pf.file_path;
            item.span = SourceSpan{sym.start_line, 1, sym.end_line, 1};
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::CodeSmell;
            item.rule_id = "maintainability/high-cyclomatic-complexity";
            item.message = "Function '" + sym.name + "' has cyclomatic complexity of " + std::to_string(complexity) + " (threshold: 15)";
            out.push_back(item);
        }
    }

    // Check for empty catch blocks
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (line.find("catch (...) {}") != std::string::npos || line.find("catch(...) {}") != std::string::npos ||
            line.find("except: pass") != std::string::npos) {
            DiagnosticItem item;
            item.file_path = pf.file_path;
            item.span = SourceSpan{static_cast<int>(i + 1), 1, static_cast<int>(i + 1), static_cast<int>(line.size())};
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::CodeSmell;
            item.rule_id = "error_handling/swallowed-exception";
            item.message = "Exception swallowed without logging or remediation in empty catch/except block";
            item.snippet = line;
            out.push_back(item);
        }
    }
}

std::vector<DiagnosticItem> DiagnosticsEngine::parseCompilerOutput(const std::string& output, const std::string& compiler_flavor) const {
    std::vector<DiagnosticItem> items;
    auto lines = splitLines(output);

    // Regex for MSVC: C:\path\file.cpp(123,45): error C2065: 'foo': undeclared identifier
    // or file.cpp(123): warning C4101: 'x': unreferenced
    static const std::regex msvc_regex(R"(^(.*?)\((\d+)(?:,\s*(\d+))?\):\s*(error|fatal error|warning)\s+([A-Z]\d+):\s*(.*)$)", std::regex::icase);

    // Regex for GCC/Clang: file.cpp:123:45: error: 'foo' was not declared
    // or file.cpp:123: error: ...
    static const std::regex gcc_regex(R"(^(.*?):(\d+)(?::(\d+))?:\s*(error|fatal error|warning|note):\s*(.*)$)", std::regex::icase);

    std::string flavor = compiler_flavor;
    std::transform(flavor.begin(), flavor.end(), flavor.begin(), [](unsigned char c) { return std::tolower(c); });

    for (const auto& line : lines) {
        std::smatch match;
        if (flavor == "msvc" || flavor == "auto") {
            if (std::regex_match(line, match, msvc_regex)) {
                DiagnosticItem item;
                item.file_path = match[1].str();
                int line_num = std::stoi(match[2].str());
                int col_num = match[3].matched ? std::stoi(match[3].str()) : 1;
                item.span = SourceSpan{line_num, col_num, line_num, col_num};

                std::string sev_str = match[4].str();
                std::transform(sev_str.begin(), sev_str.end(), sev_str.begin(), [](unsigned char c) { return std::tolower(c); });
                if (sev_str.find("fatal") != std::string::npos) item.severity = DiagnosticSeverity::Fatal;
                else if (sev_str.find("error") != std::string::npos) item.severity = DiagnosticSeverity::Error;
                else item.severity = DiagnosticSeverity::Warning;

                item.category = DiagnosticCategory::Compilation;
                item.rule_id = match[5].str();
                item.message = match[6].str();
                item.snippet = line;
                items.push_back(item);
                continue;
            }
        }

        if (flavor == "gcc" || flavor == "clang" || flavor == "auto") {
            if (std::regex_match(line, match, gcc_regex)) {
                DiagnosticItem item;
                item.file_path = match[1].str();
                int line_num = std::stoi(match[2].str());
                int col_num = match[3].matched ? std::stoi(match[3].str()) : 1;
                item.span = SourceSpan{line_num, col_num, line_num, col_num};

                std::string sev_str = match[4].str();
                std::transform(sev_str.begin(), sev_str.end(), sev_str.begin(), [](unsigned char c) { return std::tolower(c); });
                if (sev_str.find("fatal") != std::string::npos) item.severity = DiagnosticSeverity::Fatal;
                else if (sev_str.find("error") != std::string::npos) item.severity = DiagnosticSeverity::Error;
                else if (sev_str.find("warning") != std::string::npos) item.severity = DiagnosticSeverity::Warning;
                else item.severity = DiagnosticSeverity::Information;

                item.category = DiagnosticCategory::Compilation;
                item.message = match[5].str();
                item.snippet = line;

                // Extract rule tag if present (e.g. [-Wunused-variable])
                auto bracket_pos = item.message.rfind("[-W");
                if (bracket_pos != std::string::npos && item.message.back() == ']') {
                    item.rule_id = item.message.substr(bracket_pos + 1, item.message.size() - bracket_pos - 2);
                    item.message = item.message.substr(0, bracket_pos);
                    while (!item.message.empty() && item.message.back() == ' ') item.message.pop_back();
                } else {
                    item.rule_id = "compiler-diagnostic";
                }

                items.push_back(item);
                continue;
            }
        }
    }

    return items;
}

std::vector<DiagnosticItem> DiagnosticsEngine::parseClangTidyJson(const std::string& json_str) const {
    std::vector<DiagnosticItem> items;
    try {
        auto root = nlohmann::json::parse(json_str);
        if (root.contains("Diagnostics") && root["Diagnostics"].is_array()) {
            for (const auto& diag : root["Diagnostics"]) {
                DiagnosticItem item;
                item.category = DiagnosticCategory::CodeSmell;
                item.rule_id = diag.value("DiagnosticName", "clang-tidy");

                std::string level = diag.value("Level", "Warning");
                if (level == "Error") item.severity = DiagnosticSeverity::Error;
                else if (level == "Remark" || level == "Note") item.severity = DiagnosticSeverity::Information;
                else item.severity = DiagnosticSeverity::Warning;

                if (diag.contains("DiagnosticMessage") && diag["DiagnosticMessage"].is_object()) {
                    const auto& msg = diag["DiagnosticMessage"];
                    item.message = msg.value("Message", "");
                    item.file_path = msg.value("FilePath", "");
                    int offset = msg.value("FileOffset", 0);
                    item.span = SourceSpan{1, offset, 1, offset};
                }
                items.push_back(item);
            }
        }
    } catch (...) {
        // Return whatever parsed so far
    }
    return items;
}

std::vector<DiagnosticItem> DiagnosticsEngine::parseRuffJson(const std::string& json_str) const {
    std::vector<DiagnosticItem> items;
    try {
        auto root = nlohmann::json::parse(json_str);
        if (root.is_array()) {
            for (const auto& entry : root) {
                DiagnosticItem item;
                item.category = DiagnosticCategory::CodeSmell;
                item.rule_id = entry.value("code", "ruff");
                item.message = entry.value("message", "");
                item.file_path = entry.value("filename", "");
                item.severity = DiagnosticSeverity::Warning;

                if (entry.contains("location") && entry["location"].is_object()) {
                    int row = entry["location"].value("row", 1);
                    int col = entry["location"].value("column", 1);
                    int end_row = row;
                    int end_col = col;
                    if (entry.contains("end_location") && entry["end_location"].is_object()) {
                        end_row = entry["end_location"].value("row", row);
                        end_col = entry["end_location"].value("column", col);
                    }
                    item.span = SourceSpan{row, col, end_row, end_col};
                }

                if (entry.contains("fix") && entry["fix"].is_object()) {
                    SuggestedFix fix;
                    fix.description = entry["fix"].value("message", "Fix violation");
                    fix.replacement_span = item.span;
                    item.fix = fix;
                }
                items.push_back(item);
            }
        }
    } catch (...) {
    }
    return items;
}

std::vector<DiagnosticItem> DiagnosticsEngine::parseEslintJson(const std::string& json_str) const {
    std::vector<DiagnosticItem> items;
    try {
        auto root = nlohmann::json::parse(json_str);
        if (root.is_array()) {
            for (const auto& file_entry : root) {
                std::string file_path = file_entry.value("filePath", "");
                if (file_entry.contains("messages") && file_entry["messages"].is_array()) {
                    for (const auto& msg : file_entry["messages"]) {
                        DiagnosticItem item;
                        item.file_path = file_path;
                        item.category = DiagnosticCategory::CodeSmell;
                        item.rule_id = msg.value("ruleId", "eslint");
                        item.message = msg.value("message", "");
                        int sev = msg.value("severity", 1);
                        item.severity = (sev == 2) ? DiagnosticSeverity::Error : DiagnosticSeverity::Warning;

                        int line = msg.value("line", 1);
                        int col = msg.value("column", 1);
                        int end_line = msg.value("endLine", line);
                        int end_col = msg.value("endColumn", col);
                        item.span = SourceSpan{line, col, end_line, end_col};

                        items.push_back(item);
                    }
                }
            }
        }
    } catch (...) {
    }
    return items;
}

} // namespace aios::testing
