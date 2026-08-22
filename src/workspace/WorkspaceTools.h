#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "WorkspaceManager.h"
#include "tools/ToolRegistry.h"

namespace aios::workspace {

class WorkspaceTools : public aios::Tool {
public:
    explicit WorkspaceTools(std::shared_ptr<WorkspaceManager> manager = nullptr);
    ~WorkspaceTools() override = default;

    aios::ToolDefinition getDefinition() const override;
    aios::ToolResult execute(const std::unordered_map<std::string, std::string>& params) override;
    aios::ToolStats getStats() const override { return stats_; }

    static bool registerTools(aios::ToolRegistry& registry, std::shared_ptr<WorkspaceManager> manager);

private:
    std::shared_ptr<WorkspaceManager> manager_;

    aios::ToolResult createWorktree(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult removeWorktree(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult listWorktrees(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult createSnapshot(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult rollbackSnapshot(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult getDiff(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult mergeBranch(const std::unordered_map<std::string, std::string>& params);
    aios::ToolResult validatePath(const std::unordered_map<std::string, std::string>& params);
};

} // namespace aios::workspace
