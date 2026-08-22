#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>
#include "TestingTypes.h"
#include "parser/ASTParser.h"

namespace aios::testing {

class DiagnosticsEngine {
public:
    DiagnosticsEngine() = default;
    ~DiagnosticsEngine() = default;

    // Main Scanning API
    TestingResult<DiagnosticsReport> scanFile(const std::string& file_path, const std::string& content);
    TestingResult<DiagnosticsReport> scanDirectory(const std::string& dir_path, const std::vector<std::string>& file_patterns = {});

    // Compiler Error Parsing
    std::vector<DiagnosticItem> parseCompilerOutput(const std::string& output, const std::string& compiler_flavor = "msvc") const;

    // Linter Integration
    std::vector<DiagnosticItem> parseClangTidyJson(const std::string& json_str) const;
    std::vector<DiagnosticItem> parseRuffJson(const std::string& json_str) const;
    std::vector<DiagnosticItem> parseEslintJson(const std::string& json_str) const;

private:
    std::vector<DiagnosticItem> runAstRuleChecks(const ParsedFile& parsed_file, const std::string& content) const;
    void checkResourceLeaks(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
    void checkSecuritySmells(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
    void checkConcurrencySmells(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
    void checkComplexityAndLength(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
    void checkSyntaxAndStructure(const ParsedFile& pf, const std::string& content, std::vector<DiagnosticItem>& out) const;
};

} // namespace aios::testing
