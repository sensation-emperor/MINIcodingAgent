#include "WorkspaceProjectManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(logWorkspace, "brahma.workspace")

namespace Brahma {

// ProjectFile JSON Serialization
QJsonObject ProjectFile::toJson() const {
    QJsonObject json;
    json["path"] = path;
    json["relativePath"] = relativePath;
    json["size"] = size;
    json["lastModified"] = lastModified.toString(Qt::ISODate);
    json["fileType"] = fileType;
    json["language"] = language;
    json["isBinary"] = isBinary;
    
    QJsonArray tagsArray;
    for (const auto& tag : tags) {
        tagsArray.append(tag);
    }
    json["tags"] = tagsArray;
    
    if (!metadata.isEmpty()) {
        json["metadata"] = metadata;
    }
    
    return json;
}

ProjectFile ProjectFile::fromJson(const QJsonObject& json) {
    ProjectFile file;
    file.path = json["path"].toString();
    file.relativePath = json["relativePath"].toString();
    file.size = json["size"].toVariant().toLongLong();
    file.lastModified = QDateTime::fromString(json["lastModified"].toString(), Qt::ISODate);
    file.fileType = json["fileType"].toString("other");
    file.language = json["language"].toString("");
    file.isBinary = json["isBinary"].toBool(false);
    
    if (json.contains("tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        for (const auto& tagVal : tagsArray) {
            file.tags.append(tagVal.toString());
        }
    }
    
    if (json.contains("metadata")) {
        file.metadata = json["metadata"].toObject();
    }
    
    return file;
}

// ProjectConfig JSON Serialization
QJsonObject ProjectConfig::toJson() const {
    QJsonObject json;
    json["name"] = name;
    json["description"] = description;
    json["rootPath"] = rootPath;
    json["buildSystem"] = buildSystem;
    json["buildCommand"] = buildCommand;
    json["testCommand"] = testCommand;
    json["runCommand"] = runCommand;
    
    QJsonArray sourceDirsArray;
    for (const auto& dir : sourceDirectories) {
        sourceDirsArray.append(dir);
    }
    json["sourceDirectories"] = sourceDirsArray;
    
    QJsonArray excludeArray;
    for (const auto& pattern : excludePatterns) {
        excludeArray.append(pattern);
    }
    json["excludePatterns"] = excludeArray;
    
    QJsonObject envObj;
    for (auto it = environment.begin(); it != environment.end(); ++it) {
        envObj[it.key()] = it.value();
    }
    json["environment"] = envObj;
    
    if (!customSettings.isEmpty()) {
        json["customSettings"] = customSettings;
    }
    
    return json;
}

ProjectConfig ProjectConfig::fromJson(const QJsonObject& json) {
    ProjectConfig config;
    config.name = json["name"].toString();
    config.description = json["description"].toString();
    config.rootPath = json["rootPath"].toString();
    config.buildSystem = json["buildSystem"].toString("none");
    config.buildCommand = json["buildCommand"].toString();
    config.testCommand = json["testCommand"].toString();
    config.runCommand = json["runCommand"].toString();
    
    if (json.contains("sourceDirectories")) {
        QJsonArray dirsArray = json["sourceDirectories"].toArray();
        for (const auto& dirVal : dirsArray) {
            config.sourceDirectories.append(dirVal.toString());
        }
    }
    
    if (json.contains("excludePatterns")) {
        QJsonArray excludeArray = json["excludePatterns"].toArray();
        for (const auto& patVal : excludeArray) {
            config.excludePatterns.append(patVal.toString());
        }
    }
    
    if (json.contains("environment")) {
        QJsonObject envObj = json["environment"].toObject();
        for (auto it = envObj.begin(); it != envObj.end(); ++it) {
            config.environment[it.key()] = it.value().toString();
        }
    }
    
    if (json.contains("customSettings")) {
        config.customSettings = json["customSettings"].toObject();
    }
    
    return config;
}

// WorkspaceProjectManager Implementation
WorkspaceProjectManager::WorkspaceProjectManager(QObject* parent)
    : QObject(parent), m_watcher(new QFileSystemWatcher(this)) {
    
    // Default exclude patterns
    m_config.excludePatterns << "*.o" << "*.obj" << "*.pyc" << "__pycache__" 
                             << ".git" << ".svn" << "node_modules" << "*.tmp"
                             << "*.swp" << "*.bak" << "*~";
    
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
        emit fileChanged(path);
        updateFile(path);
    });
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path) {
        qCInfo(logWorkspace) << "Directory changed:" << path;
        scanDirectory(path, false);
    });
    
    qCDebug(logWorkspace) << "WorkspaceProjectManager initialized";
}

bool WorkspaceProjectManager::createProject(const QString& name, const QString& rootPath,
                                             const QString& description) {
    QMutexLocker locker(&m_mutex);
    
    QString errorMsg;
    if (!validateProjectPath(rootPath, &errorMsg)) {
        qCWarning(logWorkspace) << "Invalid project path:" << errorMsg;
        return false;
    }
    
    QDir dir(rootPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCWarning(logWorkspace) << "Cannot create project directory:" << rootPath;
            return false;
        }
    }
    
    m_config = ProjectConfig();
    m_config.name = name;
    m_config.description = description;
    m_config.rootPath = QDir::cleanPath(rootPath);
    m_config.sourceDirectories << "src" << "include" << "lib";
    
    m_files.clear();
    m_excludedPaths.clear();
    
    // Create default project structure
    dir.mkdir("src");
    dir.mkdir("include");
    dir.mkdir("docs");
    
    detectBuildSystem();
    
    qCInfo(logWorkspace) << "Project created:" << name << "at" << rootPath;
    emit projectOpened(name);
    return true;
}

bool WorkspaceProjectManager::openProject(const QString& projectFilePath) {
    QMutexLocker locker(&m_mutex);
    
    QFile file(projectFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logWorkspace) << "Cannot open project file:" << projectFilePath;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(logWorkspace) << "JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    m_config = ProjectConfig::fromJson(root["config"].toObject());
    
    QJsonArray filesArray = root["files"].toArray();
    m_files.clear();
    for (const auto& fileVal : filesArray) {
        ProjectFile projFile = ProjectFile::fromJson(fileVal.toObject());
        m_files[projFile.path] = projFile;
    }
    
    if (!m_config.rootPath.isEmpty()) {
        rebuildFileIndex();
        startWatching();
    }
    
    qCInfo(logWorkspace) << "Project opened:" << m_config.name;
    emit projectOpened(m_config.name);
    return true;
}

bool WorkspaceProjectManager::closeProject() {
    QMutexLocker locker(&m_mutex);
    
    stopWatching();
    m_config = ProjectConfig();
    m_files.clear();
    m_excludedPaths.clear();
    
    qCInfo(logWorkspace) << "Project closed";
    emit projectClosed();
    return true;
}

bool WorkspaceProjectManager::saveProject() {
    QMutexLocker locker(&m_mutex);
    
    if (m_config.rootPath.isEmpty()) {
        qCWarning(logWorkspace) << "No project to save";
        return false;
    }
    
    QString projectFilePath = m_config.rootPath + "/.brahma_project.json";
    QFile file(projectFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(logWorkspace) << "Cannot write project file:" << projectFilePath;
        return false;
    }
    
    QJsonObject root;
    root["version"] = "1.0";
    root["config"] = m_config.toJson();
    
    QJsonArray filesArray;
    for (const auto& projFile : m_files) {
        filesArray.append(projFile.toJson());
    }
    root["files"] = filesArray;
    
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qCInfo(logWorkspace) << "Project saved:" << projectFilePath;
    emit projectSaved();
    return true;
}

bool WorkspaceProjectManager::deleteProject(const QString& projectFilePath) {
    QMutexLocker locker(&m_mutex);
    
    QFile file(projectFilePath);
    if (file.exists()) {
        if (!file.remove()) {
            qCWarning(logWorkspace) << "Cannot delete project file:" << projectFilePath;
            return false;
        }
    }
    
    closeProject();
    qCInfo(logWorkspace) << "Project deleted:" << projectFilePath;
    return true;
}

QString WorkspaceProjectManager::getProjectName() const {
    QMutexLocker locker(&m_mutex);
    return m_config.name;
}

QString WorkspaceProjectManager::getProjectRoot() const {
    QMutexLocker locker(&m_mutex);
    return m_config.rootPath;
}

ProjectConfig WorkspaceProjectManager::getConfig() const {
    QMutexLocker locker(&m_mutex);
    return m_config;
}

void WorkspaceProjectManager::setConfig(const ProjectConfig& config) {
    QMutexLocker locker(&m_mutex);
    m_config = config;
    emit configUpdated();
}

QList<ProjectFile> WorkspaceProjectManager::getAllFiles() const {
    QMutexLocker locker(&m_mutex);
    return m_files.values();
}

QList<ProjectFile> WorkspaceProjectManager::getSourceFiles() const {
    QMutexLocker locker(&m_mutex);
    QList<ProjectFile> result;
    for (const auto& file : m_files) {
        if (file.fileType == "source" || file.fileType == "header") {
            result.append(file);
        }
    }
    return result;
}

QList<ProjectFile> WorkspaceProjectManager::getFilesByType(const QString& fileType) const {
    QMutexLocker locker(&m_mutex);
    QList<ProjectFile> result;
    for (const auto& file : m_files) {
        if (file.fileType == fileType) {
            result.append(file);
        }
    }
    return result;
}

QList<ProjectFile> WorkspaceProjectManager::getFilesByLanguage(const QString& language) const {
    QMutexLocker locker(&m_mutex);
    QList<ProjectFile> result;
    for (const auto& file : m_files) {
        if (file.language == language) {
            result.append(file);
        }
    }
    return result;
}

QList<ProjectFile> WorkspaceProjectManager::searchFiles(const QString& pattern) const {
    QMutexLocker locker(&m_mutex);
    QList<ProjectFile> result;
    
    QString lowerPattern = pattern.toLower();
    for (const auto& file : m_files) {
        if (file.path.toLower().contains(lowerPattern) ||
            file.relativePath.toLower().contains(lowerPattern) ||
            file.language.toLower().contains(lowerPattern)) {
            result.append(file);
        }
    }
    
    return result;
}

ProjectFile WorkspaceProjectManager::getFile(const QString& path) const {
    QMutexLocker locker(&m_mutex);
    return m_files.value(path, ProjectFile());
}

bool WorkspaceProjectManager::addFile(const QString& path) {
    QMutexLocker locker(&m_mutex);
    
    if (m_files.contains(path)) {
        return false;
    }
    
    if (isExcluded(path)) {
        return false;
    }
    
    ProjectFile file;
    file.path = path;
    file.relativePath = getRelativePath(path);
    file.language = detectLanguage(path);
    file.fileType = detectFileType(path);
    
    updateFileMetadata(file);
    
    m_files[path] = file;
    
    if (!m_watcher->files().contains(path)) {
        m_watcher->addPath(path);
    }
    
    qCInfo(logWorkspace) << "File added:" << path;
    emit fileAdded(path);
    return true;
}

bool WorkspaceProjectManager::removeFile(const QString& path) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_files.contains(path)) {
        return false;
    }
    
    m_files.remove(path);
    
    if (m_watcher->files().contains(path)) {
        m_watcher->removePath(path);
    }
    
    qCInfo(logWorkspace) << "File removed:" << path;
    emit fileRemoved(path);
    return true;
}

bool WorkspaceProjectManager::updateFile(const QString& path) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_files.contains(path)) {
        return false;
    }
    
    ProjectFile& file = m_files[path];
    updateFileMetadata(file);
    
    emit fileModified(path);
    return true;
}

bool WorkspaceProjectManager::addSourceDirectory(const QString& dirPath) {
    QMutexLocker locker(&m_mutex);
    
    if (m_config.sourceDirectories.contains(dirPath)) {
        return false;
    }
    
    m_config.sourceDirectories.append(dirPath);
    scanDirectory(dirPath);
    
    qCInfo(logWorkspace) << "Source directory added:" << dirPath;
    return true;
}

bool WorkspaceProjectManager::removeSourceDirectory(const QString& dirPath) {
    QMutexLocker locker(&m_mutex);
    
    int idx = m_config.sourceDirectories.indexOf(dirPath);
    if (idx == -1) {
        return false;
    }
    
    m_config.sourceDirectories.removeAt(idx);
    
    // Remove files in this directory
    QStringList toRemove;
    for (const auto& file : m_files) {
        if (file.relativePath.startsWith(dirPath)) {
            toRemove.append(file.path);
        }
    }
    
    for (const auto& path : toRemove) {
        removeFile(path);
    }
    
    qCInfo(logWorkspace) << "Source directory removed:" << dirPath;
    return true;
}

QStringList WorkspaceProjectManager::getSourceDirectories() const {
    QMutexLocker locker(&m_mutex);
    return m_config.sourceDirectories;
}

bool WorkspaceProjectManager::scanDirectory(const QString& dirPath, bool recursive) {
    QMutexLocker locker(&m_mutex);
    
    QDir dir(dirPath);
    if (!dir.exists()) {
        qCWarning(logWorkspace) << "Directory does not exist:" << dirPath;
        return false;
    }
    
    QDir::Filters filters = QDir::Files | QDir::Readable;
    if (recursive) {
        filters |= QDir::NoDotAndDotDot;
    } else {
        filters |= QDir::NoDotAndDotDot;
    }
    
    QStringList nameFilters;
    for (const auto& pattern : m_config.excludePatterns) {
        if (!pattern.startsWith("*.")) {
            nameFilters << pattern;
        }
    }
    
    QDirIterator it(dirPath, nameFilters, filters, 
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    
    int count = 0;
    while (it.hasNext()) {
        QString filePath = it.next();
        
        if (isExcluded(filePath)) {
            continue;
        }
        
        if (!m_files.contains(filePath)) {
            ProjectFile file;
            file.path = filePath;
            file.relativePath = getRelativePath(filePath);
            file.language = detectLanguage(filePath);
            file.fileType = detectFileType(filePath);
            updateFileMetadata(file);
            
            m_files[filePath] = file;
            count++;
        }
    }
    
    if (!m_watcher->directories().contains(dirPath)) {
        m_watcher->addPath(dirPath);
    }
    
    qCInfo(logWorkspace) << "Directory scanned:" << dirPath << "found" << count << "new files";
    emit directoryScanned(dirPath, count);
    return true;
}

bool WorkspaceProjectManager::startWatching() {
    QMutexLocker locker(&m_mutex);
    
    if (m_config.rootPath.isEmpty()) {
        return false;
    }
    
    QStringList directories = m_config.sourceDirectories;
    if (directories.isEmpty()) {
        directories << m_config.rootPath;
    }
    
    for (const auto& dir : directories) {
        QString fullPath = getAbsolutePath(dir);
        if (!m_watcher->directories().contains(fullPath)) {
            m_watcher->addPath(fullPath);
        }
    }
    
    for (const auto& file : m_files.keys()) {
        if (!m_watcher->files().contains(file)) {
            m_watcher->addPath(file);
        }
    }
    
    qCInfo(logWorkspace) << "File watching started";
    return true;
}

bool WorkspaceProjectManager::stopWatching() {
    QMutexLocker locker(&m_mutex);
    
    m_watcher->removePaths(m_watcher->files());
    m_watcher->removePaths(m_watcher->directories());
    
    qCInfo(logWorkspace) << "File watching stopped";
    return true;
}

bool WorkspaceProjectManager::isWatching() const {
    QMutexLocker locker(&m_mutex);
    return !m_watcher->files().isEmpty() || !m_watcher->directories().isEmpty();
}

QString WorkspaceProjectManager::getBuildCommand() const {
    QMutexLocker locker(&m_mutex);
    return m_config.buildCommand;
}

QString WorkspaceProjectManager::getRunCommand() const {
    QMutexLocker locker(&m_mutex);
    return m_config.runCommand;
}

QString WorkspaceProjectManager::getTestCommand() const {
    QMutexLocker locker(&m_mutex);
    return m_config.testCommand;
}

void WorkspaceProjectManager::setBuildCommand(const QString& cmd) {
    QMutexLocker locker(&m_mutex);
    m_config.buildCommand = cmd;
    emit configUpdated();
}

void WorkspaceProjectManager::setRunCommand(const QString& cmd) {
    QMutexLocker locker(&m_mutex);
    m_config.runCommand = cmd;
    emit configUpdated();
}

void WorkspaceProjectManager::setTestCommand(const QString& cmd) {
    QMutexLocker locker(&m_mutex);
    m_config.testCommand = cmd;
    emit configUpdated();
}

void WorkspaceProjectManager::setEnvironmentVariable(const QString& key, const QString& value) {
    QMutexLocker locker(&m_mutex);
    m_config.environment[key] = value;
    emit configUpdated();
}

QString WorkspaceProjectManager::getEnvironmentVariable(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_config.environment.value(key);
}

QMap<QString, QString> WorkspaceProjectManager::getEnvironment() const {
    QMutexLocker locker(&m_mutex);
    return m_config.environment;
}

QString WorkspaceProjectManager::getRelativePath(const QString& absolutePath) const {
    QMutexLocker locker(&m_mutex);
    
    if (m_config.rootPath.isEmpty()) {
        return absolutePath;
    }
    
    return QDir(m_config.rootPath).relativeFilePath(absolutePath);
}

QString WorkspaceProjectManager::getAbsolutePath(const QString& relativePath) const {
    QMutexLocker locker(&m_mutex);
    
    if (m_config.rootPath.isEmpty()) {
        return relativePath;
    }
    
    return QDir(m_config.rootPath).filePath(relativePath);
}

QString WorkspaceProjectManager::detectLanguage(const QString& filePath) const {
    QMutexLocker locker(&m_mutex);
    
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    return determineLanguageFromExtension(ext);
}

QString WorkspaceProjectManager::detectFileType(const QString& filePath) const {
    QMutexLocker locker(&m_mutex);
    
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    QString fileName = fi.fileName();
    
    if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "hh") {
        return "header";
    } else if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c" || 
               ext == "py" || ext == "java" || ext == "js" || ext == "ts") {
        return "source";
    } else if (ext == "ui" || ext == "qrc" || ext == "png" || ext == "jpg" || 
               ext == "svg" || ext == "ico") {
        return "resource";
    } else if (fileName == "CMakeLists.txt" || fileName.endsWith(".pro") || 
               fileName == "Makefile" || fileName == "setup.py" || 
               fileName == "package.json") {
        return "config";
    }
    
    return "other";
}

bool WorkspaceProjectManager::isExcluded(const QString& path) const {
    QMutexLocker locker(&m_mutex);
    return matchesPattern(path, m_config.excludePatterns);
}

void WorkspaceProjectManager::clearCache() {
    QMutexLocker locker(&m_mutex);
    m_excludedPaths.clear();
    qCInfo(logWorkspace) << "Cache cleared";
}

bool WorkspaceProjectManager::exportConfig(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonDocument doc(m_config.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

bool WorkspaceProjectManager::importConfig(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    m_config = ProjectConfig::fromJson(doc.object());
    rebuildFileIndex();
    
    emit configUpdated();
    return true;
}

bool WorkspaceProjectManager::validateProjectPath(const QString& path, QString& errorMsg) const {
    if (path.trimmed().isEmpty()) {
        errorMsg = "Project path cannot be empty";
        return false;
    }
    
    QFileInfo fi(path);
    if (fi.exists() && !fi.isDir()) {
        errorMsg = "Path exists but is not a directory";
        return false;
    }
    
    QDir dir(path);
    if (!dir.isReadable()) {
        errorMsg = "Path is not readable";
        return false;
    }
    
    return true;
}

void WorkspaceProjectManager::detectBuildSystem() {
    QDir dir(m_config.rootPath);
    
    if (dir.exists("CMakeLists.txt")) {
        m_config.buildSystem = "cmake";
        m_config.buildCommand = "cmake --build build";
    } else if (dir.exists("Makefile")) {
        m_config.buildSystem = "make";
        m_config.buildCommand = "make";
    } else if (dir.exists("setup.py")) {
        m_config.buildSystem = "python";
        m_config.buildCommand = "python setup.py build";
    } else if (dir.exists("package.json")) {
        m_config.buildSystem = "npm";
        m_config.buildCommand = "npm run build";
    }
    
    emit buildSystemChanged(m_config.buildSystem);
}

void WorkspaceProjectManager::updateFileMetadata(ProjectFile& file) {
    QFileInfo fi(file.path);
    
    if (fi.exists()) {
        file.size = fi.size();
        file.lastModified = fi.lastModified();
        file.isBinary = fi.isFile() && (fi.size() > 0);
        
        // Simple binary detection
        if (file.isBinary) {
            QFile f(file.path);
            if (f.open(QIODevice::ReadOnly)) {
                QByteArray header = f.read(512);
                file.isBinary = header.contains('\0');
                f.close();
            }
        }
    }
}

bool WorkspaceProjectManager::matchesPattern(const QString& path, const QStringList& patterns) const {
    QFileInfo fi(path);
    QString fileName = fi.fileName();
    QString basePath = fi.path();
    
    for (const auto& pattern : patterns) {
        if (fileName.contains(pattern.replace("*", ""))) {
            return true;
        }
        
        if (basePath.contains(pattern)) {
            return true;
        }
    }
    
    return false;
}

void WorkspaceProjectManager::rebuildFileIndex() {
    m_files.clear();
    
    if (m_config.rootPath.isEmpty()) {
        return;
    }
    
    QStringList dirs = m_config.sourceDirectories;
    if (dirs.isEmpty()) {
        dirs << ".";
    }
    
    for (const auto& dir : dirs) {
        scanDirectory(getAbsolutePath(dir), true);
    }
}

QString WorkspaceProjectManager::determineLanguageFromExtension(const QString& ext) const {
    static QMap<QString, QString> extToLang = {
        {"cpp", "cpp"}, {"cc", "cpp"}, {"cxx", "cpp"}, {"c++", "cpp"},
        {"h", "cpp"}, {"hpp", "cpp"}, {"hxx", "cpp"}, {"hh", "cpp"},
        {"c", "c"},
        {"py", "python"},
        {"java", "java"},
        {"js", "javascript"}, {"jsx", "javascript"},
        {"ts", "typescript"}, {"tsx", "typescript"},
        {"rb", "ruby"},
        {"go", "go"},
        {"rs", "rust"},
        {"cs", "csharp"},
        {"php", "php"},
        {"swift", "swift"},
        {"kt", "kotlin"}, {"kts", "kotlin"},
        {"scala", "scala"},
        {"sh", "bash"}, {"bash", "bash"}, {"zsh", "bash"},
        {"md", "markdown"}, {"rst", "markdown"},
        {"txt", "text"},
        {"json", "json"}, {"xml", "xml"}, {"yaml", "yaml"}, {"yml", "yaml"},
        {"html", "html"}, {"css", "css"}, {"scss", "scss"},
        {"sql", "sql"},
        {"lua", "lua"},
        {"r", "r"},
        {"m", "objectivec"}, {"mm", "objectivec"}
    };
    
    return extToLang.value(ext, "");
}

} // namespace Brahma
