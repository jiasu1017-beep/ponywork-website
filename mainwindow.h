#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QEvent>
#include <QLabel>
#include <QStatusBar>
#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include "modules/core/database.h"
#include "modules/update/updatemanager.h"
#include "modules/user/userapi.h"
#include "modules/user/usermenuwidget.h"
#include "modules/widgets/widgets_module.h"

// Forward declarations
struct ShortcutStat;

class AppManagerWidget;
class ShutdownWidget;
class SettingsWidget;
class CollectionManagerWidget;
class UpdateDialog;
class UpdateProgressDialog;
class RemoteDesktopWidget;
class WorkLogWidget;
class MemoWidget;
class BottomAppBar;
class RecommendAppWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    static constexpr int BOTTOM_APP_BAR_SHOW_DURATION = 300;
    static constexpr int BOTTOM_APP_BAR_HIDE_DURATION = 250;
    
public slots:
    void setStatusText(const QString &text);
    void showStatusMessage(const QString &text, int durationMs = 3000);
    void showLoginDialog();
    void showRegisterDialog();
    void resetApps();
    void refreshGlobalShortcut();
    void setBottomAppBarVisible(bool visible);
    void refreshBottomAppBarVisibility();
    void refreshAllWidgets();
    
private slots:
    void onTabChanged(int index);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onExitApp();
    void onShortcutActivated();
    void onUpdateAvailable(const UpdateInfo &info);
    void onNoUpdateAvailable();
    void onUpdateCheckFailed(const QString &error);
    void onUpdateNow();
    void onRemindLater();
    void onSkipThisVersion();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(const QString &filePath);
    void onDownloadFailed(const QString &error);
    void onExtractProgress(int percent);
    void onExtractFinished(const QString &extractPath);
    void onExtractFailed(const QString &error);
    void onInstallProgress(int percent);
    void onInstallFinished();
    void onInstallFailed(const QString &error);
    void onLogMessage(const QString &message);
    void onAutoLoginSuccess(const UserInfo& user);
    void onAutoLoginFailed();
    
protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    
#ifdef _WIN32
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif
    
private:
    void setupUI();
    void setupTrayIcon();
    void initPresetApps();
    void initPresetApps(bool forceReset);
    QString findOfficeAppPath(const QString &appName);
    QString getOfficeVersion();
    
    Database *db;
    QTabWidget *tabWidget;
    AppManagerWidget *appManagerWidget;
    ShutdownWidget *shutdownWidget;
    SettingsWidget *settingsWidget;
    UserWidget *userWidget;
    CollectionManagerWidget *collectionManagerWidget;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QAction *showWindowAction;
    QAction *exitAppAction;
    UpdateManager *updateManager;
    UpdateDialog *updateDialog;
    UpdateProgressDialog *updateProgressDialog;
    RemoteDesktopWidget *remoteDesktopWidget;
    WorkLogWidget *workLogWidget;
    MemoWidget *memoWidget;
    QPropertyAnimation *m_bottomAppBarAnimation;
    BottomAppBar *bottomAppBar;
    QLabel *statusLabel;
    QTimer *statusTimer;
    QString m_defaultStatusText;
    class UserMenuWidget *userMenuWidget;
    RecommendAppWidget *m_recommendAppWidget;

    void setupGlobalShortcut();
    void toggleWindow();
    void showShortcutTips();
};

#endif
