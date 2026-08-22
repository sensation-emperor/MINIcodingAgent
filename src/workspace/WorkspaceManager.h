#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>
#include <functional>

namespace aios {

struct WorkspaceConfig {
    std::string repo_path;
    std::string base_branch = "main";
    std::string sandbox_prefix = "aios/sandbox/";
    std::string worktree_root_dir; // defaults to repo_path + "/.aios_worktrees"
    bool auto_cleanup = true;
};

struct WorkspaceCheckpoint {
    std::string id;
    std::string commit_hash;
    std::string branch_name;
    std::string worktree_path;
    std::chrono::system_clock::time_point created_at;
    std::string description;
};

struct WorkspaceDiff {
    std::string branch_from;
    std::string branch_to;
    std::string diff_text;
    size_t files_changed = 0;
    size_t insertions = 0;
    size_t deletions = 0;
};

/**
 * @brief Sandboxed Git Worktree & Multi-Branch Workspace Isolation Manager
 *
 * Provides:
 * - Isolated Git worktrees for safe agent execution
 * - Atomic checkpoint creation (commits) and zero-risk rollback
 * - Diff inspection between sandbox and base branches
 * - Automatic cleanup on scope exit
 */
class WorkspaceManager {
public:
    explicit WorkspaceManager(WorkspaceConfig config);
    ~WorkspaceManager();

    // Initialize — validates git repo and sets up worktree root
    bool initialize();

    // Create an isolated sandbox worktree branched from current HEAD
    std::optional<WorkspaceCheckpoint> createSandbox(const std::string& task_id,
                                                      const std::string& description = "");

    // Commit current sandbox state as a named checkpoint
    std::optional<WorkspaceCheckpoint> checkpoint(const std::string& sandbox_path,
                                                  const std::string& description = "");

    // Rollback sandbox to a prior checkpoint (or clean state)
    bool rollback(const std::string& sandbox_path,
                  const std::string& checkpoint_id = "");

    // Apply sandbox changes back to base branch (merge or cherry-pick)
    bool applySandboxToBase(const std::string& sandbox_path,
                            const std::string& commit_msg = "");

    // Get diff between sandbox branch and base branch
    WorkspaceDiff getDiff(const std::string& sandbox_path,
                         const std::string& against_branch = "") const;

    // Destroy a sandbox worktree (prune)
    bool destroySandbox(const std::string& sandbox_path);

    // List all active sandboxes for this session
    std::vector<WorkspaceCheckpoint> listSandboxes() const;

    // Query
    std::string getRepoPath() const { return config_.repo_path; }
    std::string getBaseBranch() const { return config_.base_branch; }

private:
    std::string runGit(const std::string& args, const std::string& cwd = "") const;
    bool gitSucceeded(const std::string& args, const std::string& cwd = "") const;
    std::string generateSandboxBranch(const std::string& task_id) const;
    std::string generateCheckpointId() const;
    std::string worktreeRoot() const;

    WorkspaceConfig config_;
    std::vector<WorkspaceCheckpoint> sandboxes_;
};

/**
 * @brief RAII sandbox scope guard — auto-destroys worktree on scope exit
 */
class SandboxScope {
public:
    SandboxScope(WorkspaceManager& manager, const std::string& task_id, const std::string& desc = "");
    ~SandboxScope();

    bool valid() const { return checkpoint_.has_value(); }
    const WorkspaceCheckpoint& checkpoint() const { return *checkpoint_; }
    const std::string& path() const { return checkpoint_->worktree_path; }

    bool commit(const std::string& description = "");
    bool rollback(const std::string& checkpoint_id = "");
    WorkspaceDiff diff(const std::string& against_branch = "") const;

private:
    WorkspaceManager& manager_;
    std::optional<WorkspaceCheckpoint> checkpoint_;
};

} // namespace aios
