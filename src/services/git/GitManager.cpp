#include "GitManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QProcessEnvironment>

namespace Brahma {

GitManager::GitManager(QObject *parent)
    : QObject(parent)
    , m_isOperationInProgress(false)
{
    m_statusWatchTimer.setInterval(2000); // Check every 2 seconds
    connect(&m_statusWatchTimer, &QTimer::timeout, this, [this]() {
        if (isOpen() && !m_isOperationInProgress) {
            emit statusChanged();
        }
    });
}

GitManager::~GitManager()
{
    closeRepository();
}

bool GitManager::isGitInstalled()
{
    QProcess process;
    process.start("git", QStringList() << "--version");
    return process.waitForFinished(5000) && process.exitCode() == 0;
}

QString GitManager::gitVersion()
{
    QProcess process;
    process.start("git", QStringList() << "--version");
    if (process.waitForFinished(5000) && process.exitCode() == 0) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    return QString();
}

bool GitManager::isRepository(const QString &path) const
{
    QDir dir(path);
    return dir.exists(".git");
}

bool GitManager::openRepository(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isRepository(path)) {
        emit errorOccurred(QString("Not a git repository: %1").arg(path));
        return false;
    }
    
    m_repoPath = QDir(path).absolutePath();
    m_statusWatchTimer.start();
    
    emit repositoryOpened(m_repoPath);
    emit statusChanged();
    
    return true;
}

bool GitManager::initRepository(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    
    bool success;
    QString output = executeGit(QStringList() << "-C" << path << "init", &success);
    
    if (success) {
        openRepository(path);
        return true;
    }
    
    emit errorOccurred(QString("Failed to initialize repository: %1").arg(output));
    return false;
}

bool GitManager::cloneRepository(const QString &url, const QString &destination, const QString &branch)
{
    QMutexLocker locker(&m_mutex);
    
    QStringList args;
    args << "clone";
    
    if (!branch.isEmpty()) {
        args << "-b" << branch;
    }
    
    args << url << destination;
    
    emit operationStarted("clone");
    bool success = executeGitAsync(args);
    
    if (success) {
        // Will emit operationCompleted in onProcessFinished
        return true;
    }
    
    emit operationCompleted("clone", false);
    return false;
}

void GitManager::closeRepository()
{
    QMutexLocker locker(&m_mutex);
    m_statusWatchTimer.stop();
    m_repoPath.clear();
    emit repositoryClosed();
}

QString GitManager::executeGit(const QStringList &args, bool *success) const
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    
    if (!m_repoPath.isEmpty()) {
        process.setWorkingDirectory(m_repoPath);
    }
    
    process.start("git", args);
    
    if (!process.waitForFinished(30000)) {
        if (success) *success = false;
        return "Git operation timed out";
    }
    
    if (success) *success = (process.exitCode() == 0);
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QString error = QString::fromUtf8(process.readAllStandardError());
    
    return output.trimmed() + (error.isEmpty() ? "" : "\n" + error.trimmed());
}

bool GitManager::executeGitAsync(const QStringList &args)
{
    if (m_isOperationInProgress) {
        emit errorOccurred("Another git operation is in progress");
        return false;
    }
    
    m_gitProcess.setWorkingDirectory(m_repoPath);
    m_gitProcess.start("git", args);
    
    if (!m_gitProcess.waitForStarted(5000)) {
        m_isOperationInProgress = false;
        return false;
    }
    
    m_isOperationInProgress = true;
    return true;
}

void GitManager::onProcessReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_gitProcess.readAllStandardOutput());
    // Could emit progress signals here for long operations
}

void GitManager::onProcessReadyReadStandardError()
{
    QString error = QString::fromUtf8(m_gitProcess.readAllStandardError());
    // Could log or emit error signals
}

void GitManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QMutexLocker locker(&m_mutex);
    m_isOperationInProgress = false;
    
    bool success = (exitCode == 0 && exitStatus == QProcess::NormalExit);
    
    if (!m_pendingOperations.isEmpty()) {
        auto it = m_pendingOperations.begin();
        QString operation = it.key();
        m_pendingOperations.erase(it);
        
        emit operationCompleted(operation, success);
        
        if (success) {
            emit statusChanged();
            
            if (operation == "clone") {
                // Repository was cloned, could auto-open it
            } else if (operation.startsWith("checkout")) {
                emit branchChanged(currentBranch().name);
            } else if (operation == "commit") {
                emit commitCreated(getCurrentCommitHash());
            }
        }
    }
}

QVector<GitStatus> GitManager::getStatus() const
{
    QMutexLocker locker(&m_mutex);
    QVector<GitStatus> statuses;
    
    if (!isOpen()) return statuses;
    
    bool success;
    QString output = executeGit(QStringList() << "status" << "--porcelain" << "-b", &success);
    
    if (!success) return statuses;
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        if (line.startsWith("## ")) {
            // Branch info - skip for now
            continue;
        }
        
        if (line.length() >= 3) {
            GitStatus status;
            status.stagedStatus = line[0];
            status.status = line[1];
            status.isStaged = (status.stagedStatus != ' ' && status.stagedStatus != '?');
            status.isTracked = (status.status != '?' && status.stagedStatus != '?');
            
            QString filePath = line.mid(3).trimmed();
            
            // Handle renamed files
            if (status.status == 'R') {
                int arrowIndex = filePath.indexOf(" -> ");
                if (arrowIndex > 0) {
                    status.oldPath = filePath.left(arrowIndex);
                    status.filePath = filePath.mid(arrowIndex + 4);
                } else {
                    status.filePath = filePath;
                }
            } else {
                status.filePath = filePath;
            }
            
            statuses.append(status);
        }
    }
    
    return statuses;
}

QStringList GitManager::getUntrackedFiles() const
{
    QStringList result;
    for (const auto &status : getStatus()) {
        if (status.status == '?' || !status.isTracked) {
            result << status.filePath;
        }
    }
    return result;
}

QStringList GitManager::getModifiedFiles() const
{
    QStringList result;
    for (const auto &status : getStatus()) {
        if (status.status == 'M' || status.stagedStatus == 'M') {
            result << status.filePath;
        }
    }
    return result;
}

QStringList GitManager::getStagedFiles() const
{
    QStringList result;
    for (const auto &status : getStatus()) {
        if (status.isStaged) {
            result << status.filePath;
        }
    }
    return result;
}

GitBranch GitManager::currentBranch() const
{
    QMutexLocker locker(&m_mutex);
    GitBranch branch;
    branch.isCurrent = true;
    branch.isRemote = false;
    
    if (!isOpen()) return branch;
    
    bool success;
    QString output = executeGit(QStringList() << "rev-parse" << "--abbrev-ref" << "HEAD", &success);
    
    if (success && !output.isEmpty()) {
        branch.name = output.trimmed();
    }
    
    // Get upstream info
    QString upstreamOutput = executeGit(QStringList() << "rev-parse" << "--abbrev-ref" << "@{upstream}", &success);
    if (success && !upstreamOutput.isEmpty()) {
        branch.upstream = upstreamOutput.trimmed();
        
        // Get ahead/behind count
        QString countOutput = executeGit(QStringList() << "rev-list" << "--left-right" << "--count" << ("@{upstream}..." + branch.name), &success);
        if (success) {
            QStringList counts = countOutput.split('\t');
            if (counts.size() == 2) {
                branch.behind = counts[0].toInt();
                branch.ahead = counts[1].toInt();
            }
        }
    }
    
    return branch;
}

QVector<GitBranch> GitManager::getAllBranches() const
{
    QMutexLocker locker(&m_mutex);
    QVector<GitBranch> branches;
    
    if (!isOpen()) return branches;
    
    bool success;
    QString output = executeGit(QStringList() << "branch" << "-v", &success);
    
    if (!success) return branches;
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    GitBranch current = currentBranch();
    
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) continue;
        
        GitBranch branch;
        branch.isCurrent = line.startsWith("*");
        branch.isRemote = false;
        
        QString cleaned = line.trimmed();
        if (branch.isCurrent) {
            cleaned = cleaned.mid(1).trimmed();
        }
        
        QStringList parts = cleaned.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            branch.name = parts[0];
        }
        
        branches.append(branch);
    }
    
    return branches;
}

QVector<GitBranch> GitManager::getRemoteBranches() const
{
    QMutexLocker locker(&m_mutex);
    QVector<GitBranch> branches;
    
    if (!isOpen()) return branches;
    
    bool success;
    QString output = executeGit(QStringList() << "branch" << "-r", &success);
    
    if (!success) return branches;
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty() || line.contains("HEAD")) continue;
        
        GitBranch branch;
        branch.name = line.trimmed();
        branch.isRemote = true;
        branch.isCurrent = false;
        
        branches.append(branch);
    }
    
    return branches;
}

QString GitManager::getCurrentCommitHash() const
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return QString();
    
    bool success;
    QString output = executeGit(QStringList() << "rev-parse" << "HEAD", &success);
    
    return success ? output.trimmed() : QString();
}

QVector<GitCommit> GitManager::getLog(int count) const
{
    QMutexLocker locker(&m_mutex);
    QVector<GitCommit> commits;
    
    if (!isOpen()) return commits;
    
    bool success;
    QString format = "%H|%h|%an|%ae|%ai|%s|%P";
    QString output = executeGit(QStringList() << "log" << ("-n" + QString::number(count)) << ("--format=" + format), &success);
    
    if (!success) return commits;
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        QStringList parts = line.split('|');
        if (parts.size() >= 6) {
            GitCommit commit;
            commit.hash = parts[0];
            commit.shortHash = parts[1];
            commit.author = parts[2];
            commit.email = parts[3];
            commit.date = QDateTime::fromString(parts[4], Qt::ISODate);
            commit.message = parts[5];
            
            if (parts.size() > 6 && !parts[6].isEmpty()) {
                commit.parents = parts[6].split(' ');
            }
            
            commits.append(commit);
        }
    }
    
    return commits;
}

bool GitManager::stageFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "add" << relativeFilePath(filePath), &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::stageAll()
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "add" << ".", &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::unstageFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "reset" << "HEAD" << "--" << relativeFilePath(filePath), &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::unstageAll()
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "reset" << "HEAD", &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::commit(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    if (message.trimmed().isEmpty()) {
        emit errorOccurred("Commit message cannot be empty");
        return false;
    }
    
    emit operationStarted("commit");
    
    QStringList args;
    args << "commit" << "-m" << message;
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("commit", false);
    }
    
    return success;
}

bool GitManager::amendCommit(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    QStringList args;
    args << "commit" << "--amend";
    
    if (!message.isEmpty()) {
        args << "-m" << message;
    }
    
    bool success;
    executeGit(args, &success);
    
    if (success) {
        emit statusChanged();
        emit commitCreated(getCurrentCommitHash());
    }
    
    return success;
}

bool GitManager::createTag(const QString &tagName, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    QStringList args;
    args << "tag";
    
    if (!message.isEmpty()) {
        args << "-a" << tagName << "-m" << message;
    } else {
        args << tagName;
    }
    
    bool success;
    executeGit(args, &success);
    
    return success;
}

bool GitManager::createBranch(const QString &branchName, const QString &startPoint)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    QStringList args;
    args << "branch" << branchName;
    
    if (!startPoint.isEmpty()) {
        args << startPoint;
    }
    
    bool success;
    executeGit(args, &success);
    
    return success;
}

bool GitManager::checkoutBranch(const QString &branchName)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    emit operationStarted("checkout");
    
    QStringList args;
    args << "checkout" << branchName;
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("checkout", false);
    }
    
    return success;
}

bool GitManager::deleteBranch(const QString &branchName, bool force)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    QStringList args;
    args << "branch" << (force ? "-D" : "-d") << branchName;
    
    bool success;
    executeGit(args, &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::mergeBranch(const QString &branchName, bool noFastForward)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    emit operationStarted("merge");
    
    QStringList args;
    args << "merge";
    
    if (noFastForward) {
        args << "--no-ff";
    }
    
    args << branchName;
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("merge", false);
    }
    
    return success;
}

bool GitManager::rebaseBranch(const QString &branchName)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    emit operationStarted("rebase");
    
    QStringList args;
    args << "rebase" << branchName;
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("rebase", false);
    }
    
    return success;
}

bool GitManager::fetch(const QString &remote)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    emit operationStarted("fetch");
    
    QStringList args;
    args << "fetch" << remote;
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("fetch", false);
    }
    
    return success;
}

bool GitManager::pull(const QString &remote, const QString &branch)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    emit operationStarted("pull");
    
    QStringList args;
    args << "pull";
    
    if (!remote.isEmpty()) {
        args << remote;
        if (!branch.isEmpty()) {
            args << branch;
        }
    }
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("pull", false);
    }
    
    return success;
}

bool GitManager::push(const QString &remote, const QString &branch, bool setUpStream)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    emit operationStarted("push");
    
    QStringList args;
    args << "push";
    
    if (setUpStream) {
        args << "-u";
    }
    
    if (!remote.isEmpty()) {
        args << remote;
        if (!branch.isEmpty()) {
            args << branch;
        }
    }
    
    bool success = executeGitAsync(args);
    
    if (!success) {
        emit operationCompleted("push", false);
    }
    
    return success;
}

QStringList GitManager::getRemotes() const
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return QStringList();
    
    bool success;
    QString output = executeGit(QStringList() << "remote", &success);
    
    if (!success) return QStringList();
    
    return output.split('\n', Qt::SkipEmptyParts);
}

bool GitManager::addRemote(const QString &name, const QString &url)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "remote" << "add" << name << url, &success);
    
    return success;
}

bool GitManager::removeRemote(const QString &name)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "remote" << "remove" << name, &success);
    
    return success;
}

GitDiff GitManager::getDiff(const QString &filePath) const
{
    QMutexLocker locker(&m_mutex);
    GitDiff diff;
    
    if (!isOpen()) return diff;
    
    QStringList args;
    args << "diff";
    
    if (!filePath.isEmpty()) {
        args << "--" << relativeFilePath(filePath);
        diff.filePath = filePath;
    }
    
    bool success;
    QString output = executeGit(args, &success);
    
    if (success) {
        diff.diff = output;
        
        // Parse additions/deletions (simplified)
        int additions = 0, deletions = 0;
        for (const QString &line : output.split('\n')) {
            if (line.startsWith('+') && !line.startsWith("+++")) additions++;
            if (line.startsWith('-') && !line.startsWith("---")) deletions++;
        }
        diff.additions = additions;
        diff.deletions = deletions;
    }
    
    return diff;
}

GitDiff GitManager::getDiffBetweenCommits(const QString &commit1, const QString &commit2) const
{
    QMutexLocker locker(&m_mutex);
    GitDiff diff;
    
    if (!isOpen()) return diff;
    
    bool success;
    QString output = executeGit(QStringList() << "diff" << (commit1 + ".." + commit2), &success);
    
    if (success) {
        diff.diff = output;
    }
    
    return diff;
}

QString GitManager::getFileContent(const QString &filePath, const QString &commit) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return QString();
    
    QStringList args;
    args << "show";
    
    if (!commit.isEmpty()) {
        args << (commit + ":" + relativeFilePath(filePath));
    } else {
        args << relativeFilePath(filePath);
    }
    
    bool success;
    return executeGit(args, &success);
}

bool GitManager::stash(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    QStringList args;
    args << "stash" << "push";
    
    if (!message.isEmpty()) {
        args << "-m" << message;
    }
    
    bool success;
    executeGit(args, &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::stashPop()
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "stash" << "pop", &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::stashApply(int index)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "stash" << "apply" << ("stash@{" + QString::number(index) + "}"), &success);
    
    return success;
}

bool GitManager::stashDrop(int index)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "stash" << "drop" << ("stash@{" + QString::number(index) + "}"), &success);
    
    return success;
}

QStringList GitManager::listStashes() const
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return QStringList();
    
    bool success;
    QString output = executeGit(QStringList() << "stash" << "list", &success);
    
    if (!success) return QStringList();
    
    return output.split('\n', Qt::SkipEmptyParts);
}

QStringList GitManager::resolveConflicts() const
{
    QMutexLocker locker(&m_mutex);
    QStringList conflicts;
    
    if (!isOpen()) return conflicts;
    
    QVector<GitStatus> statuses = getStatus();
    for (const auto &status : statuses) {
        if (status.status == 'U' || status.stagedStatus == 'U') {
            conflicts << status.filePath;
        }
    }
    
    return conflicts;
}

bool GitManager::abortMerge()
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "merge" << "--abort", &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

bool GitManager::abortRebase()
{
    QMutexLocker locker(&m_mutex);
    
    if (!isOpen()) return false;
    
    bool success;
    executeGit(QStringList() << "rebase" << "--abort", &success);
    
    if (success) {
        emit statusChanged();
    }
    
    return success;
}

QString GitManager::relativeFilePath(const QString &filePath) const
{
    if (filePath.isEmpty() || m_repoPath.isEmpty()) {
        return filePath;
    }
    
    QFileInfo fileInfo(filePath);
    if (fileInfo.isAbsolute()) {
        return QDir(m_repoPath).relativeFilePath(filePath);
    }
    
    return filePath;
}

void GitManager::parseStatusOutput(const QString &output, QVector<GitStatus> &statuses) const
{
    // Implementation moved to getStatus()
}

void GitManager::parseLogOutput(const QString &output, QVector<GitCommit> &commits) const
{
    // Implementation moved to getLog()
}

void GitManager::parseBranchOutput(const QString &output, QVector<GitBranch> &branches) const
{
    // Implementation moved to getAllBranches()
}

} // namespace Brahma
