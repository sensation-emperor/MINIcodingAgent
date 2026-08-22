#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "TestingTypes.h"
#include "TestingManager.h"
#include "tools/ToolRegistry.h"

namespace aios::testing {

class TestingTools : public aios::Tool {
public:
    explicit TestingTools(std::shared_ptr<TestingManager> manager = nullptr);
    ~TestingTools() override = default;

    ToolDefinition getDefinition() const override;
    ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    ToolStats getStats() const override { return stats_; }

    static bool registerTool(std::shared_ptr<TestingManager> manager = nullptr);

private:
    std::shared_ptr<TestingManager> manager_;

    ToolResult handleGenerateTests(const std::unordered_map<std::string, std::string>& params);
    ToolResult handleRunTests(const std::unordered_map<std::string, std::string>& params);
    ToolResult handleDiagnoseCode(const std::unordered_map<std::string, std::string>& params);
    ToolResult handleAnalyzeCoverage(const std::unordered_map<std::string, std::string>& params);
    ToolResult handleAutoRepair(const std::unordered_map<std::string, std::string>& params);
};

} // namespace aios::testing
