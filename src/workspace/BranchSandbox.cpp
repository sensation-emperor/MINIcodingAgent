#include "BranchSandbox.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#endif

namespace aios::workspace {

namespace {

std::string trim(std::string_view s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

#ifdef _WIN32
std::wstring toWideString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], size_needed);
    return wstr;
}

std::string quoteArg(const std::string& arg) {
    if (arg.find_first_of(" \t\n\v\"") == std::string::npos && !arg.empty()) {
        return arg;
    }
    std::string result = "\"";
    for (size_t i = 0; i < arg.size(); ++i) {
        if (arg[i] == '\"') {
            result += "\\\"";
        } else if (arg[i] == '\\') {
            size_t backslash_count = 1;
            while (i + 1 < arg.size() && arg[i + 1] == '\\') {
                backslash_count++;
                i++;
            }
            if (i + 1 == arg.size()) {
                result.append(backslash_count * 2, '\\');
            } else if (arg[i + 1] == '\"') {
                result.append(backslash_count * 2 + 1, '\\');
            } else {
                result.append(backslash_count, '\\');
            }
        } else {
            result.push_back(arg[i]);
        }
    }
    result += "\"";
    return result;
}
#endif

} // anonymous namespace

BranchSandbox::BranchSandbox(std::filesystem::path repo_or_worktree_path)
    : working_path_(std::move(repo_or_worktree_path)) {
}

WorkspaceResult<std::string> BranchSandbox::runGit(const std::vector<std::string>& args) const {
    std::error_code ec;
    if (!std::filesystem::exists(working_path_, ec)) {
        return std::unexpected("Working path does not exist: " + working_path_.string());
    }

#ifdef _WIN32
    std::string cmdline = "git.exe";
    for (const auto& arg : args) {
        cmdline += " " + quoteArg(arg);
    }

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return std::unexpected("Failed to create process pipe");
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring wcmd = toWideString(cmdline);
    std::wstring wdir = toWideString(working_path_.string());

    BOOL success = CreateProcessW(
        NULL,
        &wcmd[0],
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        wdir.c_str(),
        &si,
        &pi
    );

    CloseHandle(hWritePipe);

    if (!success) {
        CloseHandle(hReadPipe);
        return std::unexpected("Failed to execute git: " + cmdline);
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output.append(buffer, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    if (exitCode != 0) {
        return std::unexpected("Git command failed (exit " + std::to_string(exitCode) + "): " + output);
    }

    return output;
#else
    std::string cmd = "git";
    for (const auto& arg : args) {
        cmd += " '" + arg + "'";
    }
    cmd += " 2>&1";

    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::unexpected("popen() failed");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    int rc = pclose(pipe);
    if (rc != 0) {
        return std::unexpected("Git command failed (exit " + std::to_string(rc) + "): " + result);
    }
    return result;
#endif
}

WorkspaceResult<std::string> BranchSandbox::createEphemeralBranch(const std::string& task_id, const std::string& base_ref) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string branch_name = "aios/ephemeral/" + task_id;
    std::string ref = base_ref.empty() ? "HEAD" : base_ref;

    // Check if branch already exists
    auto list_res = runGit({"branch", "--list", branch_name});
    if (list_res.has_value() && !trim(list_res.value()).empty()) {
        return branch_name;
    }

    auto res = runGit({"checkout", "-b", branch_name, ref});
    if (!res.has_value()) {
        // Fallback: create branch without checkout
        auto create_res = runGit({"branch", branch_name, ref});
        if (!create_res.has_value()) {
            return std::unexpected("Failed to create ephemeral branch: " + res.error());
        }
    }

    return branch_name;
}

WorkspaceResult<bool> BranchSandbox::checkoutBranch(const std::string& branch_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res = runGit({"checkout", branch_name});
    if (!res.has_value()) {
        return std::unexpected("Failed to checkout branch '" + branch_name + "': " + res.error());
    }
    return true;
}

WorkspaceResult<bool> BranchSandbox::deleteBranch(const std::string& branch_name, bool force) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> args = {"branch", force ? "-D" : "-d", branch_name};
    auto res = runGit(args);
    if (!res.has_value()) {
        return std::unexpected("Failed to delete branch '" + branch_name + "': " + res.error());
    }
    return true;
}

WorkspaceResult<std::vector<std::string>> BranchSandbox::listEphemeralBranches() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res = runGit({"branch", "--list", "aios/ephemeral/*"});
    if (!res.has_value()) {
        return std::unexpected("Failed to list ephemeral branches: " + res.error());
    }

    std::vector<std::string> branches;
    std::istringstream iss(res.value());
    std::string line;
    while (std::getline(iss, line)) {
        std::string b = trim(line);
        if (b.starts_with("* ")) {
            b = b.substr(2);
        }
        if (!b.empty()) {
            branches.push_back(b);
        }
    }
    return branches;
}

WorkspaceResult<std::string> BranchSandbox::stageAndCommit(const std::string& message,
                                                           const std::vector<std::string>& files,
                                                           const std::unordered_map<std::string, std::string>& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Stage files
    if (files.empty()) {
        auto add_res = runGit({"add", "-A"});
        if (!add_res.has_value()) {
            return std::unexpected("Failed to stage all files: " + add_res.error());
        }
    } else {
        for (const auto& file : files) {
            auto add_res = runGit({"add", file});
            if (!add_res.has_value()) {
                return std::unexpected("Failed to stage file '" + file + "': " + add_res.error());
            }
        }
    }

    // 2. Build commit message with trailers
    std::string full_msg = message;
    if (!metadata.empty()) {
        full_msg += "\n\n";
        for (const auto& [k, v] : metadata) {
            full_msg += k + ": " + v + "\n";
        }
    }

    // 3. Commit
    auto commit_res = runGit({"commit", "-m", full_msg});
    if (!commit_res.has_value()) {
        return std::unexpected("Git commit failed: " + commit_res.error());
    }

    // 4. Get SHA
    auto sha_res = runGit({"rev-parse", "HEAD"});
    if (!sha_res.has_value()) {
        return std::unexpected("Failed to retrieve HEAD commit SHA: " + sha_res.error());
    }

    return trim(sha_res.value());
}

WorkspaceResult<std::string> BranchSandbox::createStash(const std::string& message, bool include_untracked) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> args = {"stash", "push"};
    if (include_untracked) {
        args.push_back("-u");
    }
    if (!message.empty()) {
        args.push_back("-m");
        args.push_back(message);
    }

    auto res = runGit(args);
    if (!res.has_value()) {
        return std::unexpected("Failed to create stash: " + res.error());
    }
    return trim(res.value());
}

WorkspaceResult<bool> BranchSandbox::popStash(int stash_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string ref = "stash@{" + std::to_string(stash_index) + "}";
    auto res = runGit({"stash", "pop", ref});
    if (!res.has_value()) {
        return std::unexpected("Failed to pop stash " + ref + ": " + res.error());
    }
    return true;
}

WorkspaceResult<bool> BranchSandbox::dropStash(int stash_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string ref = "stash@{" + std::to_string(stash_index) + "}";
    auto res = runGit({"stash", "drop", ref});
    if (!res.has_value()) {
        return std::unexpected("Failed to drop stash " + ref + ": " + res.error());
    }
    return true;
}

std::vector<MergeConflictFile> BranchSandbox::parseConflictMarkersInFile(const std::filesystem::path& relative_path) const {
    std::vector<MergeConflictFile> result;
    std::filesystem::path full_path = working_path_ / relative_path;

    std::ifstream file(full_path);
    if (!file.is_open()) return result;

    MergeConflictFile cfile;
    cfile.file_path = relative_path.generic_string();

    std::string line;
    int line_num = 0;
    bool in_conflict = false;
    bool in_their = false;

    MergeConflictChunk current_chunk;

    while (std::getline(file, line)) {
        line_num++;
        if (line.starts_with("<<<<<<<")) {
            in_conflict = true;
            in_their = false;
            current_chunk = MergeConflictChunk{};
            current_chunk.start_line = line_num;
        } else if (in_conflict && line.starts_with("=======")) {
            in_their = true;
        } else if (in_conflict && line.starts_with(">>>>>>>")) {
            in_conflict = false;
            in_their = false;
            current_chunk.end_line = line_num;
            cfile.chunks.push_back(current_chunk);
        } else if (in_conflict) {
            if (!in_their) {
                current_chunk.our_content += line + "\n";
            } else {
                current_chunk.their_content += line + "\n";
            }
        }
    }

    if (!cfile.chunks.empty()) {
        result.push_back(cfile);
    }
    return result;
}

std::vector<MergeConflictFile> BranchSandbox::collectConflictingFiles() const {
    std::vector<MergeConflictFile> conflicts;
    auto diff_res = runGit({"diff", "--name-only", "--diff-filter=U"});
    if (!diff_res.has_value()) return conflicts;

    std::istringstream iss(diff_res.value());
    std::string rel_file;
    while (std::getline(iss, rel_file)) {
        std::string trimmed = trim(rel_file);
        if (!trimmed.empty()) {
            auto parsed = parseConflictMarkersInFile(trimmed);
            conflicts.insert(conflicts.end(), parsed.begin(), parsed.end());
        }
    }
    return conflicts;
}

WorkspaceResult<MergeResult> BranchSandbox::merge(const std::string& source_branch, MergeStrategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Get current branch
    auto cur_branch_res = runGit({"rev-parse", "--abbrev-ref", "HEAD"});
    std::string current_branch = cur_branch_res.has_value() ? trim(cur_branch_res.value()) : "HEAD";

    MergeResult result;
    result.base_branch = current_branch;
    result.source_branch = source_branch;

    std::vector<std::string> args = {"merge"};
    switch (strategy) {
        case MergeStrategy::FastForwardOnly:
            args.push_back("--ff-only");
            break;
        case MergeStrategy::ThreeWay:
            args.push_back("--no-ff");
            args.push_back("-m");
            args.push_back("Merge branch '" + source_branch + "' into " + current_branch);
            break;
        case MergeStrategy::Squash:
            args.push_back("--squash");
            break;
        case MergeStrategy::Rebase:
            return rebase(source_branch);
    }
    args.push_back(source_branch);

    auto merge_res = runGit(args);
    if (!merge_res.has_value()) {
        // Collect conflicts
        auto conf_files = collectConflictingFiles();
        if (!conf_files.empty()) {
            result.has_conflicts = true;
            result.success = false;
            result.conflicts = std::move(conf_files);
            result.error_message = "Merge conflict detected";
            return result;
        }

        result.success = false;
        result.error_message = merge_res.error();
        return result;
    }

    if (strategy == MergeStrategy::Squash) {
        // Squash merge requires explicit commit
        (void)runGit({"commit", "-m", "Squashed commit from " + source_branch});
    }

    auto sha_res = runGit({"rev-parse", "HEAD"});
    if (sha_res.has_value()) {
        result.merge_commit_sha = trim(sha_res.value());
    }
    result.success = true;
    result.has_conflicts = false;

    return result;
}

WorkspaceResult<MergeResult> BranchSandbox::rebase(const std::string& upstream_branch) {
    auto cur_branch_res = runGit({"rev-parse", "--abbrev-ref", "HEAD"});
    std::string current_branch = cur_branch_res.has_value() ? trim(cur_branch_res.value()) : "HEAD";

    MergeResult result;
    result.base_branch = current_branch;
    result.source_branch = upstream_branch;

    auto rebase_res = runGit({"rebase", upstream_branch});
    if (!rebase_res.has_value()) {
        auto conf_files = collectConflictingFiles();
        if (!conf_files.empty()) {
            result.has_conflicts = true;
            result.success = false;
            result.conflicts = std::move(conf_files);
            result.error_message = "Rebase conflict detected";
            return result;
        }
        result.success = false;
        result.error_message = rebase_res.error();
        return result;
    }

    auto sha_res = runGit({"rev-parse", "HEAD"});
    if (sha_res.has_value()) {
        result.merge_commit_sha = trim(sha_res.value());
    }
    result.success = true;
    result.has_conflicts = false;
    return result;
}

WorkspaceResult<bool> BranchSandbox::abortMerge() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto merge_abort = runGit({"merge", "--abort"});
    if (merge_abort.has_value()) return true;

    auto rebase_abort = runGit({"rebase", "--abort"});
    if (rebase_abort.has_value()) return true;

    return false;
}

WorkspaceResult<std::vector<MergeConflictFile>> BranchSandbox::detectConflicts(const std::string& source_branch, const std::string& target_branch) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Use git merge-tree to test 3-way merge without modifying working tree
    auto base_sha_res = runGit({"merge-base", target_branch, source_branch});
    if (!base_sha_res.has_value()) {
        return std::unexpected("Failed to find common ancestor for '" + source_branch + "' and '" + target_branch + "'");
    }

    std::string base_sha = trim(base_sha_res.value());
    auto mt_res = runGit({"merge-tree", base_sha, target_branch, source_branch});
    if (!mt_res.has_value()) {
        return std::unexpected("git merge-tree failed: " + mt_res.error());
    }

    std::vector<MergeConflictFile> conflicts;
    std::istringstream iss(mt_res.value());
    std::string line;
    MergeConflictFile current;
    bool in_conflict = false;

    while (std::getline(iss, line)) {
        if (line.find("changed in both") != std::string::npos || line.find("conflict in") != std::string::npos) {
            in_conflict = true;
            size_t idx = line.find_last_of(" \t");
            if (idx != std::string::npos) {
                current.file_path = trim(line.substr(idx));
            }
        } else if (in_conflict && line.starts_with("<<<<<<<")) {
            MergeConflictChunk chunk;
            chunk.start_line = 1;
            while (std::getline(iss, line) && !line.starts_with("=======")) {
                chunk.our_content += line + "\n";
            }
            while (std::getline(iss, line) && !line.starts_with(">>>>>>>")) {
                chunk.their_content += line + "\n";
            }
            current.chunks.push_back(chunk);
            conflicts.push_back(current);
            current = MergeConflictFile{};
            in_conflict = false;
        }
    }

    return conflicts;
}

WorkspaceResult<std::string> BranchSandbox::getBranchDiff(const std::string& base_branch, const std::string& target_branch) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res = runGit({"diff", base_branch + "..." + target_branch});
    if (!res.has_value()) {
        return std::unexpected("Failed to compute branch diff: " + res.error());
    }
    return res.value();
}

WorkspaceResult<std::string> BranchSandbox::getUncommittedDiff() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res = runGit({"diff", "HEAD"});
    if (!res.has_value()) {
        return std::unexpected("Failed to compute uncommitted diff: " + res.error());
    }
    return res.value();
}

} // namespace aios::workspace
