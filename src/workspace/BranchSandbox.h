#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include "WorkspaceTypes.h"

namespace aios::workspace {

class BranchSandbox {
public:
    explicit BranchSandbox(std::filesystem::path repo_or_worktree_path);
    ~BranchSandbox() = default;

    // Ephemeral Branch Management
    WorkspaceResult<std::string> createEphemeralBranch(const std::string& task_id, const std::string& base_ref = "HEAD");
    WorkspaceResult<bool> checkoutBranch(const std::string& branch_name);
    WorkspaceResult<bool> deleteBranch(const std::string& branch_name, bool force = false);
    WorkspaceResult<std::vector<std::string>> listEphemeralBranches() const;

    // Staging & Micro-Commits
    WorkspaceResult<std::string> stageAndCommit(const std::string& message, 
                                                const std::vector<std::string>& files = {},
                                                const std::unordered_map<std::string, std::string>& metadata = {});
    WorkspaceResult<std::string> createStash(const std::string& message, bool include_untracked = true);
    WorkspaceResult<bool> popStash(int stash_index = 0);
    WorkspaceResult<bool> dropStash(int stash_index = 0);

    // Merge, Rebase & Conflict Handling
    WorkspaceResult<MergeResult> merge(const std::string& source_branch, MergeStrategy strategy = MergeStrategy::ThreeWay);
    WorkspaceResult<MergeResult> rebase(const std::string& upstream_branch);
    WorkspaceResult<bool> abortMerge();
    WorkspaceResult<std::vector<MergeConflictFile>> detectConflicts(const std::string& source_branch, const std::string& target_branch);

    // Diffs
    WorkspaceResult<std::string> getBranchDiff(const std::string& base_branch, const std::string& target_branch) const;
    WorkspaceResult<std::string> getUncommittedDiff() const;

    std::filesystem::path getWorkingPath() const { return working_path_; }

private:
    std::filesystem::path working_path_;
    mutable std::mutex mutex_;

    WorkspaceResult<std::string> runGit(const std::vector<std::string>& args) const;
    std::vector<MergeConflictFile> parseConflictMarkersInFile(const std::filesystem::path& relative_path) const;
    std::vector<MergeConflictFile> collectConflictingFiles() const;
};

} // namespace aios::workspace
