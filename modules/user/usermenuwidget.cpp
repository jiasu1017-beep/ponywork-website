#include "usermenuwidget.h"
#include "userapi.h"
#include "userlogindialog.h"
#include "changepassworddialog.h"
#include "../core/database.h"
#include "../core/networkmonitor.h"
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QPixmap>

UserMenuWidget::UserMenuWidget(QWidget *parent)
    : QObject(parent)
    , m_parent(parent)
    , m_db(nullptr)
    , m_userMenuBtn(nullptr)
    , m_userMenu(nullptr)
    , m_loginAction(nullptr)
    , m_registerAction(nullptr)
    , m_syncTimer(nullptr)
    , m_pendingSync(false)
    , m_lastSyncTime(QDateTime::fromSecsSinceEpoch(0))
    , m_deletedTaskIds()
    , m_previousTaskIds()
    , m_networkMonitor(nullptr)
{
    // 创建用户菜单按钮
    m_userMenuBtn = new QToolButton(m_parent);
    m_userMenuBtn->setPopupMode(QToolButton::InstantPopup);
    m_userMenuBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_userMenuBtn->setCursor(Qt::PointingHandCursor);
    m_userMenuBtn->setIcon(QIcon(":/img/icon0.png"));
    m_userMenuBtn->setFixedSize(24, 24);
    m_userMenuBtn->setToolTip("用户");
    m_userMenuBtn->setStyleSheet("QToolButton::menu-indicator { image: none; }");

    // 创建用户菜单
    m_userMenu = new QMenu(m_parent);
    m_loginAction = new QAction("登录", m_userMenu);
    m_registerAction = new QAction("注册", m_userMenu);
    m_userMenu->addAction(m_loginAction);
    m_userMenu->addAction(m_registerAction);

    m_userMenu->addSeparator();

    // 添加反馈子菜单
    QMenu *feedbackMenu = m_userMenu->addMenu("反馈");
    QAction *websiteAction = new QAction("网站反馈", feedbackMenu);
    QAction *issueAction = new QAction("GitHub Issue", feedbackMenu);
    QAction *emailAction = new QAction("邮件反馈", feedbackMenu);
    QAction *wechatAction = new QAction("微信公众号", feedbackMenu);
    feedbackMenu->addAction(websiteAction);
    feedbackMenu->addAction(issueAction);
    feedbackMenu->addAction(emailAction);
    feedbackMenu->addAction(wechatAction);

    connect(websiteAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/jiasu1017-beep/PonyWork/issues"));
    });
    connect(issueAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/jiasu1017-beep/PonyWork/issues/new"));
    });
    connect(emailAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("mailto:wintersjia@163.com?subject=PonyWork反馈"));
    });
    connect(wechatAction, &QAction::triggered, this, []() {
        QMessageBox msgBox;
        msgBox.setWindowTitle("微信公众号");
        msgBox.setText("<div style='text-align:center'><p>微信公众号：梁柱墙笔记</p><p style='color:#888'>扫码关注，获取最新资讯</p></div>");
        msgBox.setIconPixmap(QPixmap(":/img/wechater.jpg").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.addButton(QMessageBox::Ok);
        msgBox.exec();
    });

    m_userMenuBtn->setMenu(m_userMenu);

    // 连接菜单动作
    connect(m_loginAction, &QAction::triggered, this, &UserMenuWidget::showLoginDialog);
    connect(m_registerAction, &QAction::triggered, this, &UserMenuWidget::showRegisterDialog);

    // 连接用户管理器信号
    connect(UserManager::instance(), &UserManager::loginSuccess, this, &UserMenuWidget::onLoginSuccess);
    connect(UserManager::instance(), &UserManager::logoutComplete, this, &UserMenuWidget::onLogoutComplete);

    // 连接任务同步信号（只连接一次）
    connect(TaskSync::instance(), &TaskSync::tasksSynced, this, &UserMenuWidget::onTasksSynced);
    connect(TaskSync::instance(), &TaskSync::tasksUploadComplete, this, &UserMenuWidget::onTasksUploadComplete);
    connect(TaskSync::instance(), &TaskSync::syncFailed, this, &UserMenuWidget::onTasksSyncFailed);

    // 检查当前登录状态
    updateMenuState();
}

UserMenuWidget::~UserMenuWidget()
{
    // m_userMenuBtn 和 m_userMenu 的父对象是 m_parent，会自动清理
    // 断开与 UserManager 的信号连接
    disconnect(UserManager::instance(), nullptr, this, nullptr);
}

void UserMenuWidget::setDatabase(Database *db)
{
    m_db = db;
    if (m_db) {
        // 创建防抖定时器（2秒延迟）
        m_syncTimer = new QTimer(this);
        m_syncTimer->setSingleShot(true);
        m_syncTimer->setInterval(2000); // 2秒防抖
        connect(m_syncTimer, &QTimer::timeout, this, &UserMenuWidget::onSyncTimerTimeout);

        // 连接任务变化信号，实现自动同步（带防抖）
        connect(m_db, &Database::tasksChanged, this, &UserMenuWidget::onTasksChanged);

        // 初始化网络监控
        m_networkMonitor = NetworkMonitor::instance();
        m_networkMonitor->startMonitoring();
        connect(m_networkMonitor, &NetworkMonitor::networkStatusChanged,
                this, &UserMenuWidget::onNetworkStatusChanged);
        connect(m_networkMonitor, &NetworkMonitor::syncRequested,
                this, &UserMenuWidget::onSyncRequested);
    }
}

void UserMenuWidget::showLoginDialog()
{
    UserLoginDialog dialog(m_parent);
    dialog.exec();
}

void UserMenuWidget::showRegisterDialog()
{
    UserLoginDialog dialog(m_parent);
    dialog.switchToRegister();
    dialog.exec();
}

void UserMenuWidget::onLoginSuccess()
{
    // 取消可能存在的防抖定时器，避免重复同步
    if (m_syncTimer && m_syncTimer->isActive()) {
        m_syncTimer->stop();
    }
    m_pendingSync = false;

    // 清理旧action
    m_loginAction = nullptr;
    m_registerAction = nullptr;

    m_userMenu->clear();
    UserInfo user = UserManager::instance()->currentUser();
    QString displayName = user.username.isEmpty() ? user.email : user.username;

    QAction *userInfo = new QAction("当前用户: " + displayName, m_userMenu);
    userInfo->setEnabled(false);
    m_userMenu->addAction(userInfo);
    m_userMenu->addSeparator();

    QAction *manageConfigAction = new QAction("云端配置管理", m_userMenu);
    connect(manageConfigAction, &QAction::triggered, this, &UserMenuWidget::onManageConfig);
    m_userMenu->addAction(manageConfigAction);

    QAction *changePwdAction = new QAction("修改密码", m_userMenu);
    connect(changePwdAction, &QAction::triggered, this, &UserMenuWidget::onChangePassword);
    m_userMenu->addAction(changePwdAction);

    m_userMenu->addSeparator();

    // 添加反馈子菜单
    QMenu *feedbackMenu = m_userMenu->addMenu("反馈");
    QAction *websiteAction = new QAction("网站反馈", feedbackMenu);
    QAction *issueAction = new QAction("GitHub Issue", feedbackMenu);
    QAction *emailAction = new QAction("邮件反馈", feedbackMenu);
    QAction *wechatAction = new QAction("微信公众号", feedbackMenu);
    feedbackMenu->addAction(websiteAction);
    feedbackMenu->addAction(issueAction);
    feedbackMenu->addAction(emailAction);
    feedbackMenu->addAction(wechatAction);

    connect(websiteAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/jiasu1017-beep/PonyWork/issues"));
    });
    connect(issueAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/jiasu1017-beep/PonyWork/issues/new"));
    });
    connect(emailAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("mailto:wintersjia@163.com?subject=PonyWork反馈"));
    });
    connect(wechatAction, &QAction::triggered, this, []() {
        QMessageBox msgBox;
        msgBox.setWindowTitle("微信公众号");
        msgBox.setText("<div style='text-align:center'><p>微信公众号：梁柱墙笔记</p><p style='color:#888'>扫码关注，获取最新资讯</p></div>");
        msgBox.setIconPixmap(QPixmap(":/img/wechater.jpg").scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.addButton(QMessageBox::Ok);
        msgBox.exec();
    });

    m_userMenu->addSeparator();

    QAction *logoutAction = new QAction("退出登录", m_userMenu);
    connect(logoutAction, &QAction::triggered, this, &UserMenuWidget::onLogout);
    m_userMenu->addAction(logoutAction);

    m_userMenuBtn->setToolTip("用户: " + displayName);
    m_userMenuBtn->setIcon(QIcon(":/img/icon.png"));

    emit statusMessageRequested("登录成功", 3000);

    // 延迟同步，等待 setCurrentUser 完成数据切换
    QTimer::singleShot(0, this, [this]() {
        syncTasksToCloud();
    });
}

void UserMenuWidget::onLogoutComplete()
{
    m_userMenu->clear();

    // 重新创建登录和注册action
    m_loginAction = new QAction("登录", m_userMenu);
    m_registerAction = new QAction("注册", m_userMenu);
    connect(m_loginAction, &QAction::triggered, this, &UserMenuWidget::showLoginDialog);
    connect(m_registerAction, &QAction::triggered, this, &UserMenuWidget::showRegisterDialog);

    m_userMenu->addAction(m_loginAction);
    m_userMenu->addAction(m_registerAction);
    m_userMenuBtn->setToolTip("用户");
    m_userMenuBtn->setIcon(QIcon(":/img/icon0.png"));

    emit statusMessageRequested("已退出登录", 3000);
}

void UserMenuWidget::onManageConfig()
{
    emit showBackupVersionsDialogRequested();
}

void UserMenuWidget::onChangePassword()
{
    ChangePasswordDialog dlg(nullptr);
    dlg.exec();
}

void UserMenuWidget::onLogout()
{
    UserManager::instance()->logout();
}

void UserMenuWidget::updateMenuState()
{
    // 检查当前登录状态并更新菜单
    UserInfo user = UserManager::instance()->currentUser();
    if (!user.email.isEmpty()) {
        onLoginSuccess();
    }
}

void UserMenuWidget::syncTasksToCloud()
{
    // 设置同步时间，登录时完整上传所有任务
    m_lastSyncTime = QDateTime::fromSecsSinceEpoch(0);

    // 先获取本地工作日志并上传，然后拉取云端工作日志
    if (m_db) {
        QList<Task> localTasks = m_db->getAllTasks();

        // 初始化上次任务ID列表
        m_previousTaskIds.clear();
        for (const Task &task : localTasks) {
            m_previousTaskIds.append(task.id);
        }

        QJsonArray tasksArray;
        for (const Task &task : localTasks) {
            QJsonObject taskObj;
            taskObj["id"] = task.id;
            taskObj["title"] = task.title;
            taskObj["description"] = task.description;
            taskObj["categoryId"] = task.categoryId;
            taskObj["priority"] = task.priority;
            taskObj["status"] = task.status;
            taskObj["workDuration"] = task.workDuration;
            taskObj["completionTime"] = task.completionTime.toUTC().toString(Qt::ISODate);
            taskObj["tags"] = QJsonArray::fromStringList(task.tags);
            taskObj["updatedAt"] = task.updatedAt.toUTC().toString(Qt::ISODate);
            tasksArray.append(taskObj);
        }
        // 登录时完整上传，清空删除列表
        TaskSync::instance()->uploadTasksWithDeleted(tasksArray, QStringList());
    }
}

void UserMenuWidget::onTasksSynced(const QJsonArray& tasks)
{
    // 收到云端工作日志，合并到本地数据库
    if (!m_db) return;

    // 获取本地已有的任务
    QList<Task> localTasks = m_db->getAllTasks();
    QMap<QString, Task> localTaskMap;
    for (const Task &task : localTasks) {
        localTaskMap[task.id] = task;
    }

    // 处理云端任务：添加新任务或更新已有任务
    for (const QJsonValue &taskVal : tasks) {
        QJsonObject taskObj = taskVal.toObject();
        QString taskId = taskObj["id"].toString();

        Task task;
        task.id = taskId;
        task.title = taskObj["title"].toString();
        task.description = taskObj["description"].toString();
        task.categoryId = taskObj["categoryId"].toInt();
        task.priority = static_cast<TaskPriority>(taskObj["priority"].toInt());
        task.status = static_cast<TaskStatus>(taskObj["status"].toInt());
        task.workDuration = taskObj["workDuration"].toDouble();
        task.completionTime = QDateTime::fromString(taskObj["completionTime"].toString(), Qt::ISODate).toLocalTime();

        // 读取云端任务的更新时间（UTC时间）
        QString updatedAtStr = taskObj["updatedAt"].toString();
        task.updatedAt = QDateTime::fromString(updatedAtStr, Qt::ISODate).toLocalTime();
        
        // 如果没有有效的更新时间，说明是旧数据，跳过该任务
        if (!task.updatedAt.isValid()) {
            qDebug() << "Skipping task with invalid updatedAt:" << taskId;
            continue;
        }

        QJsonArray tagsArray = taskObj["tags"].toArray();
        for (const QJsonValue &tagVal : tagsArray) {
            task.tags.append(tagVal.toString());
        }

        // 如果本地不存在，检查是否完全相同（避免重复添加）
        if (!localTaskMap.contains(taskId)) {
            // 检查是否有完全相同的任务（通过标题、分类、状态、工时判断）
            bool isDuplicate = false;
            for (const Task &localTask : localTasks) {
                if (localTask.title == task.title &&
                    localTask.categoryId == task.categoryId &&
                    localTask.status == task.status &&
                    qAbs(localTask.workDuration - task.workDuration) < 0.01) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                m_db->addTaskWithId(task);
            }
        } else {
            // 如果本地存在，比较更新时间，保留最新的
            Task localTask = localTaskMap[taskId];
            // 比较更新时间，保留最新的数据
            if (task.updatedAt.isValid() && localTask.updatedAt.isValid()) {
                if (task.updatedAt > localTask.updatedAt) {
                    // 云端数据更新，覆盖本地
                    m_db->updateTask(task);
                } else {
                    // 本地数据更新，不做处理（下次上传会同步到云端）
                }
            } else {
                // 如果没有有效的更新时间，默认保留本地数据
            }
        }
    }

    // 更新同步时间
    m_lastSyncTime = QDateTime::currentDateTime();

    qDebug() << "Tasks synced from cloud, count:" << tasks.size();
}

void UserMenuWidget::onTasksUploadComplete()
{
    // 上传成功后更新同步时间，确保上传失败时可以重试
    m_lastSyncTime = QDateTime::currentDateTime();
    qDebug() << "Tasks uploaded successfully, sync time updated";

    // 设置同步状态
    if (m_networkMonitor) {
        m_networkMonitor->setSyncing(false);
    }

    // 上传完成后，增量下载云端最新数据
    TaskSync::instance()->downloadIncrementalTasks(m_lastSyncTime.toUTC().toString(Qt::ISODate));
}

void UserMenuWidget::onTasksSyncFailed(const QString& error)
{
    qDebug() << "Tasks sync failed:" << error;
    if (m_networkMonitor) {
        m_networkMonitor->setSyncing(false);
    }
}

void UserMenuWidget::onNetworkStatusChanged(bool online)
{
    if (online) {
        qDebug() << "Network became online, checking for pending sync...";
        // 如果有待同步的数据，立即触发同步
        if (m_pendingSync && UserManager::instance()->isLoggedIn()) {
            onSyncTimerTimeout();
        }
    } else {
        qDebug() << "Network became offline";
    }
}

void UserMenuWidget::onSyncRequested()
{
    if (UserManager::instance()->isLoggedIn() && m_db) {
        qDebug() << "Sync requested from NetworkMonitor";
        if (!m_syncTimer->isActive()) {
            m_syncTimer->start();
        }
    }
}

void UserMenuWidget::onTasksChanged()
{
    // 当本地任务发生变化时，触发防抖同步
    // 检查网络状态：离线时不触发同步
    bool isOnline = m_networkMonitor ? m_networkMonitor->isOnline() : true;
    if (UserManager::instance()->isLoggedIn() && m_db && isOnline) {
        // 标记有待同步的数据
        m_pendingSync = true;

        // 如果定时器没有运行，启动防抖定时器
        if (!m_syncTimer->isActive()) {
            m_syncTimer->start();
            qDebug() << "Tasks changed, waiting 2s before sync...";
        } else {
            // 定时器已在运行，重置计时
            m_syncTimer->stop();
            m_syncTimer->start();
            qDebug() << "Tasks changed again, reset sync timer...";
        }
    }
}

void UserMenuWidget::onSyncTimerTimeout()
{
    // 检查网络状态
    bool isOnline = m_networkMonitor ? m_networkMonitor->isOnline() : true;
    if (!isOnline) {
        qDebug() << "Offline, skipping sync";
        return;
    }

    if (m_pendingSync && UserManager::instance()->isLoggedIn() && m_db) {
        m_pendingSync = false;

        // 设置同步状态
        if (m_networkMonitor) {
            m_networkMonitor->setSyncing(true);
        }

        qDebug() << "Sync timer timeout, uploading changed tasks to cloud...";

        // 增量上传：只上传自上次同步以来变化的任务
        QList<Task> localTasks = m_db->getAllTasks();

        // 检测被删除的任务：比较当前任务ID列表和上次同步时的任务ID列表
        QStringList currentTaskIds;
        for (const Task &task : localTasks) {
            currentTaskIds.append(task.id);
        }

        // 找出被删除的任务ID（在previous中但不在current中）
        QStringList deletedIds;
        for (const QString &prevId : m_previousTaskIds) {
            if (!currentTaskIds.contains(prevId)) {
                deletedIds.append(prevId);
                qDebug() << "Detected deleted task:" << prevId;
            }
        }

        QJsonArray tasksArray;
        for (const Task &task : localTasks) {
            // 检查任务是否在上次同步后有更新
            if (task.updatedAt.isValid() && task.updatedAt > m_lastSyncTime) {
                QJsonObject taskObj;
                taskObj["id"] = task.id;
                taskObj["title"] = task.title;
                taskObj["description"] = task.description;
                taskObj["categoryId"] = task.categoryId;
                taskObj["priority"] = task.priority;
                taskObj["status"] = task.status;
                taskObj["workDuration"] = task.workDuration;
                taskObj["completionTime"] = task.completionTime.toUTC().toString(Qt::ISODate);
                taskObj["tags"] = QJsonArray::fromStringList(task.tags);
                taskObj["updatedAt"] = task.updatedAt.toUTC().toString(Qt::ISODate);
                tasksArray.append(taskObj);
                qDebug() << "Uploading changed task:" << task.title;
            }
        }

        // 保存当前任务ID列表供下次比较
        m_previousTaskIds = currentTaskIds;

        if (tasksArray.size() > 0 || deletedIds.size() > 0) {
            // 使用新函数上传任务和删除列表
            TaskSync::instance()->uploadTasksWithDeleted(tasksArray, deletedIds);
            // 注意：m_lastSyncTime 在 uploadTasks 成功完成后更新
            // 以确保上传失败时可以重试
        } else {
            qDebug() << "No changed tasks to upload";
        }
    }
}
