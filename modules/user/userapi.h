#ifndef USERAPI_H
#define USERAPI_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>

#define CLOUD_API_URL "http://8.163.37.74:8080"

struct UserInfo {
    int id;
    QString email;
    QString username;
    int vipLevel;
    QString createdAt;
    QString lastLogin;
    
    UserInfo() : id(0), vipLevel(0) {}
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    static ApiClient* instance();
    
    void setBaseUrl(const QString& url);
    QString getBaseUrl() const { return m_baseUrl; }
    void setAuthToken(const QString& token);
    QString getAuthToken() const { return m_authToken; }
    bool isLoggedIn() const { return !m_authToken.isEmpty(); }
    
    void get(const QString& endpoint, const QJsonObject& params = {});
    void post(const QString& endpoint, const QJsonObject& data = {});
    void post(const QString& endpoint, const QString& jsonStr);
    
signals:
    void requestSuccess(const QString& endpoint, const QJsonDocument& response);
    void requestFailed(const QString& endpoint, int errorCode, const QString& error);
    
private slots:
    void onReplyFinished();
    
private:
    ApiClient();
    ~ApiClient();
    
    QNetworkAccessManager* m_manager;
    QString m_baseUrl;
    QString m_authToken;
};

class UserManager : public QObject {
    Q_OBJECT
public:
    static UserManager* instance();
    
    bool isLoggedIn() const { return m_isLoggedIn; }
    UserInfo currentUser() const { return m_currentUser; }
    QString getToken() const { return m_token; }
    
    void login(const QString& email, const QString& password);
    void loginByUsername(const QString& username, const QString& password);
    void loginAuto(const QString& identifier, const QString& password);
    void registerUser(const QString& username, const QString& email, const QString& password);
    void checkEmailExists(const QString& email);
    void checkUsernameExists(const QString& username);
    void logout();
    void fetchProfile();
    void autoLogin();
    
    void updateProfile(const QString& username, const QString& avatar);
    void changePassword(const QString& oldPassword, const QString& newPassword);
    void requestPasswordReset(const QString& email);
    void resetPassword(const QString& token, const QString& newPassword);
    
signals:
    void loginSuccess(const UserInfo& user);
    void loginFailed(const QString& error);
    void registerSuccess();
    void registerFailed(const QString& error);
    void emailCheckResult(bool exists);
    void usernameCheckResult(bool exists);
    void profileLoaded(const UserInfo& user);
    void logoutComplete();
    void profileUpdated();
    void passwordChanged();
    void passwordResetRequestComplete();
    void passwordResetRequestFailed(const QString& error);
    void passwordResetComplete();
    void passwordResetFailed(const QString& error);
    
private slots:
    void onApiResponse(const QString& endpoint, const QJsonDocument& response);
    void onLoginResponse(const QString& endpoint, const QJsonDocument& response);
    void onRegisterResponse(const QString& endpoint, const QJsonDocument& response);
    void onEmailCheckResponse(const QString& endpoint, const QJsonDocument& response);
    void onUsernameCheckResponse(const QString& endpoint, const QJsonDocument& response);
    void onProfileResponse(const QString& endpoint, const QJsonDocument& response);
    void onRequestFailed(const QString& endpoint, int errorCode, const QString& error);
    void onUpdateProfileResponse(const QString& endpoint, const QJsonDocument& response);
    void onChangePasswordResponse(const QString& endpoint, const QJsonDocument& response);
    void onPasswordResetRequestResponse(const QString& endpoint, const QJsonDocument& response);
    void onPasswordResetResponse(const QString& endpoint, const QJsonDocument& response);
    void onLogoutResponse(const QString& endpoint, const QJsonDocument& response);
    
private:
    UserManager();
    
    QString m_token;
    QString m_pendingRequest;
    UserInfo m_currentUser;
    bool m_isLoggedIn;
};

class ConfigSync : public QObject {
    Q_OBJECT
public:
    static ConfigSync* instance();

    void syncSettings();
    void loadSettings();
    void fetchConfig();
    void saveConfig(const QString& key, const QJsonObject& config);
    void saveAllConfig(const QJsonObject& configs);

    // FRPC配置同步
    void saveFRPCConfig(const QJsonObject& frpcConfig);
    void loadFRPCConfig();

signals:
    void configLoaded(const QJsonObject& configs);
    void configSaved();
    void syncFailed(const QString& error);
    void frpcConfigLoaded(const QJsonObject& frpcConfig);

private slots:
    void onConfigLoaded(const QString& endpoint, const QJsonDocument& response);
    void onConfigSaved(const QString& endpoint, const QJsonDocument& response);
    void onFRPCConfigLoaded(const QString& endpoint, const QJsonDocument& response);
    void onRequestFailed(const QString& endpoint, int errorCode, const QString& error);

private:
    ConfigSync();
    bool m_isSyncing;
};

class TaskSync : public QObject {
    Q_OBJECT
public:
    static TaskSync* instance();

    void syncTasks();
    void uploadTasks(const QJsonArray& tasks);
    void uploadTasksWithDeleted(const QJsonArray& tasks, const QStringList& deletedTaskIds);
    void downloadTasks();

    // 增量同步
    void uploadIncrementalTasks(const QJsonArray& tasks, const QString& lastSyncTime);
    void downloadIncrementalTasks(const QString& lastSyncTime);

signals:
    void tasksSynced(const QJsonArray& tasks);
    void tasksUploadComplete();
    void tasksDownloadComplete(const QJsonArray& tasks);
    void syncFailed(const QString& error);

private slots:
    void onTasksLoaded(const QString& endpoint, const QJsonDocument& response);
    void onTasksSaved(const QString& endpoint, const QJsonDocument& response);
    void onRequestFailed(const QString& endpoint, int errorCode, const QString& error);

private:
    TaskSync();
    bool m_isSyncing;
};

// 备忘录同步类
class MemoSync : public QObject {
    Q_OBJECT
public:
    static MemoSync* instance();

    // 同步方法
    void syncMemos();
    void uploadMemos(const QJsonArray& memos);
    void uploadIncrementalMemos(const QJsonArray& memos, const QString& lastSyncTime);
    void downloadMemos();
    void deleteMemos(const QStringList& memoIds);

signals:
    void memosSynced(const QJsonArray& memos);
    void memosUploadComplete();
    void memosDownloadComplete(const QJsonArray& memos);
    void memosDeleteComplete();
    void syncFailed(const QString& error);

private slots:
    void onMemosLoaded(const QString& endpoint, const QJsonDocument& response);
    void onMemosSaved(const QString& endpoint, const QJsonDocument& response);
    void onMemosDeleted(const QString& endpoint, const QJsonDocument& response);
    void onRequestFailed(const QString& endpoint, int errorCode, const QString& error);

private:
    MemoSync();
    bool m_isSyncing;
};

#endif // USERAPI_H
