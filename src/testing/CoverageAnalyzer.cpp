#include "CoverageAnalyzer.h"
#include <sstream>
#include <regex>
#include <algorithm>
#include <map>
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

void CoverageAnalyzer::computeUncoveredRanges(FileCoverage& fc, const std::unordered_map<int, int>& line_hits) const {
    if (line_hits.empty()) return;

    std::vector<int> sorted_lines;
    for (const auto& [line, hits] : line_hits) {
        sorted_lines.push_back(line);
    }
    std::sort(sorted_lines.begin(), sorted_lines.end());

    int range_start = -1;
    int prev_line = -1;

    for (int l : sorted_lines) {
        int hits = line_hits.at(l);
        if (hits == 0) {
            if (range_start == -1) {
                range_start = l;
                prev_line = l;
            } else if (l == prev_line + 1) {
                prev_line = l;
            } else {
                fc.uncovered_line_ranges.push_back(LineRange{range_start, prev_line});
                range_start = l;
                prev_line = l;
            }
        } else {
            if (range_start != -1) {
                fc.uncovered_line_ranges.push_back(LineRange{range_start, prev_line});
                range_start = -1;
                prev_line = -1;
            }
        }
    }

    if (range_start != -1) {
        fc.uncovered_line_ranges.push_back(LineRange{range_start, prev_line});
    }
}

TestingResult<CoverageReport> CoverageAnalyzer::parseLcovInfo(const std::string& lcov_content) {
    CoverageReport report;
    report.format = "lcov";

    auto lines = splitLines(lcov_content);
    if (lines.empty()) {
        return std::unexpected("Empty LCOV coverage content");
    }

    std::string current_file;
    FileCoverage current_fc;
    std::unordered_map<int, int> current_line_hits;

    size_t total_covered_lines = 0;
    size_t total_lines_all = 0;
    size_t total_covered_branches = 0;
    size_t total_branches_all = 0;
    size_t total_covered_funcs = 0;
    size_t total_funcs_all = 0;

    for (const auto& raw_line : lines) {
        if (raw_line.starts_with("SF:")) {
            current_file = raw_line.substr(3);
            current_fc = FileCoverage{};
            current_fc.file_path = current_file;
            current_line_hits.clear();
        } else if (raw_line.starts_with("DA:")) {
            auto comma = raw_line.find(',', 3);
            if (comma != std::string::npos) {
                try {
                    int line_num = std::stoi(raw_line.substr(3, comma - 3));
                    int hits = std::stoi(raw_line.substr(comma + 1));
                    current_line_hits[line_num] = hits;
                    current_fc.total_lines++;
                    if (hits > 0) current_fc.covered_lines++;
                } catch (...) {}
            }
        } else if (raw_line.starts_with("LF:")) {
            try { current_fc.total_lines = std::stoull(raw_line.substr(3)); } catch (...) {}
        } else if (raw_line.starts_with("LH:")) {
            try { current_fc.covered_lines = std::stoull(raw_line.substr(3)); } catch (...) {}
        } else if (raw_line.starts_with("FNF:")) {
            try { current_fc.total_functions = std::stoull(raw_line.substr(4)); } catch (...) {}
        } else if (raw_line.starts_with("FNH:")) {
            try { current_fc.covered_functions = std::stoull(raw_line.substr(4)); } catch (...) {}
        } else if (raw_line.starts_with("BRF:")) {
            try { current_fc.total_branches = std::stoull(raw_line.substr(4)); } catch (...) {}
        } else if (raw_line.starts_with("BRH:")) {
            try { current_fc.covered_branches = std::stoull(raw_line.substr(4)); } catch (...) {}
        } else if (raw_line.starts_with("BRDA:")) {
            // BRDA:<line>,<block>,<branch>,<taken>
            auto last_comma = raw_line.rfind(',');
            if (last_comma != std::string::npos) {
                std::string taken = raw_line.substr(last_comma + 1);
                current_fc.total_branches++;
                if (taken != "-" && taken != "0") {
                    current_fc.covered_branches++;
                }
            }
        } else if (raw_line.starts_with("end_of_record")) {
            if (!current_file.empty()) {
                if (current_fc.total_lines > 0) {
                    current_fc.line_coverage_percent = (static_cast<double>(current_fc.covered_lines) / current_fc.total_lines) * 100.0;
                }
                if (current_fc.total_branches > 0) {
                    current_fc.branch_coverage_percent = (static_cast<double>(current_fc.covered_branches) / current_fc.total_branches) * 100.0;
                }
                if (current_fc.total_functions > 0) {
                    current_fc.function_coverage_percent = (static_cast<double>(current_fc.covered_functions) / current_fc.total_functions) * 100.0;
                }
                computeUncoveredRanges(current_fc, current_line_hits);

                total_lines_all += current_fc.total_lines;
                total_covered_lines += current_fc.covered_lines;
                total_branches_all += current_fc.total_branches;
                total_covered_branches += current_fc.covered_branches;
                total_funcs_all += current_fc.total_functions;
                total_covered_funcs += current_fc.covered_functions;

                report.files[current_file] = current_fc;
                current_file.clear();
            }
        }
    }

    report.total_lines = total_lines_all;
    report.covered_lines = total_covered_lines;
    report.overall_line_coverage = (total_lines_all > 0) ? ((static_cast<double>(total_covered_lines) / total_lines_all) * 100.0) : 0.0;
    report.overall_branch_coverage = (total_branches_all > 0) ? ((static_cast<double>(total_covered_branches) / total_branches_all) * 100.0) : 0.0;
    report.overall_function_coverage = (total_funcs_all > 0) ? ((static_cast<double>(total_covered_funcs) / total_funcs_all) * 100.0) : 0.0;

    return report;
}

TestingResult<CoverageReport> CoverageAnalyzer::parseCoberturaXml(const std::string& xml_content) {
    CoverageReport report;
    report.format = "cobertura";

    if (xml_content.empty()) {
        return std::unexpected("Empty Cobertura XML content");
    }

    // Extract line-rate and branch-rate from root coverage tag
    static const std::regex root_regex(R"(<coverage[^>]*line-rate=["']([0-9.]+)["'][^>]*branch-rate=["']([0-9.]+)["'])");
    std::smatch root_match;
    if (std::regex_search(xml_content, root_match, root_regex)) {
        try {
            report.overall_line_coverage = std::stod(root_match[1].str()) * 100.0;
            report.overall_branch_coverage = std::stod(root_match[2].str()) * 100.0;
        } catch (...) {}
    }

    // Match each <class filename="..." ...> ... </class>
    static const std::regex class_regex(R"(<class[^>]*filename=["']([^"']+)["'][^>]*>([\s\S]*?)</class>)");
    static const std::regex line_regex(R"(<line[^>]*number=["'](\d+)["'][^>]*hits=["'](\d+)["'])");

    auto words_begin = std::sregex_iterator(xml_content.begin(), xml_content.end(), class_regex);
    auto words_end = std::sregex_iterator();

    size_t total_lines_all = 0;
    size_t total_covered_lines = 0;

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string filename = match[1].str();
        std::string class_body = match[2].str();

        FileCoverage fc;
        fc.file_path = filename;
        std::unordered_map<int, int> line_hits;

        auto lines_begin = std::sregex_iterator(class_body.begin(), class_body.end(), line_regex);
        auto lines_end = std::sregex_iterator();

        for (std::sregex_iterator l_it = lines_begin; l_it != lines_end; ++l_it) {
            std::smatch l_match = *l_it;
            int line_num = std::stoi(l_match[1].str());
            int hits = std::stoi(l_match[2].str());
            line_hits[line_num] = hits;
            fc.total_lines++;
            if (hits > 0) fc.covered_lines++;
        }

        if (fc.total_lines > 0) {
            fc.line_coverage_percent = (static_cast<double>(fc.covered_lines) / fc.total_lines) * 100.0;
        }
        computeUncoveredRanges(fc, line_hits);

        total_lines_all += fc.total_lines;
        total_covered_lines += fc.covered_lines;

        report.files[filename] = fc;
    }

    report.total_lines = total_lines_all;
    report.covered_lines = total_covered_lines;
    if (report.overall_line_coverage == 0.0 && total_lines_all > 0) {
        report.overall_line_coverage = (static_cast<double>(total_covered_lines) / total_lines_all) * 100.0;
    }

    return report;
}

TestingResult<CoverageReport> CoverageAnalyzer::parseJsonCoverage(const std::string& json_content) {
    CoverageReport report;
    report.format = "json";

    try {
        auto root = nlohmann::json::parse(json_content);

        // Format A: Istanbul / NYC style where keys are file paths
        size_t total_lines_all = 0;
        size_t total_covered_lines = 0;

        if (root.is_object()) {
            for (auto& [key, file_obj] : root.items()) {
                if (key == "total" && file_obj.contains("lines")) {
                    // Summary object
                    if (file_obj["lines"].contains("pct")) {
                        report.overall_line_coverage = file_obj["lines"]["pct"].get<double>();
                    }
                    if (file_obj.contains("branches") && file_obj["branches"].contains("pct")) {
                        report.overall_branch_coverage = file_obj["branches"]["pct"].get<double>();
                    }
                    continue;
                }

                if (file_obj.is_object() && (file_obj.contains("statementMap") || file_obj.contains("s") || file_obj.contains("lines"))) {
                    FileCoverage fc;
                    fc.file_path = file_obj.value("path", key);
                    std::unordered_map<int, int> line_hits;

                    if (file_obj.contains("s") && file_obj["s"].is_object()) {
                        for (auto& [stmt_id, hits_val] : file_obj["s"].items()) {
                            int hits = hits_val.get<int>();
                            int line_num = std::stoi(stmt_id) + 1;
                            line_hits[line_num] = hits;
                            fc.total_lines++;
                            if (hits > 0) fc.covered_lines++;
                        }
                    } else if (file_obj.contains("lines") && file_obj["lines"].is_object()) {
                        fc.total_lines = file_obj["lines"].value("total", 0);
                        fc.covered_lines = file_obj["lines"].value("covered", 0);
                        fc.line_coverage_percent = file_obj["lines"].value("pct", 0.0);
                    }

                    if (fc.total_lines > 0 && fc.line_coverage_percent == 0.0) {
                        fc.line_coverage_percent = (static_cast<double>(fc.covered_lines) / fc.total_lines) * 100.0;
                    }
                    computeUncoveredRanges(fc, line_hits);

                    total_lines_all += fc.total_lines;
                    total_covered_lines += fc.covered_lines;
                    report.files[fc.file_path] = fc;
                }
            }
        }

        report.total_lines = total_lines_all;
        report.covered_lines = total_covered_lines;
        if (report.overall_line_coverage == 0.0 && total_lines_all > 0) {
            report.overall_line_coverage = (static_cast<double>(total_covered_lines) / total_lines_all) * 100.0;
        }

        return report;
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to parse JSON coverage: ") + e.what());
    }
}

CoverageGateResult CoverageAnalyzer::checkThresholds(const CoverageReport& report,
                                                     double min_line_pct,
                                                     double min_branch_pct) const {
    CoverageGateResult result;
    result.required_line_percent = min_line_pct;
    result.required_branch_percent = min_branch_pct;
    result.actual_line_percent = report.overall_line_coverage;
    result.actual_branch_percent = report.overall_branch_coverage;

    bool overall_lines_passed = report.overall_line_coverage >= min_line_pct;
    bool overall_branches_passed = report.overall_branch_coverage >= min_branch_pct;

    for (const auto& [path, fc] : report.files) {
        if (fc.total_lines > 0 && fc.line_coverage_percent < min_line_pct) {
            result.deficit_files.push_back(path);
        } else if (fc.total_branches > 0 && fc.branch_coverage_percent < min_branch_pct) {
            result.deficit_files.push_back(path);
        }
    }

    result.passed = (overall_lines_passed && overall_branches_passed && result.deficit_files.empty());
    return result;
}

} // namespace aios::testing
