#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdlib>

#include "workspace/WorkspaceTypes.h"
#include "workspace/PathContainment.h"
#include "workspace/SnapshotManager.h"
#include "workspace/GitWorktree.h"
#include "workspace/BranchSandbox.h"
#include "workspace/WorkspaceManager.h"
#include "workspace/WorkspaceTools.h"
#include "tools/ToolRegistry.h"

using namespace aios::workspace;

namespace {

// Helper to create a clean temporary test directory
std::filesystem::path createTestTempDir(const std::string& prefix) {
    auto temp = std::filesystem::temp_directory_path() / (prefix + "_" + std::to_string(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    std::filesystem::create_directories(temp);
    return temp;
}

// Helper to initialize a real git repository for testing GitWorktree and BranchSandbox
bool setupTestGitRepo(const std::filesystem::path& repo_path) {
    std::error_code ec;
    std::filesystem::create_directories(repo_path, ec);

    // Set up git repo
    std::string cmd = "git -C \"" + repo_path.string() + "\" init -b main";
    if (std::system(cmd.c_str()) != 0) {
        // Fallback for older git
        std::string fallback = "git -C \"" + repo_path.string() + "\" init";
        std::system(fallback.c_str());
    }

    std::system(("git -C \"" + repo_path.string() + "\" config user.name \"AIOS Worker\"").c_str());
    std::system(("git -C \"" + repo_path.string() + "\" config user.email \"aios@worker.local\"").c_str());

    // Initial commit
    std::filesystem::path readme = repo_path / "README.md";
    std::ofstream out(readme);
    out << "# Test Repository\nInitial commit for AIOS test.\n";
    out.close();

    std::system(("git -C \"" + repo_path.string() + "\" add README.md").c_str());
    int res = std::system(("git -C \"" + repo_path.string() + "\" commit -m \"Initial commit\"").c_str());
    return res == 0;
}

} // anonymous namespace

// ==========================================
// 1. Path Containment Tests
// ==========================================

class PathContainmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_root_ = createTestTempDir("aios_path_test");
        containment_ = std::make_unique<PathContainment>(test_root_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_root_, ec);
    }

    std::filesystem::path test_root_;
    std::unique_ptr<PathContainment> containment_;
};

TEST_F(PathContainmentTest, PreventsDirectoryTraversal) {
    // Relative traversal escaping root
    auto res1 = containment_->validate("../../etc/passwd", AccessMode::ReadWrite);
    EXPECT_FALSE(res1.is_valid);
    EXPECT_NE(res1.violation_reason.find("Path traversal violation"), std::string::npos);

    auto res2 = containment_->validate("..\\..\\Windows\\System32\\cmd.exe", AccessMode::ReadWrite);
    EXPECT_FALSE(res2.is_valid);

    auto res3 = containment_->validate(test_root_ / "subdir" / ".." / ".." / "outside.txt", AccessMode::ReadWrite);
    EXPECT_FALSE(res3.is_valid);

    EXPECT_FALSE(containment_->isContained(std::filesystem::temp_directory_path() / "other_random_file.txt"));
}

TEST_F(PathContainmentTest, ProtectsGitAndSecretFiles) {
    // .git internal metadata
    auto res_git = containment_->validate(".git/HEAD", AccessMode::ReadWrite);
    EXPECT_FALSE(res_git.is_valid);
    EXPECT_TRUE(res_git.is_protected);

    auto res_config = containment_->validate(test_root_ / ".git" / "config", AccessMode::ReadWrite);
    EXPECT_FALSE(res_config.is_valid);
    EXPECT_TRUE(res_config.is_protected);

    // .env and secrets
    auto res_env = containment_->validate(".env", AccessMode::ReadWrite);
    EXPECT_FALSE(res_env.is_valid);
    EXPECT_TRUE(res_env.is_protected);

    auto res_env_prod = containment_->validate("config/.env.production", AccessMode::ReadWrite);
    EXPECT_FALSE(res_env_prod.is_valid);
    EXPECT_TRUE(res_env_prod.is_protected);

    auto res_rsa = containment_->validate("keys/id_rsa", AccessMode::ReadWrite);
    EXPECT_FALSE(res_rsa.is_valid);
    EXPECT_TRUE(res_rsa.is_protected);

    auto res_pem = containment_->validate("certs/server.pem", AccessMode::ReadWrite);
    EXPECT_FALSE(res_pem.is_valid);
    EXPECT_TRUE(res_pem.is_protected);

    EXPECT_TRUE(containment_->isProtected(test_root_ / ".aios" / "config.json"));
}

TEST_F(PathContainmentTest, AllowsReadOnlyForAllowedFiles) {
    // Normal files in workspace root
    auto valid_res = containment_->validate("src/main.cpp", AccessMode::ReadWrite);
    EXPECT_TRUE(valid_res.is_valid);
    EXPECT_FALSE(valid_res.is_protected);

    auto valid_nested = containment_->validate("nested/folder/deep/file.txt", AccessMode::ReadWrite);
    EXPECT_TRUE(valid_nested.is_valid);
    EXPECT_FALSE(valid_nested.is_protected);

    // Read-only access to protected files
    auto ro_env = containment_->validate(".env", AccessMode::ReadOnly);
    EXPECT_TRUE(ro_env.is_valid);
    EXPECT_TRUE(ro_env.is_protected);
}

TEST_F(PathContainmentTest, AllowsCustomExternalPaths) {
    auto external_dir = createTestTempDir("aios_external_dir");
    containment_->addAllowedExternalPath(external_dir, AccessMode::ReadOnly);

    // Reading allowed external path
    auto ro_ext = containment_->validate(external_dir / "doc.txt", AccessMode::ReadOnly);
    EXPECT_TRUE(ro_ext.is_valid);

    // Writing to ReadOnly external path is rejected
    auto rw_ext = containment_->validate(external_dir / "doc.txt", AccessMode::ReadWrite);
    EXPECT_FALSE(rw_ext.is_valid);

    std::error_code ec;
    std::filesystem::remove_all(external_dir, ec);
}

// ==========================================
// 2. Snapshot Manager Tests
// ==========================================

class SnapshotTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_root_ = createTestTempDir("aios_snap_test");
        containment_ = std::make_shared<PathContainment>(test_root_);
        snap_mgr_ = std::make_unique<SnapshotManager>(test_root_, containment_);

        // Create sample initial files
        std::ofstream(test_root_ / "file1.txt") << "Hello Line 1\nHello Line 2\nHello Line 3\n";
        std::ofstream(test_root_ / "file2.txt") << "Alpha\nBeta\nGamma\n";
        std::ofstream(test_root_ / "file3.txt") << "Data 100\nData 200\n";
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_root_, ec);
    }

    std::filesystem::path test_root_;
    std::shared_ptr<PathContainment> containment_;
    std::unique_ptr<SnapshotManager> snap_mgr_;
};

TEST_F(SnapshotTest, CreatesAndListsSnapshots) {
    auto cp_res = snap_mgr_->createSnapshot("snap_01", "Initial checkpoint");
    ASSERT_TRUE(cp_res.has_value());
    EXPECT_EQ(cp_res.value().id, "snap_01");
    EXPECT_GE(cp_res.value().files.size(), 3u);

    auto list = snap_mgr_->listSnapshots();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].id, "snap_01");

    auto opt = snap_mgr_->getSnapshot("snap_01");
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt->description, "Initial checkpoint");
}

TEST_F(SnapshotTest, TransactionalRollbackOnFailure) {
    // 1. Create snapshot
    auto cp_res = snap_mgr_->createSnapshot("snap_base", "Base state");
    ASSERT_TRUE(cp_res.has_value());

    // 2. Mutate files
    std::ofstream(test_root_ / "file1.txt", std::ios::trunc) << "CORRUPTED CONTENT 1\n";
    std::filesystem::remove(test_root_ / "file2.txt");
    std::ofstream(test_root_ / "untracked_new.txt") << "Newly created untracked file\n";

    EXPECT_FALSE(std::filesystem::exists(test_root_ / "file2.txt"));
    EXPECT_TRUE(std::filesystem::exists(test_root_ / "untracked_new.txt"));

    // 3. Rollback full snapshot
    auto rb_res = snap_mgr_->rollbackSnapshot("snap_base");
    ASSERT_TRUE(rb_res.has_value());
    EXPECT_TRUE(rb_res.value().success);

    // 4. Verify file restoration
    std::ifstream f1(test_root_ / "file1.txt");
    std::string s1((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
    EXPECT_EQ(s1, "Hello Line 1\nHello Line 2\nHello Line 3\n");

    EXPECT_TRUE(std::filesystem::exists(test_root_ / "file2.txt"));
    std::ifstream f2(test_root_ / "file2.txt");
    std::string s2((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
    EXPECT_EQ(s2, "Alpha\nBeta\nGamma\n");

    // 5. Verify newly created untracked file was deleted
    EXPECT_FALSE(std::filesystem::exists(test_root_ / "untracked_new.txt"));
}

TEST_F(SnapshotTest, SingleFileSurgicalRollback) {
    auto snap_res = snap_mgr_->createSnapshot("snap_single", "Surgical test");
    ASSERT_TRUE(snap_res.has_value());

    // Modify file1 and file3
    std::ofstream(test_root_ / "file1.txt", std::ios::trunc) << "MODIFIED FILE 1\n";
    std::ofstream(test_root_ / "file3.txt", std::ios::trunc) << "MODIFIED FILE 3\n";

    // Rollback only file1
    auto rb_file = snap_mgr_->rollbackFile("snap_single", "file1.txt");
    ASSERT_TRUE(rb_file.has_value());
    EXPECT_TRUE(rb_file.value());

    // file1 restored
    std::ifstream f1(test_root_ / "file1.txt");
    std::string s1((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
    EXPECT_EQ(s1, "Hello Line 1\nHello Line 2\nHello Line 3\n");

    // file3 remains modified
    std::ifstream f3(test_root_ / "file3.txt");
    std::string s3((std::istreambuf_iterator<char>(f3)), std::istreambuf_iterator<char>());
    EXPECT_EQ(s3, "MODIFIED FILE 3\n");
}

TEST_F(SnapshotTest, GeneratesStandardUnifiedDiff) {
    auto snap_res = snap_mgr_->createSnapshot("snap_diff", "Diff base");
    ASSERT_TRUE(snap_res.has_value());

    // Edit file1.txt: replace Line 2 with Modified Line 2 and add Line 4
    std::ofstream(test_root_ / "file1.txt", std::ios::trunc) << "Hello Line 1\nModified Line 2\nHello Line 3\nHello Line 4\n";

    auto diff_res = snap_mgr_->generateDiffFromSnapshot("snap_diff");
    ASSERT_TRUE(diff_res.has_value());
    const auto& diff = diff_res.value();

    EXPECT_EQ(diff.total_files_changed, 1);
    EXPECT_GT(diff.total_additions, 0);
    EXPECT_GT(diff.total_deletions, 0);
    EXPECT_NE(diff.raw_unified_diff.find("--- a/file1.txt"), std::string::npos);
    EXPECT_NE(diff.raw_unified_diff.find("+++ b/file1.txt"), std::string::npos);
    EXPECT_NE(diff.raw_unified_diff.find("+Modified Line 2"), std::string::npos);
    EXPECT_NE(diff.raw_unified_diff.find("-Hello Line 2"), std::string::npos);
}

TEST_F(SnapshotTest, DiffBetweenTwoSnapshots) {
    // Snapshot A
    auto snap_a = snap_mgr_->createSnapshot("snap_a", "Version A");
    ASSERT_TRUE(snap_a.has_value());

    // Make edits and create Snapshot B
    std::ofstream(test_root_ / "file1.txt", std::ios::trunc) << "Line 1 New\nLine 2 New\n";
    std::ofstream(test_root_ / "new_in_b.txt") << "Brand new file\n";
    auto snap_b = snap_mgr_->createSnapshot("snap_b", "Version B");
    ASSERT_TRUE(snap_b.has_value());

    auto diff_ab = snap_mgr_->generateDiffBetweenSnapshots("snap_a", "snap_b");
    ASSERT_TRUE(diff_ab.has_value());
    EXPECT_GE(diff_ab.value().total_files_changed, 2);
    EXPECT_GT(diff_ab.value().total_additions, 0);
    EXPECT_GT(diff_ab.value().total_deletions, 0);
    EXPECT_NE(diff_ab.value().raw_unified_diff.find("new_in_b.txt"), std::string::npos);
}

TEST_F(SnapshotTest, ClearAndManageSnapshots) {
    auto s1 = snap_mgr_->createSnapshot("s1");
    auto s2 = snap_mgr_->createSnapshot("s2");
    ASSERT_TRUE(s1.has_value());
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(snap_mgr_->listSnapshots().size(), 2u);

    auto del_res = snap_mgr_->deleteSnapshot("s1");
    EXPECT_TRUE(del_res.has_value());
    EXPECT_EQ(snap_mgr_->listSnapshots().size(), 1u);

    snap_mgr_->clearSnapshots();
    EXPECT_EQ(snap_mgr_->listSnapshots().size(), 0u);
}

// ==========================================
// 3. Git Worktree Tests
// ==========================================

class GitWorktreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        repo_root_ = createTestTempDir("aios_git_repo");
        git_ready_ = setupTestGitRepo(repo_root_);
        worktree_mgr_ = std::make_unique<GitWorktree>(repo_root_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(repo_root_, ec);
    }

    bool git_ready_ = false;
    std::filesystem::path repo_root_;
    std::unique_ptr<GitWorktree> worktree_mgr_;
};

TEST_F(GitWorktreeTest, CreatesAndListsIsolatedWorktree) {
    if (!git_ready_) GTEST_SKIP() << "Git CLI not initialized in test environment";

    CreateWorktreeOptions opts;
    opts.worktree_id = "task_001";
    opts.branch_name = "aios/ephemeral/task_001";
    opts.create_new_branch = true;

    auto res = worktree_mgr_->createWorktree(opts);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res.value().id, "task_001");
    EXPECT_TRUE(std::filesystem::exists(res.value().path));

    auto list_res = worktree_mgr_->listWorktrees();
    ASSERT_TRUE(list_res.has_value());
    EXPECT_GE(list_res.value().size(), 2u); // main + task_001

    // Lock worktree
    auto lock_res = worktree_mgr_->lockWorktree("task_001", "Active coder session");
    EXPECT_TRUE(lock_res.has_value());

    // Unlock worktree
    auto unlock_res = worktree_mgr_->unlockWorktree("task_001");
    EXPECT_TRUE(unlock_res.has_value());

    // Remove worktree
    auto rem_res = worktree_mgr_->removeWorktree("task_001", true);
    EXPECT_TRUE(rem_res.has_value());
    EXPECT_FALSE(std::filesystem::exists(res.value().path));
}

TEST_F(GitWorktreeTest, PrunesAndRepairsWorktrees) {
    if (!git_ready_) GTEST_SKIP() << "Git CLI not initialized in test environment";

    CreateWorktreeOptions opts;
    opts.worktree_id = "task_prune";
    opts.branch_name = "aios/ephemeral/task_prune";
    opts.create_new_branch = true;

    auto res = worktree_mgr_->createWorktree(opts);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(std::filesystem::exists(res.value().path));

    // Delete directory manually from disk
    std::error_code ec;
    std::filesystem::remove_all(res.value().path, ec);

    // Call prune
    auto prune_res = worktree_mgr_->pruneWorktrees();
    EXPECT_TRUE(prune_res.has_value());

    // Call repair
    auto repair_res = worktree_mgr_->repairWorktrees();
    EXPECT_TRUE(repair_res.has_value());
}

// ==========================================
// 4. Branch Sandbox Tests
// ==========================================

class BranchSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        repo_root_ = createTestTempDir("aios_branch_repo");
        git_ready_ = setupTestGitRepo(repo_root_);
        sandbox_ = std::make_unique<BranchSandbox>(repo_root_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(repo_root_, ec);
    }

    bool git_ready_ = false;
    std::filesystem::path repo_root_;
    std::unique_ptr<BranchSandbox> sandbox_;
};

TEST_F(BranchSandboxTest, StagesAndCommitsMicroChanges) {
    if (!git_ready_) GTEST_SKIP() << "Git CLI not initialized in test environment";

    auto branch_res = sandbox_->createEphemeralBranch("task_feat_1");
    ASSERT_TRUE(branch_res.has_value());
    EXPECT_EQ(branch_res.value(), "aios/ephemeral/task_feat_1");

    // Write a new feature file
    std::ofstream(repo_root_ / "Feature.cpp") << "// Feature implementation\nvoid feature() {}\n";

    std::unordered_map<std::string, std::string> meta;
    meta["AIOS-Task-ID"] = "task_feat_1";
    meta["AIOS-Agent"] = "CoderAgent";

    auto commit_res = sandbox_->stageAndCommit("Implement core feature", {"Feature.cpp"}, meta);
    ASSERT_TRUE(commit_res.has_value());
    EXPECT_FALSE(commit_res.value().empty());

    // Switch back to main and merge
    auto co_res = sandbox_->checkoutBranch("main");
    ASSERT_TRUE(co_res.has_value());
    auto merge_res = sandbox_->merge("aios/ephemeral/task_feat_1", MergeStrategy::ThreeWay);
    ASSERT_TRUE(merge_res.has_value());
    EXPECT_TRUE(merge_res.value().success);
    EXPECT_FALSE(merge_res.value().has_conflicts);
    EXPECT_TRUE(std::filesystem::exists(repo_root_ / "Feature.cpp"));
}

TEST_F(BranchSandboxTest, StashesAndSquashMerges) {
    if (!git_ready_) GTEST_SKIP() << "Git CLI not initialized in test environment";

    // Test stash
    std::ofstream(repo_root_ / "wip.txt") << "Work in progress...\n";
    auto stash_res = sandbox_->createStash("WIP stash");
    EXPECT_TRUE(stash_res.has_value());

    auto pop_res = sandbox_->popStash(0);
    EXPECT_TRUE(pop_res.has_value());
    EXPECT_TRUE(std::filesystem::exists(repo_root_ / "wip.txt"));
    std::filesystem::remove(repo_root_ / "wip.txt");

    // Test squash merge
    auto branch_res = sandbox_->createEphemeralBranch("task_squash");
    ASSERT_TRUE(branch_res.has_value());

    std::ofstream(repo_root_ / "Squash1.cpp") << "// 1\n";
    auto c1 = sandbox_->stageAndCommit("Commit 1", {"Squash1.cpp"});
    ASSERT_TRUE(c1.has_value());

    std::ofstream(repo_root_ / "Squash2.cpp") << "// 2\n";
    auto c2 = sandbox_->stageAndCommit("Commit 2", {"Squash2.cpp"});
    ASSERT_TRUE(c2.has_value());

    auto co = sandbox_->checkoutBranch("main");
    ASSERT_TRUE(co.has_value());
    auto sq_res = sandbox_->merge("aios/ephemeral/task_squash", MergeStrategy::Squash);
    ASSERT_TRUE(sq_res.has_value());
    EXPECT_TRUE(sq_res.value().success);
    EXPECT_TRUE(std::filesystem::exists(repo_root_ / "Squash1.cpp"));
    EXPECT_TRUE(std::filesystem::exists(repo_root_ / "Squash2.cpp"));
}

// ==========================================
// 5. Workspace Manager & Tools Tests
// ==========================================

class WorkspaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        repo_root_ = createTestTempDir("aios_ws_mgr_repo");
        setupTestGitRepo(repo_root_);
        manager_ = WorkspaceManager::create(repo_root_);
    }

    void TearDown() override {
        manager_->shutdown();
        std::error_code ec;
        std::filesystem::remove_all(repo_root_, ec);
    }

    std::filesystem::path repo_root_;
    std::shared_ptr<WorkspaceManager> manager_;
};

TEST_F(WorkspaceTest, ManagesWorkspaceFacadeAndSnapshots) {
    ASSERT_NE(manager_, nullptr);
    EXPECT_NE(manager_->getPathContainment(), nullptr);
    EXPECT_NE(manager_->getSnapshotManager(), nullptr);
    EXPECT_NE(manager_->getWorktreeEngine(), nullptr);
    EXPECT_NE(manager_->getBranchSandbox(), nullptr);

    // Create file
    std::ofstream(repo_root_ / "config.json") << "{\"version\": 1}\n";

    // Test facade createSnapshot
    auto snap_res = manager_->createSnapshot(repo_root_);
    ASSERT_TRUE(snap_res.has_value());
    std::string snap_id = snap_res.value();

    // Modify file
    std::ofstream(repo_root_ / "config.json") << "{\"version\": 2}\n";

    // Test facade computeDiff
    auto diff_res = manager_->computeDiff(snap_id, repo_root_);
    ASSERT_TRUE(diff_res.has_value());
    EXPECT_NE(diff_res.value().find("+{\"version\": 2}"), std::string::npos);

    // Test facade rollback
    auto rb_res = manager_->rollback(snap_id, repo_root_);
    EXPECT_TRUE(rb_res.has_value());

    std::ifstream cfg(repo_root_ / "config.json");
    std::string content((std::istreambuf_iterator<char>(cfg)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "{\"version\": 1}\n");
}

TEST_F(WorkspaceTest, WorkspaceToolsRegistrationAndExecution) {
    aios::ToolRegistry& registry = aios::ToolRegistry::instance();
    registry.initialize();

    bool reg_ok = WorkspaceTools::registerTools(registry, manager_);
    EXPECT_TRUE(reg_ok);

    auto tool = registry.getTool("workspace");
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->getDefinition().name, "workspace");

    // Execute validate_path tool call
    std::unordered_map<std::string, std::string> val_params;
    val_params["operation"] = "validate_path";
    val_params["path"] = "src/code.cpp";
    auto val_res = registry.executeTool("workspace", val_params);
    EXPECT_TRUE(val_res.success);

    // Execute path traversal violation via tool
    std::unordered_map<std::string, std::string> trav_params;
    trav_params["operation"] = "validate_path";
    trav_params["path"] = "../../system32";
    auto trav_res = registry.executeTool("workspace", trav_params);
    EXPECT_TRUE(trav_res.success); // Tool execution succeeds, output contains is_valid: false
    EXPECT_NE(trav_res.output.find("\"is_valid\": false"), std::string::npos);
}

TEST_F(WorkspaceTest, AllocatesAndReleasesAgentWorkspaces) {
    auto ws1 = manager_->allocateIsolatedAgentWorkspace("agent_coder_1");
    ASSERT_TRUE(ws1.has_value());
    EXPECT_EQ(ws1.value().task_id, "agent_coder_1");
    EXPECT_TRUE(std::filesystem::exists(ws1.value().path));

    auto ws2 = manager_->allocateIsolatedAgentWorkspace("agent_coder_2");
    ASSERT_TRUE(ws2.has_value());
    EXPECT_EQ(ws2.value().task_id, "agent_coder_2");
    EXPECT_TRUE(std::filesystem::exists(ws2.value().path));

    // Release workspaces
    auto rel1 = manager_->releaseAgentWorkspace("agent_coder_1", false);
    EXPECT_TRUE(rel1.has_value());

    auto rel2 = manager_->releaseAgentWorkspace("agent_coder_2", false);
    EXPECT_TRUE(rel2.has_value());
}
