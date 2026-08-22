#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <optional>
#include "WorkspaceTypes.h"
#include "PathContainment.h"

namespace aios::workspace {

class SnapshotManager {
public:
    SnapshotManager(std::filesystem::path workspace_root, std::shared_ptr<PathContainment> containment);
    ~SnapshotManager() = default;

    // Checkpoint Creation & Management
    WorkspaceResult<CheckpointInfo> createSnapshot(const std::string& snapshot_id, 
                                                  const std::string& description = "",
                                                  const std::vector<std::string>& files_to_track = {});
    WorkspaceResult<bool> deleteSnapshot(const std::string& snapshot_id);
    std::optional<CheckpointInfo> getSnapshot(const std::string& snapshot_id) const;
    std::vector<CheckpointInfo> listSnapshots() const;
    void clearSnapshots();

    // Rollback Capabilities
    WorkspaceResult<RollbackSummary> rollbackSnapshot(const std::string& snapshot_id);
    WorkspaceResult<bool> rollbackFile(const std::string& snapshot_id, const std::string& relative_path);

    // Diff Generation
    WorkspaceResult<UnifiedDiffReport> generateDiffFromSnapshot(const std::string& snapshot_id) const;
    WorkspaceResult<UnifiedDiffReport> generateDiffBetweenSnapshots(const std::string& old_snapshot_id, 
                                                                    const std::string& new_snapshot_id) const;

    // Diff computation between two text buffers
    UnifiedDiffReport computeDiffBetweenContents(const std::string& old_path, const std::string& old_content,
                                                 const std::string& new_path, const std::string& new_content) const;

    std::filesystem::path getWorkspaceRoot() const { return workspace_root_; }

private:
    std::filesystem::path workspace_root_;
    std::shared_ptr<PathContainment> containment_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CheckpointInfo> snapshots_;

    std::string computeSha256(const std::string& data) const;
    void saveDiskShadow(const CheckpointInfo& cp);
    bool loadDiskShadow(const std::string& snapshot_id, CheckpointInfo& cp) const;
    std::vector<std::string> scanWorkspaceFiles() const;
    std::string readFileContent(const std::filesystem::path& full_path) const;
    bool writeFileContent(const std::filesystem::path& full_path, const std::string& content, std::filesystem::perms perms) const;
};

} // namespace aios::workspace
