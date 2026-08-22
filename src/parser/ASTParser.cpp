#include "parser/ASTParser.h"
#include <sstream>
#include <regex>
#include <iomanip>
#include <algorithm>
#include <unordered_set>

namespace aios {

std::string ASTParser::detectLanguage(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) return "unknown";

    std::string ext = file_path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".h" || ext == ".hpp" || ext == ".c") return "cpp";
    if (ext == ".py" || ext == ".pyi") return "python";
    if (ext == ".ts" || ext == ".tsx") return "typescript";
    if (ext == ".js" || ext == ".jsx" || ext == ".mjs") return "javascript";
    if (ext == ".rs") return "rust";
    if (ext == ".go") return "go";
    if (ext == ".java") return "java";
    if (ext == ".md" || ext == ".markdown") return "markdown";
    if (ext == ".json") return "json";

    return "unknown";
}

std::string ASTParser::computeChecksum(const std::string& content) {
    uint64_t hash = 14695981039346656037ULL; // FNV-1a 64-bit offset basis
    for (char c : content) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL; // FNV prime
    }
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

std::vector<std::string> ASTParser::extractCallees(const std::string& body) {
    std::vector<std::string> callees;
    std::regex call_regex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    auto words_begin = std::sregex_iterator(body.begin(), body.end(), call_regex);
    auto words_end = std::sregex_iterator();

    std::unordered_set<std::string> keywords = {
        "if", "while", "for", "switch", "catch", "return", "sizeof", "decltype", 
        "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast"
    };

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::string name = (*i)[1].str();
        if (keywords.find(name) == keywords.end()) {
            if (std::find(callees.begin(), callees.end(), name) == callees.end()) {
                callees.push_back(name);
            }
        }
    }
    return callees;
}

std::string ASTParser::extractDocstringBefore(const std::vector<std::string>& lines, int line_idx) {
    std::string doc;
    int idx = line_idx - 1;
    while (idx >= 0) {
        std::string l = lines[idx];
        l.erase(0, l.find_first_not_of(" \t"));
        if (l.rfind("//", 0) == 0) {
            doc = l.substr(2) + "\n" + doc;
            idx--;
        } else if (l.rfind("/*", 0) == 0 || l.rfind("*", 0) == 0 || l.rfind("*/", 0) == 0 || l.rfind("#", 0) == 0) {
            doc = l + "\n" + doc;
            idx--;
        } else {
            break;
        }
    }
    return doc;
}

ParsedFile ASTParser::parseFile(const std::string& file_path, const std::string& content) {
    ParsedFile pf;
    pf.file_path = file_path;
    pf.language = detectLanguage(file_path);
    pf.checksum = computeChecksum(content);

    // Count lines
    std::stringstream count_stream(content);
    std::string temp_line;
    while (std::getline(count_stream, temp_line)) {
        pf.line_count++;
    }

    if (pf.language == "cpp") {
        parseCpp(file_path, content, pf);
    } else if (pf.language == "python") {
        parsePython(file_path, content, pf);
    } else if (pf.language == "javascript" || pf.language == "typescript") {
        parseJavaScript(file_path, content, pf);
    } else if (pf.language == "rust") {
        parseRust(file_path, content, pf);
    } else if (pf.language == "go") {
        parseGo(file_path, content, pf);
    } else if (pf.language == "java") {
        parseJava(file_path, content, pf);
    }

    return pf;
}

void ASTParser::parseCpp(const std::string& file_path, const std::string& content, ParsedFile& out) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string l;
    while (std::getline(ss, l)) lines.push_back(l);

    std::regex include_regex(R"(^\s*#include\s+([<"][^>"]+[>"]))");
    std::regex class_regex(R"(\b(?:class|struct)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::regex func_regex(R"(\b(?:(?:inline|static|virtual|explicit|constexpr|const|friend)\s+)*(?:[a-zA-Z_][a-zA-Z0-9_:*&<>]*\s+)+([a-zA-Z_][a-zA-Z0-9_:]*)\s*\(([^)]*)\))");

    std::unordered_set<std::string> ignored_names = {
        "if", "while", "for", "switch", "catch", "return", "sizeof", "decltype",
        "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",
        "class", "struct", "enum", "template", "typedef", "using", "public", "private", "protected"
    };

    std::string current_class;
    int current_brace_depth = 0;
    int class_depth = -1;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        int line_num = static_cast<int>(i + 1);

        std::smatch match;
        // 1. Includes
        if (std::regex_search(line, match, include_regex)) {
            out.imports.push_back(match[1].str());
            continue;
        }

        // 2. Classes & Structs
        if (std::regex_search(line, match, class_regex)) {
            std::string class_name = match[1].str();
            current_class = class_name;
            class_depth = current_brace_depth;
            CodeSymbol sym;
            sym.name = class_name;
            sym.type = (line.find("struct") != std::string::npos) ? SymbolType::Struct : SymbolType::Class;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            out.symbols.push_back(sym);
        }

        // 3. Functions & Methods (allow multiple per line)
        auto f_begin = std::sregex_iterator(line.begin(), line.end(), func_regex);
        auto f_end = std::sregex_iterator();
        for (auto it = f_begin; it != f_end; ++it) {
            std::smatch f_match = *it;
            std::string func_name = f_match[1].str();
            if (ignored_names.find(func_name) == ignored_names.end()) {
                CodeSymbol sym;
                sym.name = func_name;
                sym.type = current_class.empty() ? SymbolType::Function : SymbolType::Method;
                sym.file_path = file_path;
                sym.signature = f_match[0].str();
                sym.start_line = line_num;
                sym.end_line = line_num;
                sym.parent_symbol = current_class;
                sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));

                // Extract function body to collect callees
                std::string body_text;
                size_t match_end = f_match.position() + f_match.length();
                std::string remainder_first_line = line.substr(match_end);
                
                int b_depth = 0;
                bool body_started = false;

                // Check remaining text on current line
                for (char c : remainder_first_line) {
                    if (c == '{') {
                        b_depth++;
                        body_started = true;
                    } else if (c == '}') {
                        b_depth--;
                    }
                    if (body_started) {
                        body_text += c;
                    }
                    if (body_started && b_depth <= 0) {
                        break;
                    }
                }

                // If body continues across subsequent lines
                if (body_started && b_depth > 0) {
                    for (size_t j = i + 1; j < lines.size(); ++j) {
                        body_text += "\n";
                        for (char c : lines[j]) {
                            if (c == '{') {
                                b_depth++;
                            } else if (c == '}') {
                                b_depth--;
                            }
                            body_text += c;
                            if (b_depth <= 0) {
                                sym.end_line = static_cast<int>(j + 1);
                                break;
                            }
                        }
                        if (b_depth <= 0) break;
                    }
                }

                sym.callees = extractCallees(body_text.empty() ? line : body_text);
                // Remove self from callees if accidentally matched
                sym.callees.erase(std::remove(sym.callees.begin(), sym.callees.end(), func_name), sym.callees.end());
                out.symbols.push_back(sym);
            }
        }

        // Track class scope brace depth
        for (char c : line) {
            if (c == '{') {
                current_brace_depth++;
            } else if (c == '}') {
                current_brace_depth--;
            }
        }

        if (!current_class.empty() && class_depth != -1 && current_brace_depth <= class_depth) {
            current_class.clear();
            class_depth = -1;
        }
    }
}

void ASTParser::parsePython(const std::string& file_path, const std::string& content, ParsedFile& out) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string l;
    while (std::getline(ss, l)) lines.push_back(l);

    std::regex import_regex(R"(^\s*(?:import\s+([a-zA-Z0-9_., ]+)|from\s+([a-zA-Z0-9_.]+)\s+import))");
    std::regex class_regex(R"(^\s*class\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::regex func_regex(R"(^\s*(?:async\s+)?def\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\))");

    std::string current_class;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        int line_num = static_cast<int>(i + 1);

        std::smatch match;
        if (std::regex_search(line, match, import_regex)) {
            out.imports.push_back(match[1].matched ? match[1].str() : match[2].str());
            continue;
        }

        if (std::regex_search(line, match, class_regex)) {
            current_class = match[1].str();
            CodeSymbol sym;
            sym.name = current_class;
            sym.type = SymbolType::Class;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            out.symbols.push_back(sym);
            continue;
        }

        if (std::regex_search(line, match, func_regex)) {
            std::string func_name = match[1].str();
            CodeSymbol sym;
            sym.name = func_name;
            sym.type = current_class.empty() ? SymbolType::Function : SymbolType::Method;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.parent_symbol = current_class;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            sym.callees = extractCallees(line);
            out.symbols.push_back(sym);
        }
    }
}

void ASTParser::parseJavaScript(const std::string& file_path, const std::string& content, ParsedFile& out) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string l;
    while (std::getline(ss, l)) lines.push_back(l);

    std::regex import_regex(R"(^\s*import\s+.*from\s+['"]([^'"]+)['"])");
    std::regex class_regex(R"(^\s*(?:export\s+)?class\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::regex func_regex(R"(^\s*(?:export\s+)?(?:async\s+)?function\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    std::regex arrow_regex(R"(^\s*(?:export\s+)?(?:const|let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(?:async\s+)?\([^)]*\)\s*=>)");

    std::string current_class;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        int line_num = static_cast<int>(i + 1);

        std::smatch match;
        if (std::regex_search(line, match, import_regex)) {
            out.imports.push_back(match[1].str());
            continue;
        }

        if (std::regex_search(line, match, class_regex)) {
            current_class = match[1].str();
            CodeSymbol sym;
            sym.name = current_class;
            sym.type = SymbolType::Class;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            out.symbols.push_back(sym);
            continue;
        }

        if (std::regex_search(line, match, func_regex) || std::regex_search(line, match, arrow_regex)) {
            std::string func_name = match[1].str();
            CodeSymbol sym;
            sym.name = func_name;
            sym.type = current_class.empty() ? SymbolType::Function : SymbolType::Method;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.parent_symbol = current_class;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            sym.callees = extractCallees(line);
            out.symbols.push_back(sym);
        }
    }
}

void ASTParser::parseRust(const std::string& file_path, const std::string& content, ParsedFile& out) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string l;
    while (std::getline(ss, l)) lines.push_back(l);

    std::regex use_regex(R"(^\s*use\s+([^;]+);)");
    std::regex struct_regex(R"(^\s*(?:pub\s+)?struct\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::regex fn_regex(R"(^\s*(?:pub\s+)?(?:async\s+)?fn\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        int line_num = static_cast<int>(i + 1);

        std::smatch match;
        if (std::regex_search(line, match, use_regex)) {
            out.imports.push_back(match[1].str());
            continue;
        }

        if (std::regex_search(line, match, struct_regex)) {
            CodeSymbol sym;
            sym.name = match[1].str();
            sym.type = SymbolType::Struct;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            out.symbols.push_back(sym);
            continue;
        }

        if (std::regex_search(line, match, fn_regex)) {
            CodeSymbol sym;
            sym.name = match[1].str();
            sym.type = SymbolType::Function;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            sym.callees = extractCallees(line);
            out.symbols.push_back(sym);
        }
    }
}

void ASTParser::parseGo(const std::string& file_path, const std::string& content, ParsedFile& out) {
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string l;
    while (std::getline(ss, l)) lines.push_back(l);

    std::regex import_regex(R"re(^\s*import\s+(?:\(([\s\S]*?)\)|"([^"]+)"))re");
    std::regex type_regex(R"(^\s*type\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+(?:struct|interface))");
    std::regex func_regex(R"(^\s*func\s+(?:\([^)]+\)\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        int line_num = static_cast<int>(i + 1);

        std::smatch match;
        if (std::regex_search(line, match, type_regex)) {
            CodeSymbol sym;
            sym.name = match[1].str();
            sym.type = (line.find("interface") != std::string::npos) ? SymbolType::Interface : SymbolType::Struct;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            out.symbols.push_back(sym);
            continue;
        }

        if (std::regex_search(line, match, func_regex)) {
            CodeSymbol sym;
            sym.name = match[1].str();
            sym.type = SymbolType::Function;
            sym.file_path = file_path;
            sym.signature = line;
            sym.start_line = line_num;
            sym.end_line = line_num;
            sym.docstring = extractDocstringBefore(lines, static_cast<int>(i));
            sym.callees = extractCallees(line);
            out.symbols.push_back(sym);
        }
    }
}

void ASTParser::parseJava(const std::string& file_path, const std::string& content, ParsedFile& out) {
    parseCpp(file_path, content, out); // Java shares similar class & method structure
}

} // namespace aios
