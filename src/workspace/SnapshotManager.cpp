#include "SnapshotManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace aios::workspace {

namespace {

// ==========================================
// SHA-256 Self-Contained Implementation
// ==========================================

uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t sig0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

uint32_t sig1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

uint32_t gam0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

uint32_t gam1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::string sha256String(const std::string& input) {
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    std::vector<uint8_t> data(input.begin(), input.end());
    uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8;

    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t W[64];
        for (int t = 0; t < 16; ++t) {
            W[t] = (static_cast<uint32_t>(data[chunk + t * 4]) << 24) |
                   (static_cast<uint32_t>(data[chunk + t * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(data[chunk + t * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(data[chunk + t * 4 + 3]));
        }
        for (int t = 16; t < 64; ++t) {
            W[t] = gam1(W[t - 2]) + W[t - 7] + gam0(W[t - 15]) + W[t - 16];
        }

        uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];

        for (int t = 0; t < 64; ++t) {
            uint32_t T1 = h + sig1(e) + ch(e, f, g) + K[t] + W[t];
            uint32_t T2 = sig0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    std::ostringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << std::setw(8) << std::setfill('0') << H[i];
    }
    return ss.str();
}

// Helper: Split string into lines
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line.push_back(c);
        }
    }
    if (!line.empty() || (!text.empty() && text.back() == '\n')) {
        lines.push_back(line);
    }
    return lines;
}

// LCS-based Diff algorithm producing unified diff hunks
enum class EditType { Unchanged, Added, Deleted };

struct EditItem {
    EditType type;
    std::string text;
    int old_line_idx; // 1-indexed
    int new_line_idx; // 1-indexed
};

std::vector<EditItem> computeDiffEdits(const std::vector<std::string>& old_lines,
                                      const std::vector<std::string>& new_lines) {
    const size_t n = old_lines.size();
    const size_t m = new_lines.size();

    // Standard DP matrix for LCS
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (old_lines[i - 1] == new_lines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    std::vector<EditItem> edits;
    size_t i = n, j = m;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && old_lines[i - 1] == new_lines[j - 1]) {
            edits.push_back({EditType::Unchanged, old_lines[i - 1], static_cast<int>(i), static_cast<int>(j)});
            --i;
            --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            edits.push_back({EditType::Added, new_lines[j - 1], 0, static_cast<int>(j)});
            --j;
        } else if (i > 0 && (j == 0 || dp[i][j - 1] < dp[i - 1][j])) {
            edits.push_back({EditType::Deleted, old_lines[i - 1], static_cast<int>(i), 0});
            --i;
        }
    }
    std::reverse(edits.begin(), edits.end());
    return edits;
}

std::vector<DiffHunk> buildHunks(const std::vector<EditItem>& edits, int context = 3) {
    std::vector<DiffHunk> hunks;
    if (edits.empty()) return hunks;

    // Identify indices of changed edits
    std::vector<size_t> changed_indices;
    for (size_t k = 0; k < edits.size(); ++k) {
        if (edits[k].type != EditType::Unchanged) {
            changed_indices.push_back(k);
        }
    }
    if (changed_indices.empty()) return hunks;

    // Group changed indices into clusters within 2 * context of each other
    std::vector<std::pair<size_t, size_t>> clusters;
    size_t cluster_start = changed_indices[0];
    size_t cluster_end = changed_indices[0];

    for (size_t k = 1; k < changed_indices.size(); ++k) {
        if (changed_indices[k] - cluster_end <= static_cast<size_t>(2 * context)) {
            cluster_end = changed_indices[k];
        } else {
            clusters.emplace_back(cluster_start, cluster_end);
            cluster_start = changed_indices[k];
            cluster_end = changed_indices[k];
        }
    }
    clusters.emplace_back(cluster_start, cluster_end);

    // Build each hunk with context
    for (const auto& [c_start, c_end] : clusters) {
        size_t h_start = (c_start >= static_cast<size_t>(context)) ? (c_start - context) : 0;
        size_t h_end = std::min(edits.size() - 1, c_end + context);

        DiffHunk hunk;
        int old_line_start = -1;
        int new_line_start = -1;
        int old_count = 0;
        int new_count = 0;

        for (size_t idx = h_start; idx <= h_end; ++idx) {
            const auto& edit = edits[idx];
            if (edit.type == EditType::Unchanged) {
                if (old_line_start == -1) old_line_start = edit.old_line_idx;
                if (new_line_start == -1) new_line_start = edit.new_line_idx;
                old_count++;
                new_count++;
                hunk.lines.push_back(" " + edit.text);
            } else if (edit.type == EditType::Deleted) {
                if (old_line_start == -1) old_line_start = edit.old_line_idx;
                old_count++;
                hunk.lines.push_back("-" + edit.text);
            } else if (edit.type == EditType::Added) {
                if (new_line_start == -1) new_line_start = edit.new_line_idx;
                new_count++;
                hunk.lines.push_back("+" + edit.text);
            }
        }

        hunk.old_start = (old_line_start == -1) ? 1 : old_line_start;
        hunk.old_lines = old_count;
        hunk.new_start = (new_line_start == -1) ? 1 : new_line_start;
        hunk.new_lines = new_count;

        hunks.push_back(std::move(hunk));
    }

    return hunks;
}

} // anonymous namespace

SnapshotManager::SnapshotManager(std::filesystem::path workspace_root, std::shared_ptr<PathContainment> containment)
    : workspace_root_(std::move(workspace_root)), containment_(std::move(containment)) {
}

std::string SnapshotManager::computeSha256(const std::string& data) const {
    return sha256String(data);
}

std::string SnapshotManager::readFileContent(const std::filesystem::path& full_path) const {
    std::ifstream file(full_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool SnapshotManager::writeFileContent(const std::filesystem::path& full_path, const std::string& content, std::filesystem::perms perms) const {
    std::error_code ec;
    if (full_path.has_parent_path()) {
        std::filesystem::create_directories(full_path.parent_path(), ec);
    }
    std::ofstream file(full_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.close();
    std::filesystem::permissions(full_path, perms, std::filesystem::perm_options::replace, ec);
    return true;
}

std::vector<std::string> SnapshotManager::scanWorkspaceFiles() const {
    std::vector<std::string> result;
    std::error_code ec;
    if (!std::filesystem::exists(workspace_root_, ec)) {
        return result;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace_root_, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec)) {
            std::filesystem::path rel = std::filesystem::relative(entry.path(), workspace_root_, ec);
            std::string rel_str = rel.generic_string();

            // Skip internal git and aios metadata directories and build dirs
            if (rel_str.starts_with(".git") || rel_str.starts_with(".aios") || 
                rel_str.starts_with("build/") || rel_str.starts_with(".vs/")) {
                continue;
            }

            result.push_back(rel_str);
        }
    }
    return result;
}

void SnapshotManager::saveDiskShadow(const CheckpointInfo& cp) {
    std::error_code ec;
    std::filesystem::path snap_dir = workspace_root_ / ".aios" / "snapshots" / cp.id;
    std::filesystem::path files_dir = snap_dir / "files";
    std::filesystem::create_directories(files_dir, ec);

    nlohmann::json manifest;
    manifest["id"] = cp.id;
    manifest["description"] = cp.description;
    manifest["timestamp_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        cp.timestamp.time_since_epoch()).count();
    manifest["base_git_commit"] = cp.base_git_commit;
    manifest["total_size_bytes"] = cp.total_size_bytes;

    nlohmann::json files_arr = nlohmann::json::array();
    for (const auto& [rel_path, file_snap] : cp.files) {
        nlohmann::json f;
        f["relative_path"] = file_snap.relative_path;
        f["checksum_sha256"] = file_snap.checksum_sha256;
        f["size_bytes"] = file_snap.size_bytes;
        f["existed_at_snapshot"] = file_snap.existed_at_snapshot;
        files_arr.push_back(f);

        // Save file copy to disk shadow
        std::filesystem::path disk_target = files_dir / rel_path;
        writeFileContent(disk_target, file_snap.content, file_snap.permissions);
    }
    manifest["files"] = files_arr;

    std::filesystem::path manifest_path = snap_dir / "manifest.json";
    writeFileContent(manifest_path, manifest.dump(2), std::filesystem::perms::all);
}

WorkspaceResult<CheckpointInfo> SnapshotManager::createSnapshot(const std::string& snapshot_id,
                                                               const std::string& description,
                                                               const std::vector<std::string>& files_to_track) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (snapshot_id.empty()) {
        return std::unexpected("Snapshot ID cannot be empty");
    }

    CheckpointInfo cp;
    cp.id = snapshot_id;
    cp.description = description;
    cp.timestamp = std::chrono::system_clock::now();

    std::vector<std::string> target_files = files_to_track;
    if (target_files.empty()) {
        target_files = scanWorkspaceFiles();
    }

    size_t total_size = 0;
    std::error_code ec;

    for (const auto& rel_path : target_files) {
        std::filesystem::path full_path = workspace_root_ / rel_path;
        FileSnapshot fsnap;
        fsnap.relative_path = rel_path;

        if (std::filesystem::exists(full_path, ec)) {
            fsnap.content = readFileContent(full_path);
            fsnap.size_bytes = fsnap.content.size();
            fsnap.checksum_sha256 = computeSha256(fsnap.content);
            fsnap.existed_at_snapshot = true;
            fsnap.permissions = std::filesystem::status(full_path, ec).permissions();
            total_size += fsnap.size_bytes;
        } else {
            fsnap.content = "";
            fsnap.size_bytes = 0;
            fsnap.checksum_sha256 = computeSha256("");
            fsnap.existed_at_snapshot = false;
        }

        cp.files[rel_path] = std::move(fsnap);
    }

    cp.total_size_bytes = total_size;
    snapshots_[snapshot_id] = cp;
    saveDiskShadow(cp);

    return cp;
}

WorkspaceResult<bool> SnapshotManager::deleteSnapshot(const std::string& snapshot_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = snapshots_.find(snapshot_id);
    if (it != snapshots_.end()) {
        snapshots_.erase(it);
    }

    std::error_code ec;
    std::filesystem::path snap_dir = workspace_root_ / ".aios" / "snapshots" / snapshot_id;
    if (std::filesystem::exists(snap_dir, ec)) {
        std::filesystem::remove_all(snap_dir, ec);
    }

    return true;
}

std::optional<CheckpointInfo> SnapshotManager::getSnapshot(const std::string& snapshot_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = snapshots_.find(snapshot_id);
    if (it != snapshots_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<CheckpointInfo> SnapshotManager::listSnapshots() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CheckpointInfo> result;
    result.reserve(snapshots_.size());
    for (const auto& [_, cp] : snapshots_) {
        result.push_back(cp);
    }
    std::sort(result.begin(), result.end(), [](const CheckpointInfo& a, const CheckpointInfo& b) {
        return a.timestamp < b.timestamp;
    });
    return result;
}

void SnapshotManager::clearSnapshots() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshots_.clear();
    std::error_code ec;
    std::filesystem::path snaps_dir = workspace_root_ / ".aios" / "snapshots";
    std::filesystem::remove_all(snaps_dir, ec);
}

WorkspaceResult<RollbackSummary> SnapshotManager::rollbackSnapshot(const std::string& snapshot_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = snapshots_.find(snapshot_id);
    if (it == snapshots_.end()) {
        return std::unexpected("Snapshot '" + snapshot_id + "' not found");
    }

    const CheckpointInfo& cp = it->second;
    RollbackSummary summary;
    summary.snapshot_id = snapshot_id;
    summary.success = true;

    // 1. Delete files created on disk that were not tracked in the snapshot
    std::vector<std::string> current_files = scanWorkspaceFiles();
    std::error_code ec;

    for (const auto& cur_rel : current_files) {
        if (cp.files.find(cur_rel) == cp.files.end()) {
            std::filesystem::path cur_path = workspace_root_ / cur_rel;
            if (std::filesystem::remove(cur_path, ec)) {
                summary.deleted_created_files.push_back(cur_rel);
            } else if (ec) {
                summary.failed_files.push_back(cur_rel);
                summary.success = false;
            }
        }
    }

    // 2. Restore all tracked files to exact snapshot state
    for (const auto& [rel_path, fsnap] : cp.files) {
        std::filesystem::path full_path = workspace_root_ / rel_path;
        if (fsnap.existed_at_snapshot) {
            if (writeFileContent(full_path, fsnap.content, fsnap.permissions)) {
                summary.restored_files.push_back(rel_path);
            } else {
                summary.failed_files.push_back(rel_path);
                summary.success = false;
            }
        } else {
            // File did not exist at snapshot, delete if exists
            if (std::filesystem::exists(full_path, ec)) {
                std::filesystem::remove(full_path, ec);
                summary.deleted_created_files.push_back(rel_path);
            }
        }
    }

    return summary;
}

WorkspaceResult<bool> SnapshotManager::rollbackFile(const std::string& snapshot_id, const std::string& relative_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = snapshots_.find(snapshot_id);
    if (it == snapshots_.end()) {
        return std::unexpected("Snapshot '" + snapshot_id + "' not found");
    }

    const CheckpointInfo& cp = it->second;
    auto file_it = cp.files.find(relative_path);
    if (file_it == cp.files.end()) {
        return std::unexpected("File '" + relative_path + "' not found in snapshot '" + snapshot_id + "'");
    }

    const FileSnapshot& fsnap = file_it->second;
    std::filesystem::path full_path = workspace_root_ / relative_path;
    std::error_code ec;

    if (fsnap.existed_at_snapshot) {
        if (!writeFileContent(full_path, fsnap.content, fsnap.permissions)) {
            return std::unexpected("Failed to write restored file content for '" + relative_path + "'");
        }
    } else {
        if (std::filesystem::exists(full_path, ec)) {
            std::filesystem::remove(full_path, ec);
        }
    }

    return true;
}

UnifiedDiffReport SnapshotManager::computeDiffBetweenContents(const std::string& old_path, const std::string& old_content,
                                                             const std::string& new_path, const std::string& new_content) const {
    UnifiedDiffReport report;
    std::vector<std::string> old_lines = splitLines(old_content);
    std::vector<std::string> new_lines = splitLines(new_content);

    if (old_lines == new_lines) {
        return report;
    }

    std::vector<EditItem> edits = computeDiffEdits(old_lines, new_lines);
    std::vector<DiffHunk> hunks = buildHunks(edits, 3);

    FileDiff fd;
    fd.old_path = old_path;
    fd.new_path = new_path;
    fd.is_new_file = old_content.empty() && !new_content.empty();
    fd.is_deleted_file = !old_content.empty() && new_content.empty();

    std::ostringstream diff_stream;
    diff_stream << "--- a/" << old_path << "\n";
    diff_stream << "+++ b/" << new_path << "\n";

    for (const auto& hunk : hunks) {
        diff_stream << "@@ -" << hunk.old_start << "," << hunk.old_lines
                    << " +" << hunk.new_start << "," << hunk.new_lines << " @@\n";
        for (const auto& line : hunk.lines) {
            diff_stream << line << "\n";
            if (!line.empty() && line[0] == '+') {
                fd.additions++;
                report.total_additions++;
            } else if (!line.empty() && line[0] == '-') {
                fd.deletions++;
                report.total_deletions++;
            }
        }
    }

    fd.hunks = std::move(hunks);
    report.file_diffs.push_back(fd);
    report.total_files_changed = 1;
    report.raw_unified_diff = diff_stream.str();

    return report;
}

WorkspaceResult<UnifiedDiffReport> SnapshotManager::generateDiffFromSnapshot(const std::string& snapshot_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = snapshots_.find(snapshot_id);
    if (it == snapshots_.end()) {
        return std::unexpected("Snapshot '" + snapshot_id + "' not found");
    }

    const CheckpointInfo& cp = it->second;
    UnifiedDiffReport full_report;
    full_report.base_snapshot_or_commit = snapshot_id;

    std::vector<std::string> cur_files = scanWorkspaceFiles();
    std::unordered_map<std::string, bool> processed;

    std::ostringstream all_diffs;

    // Check files that existed in snapshot
    for (const auto& [rel_path, fsnap] : cp.files) {
        processed[rel_path] = true;
        std::filesystem::path cur_path = workspace_root_ / rel_path;
        std::string cur_content;
        std::error_code ec;
        if (std::filesystem::exists(cur_path, ec)) {
            cur_content = readFileContent(cur_path);
        }

        if (cur_content != fsnap.content) {
            UnifiedDiffReport single = computeDiffBetweenContents(rel_path, fsnap.content, rel_path, cur_content);
            if (!single.file_diffs.empty()) {
                full_report.file_diffs.push_back(single.file_diffs[0]);
                full_report.total_additions += single.total_additions;
                full_report.total_deletions += single.total_deletions;
                full_report.total_files_changed++;
                all_diffs << single.raw_unified_diff;
            }
        }
    }

    // Check newly created files not in snapshot
    for (const auto& cur_rel : cur_files) {
        if (!processed[cur_rel]) {
            std::filesystem::path cur_path = workspace_root_ / cur_rel;
            std::string cur_content = readFileContent(cur_path);
            UnifiedDiffReport single = computeDiffBetweenContents(cur_rel, "", cur_rel, cur_content);
            if (!single.file_diffs.empty()) {
                full_report.file_diffs.push_back(single.file_diffs[0]);
                full_report.total_additions += single.total_additions;
                full_report.total_deletions += single.total_deletions;
                full_report.total_files_changed++;
                all_diffs << single.raw_unified_diff;
            }
        }
    }

    full_report.raw_unified_diff = all_diffs.str();
    return full_report;
}

WorkspaceResult<UnifiedDiffReport> SnapshotManager::generateDiffBetweenSnapshots(const std::string& old_snapshot_id,
                                                                                const std::string& new_snapshot_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto old_it = snapshots_.find(old_snapshot_id);
    if (old_it == snapshots_.end()) {
        return std::unexpected("Snapshot '" + old_snapshot_id + "' not found");
    }
    auto new_it = snapshots_.find(new_snapshot_id);
    if (new_it == snapshots_.end()) {
        return std::unexpected("Snapshot '" + new_snapshot_id + "' not found");
    }

    const CheckpointInfo& old_cp = old_it->second;
    const CheckpointInfo& new_cp = new_it->second;

    UnifiedDiffReport full_report;
    full_report.base_snapshot_or_commit = old_snapshot_id;
    std::unordered_map<std::string, bool> processed;
    std::ostringstream all_diffs;

    for (const auto& [rel_path, old_fsnap] : old_cp.files) {
        processed[rel_path] = true;
        std::string new_content;
        auto nit = new_cp.files.find(rel_path);
        if (nit != new_cp.files.end() && nit->second.existed_at_snapshot) {
            new_content = nit->second.content;
        }

        if (new_content != old_fsnap.content) {
            UnifiedDiffReport single = computeDiffBetweenContents(rel_path, old_fsnap.content, rel_path, new_content);
            if (!single.file_diffs.empty()) {
                full_report.file_diffs.push_back(single.file_diffs[0]);
                full_report.total_additions += single.total_additions;
                full_report.total_deletions += single.total_deletions;
                full_report.total_files_changed++;
                all_diffs << single.raw_unified_diff;
            }
        }
    }

    for (const auto& [rel_path, new_fsnap] : new_cp.files) {
        if (!processed[rel_path] && new_fsnap.existed_at_snapshot) {
            UnifiedDiffReport single = computeDiffBetweenContents(rel_path, "", rel_path, new_fsnap.content);
            if (!single.file_diffs.empty()) {
                full_report.file_diffs.push_back(single.file_diffs[0]);
                full_report.total_additions += single.total_additions;
                full_report.total_deletions += single.total_deletions;
                full_report.total_files_changed++;
                all_diffs << single.raw_unified_diff;
            }
        }
    }

    full_report.raw_unified_diff = all_diffs.str();
    return full_report;
}

} // namespace aios::workspace
