#ifndef GITMANAGER_H
#define GITMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QFuture>
#include <QMutex>

namespace Brahma {

struct GitStatus {
    QString branch;
    bool isDirty = false;
    int aheadCount = 0;
    int behindCount = 0;
    QStringList stagedFiles;
    QStringList unstagedFiles;
    QStringList untrackedFiles;
    QString lastCommitHash;
    QString lastCommitMessage;
    QDateTime lastCommitTime;
};

struct GitCommit {
    QString hash;
    QString shortHash;
    QString author;
    QString email;
    QDateTime date;
    QString message;
    QStringList parents;
    int additions = 0;
    int deletions = 0;
};

struct GitDiff {
    QString filePath;
    QString oldContent;
    QString newContent;
    QStringList addedLines;
    QStringList removedLines;
    int additions = 0;
    int deletions = 0;
    bool isBinary = false;
    bool isNewFile = false;
    bool isDeleted = false;
    bool isRenamed = false;
    QString oldPath; // For renames
};

struct GitBranch {
    QString name;
    bool isCurrent = false;
    bool isRemote = false;
    QString upstream;
    QString lastCommitHash;
    QDateTime lastCommitDate;
};

struct GitRemote {
    QString name;
    QString url;
    QStringList fetchUrls;
    QStringList pushUrls;
};

class GitManager : public QObject {
    Q_OBJECT

public:
    explicit GitManager(QObject* parent = nullptr);
    ~GitManager();

    // Repository Management
    bool openRepository(const QString& repoPath);
    void closeRepository();
    bool isOpen() const;
    QString repositoryPath() const;

    // Status & Information
    GitStatus getStatus() const;
    GitStatus getDetailedStatus() const;
    QVector<GitCommit> getLog(int maxCommits = 50) const;
    QVector<GitBranch> getBranches(bool includeRemote = true) const;
    QVector<GitRemote> getRemotes() const;
    QString getCurrentBranch() const;
    QString getHEAD() const;

    // Staging Operations
    QFuture<bool> stageFile(const QString& filePath);
    QFuture<bool> stageAll();
    QFuture<bool> unstageFile(const QString& filePath);
    QFuture<bool> unstageAll();

    // Commit Operations
    QFuture<bool> commit(const QString& message, bool amend = false);
    QFuture<bool> commitWithFiles(const QString& message, const QStringList& files);
    
    // Branch Operations
    QFuture<bool> createBranch(const QString& branchName, const QString& startPoint = "HEAD");
    QFuture<bool> checkoutBranch(const QString& branchName);
    QFuture<bool> deleteBranch(const QString& branchName, bool force = false);
    QFuture<bool> renameBranch(const QString& oldName, const QString& newName);
    
    // Remote Operations
    QFuture<bool> fetch(const QString& remote = "origin", const QString& refspec = {});
    QFuture<bool> pull(const QString& remote = "origin", const QString& branch = {});
    QFuture<bool> push(const QString& remote = "origin", const QString& branch = {}, bool setUpstream = false);
    QFuture<bool> addRemote(const QString& name, const QString& url);
    QFuture<bool> removeRemote(const QString& name);

    // Diff Operations
    QVector<GitDiff> getStagedDiff() const;
    QVector<GitDiff> getUnstagedDiff() const;
    QVector<GitDiff> getDiffBetweenCommits(const QString& commit1, const QString& commit2) const;
    GitDiff getFileDiff(const QString& filePath, bool staged = false) const;
    QString getFullDiff(const QString& commit1 = {}, const QString& commit2 = {}) const;

    // Merge & Rebase
    QFuture<bool> merge(const QString& branch, bool noFastForward = false);
    QFuture<bool> rebase(const QString& branch);
    QFuture<bool> abortMerge();
    QFuture<bool> abortRebase();
    bool isMerging() const;
    bool isRebasing() const;

    // Stash Operations
    QFuture<bool> stash(const QString& message = {});
    QFuture<bool> stashPop();
    QFuture<bool> stashApply(int index = 0);
    QFuture<bool> stashDrop(int index = 0);
    QStringList listStashes() const;

    // File Operations
    QFuture<bool> addFile(const QString& filePath);
    QFuture<bool> removeFile(const QString& filePath, bool cached = false);
    QFuture<bool> moveFile(const QString& src, const QString& dst);
    QFuture<bool> restoreFile(const QString& filePath, const QString& source = "HEAD");
    QFuture<bool> cleanFile(const QString& filePath);

    // Tag Operations
    QFuture<bool> createTag(const QString& tagName, const QString& message = {}, bool annotated = false);
    QFuture<bool> deleteTag(const QString& tagName);
    QStringList listTags(const QString& pattern = "*") const;

    // Blame
    struct BlameLine {
        QString commitHash;
        QString author;
        QDateTime date;
        int lineNumber;
        QString content;
    };
    QVector<BlameLine> blame(const QString& filePath) const;

    // Search
    struct SearchMatch {
        QString filePath;
        int lineNumber;
        QString content;
        QString commitHash;
    };
    QVector<SearchMatch> searchInHistory(const QString& pattern, int maxResults = 100) const;

    // Configuration
    QString getConfig(const QString& key) const;
    bool setConfig(const QString& key, const QString& value);
    QMap<QString, QString> getAllConfig() const;

    // Hooks
    bool installHook(const QString& hookType, const QString& script);
    bool uninstallHook(const QString& hookType);

signals:
    void repositoryOpened(const QString& path);
    void repositoryClosed();
    void statusChanged(const GitStatus& status);
    void branchChanged(const QString& branch);
    void operationStarted(const QString& operation);
    void operationCompleted(const QString& operation, bool success);
    void operationFailed(const QString& operation, const QString& error);
    void progress(const QString& operation, int current, int total);

private:
    struct Private;
    QScopedPointer<Private> d;

    QStringList executeGitCommand(const QStringList& args, bool* success = nullptr) const;
    GitCommit parseCommitInfo(const QStringList& output) const;
    GitDiff parseDiffOutput(const QString& output, const QString& filePath) const;
};

} // namespace Brahma

#endif // GITMANAGER_H
