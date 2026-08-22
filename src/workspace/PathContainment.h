#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "WorkspaceTypes.h"

namespace aios::workspace {

class PathContainment {
public:
    explicit PathContainment(std::filesystem::path workspace_root);
    ~PathContainment() = default;

    // Path Validation
    PathValidationResult validate(const std::filesystem::path& target_path, AccessMode mode = AccessMode::ReadWrite) const;
    bool isContained(const std::filesystem::path& target_path) const;
    bool isProtected(const std::filesystem::path& target_path) const;

    // Security Rules Configuration
    void addProtectedPattern(const std::string& glob_or_exact);
    void addAllowedExternalPath(const std::filesystem::path& external_path, AccessMode mode = AccessMode::ReadOnly);

    // Sanitization Helpers
    std::filesystem::path sanitize(const std::filesystem::path& raw_path) const;
    std::string getRelativeString(const std::filesystem::path& target_path) const;

    std::filesystem::path getWorkspaceRoot() const { return workspace_root_; }
    std::filesystem::path getCanonicalRoot() const { return canonical_root_; }

private:
    std::filesystem::path workspace_root_;
    std::filesystem::path canonical_root_;
    mutable std::mutex mutex_;

    std::vector<std::string> protected_patterns_;
    std::unordered_map<std::string, AccessMode> allowed_external_paths_;

    void initializeDefaultProtectedPatterns();
    bool matchesPattern(const std::string& path_str, const std::string& pattern) const;
    static std::filesystem::path normalizeSeparatorsAndCase(const std::filesystem::path& p);
    static std::filesystem::path resolveWeakly(const std::filesystem::path& base, const std::filesystem::path& target);
};

} // namespace aios::workspace

namespace aios {
    using workspace::AccessMode;
    using workspace::PathValidationResult;
    using workspace::PathContainment;
}
