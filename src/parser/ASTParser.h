#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace aios {

enum class SymbolType {
    Function,
    Method,
    Class,
    Struct,
    Interface,
    Enum,
    Variable,
    Namespace,
    Import,
    TypeAlias
};

struct CodeSymbol {
    std::string name;
    SymbolType type = SymbolType::Function;
    std::string file_path;
    std::string signature;
    std::string docstring;
    int start_line = 0;
    int end_line = 0;
    std::string parent_symbol;
    std::string visibility = "public"; // "public", "protected", "private"
    std::vector<std::string> callees;  // Functions/methods called within this symbol
    std::vector<std::string> callers;  // Symbols calling this
};

struct ParsedFile {
    std::string file_path;
    std::string language; // "cpp", "python", "javascript", "typescript", "rust", "go", "java"
    std::vector<CodeSymbol> symbols;
    std::vector<std::string> imports;
    size_t line_count = 0;
    std::string checksum;
    bool success = true;
    std::string error;
};

class ASTParser {
public:
    ASTParser() = default;
    ~ASTParser() = default;

    /**
     * @brief Parse source file contents and extract all symbols, signatures, and dependencies
     */
    ParsedFile parseFile(const std::string& file_path, const std::string& content);

    /**
     * @brief Detect language from file extension or header hints
     */
    static std::string detectLanguage(const std::string& file_path);

    /**
     * @brief Compute SHA-256 / FNV-1a checksum of file contents
     */
    static std::string computeChecksum(const std::string& content);

private:
    void parseCpp(const std::string& file_path, const std::string& content, ParsedFile& out);
    void parsePython(const std::string& file_path, const std::string& content, ParsedFile& out);
    void parseJavaScript(const std::string& file_path, const std::string& content, ParsedFile& out);
    void parseRust(const std::string& file_path, const std::string& content, ParsedFile& out);
    void parseGo(const std::string& file_path, const std::string& content, ParsedFile& out);
    void parseJava(const std::string& file_path, const std::string& content, ParsedFile& out);

    std::vector<std::string> extractCallees(const std::string& body);
    std::string extractDocstringBefore(const std::vector<std::string>& lines, int line_idx);
};

} // namespace aios
