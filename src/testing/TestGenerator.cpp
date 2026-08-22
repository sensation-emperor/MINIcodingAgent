#include "testing/TestGenerator.h"
#include "logging/Logger.h"
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <regex>
#include <array>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

namespace aios {

namespace {

std::string exec_capture(const std::string& cmd) {
    std::array<char, 1024> buf;
    std::string result;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);
    return result;
}

} // namespace

// ─────────────────────────────────────────────────────────────
// DiagnosticsReport helpers
// ─────────────────────────────────────────────────────────────

std::vector<CompilerDiagnostic> DiagnosticsReport::errors() const {
    std::vector<CompilerDiagnostic> result;
    for (const auto& d : diagnostics) {
        if (d.severity == DiagnosticSeverity::Error || d.severity == DiagnosticSeverity::Fatal) {
            result.push_back(d);
        }
    }
    return result;
}

std::vector<CompilerDiagnostic> DiagnosticsReport::warnings() const {
    std::vector<CompilerDiagnostic> result;
    for (const auto& d : diagnostics) {
        if (d.severity == DiagnosticSeverity::Warning) {
            result.push_back(d);
        }
    }
    return result;
}

std::string DiagnosticsReport::toSummaryString() const {
    std::ostringstream oss;
    oss << "Build " << (has_errors ? "FAILED" : "SUCCEEDED")
        << " — " << error_count << " error(s), " << warning_count << " warning(s)\n";
    for (const auto& d : diagnostics) {
        if (d.severity == DiagnosticSeverity::Error || d.severity == DiagnosticSeverity::Fatal) {
            oss << "  [ERROR] " << d.file_path << ":" << d.line;
            if (d.column > 0) oss << ":" << d.column;
            oss << " — " << d.message << "\n";
            if (!d.suggested_fix.empty()) {
                oss << "    → Fix: " << d.suggested_fix << "\n";
            }
        }
    }
    return oss.str();
}

// ─────────────────────────────────────────────────────────────
// Test Body Generation Helpers
// ─────────────────────────────────────────────────────────────

std::string TestGenerator::generateArgsPlaceholders(const std::vector<std::string>& param_types) const {
    if (param_types.empty()) return "";

    std::vector<std::string> args;
    int idx = 0;
    for (const auto& t : param_types) {
        std::string arg;
        if (t.find("std::string") != std::string::npos || t.find("string_view") != std::string::npos) {
            arg = "\"test_arg_" + std::to_string(idx) + "\"";
        } else if (t.find("int") != std::string::npos || t.find("size_t") != std::string::npos ||
                   t.find("uint") != std::string::npos) {
            arg = "0";
        } else if (t.find("float") != std::string::npos || t.find("double") != std::string::npos) {
            arg = "0.0f";
        } else if (t.find("bool") != std::string::npos) {
            arg = "false";
        } else if (t.find("vector") != std::string::npos) {
            arg = "{}";
        } else {
            arg = "/* " + t + " */";
        }
        args.push_back(arg);
        idx++;
    }

    std::string joined;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) joined += ", ";
        joined += args[i];
    }
    return joined;
}

std::string TestGenerator::generateReturnAssertion(const std::string& return_type) const {
    if (return_type == "void") {
        return "    // No return value to assert";
    } else if (return_type == "bool") {
        return "    // EXPECT_TRUE(result); or EXPECT_FALSE(result);";
    } else if (return_type.find("string") != std::string::npos) {
        return "    EXPECT_FALSE(result.empty());";
    } else if (return_type.find("vector") != std::string::npos) {
        return "    // EXPECT_FALSE(result.empty());";
    } else if (return_type.find("optional") != std::string::npos) {
        return "    // EXPECT_TRUE(result.has_value());";
    } else if (return_type == "int" || return_type == "size_t") {
        return "    EXPECT_GE(result, 0);";
    } else {
        return "    // TODO: add assertions for return type " + return_type;
    }
}

std::string TestGenerator::generateTestBody(const ParsedSymbol& symbol) const {
    std::ostringstream body;
    std::string args = generateArgsPlaceholders(symbol.param_types);

    body << "    // Arrange\n";

    if (symbol.is_method && !symbol.class_name.empty()) {
        body << "    " << symbol.class_name << " instance;\n\n";
        body << "    // Act\n";
        if (symbol.return_type == "void") {
            body << "    instance." << symbol.qualified_name.substr(
                    symbol.qualified_name.rfind("::") != std::string::npos
                    ? symbol.qualified_name.rfind("::") + 2 : 0)
                 << "(" << args << ");\n\n";
            body << "    // Assert\n";
            body << "    // No crash expected\n";
        } else {
            size_t method_start = symbol.qualified_name.rfind("::");
            std::string method_name = (method_start != std::string::npos)
                ? symbol.qualified_name.substr(method_start + 2)
                : symbol.qualified_name;
            body << "    auto result = instance." << method_name << "(" << args << ");\n\n";
            body << "    // Assert\n";
            body << generateReturnAssertion(symbol.return_type) << "\n";
        }
    } else {
        body << "    // Act\n";
        if (symbol.return_type == "void") {
            body << "    " << symbol.qualified_name << "(" << args << ");\n\n";
            body << "    // Assert — no crash expected\n";
        } else {
            body << "    auto result = " << symbol.qualified_name << "(" << args << ");\n\n";
            body << "    // Assert\n";
            body << generateReturnAssertion(symbol.return_type) << "\n";
        }
    }

    return body.str();
}

GeneratedTestCase TestGenerator::generateTestCase(const ParsedSymbol& symbol,
                                                  const std::string& suite_name) const {
    GeneratedTestCase tc;
    tc.test_suite_name = suite_name;

    // Sanitize name for GoogleTest
    std::string raw_name = symbol.qualified_name;
    std::replace(raw_name.begin(), raw_name.end(), ':', '_');
    std::replace(raw_name.begin(), raw_name.end(), '<', '_');
    std::replace(raw_name.begin(), raw_name.end(), '>', '_');
    std::replace(raw_name.begin(), raw_name.end(), ' ', '_');
    tc.test_name = raw_name + "_BasicSanity";

    tc.test_body = generateTestBody(symbol);

    if (!symbol.file_path.empty()) {
        tc.include_headers.push_back(symbol.file_path);
    }
    tc.namespace_str = "aios";

    return tc;
}

std::string TestGenerator::generateIncludeGuard(const std::string& suite_name) const {
    std::string guard = "AIOS_AUTOGEN_" + suite_name + "_TEST_CPP";
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
    return guard;
}

std::string TestGenerator::generateIncludes(const std::vector<GeneratedTestCase>& cases) const {
    std::vector<std::string> seen;
    std::string result = "#include <gtest/gtest.h>\n";
    for (const auto& tc : cases) {
        for (const auto& hdr : tc.include_headers) {
            if (std::find(seen.begin(), seen.end(), hdr) == seen.end()) {
                result += "#include \"" + hdr + "\"\n";
                seen.push_back(hdr);
            }
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// Test Generation Entry Point
// ─────────────────────────────────────────────────────────────

TestGenerationResult TestGenerator::generateTests(const std::vector<ParsedSymbol>& symbols,
                                                  const std::string& output_dir,
                                                  const std::string& suite_name) {
    TestGenerationResult result;

    if (symbols.empty()) {
        result.error_message = "No symbols provided for test generation.";
        return result;
    }

    for (const auto& sym : symbols) {
        result.test_cases.push_back(generateTestCase(sym, suite_name));
    }

    // Build output file path
    std::string filename = "test_autogen_" + suite_name + ".cpp";
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    result.output_file_path = output_dir + "/" + filename;

    result.success = writeTestFile(result);
    if (!result.success) {
        result.error_message = "Failed to write test file: " + result.output_file_path;
    }

    return result;
}

bool TestGenerator::writeTestFile(const TestGenerationResult& result) const {
    std::ofstream f(result.output_file_path);
    if (!f.is_open()) {
        LOG_ERROR("TestGenerator: Cannot open [{}] for writing.", result.output_file_path);
        return false;
    }

    f << "// AUTO-GENERATED by AIOS TestGenerator — do not edit manually\n";
    f << "// Generated: " << __DATE__ << " " << __TIME__ << "\n\n";

    if (!result.test_cases.empty()) {
        f << generateIncludes(result.test_cases) << "\n";
    }

    for (const auto& tc : result.test_cases) {
        f << "TEST(" << tc.test_suite_name << ", " << tc.test_name << ") {\n";
        f << tc.test_body;
        f << "}\n\n";
    }

    f.close();
    LOG_INFO("TestGenerator: Wrote {} tests to [{}]",
             result.test_cases.size(), result.output_file_path);
    return true;
}

// ─────────────────────────────────────────────────────────────
// Diagnostics Parsing
// ─────────────────────────────────────────────────────────────

DiagnosticSeverity TestGenerator::parseSeverityToken(std::string_view token) const {
    if (token == "error")   return DiagnosticSeverity::Error;
    if (token == "fatal")   return DiagnosticSeverity::Fatal;
    if (token == "warning") return DiagnosticSeverity::Warning;
    if (token == "note")    return DiagnosticSeverity::Note;
    return DiagnosticSeverity::Note;
}

std::string TestGenerator::suggestFix(const CompilerDiagnostic& diag) const {
    const std::string& msg = diag.message;

    if (msg.find("undeclared identifier") != std::string::npos ||
        msg.find("was not declared") != std::string::npos) {
        return "Add missing #include or forward declaration.";
    }
    if (msg.find("no matching function") != std::string::npos ||
        msg.find("no matching member function") != std::string::npos) {
        return "Check function signature / argument types.";
    }
    if (msg.find("unused variable") != std::string::npos ||
        msg.find("unused parameter") != std::string::npos) {
        return "Prefix with (void) cast or [[maybe_unused]] attribute.";
    }
    if (msg.find("return type") != std::string::npos ||
        msg.find("cannot convert") != std::string::npos) {
        return "Check return type / implicit conversion.";
    }
    if (msg.find("undefined reference") != std::string::npos ||
        msg.find("unresolved external") != std::string::npos) {
        return "Add missing implementation or link library.";
    }
    if (msg.find("narrowing conversion") != std::string::npos) {
        return "Add explicit cast to resolve narrowing conversion.";
    }
    if (msg.find("multiple definition") != std::string::npos) {
        return "Move definition to .cpp or use inline keyword.";
    }
    return "";
}

DiagnosticsReport TestGenerator::parseGccClangOutput(const std::string& output) const {
    DiagnosticsReport report;
    report.raw_output = output;

    // GCC/Clang format: file:line:col: severity: message
    static const std::regex diag_regex(
        R"(^(.+?):(\d+):(\d+):\s*(error|warning|note|fatal error):\s*(.+)$)",
        std::regex::multiline
    );

    auto begin = std::sregex_iterator(output.begin(), output.end(), diag_regex);
    auto end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        const std::smatch& m = *it;
        CompilerDiagnostic diag;
        diag.file_path = m[1].str();
        diag.line      = std::stoi(m[2].str());
        diag.column    = std::stoi(m[3].str());
        diag.severity  = parseSeverityToken(m[4].str());
        diag.message   = m[5].str();
        diag.suggested_fix = suggestFix(diag);

        if (diag.severity == DiagnosticSeverity::Error || diag.severity == DiagnosticSeverity::Fatal) {
            report.has_errors = true;
            report.error_count++;
        } else if (diag.severity == DiagnosticSeverity::Warning) {
            report.warning_count++;
        }

        report.diagnostics.push_back(std::move(diag));
    }

    return report;
}

DiagnosticsReport TestGenerator::parseMsvcOutput(const std::string& output) const {
    DiagnosticsReport report;
    report.raw_output = output;

    // MSVC format: file(line): error CXXXX: message
    static const std::regex msvc_regex(
        R"(^(.+?)\((\d+)\)\s*:\s*(error|warning|note)\s+(C\d+)\s*:\s*(.+)$)",
        std::regex::multiline
    );

    auto begin = std::sregex_iterator(output.begin(), output.end(), msvc_regex);
    auto end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        const std::smatch& m = *it;
        CompilerDiagnostic diag;
        diag.file_path   = m[1].str();
        diag.line        = std::stoi(m[2].str());
        diag.severity    = parseSeverityToken(m[3].str());
        diag.error_code  = m[4].str();
        diag.message     = m[5].str();
        diag.suggested_fix = suggestFix(diag);

        if (diag.severity == DiagnosticSeverity::Error) {
            report.has_errors = true;
            report.error_count++;
        } else if (diag.severity == DiagnosticSeverity::Warning) {
            report.warning_count++;
        }

        report.diagnostics.push_back(std::move(diag));
    }

    return report;
}

DiagnosticsReport TestGenerator::parseDiagnostics(const std::string& compiler_output,
                                                  const std::string& compiler) const {
    if (compiler == "msvc" || compiler == "cl") {
        return parseMsvcOutput(compiler_output);
    }
    return parseGccClangOutput(compiler_output);
}

DiagnosticsReport TestGenerator::buildAndCapture(const std::string& build_command,
                                                 const std::string& working_dir) const {
    std::string cmd = build_command;
    if (!working_dir.empty()) {
        cmd = "cd \"" + working_dir + "\" && " + cmd;
    }

    LOG_INFO("TestGenerator: Running build: {}", cmd);
    std::string output = exec_capture(cmd);
    LOG_INFO("TestGenerator: Build output captured ({} chars)", output.size());

    // Auto-detect compiler from output
    std::string compiler = "gcc";
    if (output.find(": error C") != std::string::npos ||
        output.find(": warning C") != std::string::npos) {
        compiler = "msvc";
    }

    return parseDiagnostics(output, compiler);
}

std::string TestGenerator::toRemediationActions(const DiagnosticsReport& report) const {
    nlohmann::json actions = nlohmann::json::array();

    for (const auto& diag : report.diagnostics) {
        if (diag.severity != DiagnosticSeverity::Error &&
            diag.severity != DiagnosticSeverity::Fatal) continue;

        nlohmann::json action;
        action["type"]      = "fix_compiler_error";
        action["file"]      = diag.file_path;
        action["line"]      = diag.line;
        action["column"]    = diag.column;
        action["error"]     = diag.message;
        action["error_code"] = diag.error_code;
        action["fix"]       = diag.suggested_fix;
        action["context"]   = diag.source_context;
        actions.push_back(action);
    }

    nlohmann::json root;
    root["has_errors"]    = report.has_errors;
    root["error_count"]   = report.error_count;
    root["warning_count"] = report.warning_count;
    root["actions"]       = actions;
    return root.dump(2);
}

} // namespace aios
