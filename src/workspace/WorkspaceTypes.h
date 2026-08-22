#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <chrono>
#include <expected>
#include <optional>

namespace aios::workspace {

// Result type alias across workspace subsystem
template<typename T>
using WorkspaceResult = std::expected<T, std::string>;

// Access mode for path containment and sandbox permissions
enum class AccessMode {
    ReadOnly,
    ReadWrite,
    Execute
};

// Merge strategy for branch sandbox
enum class MergeStrategy {
    FastForwardOnly,
    ThreeWay,
    Squash,
    Rebase
};

// Information describing an active or registered git worktree
struct WorktreeInfo {
    std::string id;
    std::string task_id;
    std::filesystem::path path;
    std::string branch;
    std::string head_commit_sha;
    bool is_locked = false;
    bool locked = false; // alias for contract compatibility
    std::string lock_reason;
    bool is_detached = false;
    std::chrono::system_clock::time_point created_at = std::chrono::system_clock::now();
    std::string owner_task_id;
};

// Options for creating a git worktree
struct CreateWorktreeOptions {
    std::string worktree_id;
    std::string branch_name;
    std::string base_ref = "HEAD";
    bool create_new_branch = true;
    bool detach_head = false;
    bool lock_immediately = false;
    std::string lock_reason;
    std::filesystem::path custom_path;
};

// Path containment validation result
struct PathValidationResult {
    bool is_valid = false;
    std::filesystem::path canonical_path;
    std::filesystem::path relative_path;
    bool is_protected = false;
    std::string violation_reason;
};

// Snapshot representation for a single file
struct FileSnapshot {
    std::string relative_path;
    std::string content;
    std::string checksum_sha256;
    size_t size_bytes = 0;
    bool existed_at_snapshot = true;
    std::filesystem::perms permissions = std::filesystem::perms::all;
};

// Checkpoint metadata and tracked files
struct CheckpointInfo {
    std::string id;
    std::string description;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    std::string base_git_commit;
    std::unordered_map<std::string, FileSnapshot> files;
    size_t total_size_bytes = 0;
};

// Lightweight snapshot matching PROJECT.md interface contract
struct Snapshot {
    std::string snapshot_id;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    std::unordered_map<std::string, std::string> file_contents;
};

// Rollback summary report
struct RollbackSummary {
    bool success = false;
    std::string snapshot_id;
    std::vector<std::string> restored_files;
    std::vector<std::string> deleted_created_files;
    std::vector<std::string> failed_files;
    std::string error_message;
};

// Diff hunk structure
struct DiffHunk {
    int old_start = 0;
    int old_lines = 0;
    int new_start = 0;
    int new_lines = 0;
    std::vector<std::string> lines;
};

// File diff representation
struct FileDiff {
    std::string old_path;
    std::string new_path;
    bool is_new_file = false;
    bool is_deleted_file = false;
    std::vector<DiffHunk> hunks;
    int additions = 0;
    int deletions = 0;
};

// Unified diff report
struct UnifiedDiffReport {
    std::string base_snapshot_or_commit;
    std::vector<FileDiff> file_diffs;
    std::string raw_unified_diff;
    int total_additions = 0;
    int total_deletions = 0;
    int total_files_changed = 0;
};

// Conflict chunk inside a file
struct MergeConflictChunk {
    int start_line = 0;
    int end_line = 0;
    std::string our_content;
    std::string their_content;
    std::string base_content;
};

// Conflict file representation
struct MergeConflictFile {
    std::string file_path;
    std::vector<MergeConflictChunk> chunks;
    bool is_binary = false;
};

// Merge operation result
struct MergeResult {
    bool success = false;
    bool has_conflicts = false;
    std::string merge_commit_sha;
    std::string base_branch;
    std::string source_branch;
    std::vector<MergeConflictFile> conflicts;
    std::string error_message;
};

} // namespace aios::workspace

namespace aios {
    using workspace::WorktreeInfo;
    using workspace::CreateWorktreeOptions;
    using workspace::Snapshot;
    using workspace::CheckpointInfo;
    using workspace::FileSnapshot;
    using workspace::RollbackSummary;
    using workspace::UnifiedDiffReport;
    using workspace::FileDiff;
    using workspace::DiffHunk;
    using workspace::MergeStrategy;
    using workspace::MergeResult;
    using workspace::MergeConflictFile;
    using workspace::MergeConflictChunk;
    using workspace::WorkspaceResult;
    using workspace::AccessMode;
    using workspace::PathValidationResult;
}
