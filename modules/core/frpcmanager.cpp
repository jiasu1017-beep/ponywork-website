#include "frpcmanager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QCryptographicHash>
#include <QHash>
#include <QMap>
#include <QProcess>
#include <QFileInfo>

#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#endif

FRPCManager* FRPCManager::s_instance = nullptr;
QMutex FRPCManager::s_mutex;

FRPCManager::FRPCManager()
    : m_db(nullptr)
    , m_status(StatusDisconnected)
    , m_isRunning(false)
    , m_autoStopOnExit(true)
    , m_frpcPid(0)
    , m_remotePort(0)
{
    qDebug() << "[FRPC] FRPCManager constructed, m_autoStopOnExit:" << m_autoStopOnExit;
    // startDetached模式下不需要m_process管理进程，只保留timer用于心跳检测
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &FRPCManager::onHeartbeatTimeout);
}

FRPCManager::~FRPCManager()
{
    qDebug() << "[FRPC] ~FRPCManager called, m_autoStopOnExit:" << m_autoStopOnExit << ", m_isRunning:" << m_isRunning;
    fflush(stdout);

    // 使用startDetached启动时，frpc进程已完全独立
    // 但如果用户设置了autoStopOnExit=true，仍然尝试停止
    // 注意：析构函数中不能emit信号，直接清理状态
    if (m_autoStopOnExit && m_isRunning && m_frpcPid > 0) {
        qDebug() << "[FRPC] ~FRPCManager: stopping frpc process";
        QProcess p;
        p.start("taskkill", QStringList() << "/PID" << QString::number(m_frpcPid) << "/F");
        if (!p.waitForFinished(3000)) {
            qDebug() << "[FRPC] ~FRPCManager: failed to stop frpc process";
        }
    }

    // 直接清理状态，不发射信号（析构函数中emit信号可能导致未定义行为）
    m_heartbeatTimer->stop();
    m_isRunning = false;
    m_status = StatusDisconnected;
}

void FRPCManager::setAutoStopOnExit(bool enabled)
{
    m_autoStopOnExit = enabled;
}

FRPCManager* FRPCManager::instance()
{
    QMutexLocker locker(&s_mutex);
    if (!s_instance) {
        s_instance = new FRPCManager();
    }
    return s_instance;
}

void FRPCManager::initialize(Database *db)
{
    m_db = db;
    if (m_db) {
        m_config = m_db->getFRPCConfig();
    }
}

QString FRPCManager::getFRPCExecutablePath()
{
    // 优先使用程序目录下的frpc.exe
    QString appDir = QCoreApplication::applicationDirPath();
    qDebug() << "App directory:" << appDir;

    // 检查多个可能的位置
    QStringList searchPaths = {
        appDir + "/frpc.exe",
        appDir + "/../release/frpc.exe",
        "F:/00AI/Test/release/frpc.exe",
        "F:/00AI/Test/build/Desktop_Qt_5_15_2_MinGW_64_bit-Release/release/frpc.exe"
    };

    // 标准化路径并检查
    for (const QString &path : searchPaths) {
        QString normalizedPath = QDir::cleanPath(path);
        qDebug() << "Checking FRPC path:" << normalizedPath << "exists:" << QFile::exists(normalizedPath);
        if (QFile::exists(normalizedPath)) {
            return normalizedPath;
        }
    }

    // 如果都找不到，返回release下的路径
    QString defaultPath = QDir::cleanPath(appDir + "/../release/frpc.exe");
    qDebug() << "Using default FRPC path:" << defaultPath;
    return defaultPath;
}

QString FRPCManager::getConfigFilePath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + "/frpc.ini";
}

bool FRPCManager::writeConfigFile()
{
    QString configPath = getConfigFilePath();
    QFile file(configPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open config file for writing:" << configPath;
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    out << "[common]\n";
    out << "server_addr = " << m_config.serverAddr << "\n";
    out << "server_port = " << m_config.serverPort << "\n";
    out << "protocol = tcp\n";
    out << "pool_count = 1\n";
    out << "tcp_mux = true\n";
    out << "token = ponywork2024\n";
    // 不写入日志文件，让日志输出到stdout供程序读取
    // out << "log_file = ./frpc.log\n";
    out << "log_level = debug\n";
    out << "\n";

    // RDP端口映射
    // 使用固定端口，便于程序自动获取
    // 端口范围: 20000-50000 (服务器允许)
    // 每个用户+设备组合使用不同端口: 根据 userId + deviceName 生成
    QString deviceName = QHostInfo::localHostName();
    QString combined = QString::number(m_config.userId) + "_" + deviceName;
    int hash = qHash(combined) % 30000;
    int fixedPort = 20000 + qAbs(hash);

    out << "[rdp_" << deviceName << "]\n";
    out << "type = tcp\n";
    out << "local_ip = 127.0.0.1\n";
    out << "local_port = " << m_config.localPort << "\n";
    out << "remote_port = " << fixedPort << "\n";

    // 刷新并关闭
    out.flush();
    file.close();

    qDebug() << "Config file written to:" << configPath;

    // 读取并打印配置内容
    if (file.open(QIODevice::ReadOnly)) {
        qDebug() << "FRPC Config file content:\n" << file.readAll();
        file.close();
    }

    return true;
}

bool FRPCManager::startFRPC()
{
    qDebug() << "[FRPC] startFRPC called";

    if (m_isRunning) {
        qDebug() << "[FRPC] already running, returning true";
        return true;
    }

    // 先尝试检测已运行的进程
    if (checkExistingProcess()) {
        qDebug() << "[FRPC] Found existing process, recovered state";
        return true;
    }

    // 检查端口是否已被占用
    QString deviceName = QHostInfo::localHostName();
    QString combined = QString::number(m_config.userId) + "_" + deviceName;
    int hash = qHash(combined) % 30000;
    int expectedPort = 20000 + qAbs(hash);
    
    QProcess netstatProcess;
    netstatProcess.start("netstat", QStringList() << "-ano");
    netstatProcess.waitForFinished(3000);
    QString netstatOutput = netstatProcess.readAllStandardOutput();
    
    if (netstatOutput.contains(QString(":%1").arg(expectedPort))) {
        qDebug() << "[FRPC] Port" << expectedPort << "is already in use";
        // 端口已被占用，再次尝试检测
        if (checkExistingProcess()) {
            return true;
        }
        // 无法恢复，提示用户
        emit errorOccurred(QString("端口 %1 已被占用，请检查是否有其他 frpc 进程在运行").arg(expectedPort));
        return false;
    }

    if (!writeConfigFile()) {
        QString err = "无法创建FRPC配置文件";
        qDebug() << "[FRPC]" << err;
        emit errorOccurred(err);
        return false;
    }

    QString frpcPath = getFRPCExecutablePath();
    QString configPath = getConfigFilePath();

    qDebug() << "[FRPC] Starting FRPC:" << frpcPath << "-c" << configPath;

    // 检查文件是否存在
    if (!QFile::exists(frpcPath)) {
        QString error = QString("frpc.exe不存在: %1").arg(frpcPath);
        qDebug() << "[FRPC]" << error;
        emit errorOccurred(error);
        return false;
    }

    // 使用startDetached启动，使frpc完全独立于应用程序
    // 这样关闭应用程序时不会影响frpc进程
    QFileInfo fi(frpcPath);
    QString workDir = fi.absolutePath();

    qDebug() << "[FRPC] Starting with startDetached...";
    qint64 pid;
    bool success = QProcess::startDetached(frpcPath, QStringList() << "-c" << configPath, workDir, &pid);

    if (!success) {
        QString err = "FRPC进程启动失败";
        qDebug() << "[FRPC]" << err;
        emit errorOccurred(err);
        return false;
    }

    qDebug() << "[FRPC] Process started with PID:" << pid;
    m_frpcPid = pid;  // 保存PID以便后续停止
    qDebug() << "[FRPC] Saved m_frpcPid:" << m_frpcPid;

    // 使用之前计算的端口
    m_remotePort = expectedPort;

    m_isRunning = true;
    m_status = StatusConnected;
    emit statusChanged(m_status);
    emit remotePortChanged(m_remotePort);

    // 保存配置
    m_config.remotePort = m_remotePort;
    m_config.isEnabled = true;
    m_config.frpcPid = m_frpcPid;  // 保存PID到数据库
    m_config.lastUsedTime = QDateTime::currentDateTime();
    if (m_db) {
        m_db->saveFRPCConfig(m_config);
    }

    // 启动心跳定时器
    m_heartbeatTimer->start(30000);  // 30秒
    qDebug() << "[FRPC] Heartbeat timer started, interval: 30000ms";

    qDebug() << "[FRPC] Process started successfully (detached mode), port:" << m_remotePort;
    return true;
}

void FRPCManager::stopFRPC()
{
    qDebug() << "[FRPC] stopFRPC called, m_isRunning:" << m_isRunning << ", m_frpcPid:" << m_frpcPid;

    if (!m_isRunning) {
        qDebug() << "[FRPC] stopFRPC: not running, returning";
        return;
    }

    // 使用taskkill /PID 只终止自己启动的frpc进程
    if (m_frpcPid > 0) {
        qDebug() << "[FRPC] stopFRPC: using taskkill /PID" << m_frpcPid;
        QProcess p;
        p.start("taskkill", QStringList() << "/PID" << QString::number(m_frpcPid) << "/F");
        if (!p.waitForFinished(3000)) {
            qDebug() << "[FRPC] stopFRPC: failed to stop process, trying force kill";
            p.kill();
        } else if (p.exitCode() != 0) {
            qDebug() << "[FRPC] stopFRPC: taskkill returned error code:" << p.exitCode();
        }
        m_frpcPid = 0;
    }

    m_isRunning = false;
    m_status = StatusDisconnected;
    m_remotePort = 0;
    m_heartbeatTimer->stop();

    emit statusChanged(m_status);
    emit remotePortChanged(0);
    emit stopped();

    qDebug() << "[FRPC] stopFRPC: completed";
}

void FRPCManager::onHeartbeatTimeout()
{
    if (m_frpcPid <= 0) {
        qDebug() << "[FRPC] Heartbeat: m_frpcPid is 0, attempting to recover from database";
        // 尝试从数据库恢复PID
        if (m_db) {
            FRPCConfig savedConfig = m_db->getFRPCConfig();
            if (savedConfig.frpcPid > 0) {
                m_frpcPid = savedConfig.frpcPid;
                qDebug() << "[FRPC] Heartbeat: recovered PID from database:" << m_frpcPid;
            }
        }
        if (m_frpcPid <= 0) {
            return;
        }
    }

    // 使用tasklist检测进程（比wmic更可靠）
    QProcess p;
    p.start("tasklist", QStringList() << "/FI" << QString("PID eq %1").arg(m_frpcPid) << "/NH");
    
    if (!p.waitForStarted(1000)) {
        qDebug() << "[FRPC] Heartbeat: failed to start tasklist command";
        return;
    }
    
    if (!p.waitForFinished(3000)) {
        qDebug() << "[FRPC] Heartbeat: tasklist command timeout";
        p.kill();
        return;
    }

    QString output = p.readAllStandardOutput();
    QString errorOutput = p.readAllStandardError();
    
    // 检查是否有错误
    if (!errorOutput.isEmpty()) {
        qDebug() << "[FRPC] Heartbeat: tasklist error:" << errorOutput;
    }
    
    qDebug() << "[FRPC] Heartbeat: checking PID" << m_frpcPid << ", output:" << output.trimmed();

    // 检查输出中是否包含frpc.exe
    bool isRunning = output.contains("frpc.exe", Qt::CaseInsensitive);
    qDebug() << "[FRPC] Heartbeat: isRunning =" << isRunning;

    if (m_isRunning && !isRunning) {
        qDebug() << "[FRPC] Heartbeat: frpc process" << m_frpcPid << "not found, marking as stopped";
        m_isRunning = false;
        m_status = StatusDisconnected;
        m_remotePort = 0;
        m_frpcPid = 0;
        m_heartbeatTimer->stop();
        
        // 清除数据库中的PID
        if (m_db) {
            m_config.frpcPid = 0;
            m_db->saveFRPCConfig(m_config);
        }
        
        emit statusChanged(m_status);
        emit remotePortChanged(0);
        emit stopped();
    }
}

void FRPCManager::setConfig(const FRPCConfig &config)
{
    m_config = config;
    if (m_db) {
        m_db->saveFRPCConfig(config);
    }
}

// 私有方法：生成RDP文件内容
QString FRPCManager::doGenerateRDPFile(const QString &serverAddr, int port,
                                        const QString &username, const QString &password,
                                        int screenWidth, int screenHeight, bool fullScreen)
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString rdpFilePath = tempDir + "/PonyWork_RDP_" + QHostInfo::localHostName() + ".rdp";

    QFile rdpFile(rdpFilePath);
    if (!rdpFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred("无法创建RDP文件");
        return QString();
    }

    QTextStream out(&rdpFile);
    out.setCodec("UTF-8");

    // RDP文件内容
    out << "full address:s:" << serverAddr << ":" << port << "\n";
    out << "username:s:" << username << "\n";

    if (!password.isEmpty()) {
        // 使用Windows DPAPI加密密码
        out << "password 51:b:" << encryptRdpPassword(password) << "\n";
    }

    out << "screen mode id:i:" << (fullScreen ? 2 : 1) << "\n";
    out << "use multimon:i:0\n";
    out << "desktopwidth:i:" << screenWidth << "\n";
    out << "desktopheight:i:" << screenHeight << "\n";
    out << "session bpp:i:32\n";
    out << "compression:i:1\n";
    out << "keyboardhook:i:2\n";
    out << "audiomode:i:0\n";
    out << "redirectprinters:i:0\n";
    out << "redirectcomports:i:0\n";
    out << "redirectsmartcards:i:0\n";
    out << "redirectclipboard:i:1\n";
    out << "redirectposdevices:i:0\n";
    out << "autoreconnection enabled:i:1\n";
    out << "authentication level:i:2\n";
    out << "prompt for credentials:i:0\n";
    out << "negotiate security layer:i:1\n";

    rdpFile.close();

    qDebug() << "RDP file generated:" << rdpFilePath;
    return rdpFilePath;
}

QString FRPCManager::generateRDPFile(const QString &username, const QString &password,
                                      int screenWidth, int screenHeight, bool fullScreen)
{
    if (m_remotePort == 0) {
        emit errorOccurred("FRPC未连接，无法生成RDP文件");
        return QString();
    }

    return doGenerateRDPFile(m_config.serverAddr, m_remotePort, username, password,
                             screenWidth, screenHeight, fullScreen);
}

// 重载版本：接受远程端口参数
QString FRPCManager::generateRDPFile(const QString &username, const QString &password, int remotePort)
{
    if (remotePort == 0) {
        emit errorOccurred("端口号无效");
        return QString();
    }

    // 使用默认分辨率1920x1080
    return doGenerateRDPFile(m_config.serverAddr, remotePort, username, password,
                             1920, 1080, true);
}

QString FRPCManager::encryptRdpPassword(const QString &password)
{
#ifdef _WIN32
    // 使用Windows DPAPI加密密码
    QByteArray passwordBytes = password.toUtf8();

    DATA_BLOB inputBlob;
    inputBlob.cbData = passwordBytes.size();
    inputBlob.pbData = reinterpret_cast<BYTE*>(passwordBytes.data());

    DATA_BLOB outputBlob = {0, nullptr};

    if (CryptProtectData(&inputBlob, L"RDP Password", nullptr, nullptr, nullptr, 0, &outputBlob)) {
        // 返回加密数据的Base64编码
        QByteArray encrypted(reinterpret_cast<const char*>(outputBlob.pbData), outputBlob.cbData);
        QString result = QString::fromLatin1(encrypted.toBase64());

        // 释放DPAPI分配的内存
        LocalFree(outputBlob.pbData);

        return result;
    }
#endif
    // 如果加密失败，回退到简单Base64编码（不推荐用于生产环境）
    return password.toUtf8().toBase64();
}

QString FRPCManager::getLocalUsername()
{
    // 获取Windows用户名
    QString username = qgetenv("USERNAME");
    if (username.isEmpty()) {
        username = qgetenv("USER");
    }
    if (username.isEmpty()) {
        username = QHostInfo::localHostName();
    }
    return username;
}

bool FRPCManager::checkExistingProcess()
{
    qDebug() << "[FRPC] checkExistingProcess: checking for existing frpc.exe process";

    // 使用 tasklist 获取 frpc.exe 进程信息
    QProcess tasklistProcess;
    tasklistProcess.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq frpc.exe" << "/NH");
    tasklistProcess.waitForFinished(3000);

    QString tasklistOutput = tasklistProcess.readAllStandardOutput();
    qDebug() << "[FRPC] tasklist output:" << tasklistOutput;

    // 检查输出中是否包含 frpc.exe
    if (!tasklistOutput.contains("frpc.exe", Qt::CaseInsensitive)) {
        qDebug() << "[FRPC] No existing frpc.exe process found";
        return false;
    }

    // 获取本程序应使用的端口（基于配置）
    QString deviceName = QHostInfo::localHostName();
    QString combined = QString::number(m_config.userId) + "_" + deviceName;
    int hash = qHash(combined) % 30000;
    int expectedPort = 20000 + qAbs(hash);
    qDebug() << "[FRPC] Expected port for this user/device:" << expectedPort;

    // 使用 netstat 查找所有与 frpc 相关的监听端口
    QProcess netstatProcess;
    netstatProcess.start("netstat", QStringList() << "-ano");
    netstatProcess.waitForFinished(3000);

    QString netstatOutput = netstatProcess.readAllStandardOutput();
    qDebug() << "[FRPC] netstat output:" << netstatOutput;

    // 检查是否有端口在监听（LISTENING）
    // netstat -ano 格式: TCP    0.0.0.0:20000   0.0.0.0:0   LISTENING   12345
    // 注意：netstat输出不包含进程名，需要通过PID关联
    QStringList lines = netstatOutput.split("\n");
    bool foundOurPort = false;
    qint64 foundPid = 0;

    // 先收集所有监听预期端口的PID
    QMap<int, qint64> portPidMap;
    for (const QString &line : lines) {
        if (line.contains("LISTENING")) {
            QStringList parts = line.simplified().split(" ");
            for (int i = 0; i < parts.size(); ++i) {
                if (parts[i].contains(":")) {
                    QString addrPart = parts[i];
                    int colonPos = addrPart.lastIndexOf(":");
                    if (colonPos >= 0) {
                        bool ok;
                        int port = addrPart.mid(colonPos + 1).toInt(&ok);
                        if (ok && port >= 20000 && port <= 50000) {
                            // 查找PID（LISTENING后面的数字）
                            for (int j = 0; j < parts.size(); ++j) {
                                if (parts[j] == "LISTENING" && j + 1 < parts.size()) {
                                    qint64 pid = parts[j + 1].toLongLong(&ok);
                                    if (ok && pid > 0) {
                                        portPidMap[port] = pid;
                                        qDebug() << "[FRPC] Found port" << port << "with PID" << pid;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // 检查预期端口是否在监听
    if (portPidMap.contains(expectedPort)) {
        foundPid = portPidMap[expectedPort];
        // 验证这个PID是否是frpc进程
        QProcess verifyProcess;
        verifyProcess.start("tasklist", QStringList() << "/FI" << QString("PID eq %1").arg(foundPid) << "/NH");
        verifyProcess.waitForFinished(2000);
        QString verifyOutput = verifyProcess.readAllStandardOutput();
        qDebug() << "[FRPC] Verify PID" << foundPid << "output:" << verifyOutput;
        
        if (verifyOutput.contains("frpc.exe", Qt::CaseInsensitive)) {
            foundOurPort = true;
            m_remotePort = expectedPort;
            m_frpcPid = foundPid;
            qDebug() << "[FRPC] Confirmed frpc.exe on port" << expectedPort << "PID" << foundPid;
        }
    }

    // 如果找到了我们预期的端口，说明是本程序启动的
    if (foundOurPort) {
        qDebug() << "[FRPC] Found our frpc process with expected port:" << m_remotePort << "PID:" << m_frpcPid;
        m_isRunning = true;
        m_status = StatusConnected;
        // 同时保存到配置中，便于下次启动时检测
        m_config.remotePort = m_remotePort;
        m_config.frpcPid = m_frpcPid;
        m_config.isEnabled = true;
        if (m_db) {
            m_db->saveFRPCConfig(m_config);
        }
        emit statusChanged(m_status);
        emit remotePortChanged(m_remotePort);
        return true;
    }

    // 如果没有找到预期端口，检查数据库中保存的PID是否存在
    if (m_config.frpcPid > 0) {
        qDebug() << "[FRPC] Checking saved PID:" << m_config.frpcPid;
        QProcess p;
        p.start("wmic", QStringList() << "process" << "where" << QString("ProcessId=%1").arg(m_config.frpcPid) << "get" << "Name,ProcessId");
        p.waitForFinished(2000);
        QString output = p.readAllStandardOutput();
        qDebug() << "[FRPC] PID check output:" << output;

        // 如果进程不存在，清除保存的PID
        if (!output.contains(QString::number(m_config.frpcPid)) || !output.contains("frpc.exe", Qt::CaseInsensitive)) {
            qDebug() << "[FRPC] Saved PID process not found, clearing";
            m_config.frpcPid = 0;
            if (m_db) {
                m_db->saveFRPCConfig(m_config);
            }
        }
    }

    return false;
}
