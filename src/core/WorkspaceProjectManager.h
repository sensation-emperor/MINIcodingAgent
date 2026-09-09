#ifndef WORKSPACEPROJECTMANAGER_H
#define WORKSPACEPROJECTMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QFileInfo>
#include <QDir>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QUuid>

namespace Brahma {

struct ProjectFile {
    QString path;
    QString relativePath;
    qint64 size;
    QDateTime lastModified;
    QString fileType; // "source", "header", "resource", "config", "other"
    QString language; // "cpp", "python", "markdown", etc.
    bool isBinary;
    QStringList tags;
    QJsonObject metadata;
    
    ProjectFile() : size(0), isBinary(false) {}
    
    QJsonObject toJson() const;
    static ProjectFile fromJson(const QJsonObject& json);
};

struct ProjectConfig {
    QString name;
    QString description;
    QString rootPath;
    QString buildSystem; // "cmake", "qmake", "make", "python", "none"
    QString buildCommand;
    QString testCommand;
    QString runCommand;
    QStringList sourceDirectories;
    QStringList excludePatterns;
    QMap<QString, QString> environment;
    QJsonObject customSettings;
    
    ProjectConfig() : buildSystem("none") {}
    
    QJsonObject toJson() const;
    static ProjectConfig fromJson(const QJsonObject& json);
};

class WorkspaceProjectManager : public QObject {
    Q_OBJECT
    
public:
    explicit WorkspaceProjectManager(QObject* parent = nullptr);
    
    // Project Lifecycle
    bool createProject(const QString& name, const QString& rootPath, 
                       const QString& description = "");
    bool openProject(const QString& projectFilePath);
    bool closeProject();
    bool saveProject();
    bool deleteProject(const QString& projectFilePath);
    
    // Project Info
    QString getProjectName() const;
    QString getProjectRoot() const;
    ProjectConfig getConfig() const;
    void setConfig(const ProjectConfig& config);
    
    // File Management
    QList<ProjectFile> getAllFiles() const;
    QList<ProjectFile> getSourceFiles() const;
    QList<ProjectFile> getFilesByType(const QString& fileType) const;
    QList<ProjectFile> getFilesByLanguage(const QString& language) const;
    QList<ProjectFile> searchFiles(const QString& pattern) const;
    ProjectFile getFile(const QString& path) const;
    bool addFile(const QString& path);
    bool removeFile(const QString& path);
    bool updateFile(const QString& path);
    
    // Directory Operations
    bool addSourceDirectory(const QString& dirPath);
    bool removeSourceDirectory(const QString& dirPath);
    QStringList getSourceDirectories() const;
    bool scanDirectory(const QString& dirPath, bool recursive = true);
    
    // File System Watching
    bool startWatching();
    bool stopWatching();
    bool isWatching() const;
    
    // Build & Run
    QString getBuildCommand() const;
    QString getRunCommand() const;
    QString getTestCommand() const;
    void setBuildCommand(const QString& cmd);
    void setRunCommand(const QString& cmd);
    void setTestCommand(const QString& cmd);
    
    // Environment
    void setEnvironmentVariable(const QString& key, const QString& value);
    QString getEnvironmentVariable(const QString& key) const;
    QMap<QString, QString> getEnvironment() const;
    
    // Utilities
    QString getRelativePath(const QString& absolutePath) const;
    QString getAbsolutePath(const QString& relativePath) const;
    QString detectLanguage(const QString& filePath) const;
    QString detectFileType(const QString& filePath) const;
    bool isExcluded(const QString& path) const;
    void clearCache();
    
    // Persistence
    bool exportConfig(const QString& filePath);
    bool importConfig(const QString& filePath);
    
signals:
    void projectOpened(const QString& projectName);
    void projectClosed();
    void projectSaved();
    void fileAdded(const QString& filePath);
    void fileRemoved(const QString& filePath);
    void fileChanged(const QString& filePath);
    void fileModified(const QString& filePath);
    void directoryScanned(const QString& dirPath, int fileCount);
    void buildSystemChanged(const QString& buildSystem);
    void configUpdated();
    
private:
    ProjectConfig m_config;
    QMap<QString, ProjectFile> m_files;
    QFileSystemWatcher* m_watcher;
    QSet<QString> m_excludedPaths;
    mutable QMutex m_mutex;
    
    bool validateProjectPath(const QString& path, QString& errorMsg) const;
    void detectBuildSystem();
    void updateFileMetadata(ProjectFile& file);
    bool matchesPattern(const QString& path, const QStringList& patterns) const;
    void rebuildFileIndex();
    QString determineLanguageFromExtension(const QString& ext) const;
};

} // namespace Brahma

#endif // WORKSPACEPROJECTMANAGER_H
