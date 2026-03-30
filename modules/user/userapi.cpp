#include "userapi.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkCookie>
#include <QUrlQuery>
#include <QSettings>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>

QString hashPassword(const QString& password) {
    QByteArray data = password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

static QMutex s_logMutex;

static QString getHttpLogFilePath() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString logDir = appDir + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return logDir + QString("/http_request_%1.log").arg(timestamp);
}

static void writeHttpLog(const QString& type, const QString& endpoint, const QString& requestBody,
                         int statusCode, const QString& responseBody, const QString& error) {
    QMutexLocker locker(&s_logMutex);

    QString logFilePath = getHttpLogFilePath();
    QFile file(logFilePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "Cannot open http log file:" << logFilePath;
        return;
    }

    QTextStream out(&file);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString method = type;

    out << "==========" << timestamp << "==========\n";
    out << "METHOD:" << method << "\n";
    out << "ENDPOINT:" << endpoint << "\n";

    if (!requestBody.isEmpty()) {
        out << "REQUEST:" << requestBody << "\n";
    }

    if (!error.isEmpty()) {
        out << "ERROR:" << error << "\n";
    } else {
        out << "STATUS:" << statusCode << "\n";
        if (!responseBody.isEmpty()) {
            out << "RESPONSE:" << responseBody << "\n";
        }
    }
    out << "\n";
    file.close();
}

ApiClient* ApiClient::instance() {
    static ApiClient client;
    return &client;
}

ApiClient::ApiClient() {
    m_manager = new QNetworkAccessManager(this);
    m_baseUrl = CLOUD_API_URL;
    m_authToken = "";
}

ApiClient::~ApiClient() {
}

void ApiClient::setBaseUrl(const QString& url) {
    m_baseUrl = url;
}

void ApiClient::setAuthToken(const QString& token) {
    m_authToken = token;
    qDebug() << "Auth token set:" << token.left(20) << "...";
}

void ApiClient::get(const QString& endpoint, const QJsonObject& params) {
    QString url = m_baseUrl + endpoint;
    if (!params.isEmpty()) {
        QUrlQuery query;
        for (auto it = params.begin(); it != params.end(); ++it) {
            query.addQueryItem(it.key(), it.value().toString());
        }
        url += "?" + query.toString();
    }

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    QNetworkReply* reply = m_manager->get(request);
    reply->setProperty("endpoint", endpoint);
    reply->setProperty("requestBody", QString());
    reply->setProperty("httpMethod", "GET");
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onReplyFinished);
}

void ApiClient::post(const QString& endpoint, const QJsonObject& data) {
    post(endpoint, QJsonDocument(data).toJson(QJsonDocument::Compact));
}

void ApiClient::post(const QString& endpoint, const QString& jsonStr) {
    QNetworkRequest request;
    request.setUrl(QUrl(m_baseUrl + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    QNetworkReply* reply = m_manager->post(request, jsonStr.toUtf8());
    reply->setProperty("endpoint", endpoint);
    reply->setProperty("requestBody", jsonStr);
    reply->setProperty("httpMethod", "POST");
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onReplyFinished);
}

void ApiClient::onReplyFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "[API 响应] reply 为空";
        return;
    }

    QString endpoint = reply->property("endpoint").toString();
    QString requestBody = reply->property("requestBody").toString();
    QString httpMethod = reply->property("httpMethod").toString();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "[API 响应]" << "endpoint:" << endpoint << "status:" << statusCode;

    QByteArray data = reply->readAll();
    QString responseBody = QString::fromUtf8(data);
    qDebug() << "[API 响应] 响应数据:" << responseBody;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull() && doc.isObject()) {
        qDebug() << "[API 响应] JSON 解析成功，发送 requestSuccess";
        writeHttpLog(httpMethod, endpoint, requestBody, statusCode, responseBody, QString());
        emit requestSuccess(endpoint, doc);
        reply->deleteLater();
        return;
    }

    // JSON解析失败处理
    if (data.isEmpty()) {
        qDebug() << "[API 响应] 响应为空";
        writeHttpLog(httpMethod, endpoint, requestBody, statusCode, QString(), "Empty response");
        emit requestFailed(endpoint, statusCode, "Empty response");
        reply->deleteLater();
        return;
    }

    // 网络错误处理（超时、连接失败等）
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[API 响应] 网络错误:" << reply->errorString();
        writeHttpLog(httpMethod, endpoint, requestBody, statusCode, responseBody, reply->errorString());
        emit requestFailed(endpoint, statusCode, reply->errorString());
        reply->deleteLater();
        return;
    }

    // HTTP错误处理（非2xx状态码）
    if (statusCode < 200 || statusCode >= 300) {
        QString error = responseBody.isEmpty() ? QString("HTTP %1").arg(statusCode) : responseBody;
        qDebug() << "[API 响应] HTTP 错误:" << error;
        writeHttpLog(httpMethod, endpoint, requestBody, statusCode, responseBody, error);
        emit requestFailed(endpoint, statusCode, error);
        reply->deleteLater();
        return;
    }

    // JSON格式错误处理（HTTP 200但JSON无效）
    qDebug() << "[API 响应] JSON 解析失败，响应内容:" << responseBody.left(200);
    QString jsonError = "Invalid JSON response";
    writeHttpLog(httpMethod, endpoint, requestBody, statusCode, responseBody, jsonError);
    emit requestFailed(endpoint, statusCode, jsonError);
    reply->deleteLater();
}

UserManager* UserManager::instance() {
    static UserManager manager;
    return &manager;
}

UserManager::UserManager() : QObject(), m_pendingRequest(""), m_isLoggedIn(false) {
    ApiClient* client = ApiClient::instance();
    connect(client, &ApiClient::requestSuccess, this, &UserManager::onApiResponse);
    connect(client, &ApiClient::requestFailed, this, &UserManager::onRequestFailed);
}

void UserManager::login(const QString& email, const QString& password) {
    qDebug() << "[登录请求]" << QTime::currentTime().toString();
    qDebug() << "  - Email:" << email;
    
    QJsonObject data;
    data["email"] = email;
    data["password"] = hashPassword(password);
    
    m_pendingRequest = "/api/auth/login";
    ApiClient::instance()->post("/api/auth/login", data);
}

void UserManager::loginByUsername(const QString& username, const QString& password) {
    qDebug() << "[登录请求 - 用户名]" << QTime::currentTime().toString();
    qDebug() << "  - Username:" << username;
    
    QJsonObject data;
    data["username"] = username;
    data["password"] = hashPassword(password);
    
    m_pendingRequest = "/api/auth/login";
    ApiClient::instance()->post("/api/auth/login", data);
}

void UserManager::loginAuto(const QString& identifier, const QString& password) {
    qDebug() << "[登录请求 - 自动判断]" << QTime::currentTime().toString();
    qDebug() << "  - Identifier:" << identifier;
    
    QRegularExpression emailRe("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Z|a-z]{2,}$");
    bool isEmail = emailRe.match(identifier).hasMatch();
    
    if (isEmail) {
        login(identifier, password);
    } else {
        loginByUsername(identifier, password);
    }
}

void UserManager::registerUser(const QString& username, const QString& email, const QString& password) {
    qDebug() << "[注册请求]" << QTime::currentTime().toString();
    qDebug() << "  - Username:" << username;
    qDebug() << "  - Email:" << email;
    
    QJsonObject data;
    data["username"] = username;
    data["email"] = email;
    data["password"] = hashPassword(password);
    
    m_pendingRequest = "/api/auth/register";
    ApiClient::instance()->post("/api/auth/register", data);
}

void UserManager::checkEmailExists(const QString& email) {
    qDebug() << "[检查邮箱] 开始检查:" << email;
    qDebug() << "[检查邮箱] 当前 pendingRequest:" << m_pendingRequest;
    m_pendingRequest = "/api/auth/check-email";
    QString url = "/api/auth/check-email?email=" + QUrl::toPercentEncoding(email);
    qDebug() << "[检查邮箱] 请求 URL:" << url;
    ApiClient::instance()->get(url);
}

void UserManager::checkUsernameExists(const QString& username) {
    qDebug() << "[检查用户名] 开始检查:" << username;
    m_pendingRequest = "/api/auth/check-username";
    QString url = "/api/auth/check-username?username=" + QUrl::toPercentEncoding(username);
    qDebug() << "[检查用户名] 请求 URL:" << url;
    ApiClient::instance()->get(url);
}

void UserManager::logout() {
    qDebug() << "[登出] 用户:" << m_currentUser.username << "开始登出";
    
    // 调用后端 API 记录登出日志（先发送请求，收到响应后再清除本地数据）
    m_pendingRequest = "/api/auth/logout";
    ApiClient::instance()->post("/api/auth/logout", QJsonObject());
    
    qDebug() << "[登出] 已发送登出请求，等待响应";
}

void UserManager::fetchProfile() {
    if (m_token.isEmpty()) {
        emit loginFailed("Not logged in");
        return;
    }
    
    m_pendingRequest = "/api/auth/profile";
    ApiClient::instance()->get("/api/auth/profile");
}

void UserManager::autoLogin() {
    qDebug() << "[自动登录] 开始检查保存的用户信息";
    
    QSettings settings;
    QString token = settings.value("cloud_user_token").toString();
    QString email = settings.value("cloud_user_email").toString();
    
    if (!token.isEmpty() && !email.isEmpty()) {
        qDebug() << "[自动登录] 找到保存的用户信息:" << email;
        m_token = token;
        ApiClient::instance()->setAuthToken(token);
        m_pendingRequest = "/api/auth/profile";
        ApiClient::instance()->get("/api/auth/profile");
    } else {
        qDebug() << "[自动登录] 未找到保存的用户信息，跳过自动登录";
    }
}

void UserManager::updateProfile(const QString& username, const QString& avatar) {
    if (!m_isLoggedIn) {
        emit loginFailed("Not logged in");
        return;
    }
    
    m_pendingRequest = "/api/user/update-profile";
    QJsonObject data;
    if (!username.isEmpty()) {
        data["username"] = username;
    }
    if (!avatar.isEmpty()) {
        data["avatar"] = avatar;
    }
    
    ApiClient::instance()->post("/api/user/update-profile", data);
}

void UserManager::changePassword(const QString& oldPassword, const QString& newPassword) {
    if (!m_isLoggedIn) {
        emit loginFailed("Not logged in");
        return;
    }
    
    m_pendingRequest = "/api/auth/change-password";
    QJsonObject data;
    data["old_password"] = hashPassword(oldPassword);
    data["new_password"] = hashPassword(newPassword);
    
    ApiClient::instance()->post("/api/auth/change-password", data);
}

void UserManager::requestPasswordReset(const QString& email) {
    qDebug() << "[请求密码重置] 邮箱:" << email;
    m_pendingRequest = "/api/auth/request-password-reset";
    QJsonObject data;
    data["email"] = email;
    
    ApiClient::instance()->post("/api/auth/request-password-reset", data);
}

void UserManager::resetPassword(const QString& token, const QString& newPassword) {
    qDebug() << "[重置密码] 使用 token 重置密码";
    m_pendingRequest = "/api/auth/reset-password";
    QJsonObject data;
    data["token"] = token;
    data["new_password"] = hashPassword(newPassword);
    
    ApiClient::instance()->post("/api/auth/reset-password", data);
}

void UserManager::onApiResponse(const QString& endpoint, const QJsonDocument& response) {
    qDebug() << "[API 响应]" << endpoint << "状态: 成功";
    
    QString requestEndpoint = endpoint.isEmpty() ? m_pendingRequest : endpoint;
    
    // 修复：移除查询参数，只保留路径部分
    if (requestEndpoint.contains("?")) {
        requestEndpoint = requestEndpoint.split("?").first();
        qDebug() << "[API 响应] 清理后的 endpoint:" << requestEndpoint;
    }
    
    if (requestEndpoint == "/api/auth/login") {
        onLoginResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/register") {
        onRegisterResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/check-email") {
        onEmailCheckResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/check-username") {
        onUsernameCheckResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/profile") {
        onProfileResponse(endpoint, response);
    } else if (requestEndpoint == "/api/user/update-profile") {
        onUpdateProfileResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/change-password") {
        onChangePasswordResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/request-password-reset") {
        onPasswordResetRequestResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/reset-password") {
        onPasswordResetResponse(endpoint, response);
    } else if (requestEndpoint == "/api/auth/logout") {
        onLogoutResponse(endpoint, response);
    } else {
        qDebug() << "[警告] 未处理的 API 响应:" << requestEndpoint;
    }
    
    m_pendingRequest = "";
}

void UserManager::onLoginResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/login") return;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        m_token = obj["token"].toString();
        ApiClient::instance()->setAuthToken(m_token);

        QJsonObject userObj = obj["user"].toObject();
        m_currentUser.id = userObj["id"].toInt();
        m_currentUser.email = userObj["email"].toString();
        m_currentUser.username = userObj["username"].toString();
        m_currentUser.vipLevel = userObj["vipLevel"].toInt();
        m_currentUser.lastLogin = userObj["lastLogin"].toString();
        m_currentUser.createdAt = userObj["createdAt"].toString();
        m_isLoggedIn = true;
        
        QSettings settings;
        settings.setValue("cloud_user_token", m_token);
        settings.setValue("cloud_user_email", m_currentUser.email);
        
        emit loginSuccess(m_currentUser);
        qDebug() << "[登录] 成功:" << m_currentUser.email;
    } else {
        QString error = obj["error"].toString();
        if (error.isEmpty()) {
            error = "登录失败";
        }
        qDebug() << "[登录] 失败:" << error;
        emit loginFailed(error);
    }
}

void UserManager::onRegisterResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/register") return;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit registerSuccess();
        qDebug() << "Registration successful, userId:" << obj["userId"].toInt();
    } else {
        QString error = obj["error"].toString();
        emit registerFailed(error);
        qDebug() << "Registration failed:" << error;
    }
}

void UserManager::onEmailCheckResponse(const QString& endpoint, const QJsonDocument& response) {
    qDebug() << "[邮箱检查响应] endpoint:" << endpoint;
    qDebug() << "[邮箱检查响应] pendingRequest:" << m_pendingRequest;
    
    if (!endpoint.contains("/api/auth/check-email")) {
        qDebug() << "[邮箱检查响应] endpoint 不匹配，返回";
        return;
    }
    
    QJsonObject obj = response.object();
    qDebug() << "[邮箱检查响应] 完整响应:" << QJsonDocument(obj).toJson();
    
    bool exists = obj["exists"].toBool();
    qDebug() << "Email exists:" << exists;
    emit emailCheckResult(exists);
}

void UserManager::onProfileResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/profile") return;
    
    QJsonObject obj = response.object();
    if (obj.contains("user")) {
        QJsonObject userObj = obj["user"].toObject();
        m_currentUser.id = userObj["id"].toInt();
        m_currentUser.email = userObj["email"].toString();
        m_currentUser.username = userObj["username"].toString();
        m_currentUser.vipLevel = userObj["vip_level"].toInt();
        m_currentUser.createdAt = userObj["created_at"].toString();
        m_currentUser.lastLogin = userObj["last_login"].toString();
        m_isLoggedIn = true;

        emit profileLoaded(m_currentUser);
        emit loginSuccess(m_currentUser);
        qDebug() << "Profile loaded and auto login success:" << m_currentUser.email;
    }
}

void UserManager::onUpdateProfileResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/user/update-profile") return;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit profileUpdated();
        qDebug() << "Profile updated successfully";
    } else {
        QString error = obj["error"].toString();
        emit loginFailed(error);
        qDebug() << "Profile update failed:" << error;
    }
}

void UserManager::onChangePasswordResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/change-password") return;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit passwordChanged();
        qDebug() << "Password changed successfully";
    } else {
        QString error = obj["error"].toString();
        emit loginFailed(error);
        qDebug() << "Password change failed:" << error;
    }
}

void UserManager::onUsernameCheckResponse(const QString& endpoint, const QJsonDocument& response) {
    if (!endpoint.contains("/api/auth/check-username")) return;
    
    QJsonObject obj = response.object();
    bool exists = obj["exists"].toBool();
    qDebug() << "[用户名检查响应] 用户名是否存在:" << exists;
    emit usernameCheckResult(exists);
}

void UserManager::onPasswordResetRequestResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/request-password-reset") return;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit passwordResetRequestComplete();
        qDebug() << "密码重置请求成功";
    } else {
        QString error = obj["error"].toString();
        emit passwordResetRequestFailed(error);
        qDebug() << "密码重置请求失败:" << error;
    }
}

void UserManager::onPasswordResetResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/reset-password") return;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit passwordResetComplete();
        qDebug() << "密码重置成功";
    } else {
        QString error = obj["error"].toString();
        emit passwordResetFailed(error);
        qDebug() << "密码重置失败:" << error;
    }
}

void UserManager::onLogoutResponse(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/auth/logout") return;
    
    QJsonObject obj = response.object();
    if (!obj["success"].toBool()) {
        qDebug() << "[登出响应] 后端 API 调用失败:" << obj["error"].toString();
    }
    
    // 清除本地数据
    m_token.clear();
    m_currentUser = {};
    m_isLoggedIn = false;
    ApiClient::instance()->setAuthToken("");
    
    QSettings settings;
    settings.remove("cloud_user_token");
    settings.remove("cloud_user_email");
    
    emit logoutComplete();
}

void UserManager::onRequestFailed(const QString& endpoint, int errorCode, const QString& error) {
    qDebug() << "[请求失败]" << "endpoint:" << endpoint << "errorCode:" << errorCode << "error:" << error;
    
    if (endpoint == "/api/auth/login") {
        emit loginFailed(error);
    } else if (endpoint == "/api/auth/register") {
        emit registerFailed(error);
    } else if (endpoint.contains("/api/auth/check-email")) {
        qDebug() << "[邮箱检查失败] 发送 false";
        emit emailCheckResult(false);
    } else if (endpoint.contains("/api/auth/check-username")) {
        qDebug() << "[用户名检查失败] 发送 false";
        emit usernameCheckResult(false);
    } else if (endpoint == "/api/user/update-profile") {
        emit loginFailed(error);
    } else if (endpoint == "/api/user/change-password") {
        emit loginFailed(error);
    } else if (endpoint == "/api/auth/request-password-reset") {
        emit passwordResetRequestFailed(error);
    } else if (endpoint == "/api/auth/reset-password") {
        emit passwordResetFailed(error);
    } else if (endpoint == "/api/auth/logout") {
        // 登出失败也要清除本地数据
        qDebug() << "[登出请求失败] 强制清除本地数据";
        m_token.clear();
        m_currentUser = {};
        m_isLoggedIn = false;
        ApiClient::instance()->setAuthToken("");
        QSettings settings;
        settings.remove("cloud_user_token");
        settings.remove("cloud_user_email");
        emit logoutComplete();
    }
}

ConfigSync* ConfigSync::instance() {
    static ConfigSync sync;
    return &sync;
}

ConfigSync::ConfigSync() : QObject(), m_isSyncing(false) {
    ApiClient* client = ApiClient::instance();
    connect(client, &ApiClient::requestSuccess, this, &ConfigSync::onConfigLoaded);
    connect(client, &ApiClient::requestSuccess, this, &ConfigSync::onConfigSaved);
    connect(client, &ApiClient::requestSuccess, this, &ConfigSync::onFRPCConfigLoaded);
    connect(client, &ApiClient::requestFailed, this, &ConfigSync::onRequestFailed);
}

void ConfigSync::loadSettings() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    ApiClient::instance()->get("/api/config/get");
}

void ConfigSync::fetchConfig() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }
    
    if (m_isSyncing) return;
    m_isSyncing = true;
    
    ApiClient::instance()->get("/api/config/get");
}

void ConfigSync::saveConfig(const QString& key, const QJsonObject& config) {
    QJsonObject configs;
    configs[key] = config;
    saveAllConfig(configs);
}

void ConfigSync::saveAllConfig(const QJsonObject& configs) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }
    
    if (m_isSyncing) return;
    m_isSyncing = true;
    
    QJsonObject data;
    data["configs"] = configs;
    
    ApiClient::instance()->post("/api/config/save", data);
}

void ConfigSync::onConfigLoaded(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/config/get") return;
    
    m_isSyncing = false;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        QJsonObject configs = obj["configs"].toObject();
        emit configLoaded(configs);
        qDebug() << "Config loaded successfully";
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "Config load failed:" << error;
    }
}

void ConfigSync::onConfigSaved(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/config/save") return;
    
    m_isSyncing = false;
    
    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit configSaved();
        qDebug() << "Config saved successfully";
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "Config save failed:" << error;
    }
}

void ConfigSync::onRequestFailed(const QString& endpoint, int errorCode, const QString& error) {
    if (endpoint == "/api/config/get" || endpoint == "/api/config/save" ||
        endpoint == "/api/config/frpc/get" || endpoint == "/api/config/frpc/save") {
        m_isSyncing = false;
        emit syncFailed(error);
        qDebug() << "Config sync failed:" << error;
    }
}

void ConfigSync::saveFRPCConfig(const QJsonObject& frpcConfig) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    QJsonObject data;
    data["frpc"] = frpcConfig;

    ApiClient::instance()->post("/api/config/frpc/save", data);
}

void ConfigSync::loadFRPCConfig() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    ApiClient::instance()->get("/api/config/frpc/get");
}

void ConfigSync::onFRPCConfigLoaded(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/config/frpc/get") return;

    m_isSyncing = false;

    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        QJsonObject frpcConfig = obj["frpc"].toObject();
        emit frpcConfigLoaded(frpcConfig);
        qDebug() << "FRPC config loaded successfully";
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "FRPC config load failed:" << error;
    }
}

// TaskSync implementation
TaskSync* TaskSync::instance() {
    static TaskSync sync;
    return &sync;
}

TaskSync::TaskSync() : QObject(), m_isSyncing(false) {
    ApiClient* client = ApiClient::instance();
    connect(client, &ApiClient::requestSuccess, this, &TaskSync::onTasksLoaded);
    connect(client, &ApiClient::requestSuccess, this, &TaskSync::onTasksSaved);
    connect(client, &ApiClient::requestFailed, this, &TaskSync::onRequestFailed);
}

void TaskSync::syncTasks() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    ApiClient::instance()->get("/api/config/tasks/get");
}

void TaskSync::uploadTasks(const QJsonArray& tasks) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    QJsonObject data;
    data["tasks"] = tasks;

    ApiClient::instance()->post("/api/config/tasks/sync", data);
}

void TaskSync::uploadTasksWithDeleted(const QJsonArray& tasks, const QStringList& deletedTaskIds) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    QJsonObject data;
    data["tasks"] = tasks;
    data["deletedTaskIds"] = QJsonArray::fromStringList(deletedTaskIds);

    ApiClient::instance()->post("/api/config/tasks/sync", data);
}

void TaskSync::downloadTasks() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    ApiClient::instance()->get("/api/config/tasks/get");
}

// 增量上传任务
void TaskSync::uploadIncrementalTasks(const QJsonArray& tasks, const QString& lastSyncTime) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    QJsonObject data;
    data["tasks"] = tasks;
    data["lastSyncTime"] = lastSyncTime;
    data["incremental"] = true;

    ApiClient::instance()->post("/api/config/tasks/incremental", data);
}

// 增量下载任务
void TaskSync::downloadIncrementalTasks(const QString& lastSyncTime) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    QJsonObject params;
    params["lastSyncTime"] = lastSyncTime;
    params["incremental"] = true;

    ApiClient::instance()->get("/api/config/tasks/incremental", params);
}

void TaskSync::onTasksLoaded(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/config/tasks/get") return;

    m_isSyncing = false;

    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        QJsonArray tasks = obj["tasks"].toArray();
        emit tasksSynced(tasks);
        qDebug() << "Tasks loaded successfully, count:" << tasks.size();
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "Tasks load failed:" << error;
    }
}

void TaskSync::onTasksSaved(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/config/tasks/sync") return;

    m_isSyncing = false;

    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit tasksUploadComplete();
        qDebug() << "Tasks saved successfully";
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "Tasks save failed:" << error;
    }
}

void TaskSync::onRequestFailed(const QString& endpoint, int errorCode, const QString& error) {
    if (endpoint == "/api/config/tasks/get" || endpoint == "/api/config/tasks/sync") {
        m_isSyncing = false;
        emit syncFailed(error);
        qDebug() << "Tasks sync failed:" << error;
    }
}

// ========== MemoSync 实现 ==========

MemoSync::MemoSync() : m_isSyncing(false) {
    ApiClient *client = ApiClient::instance();
    connect(client, &ApiClient::requestSuccess, this, &MemoSync::onMemosLoaded);
    connect(client, &ApiClient::requestSuccess, this, &MemoSync::onMemosSaved);
    connect(client, &ApiClient::requestFailed, this, &MemoSync::onRequestFailed);
}

MemoSync* MemoSync::instance() {
    static MemoSync instance;
    return &instance;
}

void MemoSync::syncMemos() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    ApiClient::instance()->get("/api/memos/get");
}

void MemoSync::uploadMemos(const QJsonArray& memos) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    QJsonObject data;
    data["memos"] = memos;

    ApiClient::instance()->post("/api/memos/sync", data);
}

void MemoSync::uploadIncrementalMemos(const QJsonArray& memos, const QString& lastSyncTime) {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    QJsonObject data;
    data["memos"] = memos;
    data["lastSyncTime"] = lastSyncTime;
    data["incremental"] = true;

    ApiClient::instance()->post("/api/memos/incremental", data);
}

void MemoSync::downloadMemos() {
    if (!UserManager::instance()->isLoggedIn()) {
        emit syncFailed("Not logged in");
        return;
    }

    if (m_isSyncing) return;
    m_isSyncing = true;

    ApiClient::instance()->get("/api/memos/get");
}

void MemoSync::onMemosLoaded(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/memos/get") return;

    m_isSyncing = false;

    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        QJsonArray memos = obj["memos"].toArray();
        emit memosSynced(memos);
        qDebug() << "Memos loaded successfully, count:" << memos.size();
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "Memos load failed:" << error;
    }
}

void MemoSync::onMemosSaved(const QString& endpoint, const QJsonDocument& response) {
    if (endpoint != "/api/memos/sync" && endpoint != "/api/memos/incremental") return;

    m_isSyncing = false;

    QJsonObject obj = response.object();
    if (obj["success"].toBool()) {
        emit memosUploadComplete();
        qDebug() << "Memos saved successfully";
    } else {
        QString error = obj["error"].toString();
        emit syncFailed(error);
        qDebug() << "Memos save failed:" << error;
    }
}

void MemoSync::onRequestFailed(const QString& endpoint, int errorCode, const QString& error) {
    if (endpoint == "/api/memos/get" || endpoint == "/api/memos/sync" || endpoint == "/api/memos/incremental") {
        m_isSyncing = false;
        emit syncFailed(error);
        qDebug() << "Memos sync failed:" << error;
    }
}
