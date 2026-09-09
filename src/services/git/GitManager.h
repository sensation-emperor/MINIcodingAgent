#ifndef GITMANAGER_H
#define GITMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QProcess>
#include <QMutex>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace Brahma {

struct GitStatus {
    QString filePath;
    QString status; // M, A, D, R, U, ?
    QString stagedStatus;
    bool isStaged;
    bool isTracked;
};

struct GitCommit {
    QString hash;
    QString shortHash;
    QString author;
    QString email;
    QDateTime date;
    QString message;
    QStringList parents;
    int additions;
    int deletions;
};

struct GitBranch {
    QString name;
    bool isCurrent;
    bool isRemote;
    QString upstream;
    int ahead;
    int behind;
};

struct GitDiff {
    QString filePath;
    QString oldPath;
    QString newPath;
    QString diff;
    int additions;
    int deletions;
    bool isBinary;
    QString status; // added, modified, deleted, renamed
};

class GitManager : public QObject
{
    Q_OBJECT

public:
    explicit GitManager(QObject *parent = nullptr);
    ~GitManager();

    // Repository Management
    bool isRepository(const QString &path) const;
    bool openRepository(const QString &path);
    bool initRepository(const QString &path);
    bool cloneRepository(const QString &url, const QString &destination, const QString &branch = "");
    void closeRepository();
    QString repositoryPath() const { return m_repoPath; }
    bool isOpen() const { return !m_repoPath.isEmpty(); }

    // Status & Info
    QVector<GitStatus> getStatus() const;
    QStringList getUntrackedFiles() const;
    QStringList getModifiedFiles() const;
    QStringList getStagedFiles() const;
    GitBranch currentBranch() const;
    QVector<GitBranch> getAllBranches() const;
    QVector<GitBranch> getRemoteBranches() const;
    QString getCurrentCommitHash() const;
    QVector<GitCommit> getLog(int count = 20) const;

    // Staging Operations
    bool stageFile(const QString &filePath);
    bool stageAll();
    bool unstageFile(const QString &filePath);
    bool unstageAll();

    // Commit Operations
    bool commit(const QString &message);
    bool amendCommit(const QString &message = "");
    bool createTag(const QString &tagName, const QString &message = "");

    // Branch Operations
    bool createBranch(const QString &branchName, const QString &startPoint = "");
    bool checkoutBranch(const QString &branchName);
    bool deleteBranch(const QString &branchName, bool force = false);
    bool mergeBranch(const QString &branchName, bool noFastForward = false);
    bool rebaseBranch(const QString &branchName);

    // Remote Operations
    bool fetch(const QString &remote = "origin");
    bool pull(const QString &remote = "origin", const QString &branch = "");
    bool push(const QString &remote = "origin", const QString &branch = "", bool setUpstream = false);
    QStringList getRemotes() const;
    bool addRemote(const QString &name, const QString &url);
    bool removeRemote(const QString &name);

    // Diff Operations
    GitDiff getDiff(const QString &filePath = "") const;
    GitDiff getDiffBetweenCommits(const QString &commit1, const QString &commit2) const;
    QString getFileContent(const QString &filePath, const QString &commit = "") const;

    // Stash Operations
    bool stash(const QString &message = "");
    bool stashPop();
    bool stashApply(int index = 0);
    bool stashDrop(int index = 0);
    QStringList listStashes() const;

    // Utility
    static bool isGitInstalled();
    static QString gitVersion();
    QStringList resolveConflicts() const;
    bool abortMerge();
    bool abortRebase();

signals:
    void repositoryOpened(const QString &path);
    void repositoryClosed();
    void statusChanged();
    void branchChanged(const QString &branchName);
    void commitCreated(const QString &hash);
    void operationStarted(const QString &operation);
    void operationCompleted(const QString &operation, bool success);
    void errorOccurred(const QString &message);
    void progressUpdated(int percent, const QString &message);

private slots:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString executeGit(const QStringList &args, bool *success = nullptr) const;
    bool executeGitAsync(const QStringList &args);
    void parseStatusOutput(const QString &output, QVector<GitStatus> &statuses) const;
    void parseLogOutput(const QString &output, QVector<GitCommit> &commits) const;
    void parseBranchOutput(const QString &output, QVector<GitBranch> &branches) const;
    QString relativeFilePath(const QString &filePath) const;

    QString m_repoPath;
    mutable QProcess m_gitProcess;
    mutable QMutex m_mutex;
    QTimer m_statusWatchTimer;
    bool m_isOperationInProgress;
    QMap<QString, QString> m_pendingOperations;
};

} // namespace Brahma

#endif // GITMANAGER_H
