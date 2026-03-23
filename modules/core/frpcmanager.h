#ifndef FRPCMANAGER_H
#define FRPCMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QHostInfo>
#include "database.h"

class FRPCManager : public QObject
{
    Q_OBJECT

public:
    enum ConnectionStatus {
        StatusDisconnected,
        StatusConnecting,
        StatusConnected,
        StatusError
    };

    static FRPCManager* instance();

    void setAutoStopOnExit(bool enabled);
    bool autoStopOnExit() const { return m_autoStopOnExit; }

    void initialize(Database *db);

    // FRPC控制
    bool startFRPC();
    void stopFRPC();
    bool isRunning() const { return m_isRunning; }

    // 检测已运行的frpc进程（程序启动时调用）
    bool checkExistingProcess();
    ConnectionStatus status() const { return m_status; }

    // 配置
    void setConfig(const FRPCConfig &config);
    FRPCConfig getConfig() const { return m_config; }
    int getRemotePort() const { return m_remotePort; }

    // RDP文件生成
    QString generateRDPFile(const QString &username, const QString &password,
                           int screenWidth = 1920, int screenHeight = 1080,
                           bool fullScreen = true);
    QString generateRDPFile(const QString &username, const QString &password, int remotePort);

    // 获取本地Windows用户名
    static QString getLocalUsername();

signals:
    void statusChanged(ConnectionStatus status);
    void errorOccurred(const QString &error);
    void remotePortChanged(int port);
    void stopped();

private slots:
    void onProcessStarted();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onReadOutput();
    void onHeartbeatTimeout();

private:
    FRPCManager();
    ~FRPCManager();

    QString getFRPCExecutablePath();
    QString getConfigFilePath();
    bool writeConfigFile();
    void parseFRPCOutput(const QString &output);
    int parseRemotePort(const QString &output);

    static FRPCManager *s_instance;

    Database *m_db;
    FRPCConfig m_config;
    QProcess *m_process;
    ConnectionStatus m_status;
    bool m_isRunning;
    bool m_stopping;  // 是否主动停止
    bool m_autoStopOnExit;  // 退出时是否自动停止
    qint64 m_frpcPid;  // 记录启动的frpc进程PID
    int m_remotePort;
    QTimer *m_heartbeatTimer;
};

#endif // FRPCMANAGER_H
