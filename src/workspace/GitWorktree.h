#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <unordered_map>
#include "WorkspaceTypes.h"

namespace aios::workspace {

class GitWorktree {
public:
    explicit GitWorktree(std::filesystem::path repository_root);
    ~GitWorktree();

    // Worktree Lifecycle
    WorkspaceResult<WorktreeInfo> createWorktree(const CreateWorktreeOptions& options);
    WorkspaceResult<bool> removeWorktree(const std::string& worktree_id_or_path, bool force = true);
    WorkspaceResult<bool> lockWorktree(const std::string& worktree_id_or_path, const std::string& reason = "");
    WorkspaceResult<bool> unlockWorktree(const std::string& worktree_id_or_path);
    WorkspaceResult<size_t> pruneWorktrees(const std::string& expire = "now");
    WorkspaceResult<bool> repairWorktrees();

    // Querying
    WorkspaceResult<std::vector<WorktreeInfo>> listWorktrees() const;
    std::optional<WorktreeInfo> getWorktree(const std::string& worktree_id) const;
    bool hasWorktree(const std::string& worktree_id) const;

    // Path Accessors
    std::filesystem::path getRepositoryRoot() const { return repository_root_; }
    std::filesystem::path getWorktreesRoot() const { return worktrees_root_; }

private:
    std::filesystem::path repository_root_;
    std::filesystem::path worktrees_root_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, WorktreeInfo> active_worktrees_;

    WorkspaceResult<std::string> executeGitCommand(const std::vector<std::string>& args, 
                                                   const std::filesystem::path& working_dir = {}) const;
    std::vector<WorktreeInfo> parsePorcelainWorktreeList(const std::string& output) const;
    void refreshActiveWorktreesUnlocked();
    std::filesystem::path resolveWorktreePath(const std::string& worktree_id_or_path) const;
};

} // namespace aios::workspace
