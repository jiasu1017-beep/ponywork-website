#include "updatemanager.h"
#include "modules/core/logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMessageBox>
#include <QProcess>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QDesktopServices>
#include <QRegExp>

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);
    periodicTimer = new QTimer(this);
    retryTimer = new QTimer(this);
    retryTimer->setSingleShot(true);
    
    m_currentVersion = QCoreApplication::applicationVersion();
    m_repoOwner = "jiasu1017-beep";
    m_repoName = "ponywork-website";
    m_latestUpdate.isValid = false;
    m_retryCount = 0;
    m_maxRetries = 3;
    m_retryDelay = 2000;
    
    connect(periodicTimer, &QTimer::timeout, this, &UpdateManager::onPeriodicCheck);
    connect(retryTimer, &QTimer::timeout, this, &UpdateManager::retryDownload);
}

void UpdateManager::checkForUpdates()
{
    emit checkForUpdatesStarted();
    
    log("开始检查更新...");
    log(QString("当前版本: %1").arg(m_currentVersion));
    
    QString apiUrl = QString("https://api.github.com/repos/%1/%2/releases/latest")
                        .arg(m_repoOwner, m_repoName);
    
    log(QString("请求API: %1").arg(apiUrl));
    
    QUrl url(apiUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PonyWork-Updater");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        log("收到网络响应");
        onReleaseInfoReceived(reply);
    });
}

void UpdateManager::startPeriodicChecks()
{
    periodicTimer->start(24 * 60 * 60 * 1000);
    checkForUpdates();
}

void UpdateManager::stopPeriodicChecks()
{
    periodicTimer->stop();
}

QString UpdateManager::currentVersion() const
{
    return m_currentVersion;
}

UpdateInfo UpdateManager::latestUpdate() const
{
    return m_latestUpdate;
}

bool UpdateManager::isUpdateAvailable() const
{
    if (!m_latestUpdate.isValid) {
        return false;
    }
    
    if (m_latestUpdate.version == m_ignoredVersion) {
        return false;
    }
    
    return compareVersions(m_currentVersion, m_latestUpdate.version);
}

QString UpdateManager::ignoredVersion() const
{
    return m_ignoredVersion;
}

void UpdateManager::setIgnoredVersion(const QString &version)
{
    m_ignoredVersion = version;
}

void UpdateManager::downloadUpdate()
{
    if (!m_latestUpdate.isValid) {
        QString error = "无法下载：没有有效的版本信息";
        log(error);
        emit downloadFailed(error);
        return;
    }
    
    if (m_latestUpdate.downloadUrl.isEmpty()) {
        QString error = "无法下载：该版本没有可用的下载链接\n\n"
                       "可能原因：\n"
                       "1. GitHub Release 尚未上传 Assets 文件\n"
                       "2. Release 正在创建中\n\n"
                       "建议：请在 GitHub 上创建 Release 并上传压缩包文件后再试";
        log(error);
        emit downloadFailed(error);
        return;
    }
    
    m_retryCount = 0;
    log(QString("开始下载更新: %1").arg(m_latestUpdate.downloadUrl));
    log(QString("预期文件大小: %1").arg(formatFileSize(m_latestUpdate.fileSize)));
    
    QUrl url(m_latestUpdate.downloadUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PonyWork-Updater");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, &UpdateManager::onDownloadError);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadFinished(reply);
    });
}

void UpdateManager::retryDownload()
{
    m_retryCount++;
    log(QString("重试下载 (%1/%2)...").arg(m_retryCount).arg(m_maxRetries));
    
    QUrl url(m_latestUpdate.downloadUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PonyWork-Updater");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, &UpdateManager::onDownloadError);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadFinished(reply);
    });
}

void UpdateManager::remindLater()
{
}

void UpdateManager::skipThisVersion()
{
    if (m_latestUpdate.isValid) {
        m_ignoredVersion = m_latestUpdate.version;
    }
}

void UpdateManager::installUpdate(const QString &filePath)
{
    log("=== 开始安装更新 ===");
    emit installProgress(0);
    
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/PonyWork-update";
    QDir dir(tempDir);
    
    if (dir.exists()) {
        dir.removeRecursively();
    }
    dir.mkpath(tempDir);
    
    log(QString("解压到: %1").arg(tempDir));
    
    if (!extractZipFile(filePath, tempDir)) {
        emit extractFailed("解压失败");
        return;
    }
    
    emit extractProgress(100);
    emit extractFinished(tempDir);
    
    log("备份当前版本...");
    emit installProgress(25);
    
    if (!backupCurrentVersion()) {
        log("备份失败，继续安装...");
    }
    
    emit installProgress(50);
    
    log("准备启动更新助手...");
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString updaterPath = appDir + "/Updater.exe";
    
    if (!QFile::exists(updaterPath)) {
        log("警告: 更新助手程序不存在，尝试直接替换文件");
        QStringList filters;
        filters << "*.ico" << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp";
        replaceFiles(tempDir, appDir, filters);
        
        emit installProgress(75);
        emit installProgress(100);
        emit installFinished();
        
        log("=== 更新安装完成（部分文件） ===");
        log("准备重启应用程序...");
        QTimer::singleShot(500, this, [this]() {
            restartApplication();
        });
        return;
    }
    
    log("启动更新助手程序");
    emit installProgress(75);
    
    QString appPath = QCoreApplication::applicationFilePath();
    
    QStringList arguments;
    arguments << tempDir << appDir << appPath;
    
    log(QString("更新助手参数: %1").arg(arguments.join(" ")));
    
    QProcess::startDetached(updaterPath, arguments);
    
    emit installProgress(100);
    emit installFinished();
    
    log("=== 更新安装完成 ===");
    log("更新助手已启动，当前程序即将退出");
    
    QTimer::singleShot(500, this, [this]() {
        QCoreApplication::quit();
    });
}

void UpdateManager::restartApplication()
{
    log("正在重启应用程序...");
    
    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = QCoreApplication::arguments();
    
    arguments.removeFirst();
    
    log(QString("程序路径: %1").arg(program));
    log(QString("参数: %1").arg(arguments.join(", ")));
    
    QProcess::startDetached(program, arguments);
    
    log("已启动新进程，当前进程即将退出");
    QCoreApplication::quit();
}

void UpdateManager::onReleaseInfoReceived(QNetworkReply *reply)
{
    log("开始处理响应...");
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        log(QString("检查更新失败: %1").arg(errorMsg));
        
        if (errorMsg.contains("TLS") || errorMsg.contains("SSL")) {
            errorMsg = "SSL/TLS 初始化失败\n\n"
                       "可能原因：\n"
                       "1. 缺少 OpenSSL 库文件\n"
                       "2. 系统不支持 HTTPS 连接\n\n"
                       "建议：请确保发布包包含 libssl-1_1-x64.dll 和 libcrypto-1_1-x64.dll";
        }
        
        emit updateCheckFailed(errorMsg);
        reply->deleteLater();
        return;
    }
    
    log("收到更新信息，正在解析...");
    
    QByteArray data = reply->readAll();
    log(QString("收到数据大小: %1 字节").arg(data.size()));
    
    parseReleaseInfo(data);
    reply->deleteLater();
    
    log(QString("解析后版本: %1").arg(m_latestUpdate.version));
    log(QString("是否有效: %1").arg(m_latestUpdate.isValid));
    log(QString("是否有更新: %1").arg(isUpdateAvailable()));
    
    if (isUpdateAvailable()) {
        log(QString("发现新版本: v%1").arg(m_latestUpdate.version));
        emit updateAvailable(m_latestUpdate);
    } else {
        log("当前已是最新版本");
        emit noUpdateAvailable();
    }
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void UpdateManager::onDownloadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString errorMsg = reply->errorString();
    log(QString("下载错误: %1").arg(errorMsg));
    
    if (errorMsg.contains("TLS") || errorMsg.contains("SSL")) {
        errorMsg = "SSL/TLS 连接失败\n\n"
                   "可能原因：\n"
                   "1. 缺少 OpenSSL 库文件\n"
                   "2. 系统不支持 HTTPS 连接\n\n"
                   "建议：请确保发布包包含 libssl-1_1-x64.dll 和 libcrypto-1_1-x64.dll";
    }
    
    if (m_retryCount < m_maxRetries) {
        int delay = m_retryDelay * (1 << m_retryCount);
        log(QString("%1秒后重试...").arg(delay / 1000));
        retryTimer->start(delay);
    } else {
        log("已达到最大重试次数");
        emit downloadFailed(errorMsg);
    }
}

void UpdateManager::onDownloadFinished(QNetworkReply *reply)
{
    log("=== 下载完成处理开始 ===");
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        log(QString("下载请求失败: %1").arg(errorMsg));
        reply->deleteLater();
        return;
    }
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    log(QString("HTTP 状态码: %1").arg(statusCode));
    
    QVariant lengthVar = reply->header(QNetworkRequest::ContentLengthHeader);
    qint64 contentLength = lengthVar.isValid() ? lengthVar.toLongLong() : -1;
    log(QString("Content-Length: %1").arg(contentLength));
    
    if (statusCode != 200 && statusCode != 301 && statusCode != 302 && statusCode != 307 && statusCode != 308) {
        QString error = QString("HTTP 请求失败，状态码: %1\n\n").arg(statusCode);
        if (statusCode == 404) {
            error += "可能原因：\n"
                    "1. GitHub Release 的 Assets 文件不存在\n"
                    "2. 下载链接已过期或无效\n\n"
                    "建议：请确认 GitHub Release 已正确上传 Assets 文件";
        } else if (statusCode >= 300 && statusCode < 400) {
            error += "注意：已启用自动重定向，若问题持续请检查网络连接";
        }
        log(error);
        emit downloadFailed(error);
        reply->deleteLater();
        return;
    }
    
    if (statusCode >= 300 && statusCode < 400) {
        log(QString("收到重定向响应 (状态码: %1)，正在自动跟随...").arg(statusCode));
    }
    
    log("开始读取下载数据...");
    QByteArray data = reply->readAll();
    log(QString("读取到的数据大小: %1 字节").arg(data.size()));
    
    if (data.isEmpty()) {
        QString error = "下载的数据为空\n\n"
                       "可能原因：\n"
                       "1. GitHub Release 尚未上传 Assets 文件\n"
                       "2. 下载链接指向的是空文件\n"
                       "3. 网络传输中断\n\n"
                       "建议：请先在 GitHub 上创建 Release 并上传压缩包文件";
        log(error);
        emit downloadFailed(error);
        reply->deleteLater();
        return;
    }
    
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString fileName = QFileInfo(reply->url().path()).fileName();
    if (fileName.isEmpty()) {
        fileName = "PonyWork-update.zip";
    }
    
    QString filePath = tempDir + "/" + fileName;
    m_downloadedFilePath = filePath;
    
    log(QString("保存路径: %1").arg(filePath));
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QString error = "无法保存文件: " + file.errorString();
        log(error);
        emit downloadFailed(error);
        reply->deleteLater();
        return;
    }
    
    log("正在写入文件...");
    qint64 bytesWritten = file.write(data);
    file.flush();
    file.close();
    reply->deleteLater();
    
    log(QString("写入字节数: %1").arg(bytesWritten));
    
    if (bytesWritten <= 0) {
        QString error = "文件写入失败，写入字节数为0\n\n"
                       "可能原因：\n"
                       "1. 磁盘空间不足\n"
                       "2. 没有写入权限\n"
                       "3. 临时目录不可用";
        log(error);
        emit downloadFailed(error);
        return;
    }
    
    if (bytesWritten != data.size()) {
        log(QString("警告: 写入字节数(%1)与数据大小(%2)不匹配").arg(bytesWritten).arg(data.size()));
    }
    
    log(QString("文件已保存: %1").arg(filePath));
    log(QString("文件大小: %1 (写入 %2 字节)").arg(formatFileSize(data.size())).arg(bytesWritten));
    
    log("开始验证文件...");
    if (!verifyDownloadedFile(filePath, m_latestUpdate.fileSize)) {
        QString error = "文件验证失败\n\n"
                       "可能原因：\n"
                       "1. GitHub 上尚未发布该版本的 Release Assets\n"
                       "2. 网络传输过程中数据损坏\n"
                       "3. 文件下载不完整\n\n"
                       "建议：请先在 GitHub 上创建 Release 并上传压缩包文件";
        log("文件验证失败，终止更新流程");
        emit downloadFailed(error);
        return;
    }
    
    log("文件验证成功，可以继续安装");
    log("=== 下载完成处理结束 ===");
    emit downloadFinished(filePath);
}

bool UpdateManager::verifyDownloadedFile(const QString &filePath, qint64 expectedSize)
{
    QFile file(filePath);
    if (!file.exists()) {
        log("验证失败: 文件不存在");
        return false;
    }
    
    qint64 actualSize = file.size();
    log(QString("验证文件 - 预期: %1, 实际: %2").arg(formatFileSize(expectedSize), formatFileSize(actualSize)));
    
    if (actualSize == 0) {
        log("验证失败: 文件大小为0字节");
        return false;
    }
    
    if (expectedSize > 0 && actualSize != expectedSize) {
        log(QString("警告: 文件大小与预期不匹配，但仍将继续 (预期: %1, 实际: %2)").arg(formatFileSize(expectedSize), formatFileSize(actualSize)));
    }
    
    log("文件验证通过");
    return true;
}

bool UpdateManager::extractZipFile(const QString &zipPath, const QString &extractPath)
{
    log(QString("正在解压: %1").arg(zipPath));
    
    QProcess process;
    QStringList arguments;
    arguments << "x" << zipPath << "-o" << extractPath << "-y";
    
    process.start("7z", arguments);
    if (!process.waitForStarted()) {
        log("7z未找到，尝试使用PowerShell...");
        
        QString zipPathEscaped = zipPath;
        QString extractPathEscaped = extractPath;
        zipPathEscaped.replace("\\", "\\\\");
        extractPathEscaped.replace("\\", "\\\\");
        
        QString script = QString(
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "[System.IO.Compression.ZipFile]::ExtractToDirectory('%1', '%2')"
        ).arg(zipPathEscaped, extractPathEscaped);
        
        QProcess powershell;
        powershell.start("powershell", QStringList() << "-Command" << script);
        
        if (!powershell.waitForFinished(60000)) {
            log("解压超时");
            return false;
        }
        
        if (powershell.exitCode() != 0) {
            log(QString("解压失败: %1").arg(QString::fromLocal8Bit(powershell.readAllStandardError())));
            return false;
        }
    } else {
        if (!process.waitForFinished(60000)) {
            log("解压超时");
            return false;
        }
        
        if (process.exitCode() != 0) {
            log(QString("解压失败: %1").arg(QString::fromLocal8Bit(process.readAllStandardError())));
            return false;
        }
    }
    
    emit extractProgress(100);
    log("解压完成");
    return true;
}

bool UpdateManager::backupCurrentVersion()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString backupDir = appDir + ".backup";
    
    QDir dir(backupDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    
    QDir sourceDir(appDir);
    if (!sourceDir.exists()) {
        return false;
    }
    
    QDir().mkpath(backupDir);
    
    QFileInfoList fileList = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : fileList) {
        QString srcPath = info.absoluteFilePath();
        QString destPath = backupDir + "/" + info.fileName();
        
        if (info.isDir()) {
            QDir().mkpath(destPath);
        } else {
            QFile::copy(srcPath, destPath);
        }
    }
    
    log(QString("已备份到: %1").arg(backupDir));
    return true;
}

bool UpdateManager::replaceFiles(const QString &sourcePath, const QString &targetPath, const QStringList &filters)
{
    QDir sourceDir(sourcePath);
    QDir targetDir(targetPath);
    
    if (!sourceDir.exists()) {
        log("源目录不存在");
        return false;
    }
    
    int successCount = 0;
    int skipCount = 0;
    QString appExeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    
    QFileInfoList fileList = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : fileList) {
        QString srcPath = info.absoluteFilePath();
        QString destPath = targetPath + "/" + info.fileName();
        
        if (info.isDir()) {
            QDir().mkpath(destPath);
            if (replaceFiles(srcPath, destPath, filters)) {
                successCount++;
            }
        } else {
            if (!filters.isEmpty()) {
                bool match = false;
                for (const QString &filter : filters) {
                    QRegExp regExp(filter, Qt::CaseInsensitive, QRegExp::Wildcard);
                    if (regExp.exactMatch(info.fileName())) {
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    skipCount++;
                    continue;
                }
            }
            
            if (info.fileName().compare(appExeName, Qt::CaseInsensitive) == 0) {
                log(QString("跳过正在运行的可执行文件: %1").arg(info.fileName()));
                skipCount++;
                continue;
            }
            
            bool fileReplaced = false;
            QFile destFile(destPath);
            
            if (destFile.exists()) {
                if (destFile.remove()) {
                    if (QFile::copy(srcPath, destPath)) {
                        log(QString("已替换: %1").arg(info.fileName()));
                        successCount++;
                        fileReplaced = true;
                    } else {
                        log(QString("无法复制文件: %1 -> %2").arg(srcPath, destPath));
                    }
                } else {
                    log(QString("无法删除现有文件，跳过: %1").arg(destPath));
                    skipCount++;
                }
            } else {
                if (QFile::copy(srcPath, destPath)) {
                    log(QString("已复制: %1").arg(info.fileName()));
                    successCount++;
                    fileReplaced = true;
                } else {
                    log(QString("无法复制新文件: %1 -> %2").arg(srcPath, destPath));
                }
            }
        }
    }
    
    log(QString("文件替换完成 - 成功: %1, 跳过: %2").arg(successCount).arg(skipCount));
    
    if (successCount > 0) {
        return true;
    } else {
        log("警告: 没有成功替换任何文件");
        return false;
    }
}

void UpdateManager::rollbackUpdate()
{
    log("开始回滚...");
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString backupDir = appDir + ".backup";
    
    QDir backupDirObj(backupDir);
    if (!backupDirObj.exists()) {
        log("没有找到备份，无法回滚");
        return;
    }
    
    replaceFiles(backupDir, appDir);
    backupDirObj.removeRecursively();
    
    log("回滚完成");
}

void UpdateManager::onPeriodicCheck()
{
    checkForUpdates();
}

void UpdateManager::parseReleaseInfo(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        log("解析失败: 响应不是有效的JSON对象");
        m_latestUpdate.isValid = false;
        return;
    }
    
    QJsonObject obj = doc.object();
    
    m_latestUpdate.version = obj["tag_name"].toString().remove('v');
    m_latestUpdate.releaseDate = obj["published_at"].toString();
    m_latestUpdate.changelog = obj["body"].toString();
    
    log(QString("解析到版本: v%1").arg(m_latestUpdate.version));
    
    QJsonArray assets = obj["assets"].toArray();
    if (!assets.isEmpty()) {
        QJsonObject asset = assets[0].toObject();
        m_latestUpdate.downloadUrl = asset["browser_download_url"].toString();
        m_latestUpdate.fileSize = asset["size"].toVariant().toLongLong();
        log(QString("找到下载资产: %1").arg(QFileInfo(m_latestUpdate.downloadUrl).fileName()));
        log(QString("文件大小: %1").arg(formatFileSize(m_latestUpdate.fileSize)));
    } else {
        log("警告: 该Release没有上传任何Assets文件");
        m_latestUpdate.downloadUrl.clear();
        m_latestUpdate.fileSize = 0;
    }
    
    m_latestUpdate.isValid = !m_latestUpdate.version.isEmpty();
    
    if (m_latestUpdate.isValid && m_latestUpdate.downloadUrl.isEmpty()) {
        log("注意: 版本信息有效，但缺少下载链接");
    }
}

bool UpdateManager::compareVersions(const QString &local, const QString &remote) const
{
    QVersionNumber localVer = QVersionNumber::fromString(local);
    QVersionNumber remoteVer = QVersionNumber::fromString(remote);
    
    return remoteVer > localVer;
}

QString UpdateManager::formatFileSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    
    if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

void UpdateManager::log(const QString &message)
{
    Logger::instance()->log(message);
    emit logMessage(message);
}

QString UpdateManager::calculateFileHash(const QString &filePath, QCryptographicHash::Algorithm algorithm)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(algorithm);
    if (hash.addData(&file)) {
        return hash.result().toHex();
    }
    
    return QString();
}
