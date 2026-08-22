#include "WorkspaceTools.h"
#include <chrono>
#include <nlohmann/json.hpp>

namespace aios::workspace {

WorkspaceTools::WorkspaceTools(std::shared_ptr<WorkspaceManager> manager)
    : manager_(manager ? std::move(manager) : std::shared_ptr<WorkspaceManager>(&WorkspaceManager::instance(), [](WorkspaceManager*){})) {
}

aios::ToolDefinition WorkspaceTools::getDefinition() const {
    aios::ToolDefinition def;
    def.name = "workspace";
    def.description = "Manages sandboxed git worktrees, ephemeral branches, path containment security, snapshots and rollbacks";
    def.category = aios::ToolCategory::Git;
    def.required_permissions = {aios::ToolPermission::Read, aios::ToolPermission::Write};
    def.parameters = {"operation", "task_id", "path", "snapshot_id", "source_branch", "strategy", "force"};
    def.parameter_descriptions["operation"] = "Operation to perform: create_worktree, remove_worktree, list_worktrees, create_snapshot, rollback_snapshot, get_diff, merge_branch, validate_path";
    def.parameter_descriptions["task_id"] = "Identifier for the task or agent worktree";
    def.parameter_descriptions["path"] = "Target file or directory path";
    def.parameter_descriptions["snapshot_id"] = "Snapshot ID for rollback or diff";
    def.parameter_descriptions["source_branch"] = "Source branch for merge";
    def.parameter_descriptions["strategy"] = "Merge strategy: fast_forward, 3way, squash, rebase";
    def.parameter_descriptions["force"] = "Whether to force deletion or removal (true/false)";
    def.parameter_required["operation"] = true;
    def.returns_description = "Status of the workspace operation or json report";
    return def;
}

aios::ToolResult WorkspaceTools::execute(const std::unordered_map<std::string, std::string>& params) {
    auto start_time = std::chrono::steady_clock::now();

    auto op_it = params.find("operation");
    if (op_it == params.end()) {
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        recordCall(false, dur);
        return aios::ToolResult::error("Missing required parameter: operation");
    }

    const std::string& op = op_it->second;
    aios::ToolResult result;

    if (op == "create_worktree") {
        result = createWorktree(params);
    } else if (op == "remove_worktree") {
        result = removeWorktree(params);
    } else if (op == "list_worktrees") {
        result = listWorktrees(params);
    } else if (op == "create_snapshot") {
        result = createSnapshot(params);
    } else if (op == "rollback_snapshot") {
        result = rollbackSnapshot(params);
    } else if (op == "get_diff") {
        result = getDiff(params);
    } else if (op == "merge_branch") {
        result = mergeBranch(params);
    } else if (op == "validate_path") {
        result = validatePath(params);
    } else {
        result = aios::ToolResult::error("Unknown workspace operation: " + op);
    }

    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    result.execution_time = dur;
    recordCall(result.success, dur);
    return result;
}

aios::ToolResult WorkspaceTools::createWorktree(const std::unordered_map<std::string, std::string>& params) {
    auto task_id_it = params.find("task_id");
    if (task_id_it == params.end() || task_id_it->second.empty()) {
        return aios::ToolResult::error("Missing parameter: task_id");
    }

    std::string base_ref = "HEAD";
    auto base_it = params.find("base_ref");
    if (base_it != params.end() && !base_it->second.empty()) {
        base_ref = base_it->second;
    }

    auto res = manager_->allocateIsolatedAgentWorkspace(task_id_it->second, base_ref);
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    nlohmann::json out;
    out["status"] = "success";
    out["id"] = res.value().id;
    out["path"] = res.value().path.string();
    out["branch"] = res.value().branch;
    out["commit"] = res.value().head_commit_sha;

    aios::ToolResult tr = aios::ToolResult::ok(out.dump(2));
    tr.data["path"] = res.value().path.string();
    tr.data["branch"] = res.value().branch;
    return tr;
}

aios::ToolResult WorkspaceTools::removeWorktree(const std::unordered_map<std::string, std::string>& params) {
    auto task_id_it = params.find("task_id");
    if (task_id_it == params.end() || task_id_it->second.empty()) {
        return aios::ToolResult::error("Missing parameter: task_id");
    }

    bool force = true;
    auto force_it = params.find("force");
    if (force_it != params.end() && force_it->second == "false") {
        force = false;
    }

    auto res = manager_->removeWorktree(task_id_it->second, force);
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    return aios::ToolResult::ok("Worktree '" + task_id_it->second + "' successfully removed");
}

aios::ToolResult WorkspaceTools::listWorktrees(const std::unordered_map<std::string, std::string>& /*params*/) {
    auto engine = manager_->getWorktreeEngine();
    if (!engine) {
        return aios::ToolResult::error("Worktree engine not available");
    }

    auto res = engine->listWorktrees();
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& wt : res.value()) {
        nlohmann::json item;
        item["id"] = wt.id;
        item["path"] = wt.path.string();
        item["branch"] = wt.branch;
        item["commit"] = wt.head_commit_sha;
        item["locked"] = wt.is_locked;
        item["lock_reason"] = wt.lock_reason;
        arr.push_back(item);
    }

    return aios::ToolResult::ok(arr.dump(2));
}

aios::ToolResult WorkspaceTools::createSnapshot(const std::unordered_map<std::string, std::string>& params) {
    std::filesystem::path target_path;
    auto p_it = params.find("path");
    if (p_it != params.end() && !p_it->second.empty()) {
        target_path = p_it->second;
    }

    auto res = manager_->createSnapshot(target_path);
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    nlohmann::json out;
    out["status"] = "success";
    out["snapshot_id"] = res.value();

    aios::ToolResult tr = aios::ToolResult::ok(out.dump(2));
    tr.data["snapshot_id"] = res.value();
    return tr;
}

aios::ToolResult WorkspaceTools::rollbackSnapshot(const std::unordered_map<std::string, std::string>& params) {
    auto id_it = params.find("snapshot_id");
    if (id_it == params.end() || id_it->second.empty()) {
        return aios::ToolResult::error("Missing parameter: snapshot_id");
    }

    std::filesystem::path target_path;
    auto p_it = params.find("path");
    if (p_it != params.end() && !p_it->second.empty()) {
        target_path = p_it->second;
    }

    auto res = manager_->rollback(id_it->second, target_path);
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    return aios::ToolResult::ok("Successfully rolled back to snapshot '" + id_it->second + "'");
}

aios::ToolResult WorkspaceTools::getDiff(const std::unordered_map<std::string, std::string>& params) {
    auto id_it = params.find("snapshot_id");
    if (id_it == params.end() || id_it->second.empty()) {
        return aios::ToolResult::error("Missing parameter: snapshot_id");
    }

    std::filesystem::path target_path;
    auto p_it = params.find("path");
    if (p_it != params.end() && !p_it->second.empty()) {
        target_path = p_it->second;
    }

    auto res = manager_->computeDiff(id_it->second, target_path);
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    return aios::ToolResult::ok(res.value());
}

aios::ToolResult WorkspaceTools::mergeBranch(const std::unordered_map<std::string, std::string>& params) {
    auto src_it = params.find("source_branch");
    if (src_it == params.end() || src_it->second.empty()) {
        return aios::ToolResult::error("Missing parameter: source_branch");
    }

    MergeStrategy strat = MergeStrategy::ThreeWay;
    auto strat_it = params.find("strategy");
    if (strat_it != params.end()) {
        if (strat_it->second == "fast_forward") strat = MergeStrategy::FastForwardOnly;
        else if (strat_it->second == "squash") strat = MergeStrategy::Squash;
        else if (strat_it->second == "rebase") strat = MergeStrategy::Rebase;
    }

    auto sandbox = manager_->getBranchSandbox();
    if (!sandbox) {
        return aios::ToolResult::error("Branch sandbox not available");
    }

    auto res = sandbox->merge(src_it->second, strat);
    if (!res.has_value()) {
        return aios::ToolResult::error(res.error());
    }

    nlohmann::json out;
    out["success"] = res.value().success;
    out["has_conflicts"] = res.value().has_conflicts;
    out["merge_commit_sha"] = res.value().merge_commit_sha;
    out["error_message"] = res.value().error_message;

    return aios::ToolResult::ok(out.dump(2));
}

aios::ToolResult WorkspaceTools::validatePath(const std::unordered_map<std::string, std::string>& params) {
    auto p_it = params.find("path");
    if (p_it == params.end() || p_it->second.empty()) {
        return aios::ToolResult::error("Missing parameter: path");
    }

    AccessMode mode = AccessMode::ReadWrite;
    auto m_it = params.find("mode");
    if (m_it != params.end() && m_it->second == "read") {
        mode = AccessMode::ReadOnly;
    }

    auto cont = manager_->getPathContainment();
    if (!cont) {
        return aios::ToolResult::error("PathContainment not available");
    }

    auto val = cont->validate(p_it->second, mode);
    nlohmann::json out;
    out["is_valid"] = val.is_valid;
    out["is_protected"] = val.is_protected;
    out["canonical_path"] = val.canonical_path.string();
    out["relative_path"] = val.relative_path.string();
    out["violation_reason"] = val.violation_reason;

    return aios::ToolResult::ok(out.dump(2));
}

bool WorkspaceTools::registerTools(aios::ToolRegistry& registry, std::shared_ptr<WorkspaceManager> manager) {
    return registry.registerTool("workspace", std::make_shared<WorkspaceTools>(manager));
}

} // namespace aios::workspace
