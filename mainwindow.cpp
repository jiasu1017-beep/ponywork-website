#include "mainwindow.h"
#include "modules/core/database.h"
#include "modules/widgets/appmanagerwidget.h"
#include "modules/widgets/shutdownwidget.h"
#include "modules/widgets/settingswidget.h"
#include "modules/widgets/userwidget.h"
#include "modules/user/userlogindialog.h"
#include "modules/user/changepassworddialog.h"
#include "modules/user/userapi.h"
#include "modules/widgets/collectionmanagerwidget.h"
#include "modules/widgets/worklogwidget.h"
#include "modules/widgets/memowidget.h"
#include "modules/widgets/bottomappbar.h"
#include "modules/update/updatedialog.h"
#include "modules/update/updateprogressdialog.h"
#include "modules/widgets/remotedesktopwidget.h"
#include "modules/core/frpcmanager.h"
#include <QApplication>
#include <QStyle>
#include <QStandardPaths>
#include <QIcon>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QRegExp>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    db = new Database(this);
    if (!db->init()) {
        qWarning("Failed to initialize database!");
    }
    
    initPresetApps();
    
    updateManager = new UpdateManager(this);
    updateManager->setIgnoredVersion(db->getIgnoredVersion());
    
    connect(updateManager, &UpdateManager::checkForUpdatesStarted, this, [this]() {
        QString version = qApp->applicationVersion();
        setStatusText(QString("小马办公 v%1 - 正在检查更新...").arg(version));
    });
    connect(updateManager, &UpdateManager::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(updateManager, &UpdateManager::noUpdateAvailable, this, &MainWindow::onNoUpdateAvailable);
    connect(updateManager, &UpdateManager::updateCheckFailed, this, &MainWindow::onUpdateCheckFailed);
    connect(updateManager, &UpdateManager::downloadProgress, this, &MainWindow::onDownloadProgress);
    connect(updateManager, &UpdateManager::downloadFinished, this, &MainWindow::onDownloadFinished);
    connect(updateManager, &UpdateManager::downloadFailed, this, &MainWindow::onDownloadFailed);
    connect(updateManager, &UpdateManager::extractProgress, this, &MainWindow::onExtractProgress);
    connect(updateManager, &UpdateManager::extractFinished, this, &MainWindow::onExtractFinished);
    connect(updateManager, &UpdateManager::extractFailed, this, &MainWindow::onExtractFailed);
    connect(updateManager, &UpdateManager::installProgress, this, &MainWindow::onInstallProgress);
    connect(updateManager, &UpdateManager::installFinished, this, &MainWindow::onInstallFinished);
    connect(updateManager, &UpdateManager::installFailed, this, &MainWindow::onInstallFailed);
    connect(updateManager, &UpdateManager::logMessage, this, &MainWindow::onLogMessage);
    
    setupUI();
    setupTrayIcon();

    qDebug() << "[MainWindow] 启动自动登录流程";
    connect(UserManager::instance(), &UserManager::profileLoaded, this, &MainWindow::onAutoLoginSuccess);
    connect(UserManager::instance(), &UserManager::loginFailed, this, &MainWindow::onAutoLoginFailed);
    // 登录成功时切换用户任务数据（任务同步由UserMenuWidget统一处理）
    connect(UserManager::instance(), &UserManager::loginSuccess, this, [this](const UserInfo& user) {
        db->setCurrentUser(user.id);
        // 登录成功后自动同步备忘录
        if (memoWidget) {
            QTimer::singleShot(500, memoWidget, SLOT(onSyncMemos()));
        }
    });
    // 任务同步完成后刷新UI
    connect(TaskSync::instance(), &TaskSync::tasksSynced, this, [this]() {
        emit db->tasksChanged();
    });
    // 登出时切换回默认任务数据
    connect(UserManager::instance(), &UserManager::logoutComplete, this, [this]() {
        db->setCurrentUser(0);
    });
    UserManager::instance()->autoLogin();
    
    if (db->getAutoCheckUpdate()) {
        updateManager->startPeriodicChecks();
    }
    
    QList<AppCollection> collections = db->getAllCollections();
    if (!collections.isEmpty()) {
        tabWidget->setCurrentWidget(collectionManagerWidget);
    }
    
    QString version = qApp->applicationVersion();
    setWindowTitle(QString("小马办公 - PonyWork v%1").arg(version));
    setMinimumSize(1000, 700);
    resize(1100, 750);
    
    setupGlobalShortcut();
}

MainWindow::~MainWindow()
{
    if (m_bottomAppBarAnimation) {
        m_bottomAppBarAnimation->stop();
        m_bottomAppBarAnimation->deleteLater();
        m_bottomAppBarAnimation = nullptr;
    }
    if (bottomAppBar) {
        bottomAppBar->deleteLater();
    }
}

void MainWindow::showLoginDialog()
{
    userMenuWidget->showLoginDialog();
}

void MainWindow::showRegisterDialog()
{
    userMenuWidget->showRegisterDialog();
}

void MainWindow::refreshAllWidgets()
{
    // 刷新应用管理列表
    if (appManagerWidget) {
        appManagerWidget->refreshAppList();
    }

    // 刷新收藏管理列表
    if (collectionManagerWidget) {
        collectionManagerWidget->refreshCollectionList();
    }

    // 刷新远程桌面列表
    if (remoteDesktopWidget) {
        remoteDesktopWidget->refreshConnectionList();
    }

    qDebug() << "[MainWindow] All widgets refreshed";
}

void MainWindow::onAutoLoginSuccess(const UserInfo& user) {
    qDebug() << "[MainWindow] 自动登录成功:" << user.email;
    // 切换到该用户的任务数据（任务同步由UserMenuWidget统一处理）
    db->setCurrentUser(user.id);
    if (settingsWidget) {
        settingsWidget->updateCloudLoginStatus(user);
    }
    showStatusMessage("自动登录成功", 3000);
}

void MainWindow::onAutoLoginFailed() {
    qDebug() << "[MainWindow] 自动登录失败或未保存用户信息";
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    tabWidget = new QTabWidget(this);
    
    appManagerWidget = new AppManagerWidget(db, this);
    collectionManagerWidget = new CollectionManagerWidget(db, this);
    remoteDesktopWidget = new RemoteDesktopWidget(db, this);
    shutdownWidget = new ShutdownWidget(this);
    settingsWidget = new SettingsWidget(db, this);
    settingsWidget->setUpdateManager(updateManager);
    settingsWidget->setMainWindow(this);

    // 自动启动远程桌面（如果配置了自动启动）
    if (db->getRemoteDesktopAutoStart()) {
        FRPCConfig config = FRPCManager::instance()->getConfig();
        // 先检测是否有已运行的 frpc 进程
        if (!FRPCManager::instance()->isRunning()) {
            bool existingRunning = FRPCManager::instance()->checkExistingProcess();
            qDebug() << "[MainWindow] checkExistingProcess result:" << existingRunning;
        }
        // 如果检测后不在运行且配置为启用，则尝试启动
        if (config.isEnabled && !FRPCManager::instance()->isRunning()) {
            if (!FRPCManager::instance()->startFRPC()) {
                statusBar()->showMessage("自动启动远程桌面失败", 5000);
            }
        }
    }

    // 创建用户中心界面
    userWidget = new UserWidget(db, this);
    userWidget->setMainWindow(this);

    workLogWidget = new WorkLogWidget(db, this);

    memoWidget = new MemoWidget(db, this);
    
    connect(appManagerWidget, &AppManagerWidget::resetAppsRequested, this, &MainWindow::resetApps);
    connect(remoteDesktopWidget, &RemoteDesktopWidget::appListNeedsRefresh, appManagerWidget, &AppManagerWidget::refreshAppList);
    connect(remoteDesktopWidget, &RemoteDesktopWidget::collectionNeedsRefresh, collectionManagerWidget, &CollectionManagerWidget::refreshCollectionList);
    connect(remoteDesktopWidget, &RemoteDesktopWidget::collectionNeedsRefresh, collectionManagerWidget, &CollectionManagerWidget::refreshCollectionApps);
    connect(collectionManagerWidget, &CollectionManagerWidget::statusMessageRequested, this, &MainWindow::setStatusText);
    connect(remoteDesktopWidget, &RemoteDesktopWidget::statusMessageRequested, this, &MainWindow::setStatusText);
    
    tabWidget->addTab(appManagerWidget, QApplication::style()->standardIcon(QStyle::SP_DesktopIcon), "应用管理");
    tabWidget->addTab(collectionManagerWidget, QApplication::style()->standardIcon(QStyle::SP_DirHomeIcon), "集合管理");
    tabWidget->addTab(workLogWidget, QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView), "工作日志");
    tabWidget->addTab(remoteDesktopWidget, QApplication::style()->standardIcon(QStyle::SP_ComputerIcon), "远程桌面");
    tabWidget->addTab(shutdownWidget, QApplication::style()->standardIcon(QStyle::SP_BrowserStop), "定时关机");
    tabWidget->addTab(memoWidget, QApplication::style()->standardIcon(QStyle::SP_FileIcon), "备忘录");
    tabWidget->addTab(settingsWidget, QApplication::style()->standardIcon(QStyle::SP_FileDialogInfoView), "设置");
 
    tabWidget->setIconSize(QSize(24, 24));
    
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    
    mainLayout->addWidget(tabWidget);
    
    // 创建底部快捷应用条
    bottomAppBar = new BottomAppBar(db, this);
    mainLayout->addWidget(bottomAppBar);
    
    // 连接应用数据变化信号到底部快捷应用栏
    connect(db, &Database::appsChanged, bottomAppBar, &BottomAppBar::refreshApps);
    
    // 初始化动画指针
    m_bottomAppBarAnimation = nullptr;
    
    // 创建状态栏布局
    QWidget *statusBarWidget = new QWidget();
    QHBoxLayout *statusBarLayout = new QHBoxLayout(statusBarWidget);
    statusBarLayout->setContentsMargins(5, 0, 5, 0);
    
    statusLabel = new QLabel(this);
    QString version = qApp->applicationVersion();
    m_defaultStatusText = QString("小马办公 v%1 - 就绪").arg(version);
    statusLabel->setText(m_defaultStatusText);

    // 状态栏消息定时器
    statusTimer = new QTimer(this);
    statusTimer->setSingleShot(true);
    
    QPushButton *shortcutTipsBtn = new QPushButton("快捷键提示", this);
    shortcutTipsBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e0e0e0;"
        "    border: 1px solid #cccccc;"
        "    border-radius: 3px;"
        "    padding: 2px 8px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #d0d0d0;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #c0c0c0;"
        "}"
    );
    connect(shortcutTipsBtn, &QPushButton::clicked, this, &MainWindow::showShortcutTips);

    statusBarLayout->addWidget(statusLabel);

    // 用户菜单组件
    userMenuWidget = new UserMenuWidget(this);
    userMenuWidget->setDatabase(db);

    // 连接用户菜单信号
    connect(userMenuWidget, &UserMenuWidget::statusMessageRequested, this, &MainWindow::showStatusMessage);
    connect(userMenuWidget, &UserMenuWidget::showBackupVersionsDialogRequested, this, [=]() {
        userWidget->showBackupVersionsDialog();
    });

    // 添加工具栏放在右上角，与tab同一行
    QToolBar *userToolBar = new QToolBar(this);
    userToolBar->setMovable(false);
    userToolBar->setFloatable(false);
    userToolBar->addWidget(userMenuWidget->getUserMenuButton());
    userToolBar->setStyleSheet("QToolBar { border: none; }");
    userToolBar->setLayoutDirection(Qt::RightToLeft);
    addToolBar(Qt::TopToolBarArea, userToolBar);

    statusBarLayout->addStretch();
    statusBarLayout->addWidget(shortcutTipsBtn);
    
    statusBar()->addPermanentWidget(statusBarWidget, 1);
    
    // 初始化底部应用条显示状态
    setBottomAppBarVisible(db->getShowBottomAppBar());
}

void MainWindow::initPresetApps()
{
    initPresetApps(false);
}

void MainWindow::initPresetApps(bool forceReset)
{
    QList<AppInfo> existingApps = db->getAllApps();
    if (!existingApps.isEmpty() && !forceReset) {
        return;
    }
    
    if (forceReset) {
        for (const AppInfo &app : existingApps) {
            db->deleteApp(app.id);
        }
    }
    
    QString winDir = qgetenv("WINDIR");
    QString systemDir = winDir + "/System32";
    
    QList<AppInfo> presetApps;
    QStringList detectionMessages;
    
    AppInfo notepad;
    notepad.name = "记事本";
    notepad.path = systemDir + "/notepad.exe";
    notepad.category = "系统工具";
    notepad.isFavorite = true;
    notepad.sortOrder = 0;
    presetApps.append(notepad);
    
    AppInfo calc;
    calc.name = "计算器";
    calc.path = systemDir + "/calc.exe";
    calc.category = "系统工具";
    calc.isFavorite = true;
    calc.sortOrder = 1;
    presetApps.append(calc);
    
    AppInfo cmd;
    cmd.name = "命令提示符";
    cmd.path = systemDir + "/cmd.exe";
    cmd.category = "系统工具";
    cmd.sortOrder = 2;
    presetApps.append(cmd);
    
    AppInfo powershell;
    powershell.name = "PowerShell";
    powershell.path = systemDir + "/WindowsPowerShell/v1.0/powershell.exe";
    powershell.category = "系统工具";
    powershell.sortOrder = 3;
    presetApps.append(powershell);
    
    AppInfo mspaint;
    mspaint.name = "画图";
    mspaint.path = systemDir + "/mspaint.exe";
    mspaint.category = "系统工具";
    mspaint.sortOrder = 4;
    presetApps.append(mspaint);
    
    AppInfo taskmgr;
    taskmgr.name = "任务管理器";
    taskmgr.path = systemDir + "/taskmgr.exe";
    taskmgr.category = "系统工具";
    taskmgr.sortOrder = 5;
    presetApps.append(taskmgr);
    
    QString officeVersion = getOfficeVersion();
    if (!officeVersion.isEmpty()) {
        detectionMessages << QString("检测到 Microsoft Office 版本: %1").arg(officeVersion);
    }
    
    QString wordPath = findOfficeAppPath("WINWORD.EXE");
    if (!wordPath.isEmpty() && QFile::exists(wordPath)) {
        AppInfo word;
        word.name = "Microsoft Word";
        word.path = wordPath;
        word.category = "Office办公";
        word.sortOrder = 6;
        presetApps.append(word);
        detectionMessages << "✓ 已检测到 Microsoft Word";
    } else {
        detectionMessages << "✗ 未检测到 Microsoft Word";
    }
    
    QString excelPath = findOfficeAppPath("EXCEL.EXE");
    if (!excelPath.isEmpty() && QFile::exists(excelPath)) {
        AppInfo excel;
        excel.name = "Microsoft Excel";
        excel.path = excelPath;
        excel.category = "Office办公";
        excel.sortOrder = 7;
        presetApps.append(excel);
        detectionMessages << "✓ 已检测到 Microsoft Excel";
    } else {
        detectionMessages << "✗ 未检测到 Microsoft Excel";
    }
    
    QString pptPath = findOfficeAppPath("POWERPNT.EXE");
    if (!pptPath.isEmpty() && QFile::exists(pptPath)) {
        AppInfo powerpoint;
        powerpoint.name = "Microsoft PowerPoint";
        powerpoint.path = pptPath;
        powerpoint.category = "Office办公";
        powerpoint.sortOrder = 8;
        presetApps.append(powerpoint);
        detectionMessages << "✓ 已检测到 Microsoft PowerPoint";
    } else {
        detectionMessages << "✗ 未检测到 Microsoft PowerPoint";
    }
    
    for (const AppInfo &app : presetApps) {
        db->addApp(app);
    }
    
    QTimer::singleShot(1000, this, [this, detectionMessages]() {
        QString message = "🔍 Office 应用检测完成\n\n" + detectionMessages.join("\n");
        QMessageBox::information(this, "Office 检测完成", message);
    });
}

void MainWindow::onTabChanged(int index)
{
    if (tabWidget->widget(index) == collectionManagerWidget) {
        collectionManagerWidget->selectFirstCollection();
    }
    
    // 根据当前页面显示/隐藏底部应用栏
    bool shouldShowBottomAppBar = false;
    
    if (tabWidget->widget(index) == collectionManagerWidget ||
        tabWidget->widget(index) == workLogWidget ||
        tabWidget->widget(index) == remoteDesktopWidget) {
        shouldShowBottomAppBar = true;
    }
    
    // 检查用户设置是否允许显示
    if (db && !db->getShowBottomAppBar()) {
        shouldShowBottomAppBar = false;
    }
    
    setBottomAppBarVisible(shouldShowBottomAppBar);
}

void MainWindow::refreshBottomAppBarVisibility()
{
    // 强制刷新底部应用条的显示状态
    // 当用户在设置中切换"显示快捷应用条"开关时调用此方法
    // 通过重新评估当前标签页和用户设置来更新底部应用栏的显示状态
    int currentIndex = tabWidget->currentIndex();
    onTabChanged(currentIndex);
}

void MainWindow::setupTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    
    QIcon appIcon(":/img/icon.ico");
    if (appIcon.isNull()) {
        appIcon = qApp->style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    trayIcon->setIcon(appIcon);
    
    QString version = qApp->applicationVersion();
    trayIcon->setToolTip(QString("小马办公 - PonyWork v%1\n正在运行中").arg(version));
    
    trayMenu = new QMenu(this);
    
    showWindowAction = new QAction("打开窗口", this);
    connect(showWindowAction, &QAction::triggered, this, &MainWindow::onShowWindow);
    trayMenu->addAction(showWindowAction);
    
    trayMenu->addSeparator();
    
    exitAppAction = new QAction("退出程序", this);
    connect(exitAppAction, &QAction::triggered, this, &MainWindow::onExitApp);
    trayMenu->addAction(exitAppAction);
    
    trayIcon->setContextMenu(trayMenu);
    
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    
    trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        onShowWindow();
    }
}

void MainWindow::onShowWindow()
{
    if (isHidden()) {
        showNormal();
        activateWindow();
        raise();
    } else {
        showNormal();
        activateWindow();
        raise();
    }
}

void MainWindow::onExitApp()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    UnregisterHotKey(hwnd, 1);

    // 处理FRPC停止逻辑（与closeEvent相同的逻辑）
    qDebug() << "[MainWindow] onExitApp: autoStop=" << db->getRemoteDesktopAutoStop();
    if (db->getRemoteDesktopAutoStop()) {
        FRPCManager::instance()->stopFRPC();
    } else {
        FRPCManager::instance()->setAutoStopOnExit(false);
    }

    QApplication::quit();
}

#ifdef _WIN32
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY) {
        int hotkeyId = msg->wParam;
        if (hotkeyId == 1) {
            QString shortcut = db->getShortcutKey();
            db->recordShortcutUsage(shortcut);
            QTimer::singleShot(0, this, &MainWindow::toggleWindow);
            *result = 1;
            return true;
        }
    }
    return false;
}
#endif

void MainWindow::setupGlobalShortcut()
{
    QString shortcutStr = db->getShortcutKey();
    
    UINT modifiers = 0;
    UINT vk = 0;
    
    QStringList parts = shortcutStr.split("+");
    for (int i = 0; i < parts.size() - 1; ++i) {
        QString mod = parts[i].trimmed();
        if (mod == "Ctrl" || mod == "Control") {
            modifiers |= MOD_CONTROL;
        } else if (mod == "Alt") {
            modifiers |= MOD_ALT;
        } else if (mod == "Shift") {
            modifiers |= MOD_SHIFT;
        } else if (mod == "Win" || mod == "Windows") {
            modifiers |= MOD_WIN;
        }
    }
    
    QString key = parts.last().trimmed();
    if (key.length() == 1) {
        vk = VkKeyScanA(key[0].toLatin1()) & 0xFF;
    } else if (key == "F1") vk = VK_F1;
    else if (key == "F2") vk = VK_F2;
    else if (key == "F3") vk = VK_F3;
    else if (key == "F4") vk = VK_F4;
    else if (key == "F5") vk = VK_F5;
    else if (key == "F6") vk = VK_F6;
    else if (key == "F7") vk = VK_F7;
    else if (key == "F8") vk = VK_F8;
    else if (key == "F9") vk = VK_F9;
    else if (key == "F10") vk = VK_F10;
    else if (key == "F11") vk = VK_F11;
    else if (key == "F12") vk = VK_F12;
    else if (key == "Space") vk = VK_SPACE;
    else if (key == "Tab") vk = VK_TAB;
    else if (key == "Escape" || key == "Esc") vk = VK_ESCAPE;
    else if (key == "Enter" || key == "Return") vk = VK_RETURN;
    else if (key == "0") vk = 0x30;
    else if (key == "1") vk = 0x31;
    else if (key == "2") vk = 0x32;
    else if (key == "3") vk = 0x33;
    else if (key == "4") vk = 0x34;
    else if (key == "5") vk = 0x35;
    else if (key == "6") vk = 0x36;
    else if (key == "7") vk = 0x37;
    else if (key == "8") vk = 0x38;
    else if (key == "9") vk = 0x39;
    
    if (vk != 0) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        RegisterHotKey(hwnd, 1, modifiers, vk);
    }
}

void MainWindow::toggleWindow()
{
    if (isVisible()) {
        if (isMinimized()) {
            showNormal();
            activateWindow();
            raise();
            setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        } else if (isActiveWindow()) {
            if (db->getMinimizeToTray()) {
                hide();
                QString shortcut = db->getShortcutKey();
                trayIcon->showMessage(
                    "小马办公",
                    QString("已最小化到系统托盘\n点击托盘图标或按 %1 可重新打开窗口").arg(shortcut),
                    QSystemTrayIcon::Information,
                    4000
                );
            } else {
                showMinimized();
            }
        } else {
            activateWindow();
            raise();
            setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        }
    } else {
        show();
        activateWindow();
        raise();
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
}

void MainWindow::onShortcutActivated()
{
    toggleWindow();
}

void MainWindow::showShortcutTips()
{
    QString currentShortcut = db->getShortcutKey();
    QList<ShortcutStat> stats = db->getShortcutStats();
    
    QString tipsText = QString("当前全局快捷键: %1\n\n").arg(currentShortcut);
    tipsText += "快捷键使用统计:\n";
    
    if (stats.isEmpty()) {
        tipsText += "暂无使用记录";
    } else {
        for (int i = 0; i < std::min(5, stats.size()); ++i) {  // 显示前5个最常用的快捷键
            const ShortcutStat &stat = stats[i];
            tipsText += QString("%1: %2次 (最后使用: %3)\n")
                           .arg(stat.shortcut)
                           .arg(stat.useCount)
                           .arg(stat.lastUsed.toString("MM-dd hh:mm"));
        }
    }
    
    QMessageBox::information(this, "快捷键提示", tipsText);
}

void MainWindow::refreshGlobalShortcut()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    UnregisterHotKey(hwnd, 1);
    setupGlobalShortcut();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    bool minimizeToTray = db->getMinimizeToTray();
    bool showPrompt = db->getShowClosePrompt();
    bool shouldStopFRPC = false;
    
    if (!showPrompt) {
        if (minimizeToTray) {
            hide();
            QString version = qApp->applicationVersion();
            QString shortcut = db->getShortcutKey();
            trayIcon->showMessage(
                "小马办公",
                QString("已最小化到系统托盘\n点击托盘图标或按 %1 可重新打开窗口").arg(shortcut),
                QSystemTrayIcon::Information,
                4000
            );
            event->ignore();
        } else {
            shouldStopFRPC = true;
            // 直接退出时保存并上传任务数据
            if (UserManager::instance()->isLoggedIn()) {
                db->saveTaskData();
                QList<Task> tasks = db->getAllTasks();
                QJsonArray tasksArray;
                for (const Task &task : tasks) {
                    tasksArray.append(db->taskToJson(task));
                }
                TaskSync::instance()->uploadTasks(tasksArray);
            }
            event->accept();
        }
        return;
    }
    
    QMessageBox msgBox(this);
    
    if (minimizeToTray) {
        msgBox.setWindowTitle("关闭提示");
        msgBox.setText("关闭窗口将最小化到系统托盘");
        msgBox.setInformativeText("程序将在后台继续运行，您可以通过系统托盘图标重新打开窗口。\n\n勾选「不再显示此提示」可以跳过此确认。");
        msgBox.setIcon(QMessageBox::Information);
        
        QCheckBox *dontShowAgain = new QCheckBox("不再显示此提示", &msgBox);
        msgBox.setCheckBox(dontShowAgain);
        
        QPushButton *okButton = msgBox.addButton("确定", QMessageBox::AcceptRole);
        msgBox.addButton("取消", QMessageBox::RejectRole);
        msgBox.setDefaultButton(okButton);
        
        msgBox.exec();
        
        if (msgBox.clickedButton() == okButton) {
            if (dontShowAgain->isChecked()) {
                db->setShowClosePrompt(false);
            }
            hide();
            QString version = qApp->applicationVersion();
            QString shortcut = db->getShortcutKey();
            trayIcon->showMessage(
                "小马办公",
                QString("已最小化到系统托盘\n点击托盘图标或按 %1 可重新打开窗口").arg(shortcut),
                QSystemTrayIcon::Information,
                4000
            );
            event->ignore();
        } else {
            event->ignore();
        }
    } else {
        msgBox.setWindowTitle("退出确认");
        msgBox.setText("确定要完全退出应用吗？");
        msgBox.setInformativeText("程序将完全退出，所有功能将停止运行。\n\n勾选「不再显示此提示」可以跳过此确认。");
        msgBox.setIcon(QMessageBox::Warning);
        
        QCheckBox *dontShowAgain = new QCheckBox("不再显示此提示", &msgBox);
        msgBox.setCheckBox(dontShowAgain);
        
        QPushButton *okButton = msgBox.addButton("退出", QMessageBox::AcceptRole);
        msgBox.addButton("取消", QMessageBox::RejectRole);
        msgBox.setDefaultButton(okButton);
        
        msgBox.exec();
        
        if (msgBox.clickedButton() == okButton) {
            if (dontShowAgain->isChecked()) {
                db->setShowClosePrompt(false);
            }
            shouldStopFRPC = true;
            // 保存并上传当前用户任务数据
            if (UserManager::instance()->isLoggedIn()) {
                db->saveTaskData();
                QList<Task> tasks = db->getAllTasks();
                QJsonArray tasksArray;
                for (const Task &task : tasks) {
                    tasksArray.append(db->taskToJson(task));
                }
                TaskSync::instance()->uploadTasks(tasksArray);
            }
            event->accept();
        } else {
            event->ignore();
        }
    }

    // 自动停止远程桌面
    qDebug() << "[MainWindow] closeEvent: shouldStopFRPC=" << shouldStopFRPC << ", autoStop=" << db->getRemoteDesktopAutoStop();
    if (shouldStopFRPC) {
        if (db->getRemoteDesktopAutoStop()) {
            FRPCManager::instance()->stopFRPC();
        } else {
            // 用户未勾选自动停止，frpc进程保持运行（使用startDetached启动，已完全独立）
            FRPCManager::instance()->setAutoStopOnExit(false);
            qDebug() << "[MainWindow] auto stop disabled, FRPC will continue running";
        }
    }
}

void MainWindow::setStatusText(const QString &text)
{
    if (statusLabel) {
        statusLabel->setText(text);
    }
}

void MainWindow::showStatusMessage(const QString &text, int durationMs)
{
    if (statusLabel) {
        statusLabel->setText(text);
        if (durationMs > 0) {
            statusTimer->stop();
            disconnect(statusTimer, &QTimer::timeout, this, nullptr);
            connect(statusTimer, &QTimer::timeout, this, [this]() {
                statusLabel->setText(m_defaultStatusText);
            });
            statusTimer->start(durationMs);
        }
    }
}

void MainWindow::onUpdateAvailable(const UpdateInfo &info)
{
    setStatusText(QString("发现新版本 v%1").arg(info.version));
    updateDialog = new UpdateDialog(info, this);
    connect(updateDialog, &UpdateDialog::updateNow, this, &MainWindow::onUpdateNow);
    connect(updateDialog, &UpdateDialog::remindLater, this, &MainWindow::onRemindLater);
    connect(updateDialog, &UpdateDialog::skipThisVersion, this, &MainWindow::onSkipThisVersion);
    updateDialog->show();
}

void MainWindow::onNoUpdateAvailable()
{
    QString version = qApp->applicationVersion();
    setStatusText(QString("小马办公 v%1 - 已是最新版本").arg(version));
}

void MainWindow::onUpdateCheckFailed(const QString &error)
{
    qWarning() << "Update check failed:" << error;
    QString version = qApp->applicationVersion();
    setStatusText(QString("小马办公 v%1 - 检查更新失败").arg(version));
}

void MainWindow::onUpdateNow()
{
    updateProgressDialog = new UpdateProgressDialog(this);
    updateProgressDialog->setUpdateManager(updateManager);
    updateProgressDialog->show();
    
    updateManager->downloadUpdate();
}

void MainWindow::onRemindLater()
{
}

void MainWindow::onSkipThisVersion()
{
    updateManager->skipThisVersion();
    db->setIgnoredVersion(updateManager->ignoredVersion());
}

void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
}

void MainWindow::onDownloadFinished(const QString &filePath)
{
    updateManager->installUpdate(filePath);
}

void MainWindow::onExtractProgress(int percent)
{
}

void MainWindow::onExtractFinished(const QString &extractPath)
{
    Q_UNUSED(extractPath);
}

void MainWindow::onExtractFailed(const QString &error)
{
    Q_UNUSED(error);
}

void MainWindow::onInstallProgress(int percent)
{
    Q_UNUSED(percent);
}

void MainWindow::onInstallFinished()
{
}

void MainWindow::onInstallFailed(const QString &error)
{
    Q_UNUSED(error);
}

void MainWindow::onLogMessage(const QString &message)
{
    Q_UNUSED(message);
}

void MainWindow::onDownloadFailed(const QString &error)
{
    Q_UNUSED(error);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            bool minimizeToTray = db->getMinimizeToTray();
            if (minimizeToTray) {
                hide();
                QString version = qApp->applicationVersion();
                QString shortcut = db->getShortcutKey();
                trayIcon->showMessage(
                    "小马办公",
                    QString("已最小化到系统托盘\n点击托盘图标或按 %1 可重新打开窗口").arg(shortcut),
                    QSystemTrayIcon::Information,
                    4000
                );
            }
        }
    }
}

QString MainWindow::findOfficeAppPath(const QString &appName)
{
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + appName, QSettings::NativeFormat);
    QString path = settings.value(".").toString();
    
    if (path.isEmpty() || !QFile::exists(path)) {
        QSettings settings32("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + appName, QSettings::NativeFormat);
        path = settings32.value(".").toString();
    }
    
    return path;
}

QString MainWindow::getOfficeVersion()
{
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Office", QSettings::NativeFormat);
    QStringList versions = settings.childGroups();
    
    QString latestVersion = "";
    for (const QString &version : versions) {
        if (version.contains(QRegExp("^\\d+\\.\\d+$"))) {
            if (latestVersion.isEmpty() || version > latestVersion) {
                latestVersion = version;
            }
        }
    }
    
    return latestVersion;
}

void MainWindow::resetApps()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认初始化");
    msgBox.setText("确定要初始化应用列表吗？");
    msgBox.setInformativeText("原应用列表可能会被覆盖，此操作将重新检测并添加所有预设应用。\n\n此操作不可撤销！");
    msgBox.setIcon(QMessageBox::Warning);
    
    QPushButton *confirmBtn = msgBox.addButton("确认", QMessageBox::AcceptRole);
    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.setDefaultButton(cancelBtn);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == confirmBtn) {
        initPresetApps(true);
        if (appManagerWidget) {
            appManagerWidget->refreshAppList();
        }
        QMessageBox::information(this, "初始化完成", "应用列表已成功初始化！");
    }
}

void MainWindow::setBottomAppBarVisible(bool visible)
{
    if (bottomAppBar) {
        if (m_bottomAppBarAnimation) {
            m_bottomAppBarAnimation->stop();
            m_bottomAppBarAnimation->deleteLater();
            m_bottomAppBarAnimation = nullptr;
        }
        
        if (visible) {
            bottomAppBar->show();
            m_bottomAppBarAnimation = new QPropertyAnimation(bottomAppBar, "maximumHeight", this);
            m_bottomAppBarAnimation->setDuration(BOTTOM_APP_BAR_SHOW_DURATION);
            m_bottomAppBarAnimation->setEasingCurve(QEasingCurve::OutCubic);
            m_bottomAppBarAnimation->setStartValue(0);
            m_bottomAppBarAnimation->setEndValue(bottomAppBar->height());
            disconnect(m_bottomAppBarAnimation, &QPropertyAnimation::finished, nullptr, nullptr);
            connect(m_bottomAppBarAnimation, &QPropertyAnimation::finished, this, [this]() {
                bottomAppBar->setMaximumHeight(QWIDGETSIZE_MAX);
                m_bottomAppBarAnimation->deleteLater();
                m_bottomAppBarAnimation = nullptr;
            });
            m_bottomAppBarAnimation->start();
        } else {
            m_bottomAppBarAnimation = new QPropertyAnimation(bottomAppBar, "maximumHeight", this);
            m_bottomAppBarAnimation->setDuration(BOTTOM_APP_BAR_HIDE_DURATION);
            m_bottomAppBarAnimation->setEasingCurve(QEasingCurve::InCubic);
            m_bottomAppBarAnimation->setStartValue(bottomAppBar->height());
            m_bottomAppBarAnimation->setEndValue(0);
            disconnect(m_bottomAppBarAnimation, &QPropertyAnimation::finished, nullptr, nullptr);
            connect(m_bottomAppBarAnimation, &QPropertyAnimation::finished, this, [this]() {
                bottomAppBar->hide();
                bottomAppBar->setMaximumHeight(QWIDGETSIZE_MAX);
                m_bottomAppBarAnimation->deleteLater();
                m_bottomAppBarAnimation = nullptr;
            });
            m_bottomAppBarAnimation->start();
        }
    }
}
