#include "PathContainment.h"
#include <algorithm>
#include <cctype>
#include <system_error>

namespace aios::workspace {

namespace {

// Helper: Normalize string to lowercase
std::string toLower(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

// Helper: Replace all backslashes with forward slashes for uniform string representation
std::string normalizeSeparators(std::string_view p) {
    std::string s(p);
    std::replace(s.begin(), s.end(), '\\', '/');
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
    return s;
}

// Simple recursive glob matcher supporting '*' and '?'
bool globMatch(std::string_view pattern, std::string_view str) {
    size_t p = 0, s = 0;
    size_t star_p = std::string_view::npos, star_s = 0;

    while (s < str.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
            p++;
            s++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star_p = p++;
            star_s = s;
        } else if (star_p != std::string_view::npos) {
            p = star_p + 1;
            s = ++star_s;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        p++;
    }

    return p == pattern.size();
}

} // anonymous namespace

PathContainment::PathContainment(std::filesystem::path workspace_root)
    : workspace_root_(std::move(workspace_root)) {
    std::error_code ec;
    if (std::filesystem::exists(workspace_root_, ec)) {
        canonical_root_ = std::filesystem::canonical(workspace_root_, ec);
    }
    if (canonical_root_.empty()) {
        canonical_root_ = std::filesystem::weakly_canonical(workspace_root_, ec);
    }
    initializeDefaultProtectedPatterns();
}

void PathContainment::initializeDefaultProtectedPatterns() {
    protected_patterns_ = {
        ".git",
        ".git/*",
        "*/.git",
        "*/.git/*",
        ".env",
        ".env.*",
        "*/.env",
        "*/.env.*",
        "*credentials*",
        "*.pem",
        "*.key",
        "id_rsa",
        "*/id_rsa",
        "id_ed25519",
        "*/id_ed25519",
        ".aios/config.json",
        "*/.aios/config.json",
        "/etc/*",
        "/usr/*",
        "/bin/*",
        "/sbin/*",
        "C:/Windows/*",
        "C:/Program Files/*",
        "C:/Program Files (x86)/*"
    };
}

std::filesystem::path PathContainment::normalizeSeparatorsAndCase(const std::filesystem::path& p) {
    std::string s = normalizeSeparators(p.string());
#ifdef _WIN32
    if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') {
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    }
#endif
    return std::filesystem::path(s);
}

std::filesystem::path PathContainment::resolveWeakly(const std::filesystem::path& base, const std::filesystem::path& target) {
    std::filesystem::path full = target.is_absolute() ? target : (base / target);
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(full, ec);
    if (!canonical.empty() && !ec) {
        return normalizeSeparatorsAndCase(canonical);
    }
    return normalizeSeparatorsAndCase(full.lexically_normal());
}

bool PathContainment::matchesPattern(const std::string& path_str, const std::string& pattern) const {
    std::string norm_path = normalizeSeparators(path_str);
    std::string norm_pat = normalizeSeparators(pattern);

    // Case-insensitive check on Windows
#ifdef _WIN32
    std::string p_lower = toLower(norm_path);
    std::string pat_lower = toLower(norm_pat);
#else
    const std::string& p_lower = norm_path;
    const std::string& pat_lower = norm_pat;
#endif

    if (globMatch(pat_lower, p_lower)) {
        return true;
    }

    // Check filename component against pattern
    std::filesystem::path p(norm_path);
    std::string filename = p.filename().string();
#ifdef _WIN32
    std::string fn_lower = toLower(filename);
#else
    const std::string& fn_lower = filename;
#endif
    if (globMatch(pat_lower, fn_lower)) {
        return true;
    }

    return false;
}

bool PathContainment::isProtected(const std::filesystem::path& target_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string rel_str = getRelativeString(target_path);
    std::string target_str = normalizeSeparators(target_path.string());

    for (const auto& pattern : protected_patterns_) {
        if (matchesPattern(rel_str, pattern) || matchesPattern(target_str, pattern)) {
            return true;
        }
    }

    // Additional hardcoded checks for .git directory or files within .git
    std::filesystem::path norm_rel(normalizeSeparators(rel_str));
    for (const auto& part : norm_rel) {
        if (part.string() == ".git") {
            return true;
        }
    }

    return false;
}

void PathContainment::addProtectedPattern(const std::string& glob_or_exact) {
    std::lock_guard<std::mutex> lock(mutex_);
    protected_patterns_.push_back(glob_or_exact);
}

void PathContainment::addAllowedExternalPath(const std::filesystem::path& external_path, AccessMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(external_path, ec);
    allowed_external_paths_[normalizeSeparators(canon.string())] = mode;
}

bool PathContainment::isContained(const std::filesystem::path& target_path) const {
    std::error_code ec;
    std::filesystem::path resolved = resolveWeakly(workspace_root_, target_path);

    // Symlink escape check: if exists, resolve full canonical target
    if (std::filesystem::exists(resolved, ec) && std::filesystem::is_symlink(resolved, ec)) {
        std::filesystem::path symlink_target = std::filesystem::canonical(resolved, ec);
        if (!ec && !symlink_target.empty()) {
            resolved = normalizeSeparatorsAndCase(symlink_target);
        }
    }

    std::string resolved_str = normalizeSeparators(resolved.string());
    std::string root_str = normalizeSeparators(canonical_root_.string());

#ifdef _WIN32
    resolved_str = toLower(resolved_str);
    root_str = toLower(root_str);
#endif

    // Check if within workspace root
    if (resolved_str == root_str) {
        return true;
    }
    if (resolved_str.size() > root_str.size() && 
        resolved_str.starts_with(root_str) && 
        (root_str.back() == '/' || resolved_str[root_str.size()] == '/')) {
        return true;
    }

    // Check if explicitly allowed external path
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [ext_path, _] : allowed_external_paths_) {
        std::string allowed_str = normalizeSeparators(ext_path);
#ifdef _WIN32
        allowed_str = toLower(allowed_str);
#endif
        if (resolved_str == allowed_str || 
            (resolved_str.starts_with(allowed_str) && 
             (allowed_str.back() == '/' || resolved_str[allowed_str.size()] == '/'))) {
            return true;
        }
    }

    return false;
}

PathValidationResult PathContainment::validate(const std::filesystem::path& target_path, AccessMode mode) const {
    PathValidationResult result;
    std::filesystem::path resolved = resolveWeakly(workspace_root_, target_path);
    result.canonical_path = resolved;

    std::error_code ec;
    if (std::filesystem::exists(resolved, ec) && std::filesystem::is_symlink(resolved, ec)) {
        std::filesystem::path symlink_target = std::filesystem::canonical(resolved, ec);
        if (!ec && !symlink_target.empty()) {
            resolved = normalizeSeparatorsAndCase(symlink_target);
            result.canonical_path = resolved;
        }
    }

    std::string rel = getRelativeString(resolved);
    result.relative_path = std::filesystem::path(rel);

    // 1. Traversal / containment check
    if (!isContained(resolved)) {
        result.is_valid = false;
        result.violation_reason = "Path traversal violation: target path '" + target_path.string() + 
                                  "' escapes workspace root '" + workspace_root_.string() + "'";
        return result;
    }

    // 2. Protected path check
    bool is_prot = isProtected(resolved);
    result.is_protected = is_prot;

    if (is_prot && (mode == AccessMode::ReadWrite || mode == AccessMode::Execute)) {
        result.is_valid = false;
        result.violation_reason = "Access denied: target path '" + rel + "' is a protected system or repository file";
        return result;
    }

    // 3. External path access mode check
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string resolved_str = normalizeSeparators(resolved.string());
        for (const auto& [ext_path, allowed_mode] : allowed_external_paths_) {
            std::string allowed_str = normalizeSeparators(ext_path);
            if (resolved_str.starts_with(allowed_str)) {
                if (mode == AccessMode::ReadWrite && allowed_mode == AccessMode::ReadOnly) {
                    result.is_valid = false;
                    result.violation_reason = "Access denied: external path is configured as ReadOnly";
                    return result;
                }
            }
        }
    }

    result.is_valid = true;
    return result;
}

std::filesystem::path PathContainment::sanitize(const std::filesystem::path& raw_path) const {
    return resolveWeakly(workspace_root_, raw_path);
}

std::string PathContainment::getRelativeString(const std::filesystem::path& target_path) const {
    std::filesystem::path resolved = resolveWeakly(workspace_root_, target_path);
    std::error_code ec;
    std::filesystem::path rel = std::filesystem::relative(resolved, canonical_root_, ec);
    if (!ec && !rel.empty()) {
        return normalizeSeparators(rel.string());
    }
    return normalizeSeparators(resolved.string());
}

} // namespace aios::workspace
