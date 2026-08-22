#include "workspace/WorkspaceManager.h"
#include "logging/Logger.h"

#include <sstream>
#include <array>
#include <chrono>
#include <random>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#define popen  _popen
#define pclose _pclose
#endif

namespace aios {

namespace {

std::string exec_cmd(const std::string& cmd) {
    std::array<char, 512> buf;
    std::string result;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string timestamp_id() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::mt19937_64 rng(static_cast<uint64_t>(ms));
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFF);
    std::ostringstream oss;
    oss << ms << "_" << std::hex << dist(rng);
    return oss.str();
}

} // namespace

// ─────────────────────────────────────────────────────────────
// WorkspaceManager
// ─────────────────────────────────────────────────────────────

WorkspaceManager::WorkspaceManager(WorkspaceConfig config) : config_(std::move(config)) {
    if (config_.worktree_root_dir.empty()) {
        config_.worktree_root_dir = config_.repo_path + "/.aios_worktrees";
    }
}

WorkspaceManager::~WorkspaceManager() {
    if (config_.auto_cleanup) {
        for (auto& sb : sandboxes_) {
            if (!sb.worktree_path.empty()) {
                destroySandbox(sb.worktree_path);
            }
        }
    }
}

bool WorkspaceManager::initialize() {
    // Validate git repo
    std::string check = runGit("rev-parse --is-inside-work-tree", config_.repo_path);
    if (check != "true") {
        LOG_ERROR("WorkspaceManager: [{}] is not inside a Git repository.", config_.repo_path);
        return false;
    }

    // Ensure worktree root exists
    std::filesystem::create_directories(config_.worktree_root_dir);

    // Verify base branch exists
    std::string branch_check = runGit("branch --list " + config_.base_branch, config_.repo_path);
    if (branch_check.empty()) {
        // Try to use current branch
        std::string cur_branch = runGit("rev-parse --abbrev-ref HEAD", config_.repo_path);
        if (!cur_branch.empty()) {
            config_.base_branch = cur_branch;
            LOG_INFO("WorkspaceManager: Base branch not found, using current: {}", cur_branch);
        }
    }

    LOG_INFO("WorkspaceManager: Initialized (repo={}, base={})", config_.repo_path, config_.base_branch);
    return true;
}

std::string WorkspaceManager::worktreeRoot() const {
    return config_.worktree_root_dir;
}

std::string WorkspaceManager::generateSandboxBranch(const std::string& task_id) const {
    return config_.sandbox_prefix + task_id + "_" + timestamp_id().substr(0, 8);
}

std::string WorkspaceManager::generateCheckpointId() const {
    return "ckpt_" + timestamp_id();
}

std::string WorkspaceManager::runGit(const std::string& args, const std::string& cwd) const {
    std::string working = cwd.empty() ? config_.repo_path : cwd;
    std::string cmd = "git -C \"" + working + "\" " + args;
    return exec_cmd(cmd);
}

bool WorkspaceManager::gitSucceeded(const std::string& args, const std::string& cwd) const {
    std::string working = cwd.empty() ? config_.repo_path : cwd;
    std::string cmd = "git -C \"" + working + "\" " + args + " > /dev/null 2>&1";
#ifdef _WIN32
    cmd = "git -C \"" + working + "\" " + args + " > NUL 2>&1";
#endif
    return std::system(cmd.c_str()) == 0;
}

std::optional<WorkspaceCheckpoint> WorkspaceManager::createSandbox(
        const std::string& task_id, const std::string& description) {

    std::string branch_name = generateSandboxBranch(task_id);
    std::string worktree_path = worktreeRoot() + "/" + task_id;

    // Replace path separators
    std::replace(worktree_path.begin(), worktree_path.end(), ':', '_');

    // Create branch from HEAD then add worktree
    std::string create_result = runGit(
        "worktree add -b \"" + branch_name + "\" \"" + worktree_path + "\" " + config_.base_branch
    );

    if (create_result.find("Preparing") == std::string::npos &&
        !std::filesystem::exists(worktree_path)) {
        LOG_ERROR("WorkspaceManager: Failed to create worktree at [{}]: {}", worktree_path, create_result);
        return std::nullopt;
    }

    std::string commit_hash = runGit("rev-parse HEAD", worktree_path);

    WorkspaceCheckpoint ckpt;
    ckpt.id             = generateCheckpointId();
    ckpt.commit_hash    = commit_hash;
    ckpt.branch_name    = branch_name;
    ckpt.worktree_path  = worktree_path;
    ckpt.created_at     = std::chrono::system_clock::now();
    ckpt.description    = description.empty() ? "Sandbox for " + task_id : description;

    sandboxes_.push_back(ckpt);
    LOG_INFO("WorkspaceManager: Created sandbox [{}] at [{}]", branch_name, worktree_path);
    return ckpt;
}

std::optional<WorkspaceCheckpoint> WorkspaceManager::checkpoint(
        const std::string& sandbox_path, const std::string& description) {

    // Stage all changes and commit
    runGit("add -A", sandbox_path);
    std::string msg = description.empty() ? "AIOS checkpoint" : description;
    std::string result = runGit("commit -m \"" + msg + "\" --allow-empty", sandbox_path);
    std::string commit_hash = runGit("rev-parse HEAD", sandbox_path);
    std::string branch_name = runGit("rev-parse --abbrev-ref HEAD", sandbox_path);

    WorkspaceCheckpoint ckpt;
    ckpt.id            = generateCheckpointId();
    ckpt.commit_hash   = commit_hash;
    ckpt.branch_name   = branch_name;
    ckpt.worktree_path = sandbox_path;
    ckpt.created_at    = std::chrono::system_clock::now();
    ckpt.description   = msg;

    LOG_INFO("WorkspaceManager: Checkpoint [{}] created at {}", ckpt.id, commit_hash);
    return ckpt;
}

bool WorkspaceManager::rollback(const std::string& sandbox_path, const std::string& checkpoint_id) {
    if (checkpoint_id.empty()) {
        // Hard reset to last clean commit — discard all working tree changes
        runGit("checkout -- .", sandbox_path);
        runGit("clean -fd", sandbox_path);
        LOG_INFO("WorkspaceManager: Rolled back [{}] to clean HEAD state.", sandbox_path);
        return true;
    }

    // Find commit hash for checkpoint_id
    for (const auto& ckpt : sandboxes_) {
        if (ckpt.id == checkpoint_id && ckpt.worktree_path == sandbox_path) {
            std::string result = runGit("reset --hard " + ckpt.commit_hash, sandbox_path);
            LOG_INFO("WorkspaceManager: Rolled back [{}] to checkpoint [{}] ({})",
                     sandbox_path, checkpoint_id, ckpt.commit_hash);
            return result.find("HEAD is now") != std::string::npos;
        }
    }

    LOG_WARN("WorkspaceManager: Checkpoint [{}] not found for path [{}]", checkpoint_id, sandbox_path);
    return false;
}

bool WorkspaceManager::applySandboxToBase(const std::string& sandbox_path,
                                         const std::string& commit_msg) {
    // Stage and commit any remaining changes in sandbox
    runGit("add -A", sandbox_path);
    std::string msg = commit_msg.empty() ? "AIOS: apply sandbox changes" : commit_msg;
    runGit("commit -m \"" + msg + "\" --allow-empty", sandbox_path);

    std::string sandbox_branch = runGit("rev-parse --abbrev-ref HEAD", sandbox_path);
    std::string sandbox_commit = runGit("rev-parse HEAD", sandbox_path);

    // Switch to base and cherry-pick the sandbox tip
    bool ok = gitSucceeded("cherry-pick " + sandbox_commit, config_.repo_path);
    if (!ok) {
        LOG_WARN("WorkspaceManager: cherry-pick failed for {}; attempting merge.", sandbox_branch);
        std::string merge = runGit("merge --no-ff \"" + sandbox_branch + "\" -m \"" + msg + "\"",
                                   config_.repo_path);
        return merge.find("conflict") == std::string::npos;
    }

    LOG_INFO("WorkspaceManager: Applied sandbox [{}] to base branch [{}].",
             sandbox_branch, config_.base_branch);
    return true;
}

WorkspaceDiff WorkspaceManager::getDiff(const std::string& sandbox_path,
                                       const std::string& against_branch) const {
    std::string base = against_branch.empty() ? config_.base_branch : against_branch;
    WorkspaceDiff diff;
    diff.branch_to   = base;
    diff.branch_from = runGit("rev-parse --abbrev-ref HEAD", sandbox_path);

    diff.diff_text = runGit("diff " + base + "..HEAD", sandbox_path);

    // Count stats
    std::string stat = runGit("diff --stat " + base + "..HEAD", sandbox_path);
    std::istringstream ss(stat);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("changed") != std::string::npos) {
            // Parse "N files changed, N insertions(+), N deletions(-)"
            std::istringstream ls(line);
            int n;
            std::string tok;
            while (ls >> n >> tok) {
                if (tok.find("file") != std::string::npos) diff.files_changed = n;
                else if (tok.find("insertion") != std::string::npos) diff.insertions = n;
                else if (tok.find("deletion") != std::string::npos) diff.deletions = n;
            }
        }
    }

    return diff;
}

bool WorkspaceManager::destroySandbox(const std::string& sandbox_path) {
    std::string branch_name = runGit("rev-parse --abbrev-ref HEAD", sandbox_path);

    // Remove worktree
    bool ok = gitSucceeded("worktree remove --force \"" + sandbox_path + "\"");
    if (!ok) {
        // Force filesystem removal
        std::filesystem::remove_all(sandbox_path);
    }

    // Delete the branch
    gitSucceeded("branch -D \"" + branch_name + "\"");
    gitSucceeded("worktree prune");

    // Remove from registry
    sandboxes_.erase(std::remove_if(sandboxes_.begin(), sandboxes_.end(),
        [&](const WorkspaceCheckpoint& c) { return c.worktree_path == sandbox_path; }),
        sandboxes_.end());

    LOG_INFO("WorkspaceManager: Destroyed sandbox [{}]", sandbox_path);
    return true;
}

std::vector<WorkspaceCheckpoint> WorkspaceManager::listSandboxes() const {
    return sandboxes_;
}

// ─────────────────────────────────────────────────────────────
// SandboxScope (RAII)
// ─────────────────────────────────────────────────────────────

SandboxScope::SandboxScope(WorkspaceManager& manager, const std::string& task_id, 
                           const std::string& desc)
    : manager_(manager) {
    checkpoint_ = manager_.createSandbox(task_id, desc);
}

SandboxScope::~SandboxScope() {
    if (checkpoint_.has_value()) {
        manager_.destroySandbox(checkpoint_->worktree_path);
    }
}

bool SandboxScope::commit(const std::string& description) {
    if (!checkpoint_) return false;
    auto new_ckpt = manager_.checkpoint(checkpoint_->worktree_path, description);
    if (new_ckpt) {
        checkpoint_ = new_ckpt;
        return true;
    }
    return false;
}

bool SandboxScope::rollback(const std::string& checkpoint_id) {
    if (!checkpoint_) return false;
    return manager_.rollback(checkpoint_->worktree_path, checkpoint_id);
}

WorkspaceDiff SandboxScope::diff(const std::string& against_branch) const {
    if (!checkpoint_) return {};
    return manager_.getDiff(checkpoint_->worktree_path, against_branch);
}

} // namespace aios
