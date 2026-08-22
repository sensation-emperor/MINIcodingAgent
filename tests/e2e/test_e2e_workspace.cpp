#include <gtest/gtest.h>
#include "test_e2e_harness.h"

using namespace aios;
using namespace aios::workspace;
using namespace aios::e2e;

class E2EWorkspaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        harness_.setUp("workspace");
    }

    void TearDown() override {
        harness_.tearDown();
    }

    E2ETestHarness harness_;
};

// ============================================================================
// TIER 1: FEATURE COVERAGE (Features 9 - 14)
// ============================================================================

// ----------------------------------------------------------------------------
// Feature 9: Git Worktree Lifecycle
// ----------------------------------------------------------------------------

TEST_F(E2EWorkspaceTest, Tier1_GitWorktree_CreationAndDescriptorTracking) {
    auto worktree_engine = harness_.getWorktreeEngine();
    CreateWorktreeOptions opts;
    opts.worktree_id = "task_1001";
    opts.branch_name = "aios/ephemeral/task_1001";
    opts.base_ref = "HEAD";
    opts.create_new_branch = true;

    auto result = worktree_engine->createWorktree(opts);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, "task_1001");
    EXPECT_EQ(result->branch, "aios/ephemeral/task_1001");
    EXPECT_TRUE(worktree_engine->hasWorktree("task_1001"));
}

TEST_F(E2EWorkspaceTest, Tier1_GitWorktree_LockingAndUnlocking) {
    auto worktree_engine = harness_.getWorktreeEngine();
    CreateWorktreeOptions opts;
    opts.worktree_id = "task_lock_test";
    opts.branch_name = "aios/lock_test";
    auto wt = worktree_engine->createWorktree(opts);
    ASSERT_TRUE(wt.has_value());

    auto lock_res = worktree_engine->lockWorktree("task_lock_test", "Agent actively compiling");
    ASSERT_TRUE(lock_res.has_value());
    EXPECT_TRUE(lock_res.value());

    auto wt_info = worktree_engine->getWorktree("task_lock_test");
    ASSERT_TRUE(wt_info.has_value());
    EXPECT_TRUE(wt_info->is_locked);
    EXPECT_EQ(wt_info->lock_reason, "Agent actively compiling");

    auto unlock_res = worktree_engine->unlockWorktree("task_lock_test");
    ASSERT_TRUE(unlock_res.has_value());
    EXPECT_TRUE(unlock_res.value());

    auto wt_unlocked = worktree_engine->getWorktree("task_lock_test");
    ASSERT_TRUE(wt_unlocked.has_value());
    EXPECT_FALSE(wt_unlocked->is_locked);
}

TEST_F(E2EWorkspaceTest, Tier1_GitWorktree_ListWorktrees) {
    auto worktree_engine = harness_.getWorktreeEngine();
    CreateWorktreeOptions opts1;
    opts1.worktree_id = "wt_1";
    opts1.branch_name = "branch_1";
    worktree_engine->createWorktree(opts1);

    CreateWorktreeOptions opts2;
    opts2.worktree_id = "wt_2";
    opts2.branch_name = "branch_2";
    worktree_engine->createWorktree(opts2);

    auto list_res = worktree_engine->listWorktrees();
    ASSERT_TRUE(list_res.has_value());
    EXPECT_GE(list_res->size(), 2);
}

TEST_F(E2EWorkspaceTest, Tier1_GitWorktree_RemovalAndPruning) {
    auto worktree_engine = harness_.getWorktreeEngine();
    CreateWorktreeOptions opts;
    opts.worktree_id = "wt_to_remove";
    opts.branch_name = "branch_rm";
    auto wt = worktree_engine->createWorktree(opts);
    ASSERT_TRUE(wt.has_value());

    auto rm_res = worktree_engine->removeWorktree("wt_to_remove", true);
    ASSERT_TRUE(rm_res.has_value());
    EXPECT_TRUE(rm_res.value());
    EXPECT_FALSE(worktree_engine->hasWorktree("wt_to_remove"));

    auto prune_res = worktree_engine->pruneWorktrees();
    ASSERT_TRUE(prune_res.has_value());
}

TEST_F(E2EWorkspaceTest, Tier1_GitWorktree_RepairWorktrees) {
    auto worktree_engine = harness_.getWorktreeEngine();
    auto repair_res = worktree_engine->repairWorktrees();
    ASSERT_TRUE(repair_res.has_value());
    EXPECT_TRUE(repair_res.value());
}

// ----------------------------------------------------------------------------
// Feature 10: Ephemeral Branch Sandbox
// ----------------------------------------------------------------------------

TEST_F(E2EWorkspaceTest, Tier1_BranchSandbox_CreateAndListEphemeralBranches) {
    auto branch_sandbox = harness_.getBranchSandbox();
    auto b1 = branch_sandbox->createEphemeralBranch("task_1");
    ASSERT_TRUE(b1.has_value());
    EXPECT_NE(b1->find("aios/ephemeral/task_1"), std::string::npos);

    auto b2 = branch_sandbox->createEphemeralBranch("task_2");
    ASSERT_TRUE(b2.has_value());

    auto branches = branch_sandbox->listEphemeralBranches();
    ASSERT_TRUE(branches.has_value());
    EXPECT_GE(branches->size(), 2);
}

TEST_F(E2EWorkspaceTest, Tier1_BranchSandbox_StageAndCommit) {
    auto branch_sandbox = harness_.getBranchSandbox();
    harness_.createSourceFile("src/file1.cpp", "int a = 1;");
    
    std::unordered_map<std::string, std::string> metadata;
    metadata["AIOS-Agent"] = "CoderAgent";
    metadata["AIOS-Task-ID"] = "task_42";

    auto commit_res = branch_sandbox->stageAndCommit("Initial micro-commit", {"src/file1.cpp"}, metadata);
    ASSERT_TRUE(commit_res.has_value());
    EXPECT_FALSE(commit_res->empty());
}

TEST_F(E2EWorkspaceTest, Tier1_BranchSandbox_StashAndPop) {
    auto branch_sandbox = harness_.getBranchSandbox();
    harness_.createSourceFile("src/stash_file.cpp", "int stashed = 100;");

    auto stash_res = branch_sandbox->createStash("Work in progress");
    ASSERT_TRUE(stash_res.has_value());

    auto pop_res = branch_sandbox->popStash(0);
    ASSERT_TRUE(pop_res.has_value());
    EXPECT_TRUE(pop_res.value());
}

TEST_F(E2EWorkspaceTest, Tier1_BranchSandbox_MergeOperations) {
    auto branch_sandbox = harness_.getBranchSandbox();
    auto branch_name = branch_sandbox->createEphemeralBranch("merge_task");
    ASSERT_TRUE(branch_name.has_value());

    auto merge_res = branch_sandbox->merge(branch_name.value(), MergeStrategy::ThreeWay);
    ASSERT_TRUE(merge_res.has_value());
    EXPECT_TRUE(merge_res->success);
}

TEST_F(E2EWorkspaceTest, Tier1_BranchSandbox_DiffInspection) {
    auto branch_sandbox = harness_.getBranchSandbox();
    harness_.createSourceFile("src/diff_test.cpp", "int x = 10;\nint y = 20;");
    
    auto uncommitted = branch_sandbox->getUncommittedDiff();
    ASSERT_TRUE(uncommitted.has_value());
    EXPECT_FALSE(uncommitted->empty());
}

// ----------------------------------------------------------------------------
// Feature 12: Path Containment Security
// ----------------------------------------------------------------------------

TEST_F(E2EWorkspaceTest, Tier1_PathContainment_ValidatesInsideWorkspace) {
    auto containment = harness_.getPathContainment();
    auto valid_file = harness_.getSandboxRoot() / "src/main.cpp";
    
    auto result = containment->validate(valid_file, AccessMode::ReadWrite);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FALSE(result.is_protected);
    EXPECT_TRUE(containment->isContained(valid_file));
}

TEST_F(E2EWorkspaceTest, Tier1_PathContainment_BlocksDirectoryTraversal) {
    auto containment = harness_.getPathContainment();
    auto traversal_path = harness_.getSandboxRoot() / "../../Windows/System32/cmd.exe";
    
    auto result = containment->validate(traversal_path, AccessMode::ReadWrite);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(containment->isContained(traversal_path));
    EXPECT_NE(result.violation_reason.find("traversal"), std::string::npos);
}

TEST_F(E2EWorkspaceTest, Tier1_PathContainment_ProtectsGitMetadata) {
    auto containment = harness_.getPathContainment();
    auto git_config = harness_.getSandboxRoot() / ".git/config";
    
    auto result = containment->validate(git_config, AccessMode::ReadWrite);
    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(result.is_protected);
    EXPECT_TRUE(containment->isProtected(git_config));
}

TEST_F(E2EWorkspaceTest, Tier1_PathContainment_ProtectsSecretFiles) {
    auto containment = harness_.getPathContainment();
    auto env_file = harness_.getSandboxRoot() / ".env";
    auto key_file = harness_.getSandboxRoot() / "id_rsa";
    
    EXPECT_TRUE(containment->isProtected(env_file));
    EXPECT_TRUE(containment->isProtected(key_file));

    auto res_env = containment->validate(env_file, AccessMode::ReadWrite);
    EXPECT_FALSE(res_env.is_valid);
    EXPECT_TRUE(res_env.is_protected);
}

TEST_F(E2EWorkspaceTest, Tier1_PathContainment_CustomProtectedPatternsAndExternalAllowedPaths) {
    auto containment = harness_.getPathContainment();
    containment->addProtectedPattern("*.secret");
    
    auto custom_secret = harness_.getSandboxRoot() / "database.secret";
    EXPECT_TRUE(containment->isProtected(custom_secret));

    auto ext_path = std::filesystem::temp_directory_path() / "allowed_shared_lib";
    containment->addAllowedExternalPath(ext_path, AccessMode::ReadOnly);
    auto res_ext = containment->validate(ext_path, AccessMode::ReadOnly);
    EXPECT_TRUE(res_ext.is_valid);
}

// ----------------------------------------------------------------------------
// Feature 13: Snapshot & Rollback Engine
// ----------------------------------------------------------------------------

TEST_F(E2EWorkspaceTest, Tier1_SnapshotManager_CreateAndGetSnapshot) {
    auto snap_mgr = harness_.getSnapshotManager();
    harness_.createSourceFile("src/engine.cpp", "class Engine { public: void start(); };");
    harness_.createSourceFile("src/engine.h", "#pragma once\nclass Engine;");

    auto snap_res = snap_mgr->createSnapshot("snap_1", "Initial clean state");
    ASSERT_TRUE(snap_res.has_value());
    EXPECT_EQ(snap_res->id, "snap_1");
    EXPECT_EQ(snap_res->description, "Initial clean state");
    EXPECT_EQ(snap_res->files.size(), 2);

    auto retrieved = snap_mgr->getSnapshot("snap_1");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->files.size(), 2);
}

TEST_F(E2EWorkspaceTest, Tier1_SnapshotManager_UnifiedDiffGeneration) {
    auto snap_mgr = harness_.getSnapshotManager();
    harness_.createSourceFile("src/diff_demo.cpp", "int calculate() {\n    return 10;\n}\n");
    
    auto snap1 = snap_mgr->createSnapshot("snap_v1", "Version 1");
    ASSERT_TRUE(snap1.has_value());

    // Modify file
    harness_.createSourceFile("src/diff_demo.cpp", "int calculate() {\n    return 20;\n}\n");

    auto diff_res = snap_mgr->generateDiffFromSnapshot("snap_v1");
    ASSERT_TRUE(diff_res.has_value());
    EXPECT_EQ(diff_res->total_files_changed, 1);
    EXPECT_GT(diff_res->total_additions, 0);
    EXPECT_GT(diff_res->total_deletions, 0);
}

TEST_F(E2EWorkspaceTest, Tier1_SnapshotManager_FullWorkspaceRollback) {
    auto snap_mgr = harness_.getSnapshotManager();
    harness_.createSourceFile("src/fileA.cpp", "int a = 1;");
    harness_.createSourceFile("src/fileB.cpp", "int b = 2;");

    auto snap = snap_mgr->createSnapshot("checkpoint_alpha", "Baseline");
    ASSERT_TRUE(snap.has_value());

    // Modify fileA, delete fileB, create fileC
    harness_.createSourceFile("src/fileA.cpp", "int a = 999;");
    harness_.removeSourceFile("src/fileB.cpp");
    harness_.createSourceFile("src/fileC.cpp", "int c = 3;");

    auto rollback_res = snap_mgr->rollbackSnapshot("checkpoint_alpha");
    ASSERT_TRUE(rollback_res.has_value());
    EXPECT_TRUE(rollback_res->success);

    // Verify fileA restored
    EXPECT_EQ(harness_.readSourceFile("src/fileA.cpp"), "int a = 1;");
    // Verify fileB restored
    EXPECT_EQ(harness_.readSourceFile("src/fileB.cpp"), "int b = 2;");
    // Verify fileC removed
    EXPECT_FALSE(harness_.fileExists("src/fileC.cpp"));
}

TEST_F(E2EWorkspaceTest, Tier1_SnapshotManager_SingleFileRollback) {
    auto snap_mgr = harness_.getSnapshotManager();
    harness_.createSourceFile("src/mod1.cpp", "int m1 = 1;");
    harness_.createSourceFile("src/mod2.cpp", "int m2 = 2;");

    snap_mgr->createSnapshot("snap_single", "Checkpoint");

    harness_.createSourceFile("src/mod1.cpp", "int m1 = 999;");
    harness_.createSourceFile("src/mod2.cpp", "int m2 = 888;");

    auto rb_res = snap_mgr->rollbackFile("snap_single", "src/mod1.cpp");
    ASSERT_TRUE(rb_res.has_value());
    EXPECT_TRUE(rb_res.value());

    EXPECT_EQ(harness_.readSourceFile("src/mod1.cpp"), "int m1 = 1;");
    EXPECT_EQ(harness_.readSourceFile("src/mod2.cpp"), "int m2 = 888;"); // mod2 remains modified
}

TEST_F(E2EWorkspaceTest, Tier1_SnapshotManager_DeleteAndClearSnapshots) {
    auto snap_mgr = harness_.getSnapshotManager();
    harness_.createSourceFile("src/test.cpp", "int t = 0;");
    snap_mgr->createSnapshot("s1", "first");
    snap_mgr->createSnapshot("s2", "second");

    EXPECT_EQ(snap_mgr->listSnapshots().size(), 2);
    
    auto del_res = snap_mgr->deleteSnapshot("s1");
    ASSERT_TRUE(del_res.has_value());
    EXPECT_EQ(snap_mgr->listSnapshots().size(), 1);

    snap_mgr->clearSnapshots();
    EXPECT_EQ(snap_mgr->listSnapshots().size(), 0);
}

// ----------------------------------------------------------------------------
// Feature 14: Workspace Tools & ToolRegistry Integration
// ----------------------------------------------------------------------------

TEST_F(E2EWorkspaceTest, Tier1_WorkspaceTools_RegisteredAndExecutable) {
    auto tool_registry = harness_.getToolRegistry();
    EXPECT_TRUE(tool_registry->hasTool("workspace_create_sandbox"));
    EXPECT_TRUE(tool_registry->hasTool("workspace_snapshot"));
    EXPECT_TRUE(tool_registry->hasTool("workspace_rollback"));
    EXPECT_TRUE(tool_registry->hasTool("workspace_diff"));

    nlohmann::json snap_params;
    snap_params["description"] = "Tool snapshot";
    snap_params["snapshot_id"] = "tool_snap_1";

    auto res = tool_registry->executeTool("workspace_snapshot", snap_params.dump());
    EXPECT_TRUE(res.success);
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASES
// ============================================================================

TEST_F(E2EWorkspaceTest, Tier2_PathContainment_AbsoluteAndRelativeComplexJailBreak) {
    auto containment = harness_.getPathContainment();
    
    // Deep backslashes / slashes escape
    auto bad_path1 = harness_.getSandboxRoot() / "sub/../../../../etc/shadow";
    EXPECT_FALSE(containment->isContained(bad_path1));

    // Windows device paths or root escapes
    auto bad_path2 = std::filesystem::path("C:/Windows/explorer.exe");
    EXPECT_FALSE(containment->isContained(bad_path2));
}

TEST_F(E2EWorkspaceTest, Tier2_SnapshotManager_NonExistentSnapshotReturnsError) {
    auto snap_mgr = harness_.getSnapshotManager();
    auto res = snap_mgr->rollbackSnapshot("non_existent_snapshot_id");
    EXPECT_FALSE(res.has_value());
}

TEST_F(E2EWorkspaceTest, Tier2_SnapshotManager_LargeFilePayloadSnapshot) {
    auto snap_mgr = harness_.getSnapshotManager();
    std::string large_code(500000, 'X'); // 500KB file
    harness_.createSourceFile("src/large_file.cpp", large_code);

    auto snap = snap_mgr->createSnapshot("large_snap", "Snapshot with large file");
    ASSERT_TRUE(snap.has_value());
    EXPECT_GE(snap->total_size_bytes, 500000);

    harness_.createSourceFile("src/large_file.cpp", "overwritten");
    auto rb = snap_mgr->rollbackSnapshot("large_snap");
    ASSERT_TRUE(rb.has_value());
    EXPECT_EQ(harness_.readSourceFile("src/large_file.cpp").size(), 500000);
}

TEST_F(E2EWorkspaceTest, Tier2_BranchSandbox_DuplicateBranchCreationError) {
    auto branch_sandbox = harness_.getBranchSandbox();
    auto b1 = branch_sandbox->createEphemeralBranch("unique_task");
    ASSERT_TRUE(b1.has_value());

    auto b2 = branch_sandbox->createEphemeralBranch("unique_task");
    ASSERT_TRUE(b2.has_value());
    EXPECT_NE(b1.value(), b2.value()); // Timestamp suffix ensures uniqueness
}

TEST_F(E2EWorkspaceTest, Tier2_GitWorktree_DuplicateWorktreeIdHandling) {
    auto worktree_engine = harness_.getWorktreeEngine();
    CreateWorktreeOptions opts;
    opts.worktree_id = "dup_id";
    opts.branch_name = "dup_branch";
    
    auto r1 = worktree_engine->createWorktree(opts);
    ASSERT_TRUE(r1.has_value());

    auto r2 = worktree_engine->createWorktree(opts);
    EXPECT_FALSE(r2.has_value());
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATIONS
// ============================================================================

TEST_F(E2EWorkspaceTest, Tier3_WorkspaceManager_AllocateAndReleaseAgentIsolation) {
    auto ws_mgr = harness_.getWorkspaceManager();
    auto alloc_res = ws_mgr->allocateIsolatedAgentWorkspace("agent_coder_1", "HEAD");
    ASSERT_TRUE(alloc_res.has_value());
    EXPECT_EQ(alloc_res->owner_task_id, "agent_coder_1");

    auto rel_res = ws_mgr->releaseAgentWorkspace("agent_coder_1", false);
    ASSERT_TRUE(rel_res.has_value());
    EXPECT_TRUE(rel_res.value());
}

TEST_F(E2EWorkspaceTest, Tier3_ProjectContract_VirtualMethods) {
    auto ws_mgr = harness_.getWorkspaceManager();
    
    auto wt_res = ws_mgr->createWorktree("task_contract_test", "HEAD");
    ASSERT_TRUE(wt_res.has_value());

    auto snap_res = ws_mgr->createSnapshot(harness_.getSandboxRoot());
    ASSERT_TRUE(snap_res.has_value());

    auto diff_res = ws_mgr->computeDiff(snap_res.value(), harness_.getSandboxRoot());
    ASSERT_TRUE(diff_res.has_value());

    auto rm_res = ws_mgr->removeWorktree("task_contract_test", true);
    ASSERT_TRUE(rm_res.has_value());
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO
// ============================================================================

TEST_F(E2EWorkspaceTest, Tier4_MultiAgentIsolatedSandboxWorkflow) {
    auto ws_mgr = harness_.getWorkspaceManager();
    
    // 1. Orchestrator allocates isolated sandbox for CoderAgent
    auto coder_ws = ws_mgr->allocateIsolatedAgentWorkspace("task_feature_x", "HEAD");
    ASSERT_TRUE(coder_ws.has_value());

    // 2. Snapshot baseline state
    auto baseline_snap = ws_mgr->createSnapshot(harness_.getSandboxRoot());
    ASSERT_TRUE(baseline_snap.has_value());

    // 3. CoderAgent writes feature code within workspace
    harness_.createSourceFile("src/feature_x.cpp", "int featureX() { return 100; }");

    // 4. Validate path containment
    EXPECT_TRUE(ws_mgr->isPathContained(harness_.getSandboxRoot() / "src/feature_x.cpp", harness_.getSandboxRoot()));

    // 5. Compute diff of changes
    auto diff = ws_mgr->computeDiff(baseline_snap.value(), harness_.getSandboxRoot());
    ASSERT_TRUE(diff.has_value());
    EXPECT_FALSE(diff.value().empty());

    // 6. Release workspace and merge changes
    auto release = ws_mgr->releaseAgentWorkspace("task_feature_x", true);
    ASSERT_TRUE(release.has_value());
    EXPECT_TRUE(release.value());
}
