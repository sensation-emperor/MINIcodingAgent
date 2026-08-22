#include "GitWorktree.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <system_error>

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

std::string fromWideString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size_needed, NULL, NULL);
    return str;
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

GitWorktree::GitWorktree(std::filesystem::path repository_root)
    : repository_root_(std::move(repository_root)) {
    worktrees_root_ = repository_root_ / ".aios" / "worktrees";
    std::error_code ec;
    std::filesystem::create_directories(worktrees_root_, ec);
}

GitWorktree::~GitWorktree() = default;

WorkspaceResult<std::string> GitWorktree::executeGitCommand(const std::vector<std::string>& args, 
                                                           const std::filesystem::path& working_dir) const {
    std::filesystem::path run_dir = working_dir.empty() ? repository_root_ : working_dir;
    std::error_code ec;
    if (!std::filesystem::exists(run_dir, ec)) {
        return std::unexpected("Working directory does not exist: " + run_dir.string());
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
    std::wstring wdir = toWideString(run_dir.string());

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
        return std::unexpected("Failed to execute git process: " + cmdline);
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output.append(buffer, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, 30000); // 30s timeout

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
        return std::unexpected("popen() failed!");
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

std::filesystem::path GitWorktree::resolveWorktreePath(const std::string& worktree_id_or_path) const {
    std::filesystem::path p(worktree_id_or_path);
    if (p.is_absolute()) {
        return p;
    }
    auto it = active_worktrees_.find(worktree_id_or_path);
    if (it != active_worktrees_.end()) {
        return it->second.path;
    }
    return worktrees_root_ / ("wt_" + worktree_id_or_path);
}

std::vector<WorktreeInfo> GitWorktree::parsePorcelainWorktreeList(const std::string& output) const {
    std::vector<WorktreeInfo> worktrees;
    std::istringstream stream(output);
    std::string line;

    WorktreeInfo current;
    bool has_entry = false;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            if (has_entry) {
                worktrees.push_back(current);
                current = WorktreeInfo{};
                has_entry = false;
            }
            continue;
        }

        if (trimmed.starts_with("worktree ")) {
            if (has_entry) {
                worktrees.push_back(current);
                current = WorktreeInfo{};
            }
            has_entry = true;
            current.path = trimmed.substr(9);
            current.id = current.path.filename().string();
            if (current.id.starts_with("wt_")) {
                current.id = current.id.substr(3);
            }
            current.task_id = current.id;
            current.owner_task_id = current.id;
        } else if (trimmed.starts_with("HEAD ")) {
            current.head_commit_sha = trimmed.substr(5);
        } else if (trimmed.starts_with("branch refs/heads/")) {
            current.branch = trimmed.substr(18);
        } else if (trimmed == "detached") {
            current.is_detached = true;
            current.branch = "HEAD (detached)";
        } else if (trimmed.starts_with("locked")) {
            current.is_locked = true;
            current.locked = true;
            if (trimmed.size() > 7) {
                current.lock_reason = trimmed.substr(7);
            }
        }
    }

    if (has_entry) {
        worktrees.push_back(current);
    }

    return worktrees;
}

void GitWorktree::refreshActiveWorktreesUnlocked() {
    auto res = executeGitCommand({"worktree", "list", "--porcelain"});
    if (res.has_value()) {
        auto list = parsePorcelainWorktreeList(res.value());
        for (const auto& wt : list) {
            active_worktrees_[wt.id] = wt;
        }
    }
}

WorkspaceResult<WorktreeInfo> GitWorktree::createWorktree(const CreateWorktreeOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (options.worktree_id.empty()) {
        return std::unexpected("Worktree ID cannot be empty");
    }

    std::filesystem::path wt_path = options.custom_path.empty() 
        ? (worktrees_root_ / ("wt_" + options.worktree_id)) 
        : options.custom_path;

    std::error_code ec;
    if (std::filesystem::exists(wt_path, ec) && !std::filesystem::is_empty(wt_path, ec)) {
        return std::unexpected("Worktree directory already exists and is not empty: " + wt_path.string());
    }

    std::string branch = options.branch_name.empty() 
        ? ("aios/task_" + options.worktree_id + "_wt") 
        : options.branch_name;

    std::vector<std::string> args = {"worktree", "add"};
    if (options.detach_head) {
        args.push_back("--detach");
        args.push_back(wt_path.string());
        args.push_back(options.base_ref.empty() ? "HEAD" : options.base_ref);
    } else if (options.create_new_branch) {
        args.push_back("-b");
        args.push_back(branch);
        args.push_back(wt_path.string());
        args.push_back(options.base_ref.empty() ? "HEAD" : options.base_ref);
    } else {
        args.push_back(wt_path.string());
        args.push_back(branch);
    }

    auto res = executeGitCommand(args);
    if (!res.has_value()) {
        // Fallback: If branch already exists, attach to existing branch or detach
        std::vector<std::string> fallback_args = {"worktree", "add", wt_path.string(), branch};
        auto fallback_res = executeGitCommand(fallback_args);
        if (!fallback_res.has_value()) {
            return std::unexpected("Failed to create worktree: " + res.error());
        }
    }

    WorktreeInfo info;
    info.id = options.worktree_id;
    info.task_id = options.worktree_id;
    info.owner_task_id = options.worktree_id;
    info.path = wt_path;
    info.branch = branch;
    info.is_detached = options.detach_head;
    info.created_at = std::chrono::system_clock::now();

    // Query HEAD commit SHA
    auto sha_res = executeGitCommand({"rev-parse", "HEAD"}, wt_path);
    if (sha_res.has_value()) {
        info.head_commit_sha = trim(sha_res.value());
    }

    if (options.lock_immediately) {
        std::vector<std::string> lock_args = {"worktree", "lock"};
        if (!options.lock_reason.empty()) {
            lock_args.push_back("--reason");
            lock_args.push_back(options.lock_reason);
        }
        lock_args.push_back(wt_path.string());
        (void)executeGitCommand(lock_args);
        info.is_locked = true;
        info.locked = true;
        info.lock_reason = options.lock_reason;
    }

    active_worktrees_[options.worktree_id] = info;
    return info;
}

WorkspaceResult<bool> GitWorktree::removeWorktree(const std::string& worktree_id_or_path, bool force) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path wt_path = resolveWorktreePath(worktree_id_or_path);

    // Check if worktree is locked
    auto wt_it = active_worktrees_.find(worktree_id_or_path);
    if (wt_it != active_worktrees_.end() && wt_it->second.is_locked && !force) {
        return std::unexpected("Worktree is locked: " + wt_it->second.lock_reason);
    }

    // Unlock first if forcing removal
    if (wt_it != active_worktrees_.end() && wt_it->second.is_locked && force) {
        (void)executeGitCommand({"worktree", "unlock", wt_path.string()});
    }

    std::vector<std::string> args = {"worktree", "remove"};
    if (force) {
        args.push_back("--force");
    }
    args.push_back(wt_path.string());

    auto res = executeGitCommand(args);
    if (!res.has_value()) {
        // If removal failed because directory was deleted manually, prune worktrees
        (void)executeGitCommand({"worktree", "prune"});
    }

    std::error_code ec;
    if (std::filesystem::exists(wt_path, ec)) {
        std::filesystem::remove_all(wt_path, ec);
    }

    active_worktrees_.erase(worktree_id_or_path);
    for (auto it = active_worktrees_.begin(); it != active_worktrees_.end();) {
        if (it->second.path == wt_path) {
            it = active_worktrees_.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}

WorkspaceResult<bool> GitWorktree::lockWorktree(const std::string& worktree_id_or_path, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path wt_path = resolveWorktreePath(worktree_id_or_path);

    std::vector<std::string> args = {"worktree", "lock"};
    if (!reason.empty()) {
        args.push_back("--reason");
        args.push_back(reason);
    }
    args.push_back(wt_path.string());

    auto res = executeGitCommand(args);
    if (!res.has_value()) {
        return std::unexpected("Failed to lock worktree: " + res.error());
    }

    for (auto& [_, wt] : active_worktrees_) {
        if (wt.path == wt_path || wt.id == worktree_id_or_path) {
            wt.is_locked = true;
            wt.locked = true;
            wt.lock_reason = reason;
        }
    }

    return true;
}

WorkspaceResult<bool> GitWorktree::unlockWorktree(const std::string& worktree_id_or_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path wt_path = resolveWorktreePath(worktree_id_or_path);

    auto res = executeGitCommand({"worktree", "unlock", wt_path.string()});
    if (!res.has_value()) {
        return std::unexpected("Failed to unlock worktree: " + res.error());
    }

    for (auto& [_, wt] : active_worktrees_) {
        if (wt.path == wt_path || wt.id == worktree_id_or_path) {
            wt.is_locked = false;
            wt.locked = false;
            wt.lock_reason = "";
        }
    }

    return true;
}

WorkspaceResult<size_t> GitWorktree::pruneWorktrees(const std::string& expire) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> args = {"worktree", "prune"};
    if (!expire.empty()) {
        args.push_back("--expire");
        args.push_back(expire);
    }

    auto res = executeGitCommand(args);
    if (!res.has_value()) {
        return std::unexpected("Failed to prune worktrees: " + res.error());
    }

    refreshActiveWorktreesUnlocked();
    return active_worktrees_.size();
}

WorkspaceResult<bool> GitWorktree::repairWorktrees() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res = executeGitCommand({"worktree", "repair"});
    if (!res.has_value()) {
        return std::unexpected("Failed to repair worktrees: " + res.error());
    }
    return true;
}

WorkspaceResult<std::vector<WorktreeInfo>> GitWorktree::listWorktrees() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto res = executeGitCommand({"worktree", "list", "--porcelain"});
    if (!res.has_value()) {
        return std::unexpected("Failed to list worktrees: " + res.error());
    }

    auto list = parsePorcelainWorktreeList(res.value());
    return list;
}

std::optional<WorktreeInfo> GitWorktree::getWorktree(const std::string& worktree_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_worktrees_.find(worktree_id);
    if (it != active_worktrees_.end()) {
        return it->second;
    }
    auto list_res = listWorktrees();
    if (list_res.has_value()) {
        for (const auto& wt : list_res.value()) {
            if (wt.id == worktree_id || wt.task_id == worktree_id) {
                return wt;
            }
        }
    }
    return std::nullopt;
}

bool GitWorktree::hasWorktree(const std::string& worktree_id) const {
    return getWorktree(worktree_id).has_value();
}

} // namespace aios::workspace
