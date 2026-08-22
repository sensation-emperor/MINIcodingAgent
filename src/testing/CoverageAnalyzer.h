#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "TestingTypes.h"

namespace aios::testing {

class CoverageAnalyzer {
public:
    CoverageAnalyzer() = default;
    ~CoverageAnalyzer() = default;

    TestingResult<CoverageReport> parseLcovInfo(const std::string& lcov_content);
    TestingResult<CoverageReport> parseCoberturaXml(const std::string& xml_content);
    TestingResult<CoverageReport> parseJsonCoverage(const std::string& json_content);

    CoverageGateResult checkThresholds(const CoverageReport& report,
                                       double min_line_pct = 80.0,
                                       double min_branch_pct = 70.0) const;

private:
    void computeUncoveredRanges(FileCoverage& fc, const std::unordered_map<int, int>& line_hits) const;
};

} // namespace aios::testing
