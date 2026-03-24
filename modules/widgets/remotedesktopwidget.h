#ifndef REMOTEDESKTOPWIDGET_H
#define REMOTEDESKTOPWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QIODevice>
#include <QGroupBox>
#include <QToolBar>
#include <QSignalMapper>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "modules/core/database.h"
#include "modules/core/applicationmanager.h"
#include "modules/core/frpcmanager.h"

struct RDPConnectionInfo {
    QString serverAddress;
    int port;
    QString username;
    QString domain;
    int screenWidth;
    int screenHeight;
    int colorDepth;
    bool fullScreen;
    bool useAllMonitors;
    bool enableAudio;
    bool enableClipboard;
    bool enablePrinter;
    bool enableDrive;
    bool isValid;
    QString errorMessage;
};

// 分类选项常量（全局）
const QStringList CATEGORIES = {"未分类", "内网", "外网", "工作", "个人", "测试"};

class RemoteDesktopWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteDesktopWidget(Database *db, QWidget *parent = nullptr);

    static void launchRemoteDesktop(const RemoteDesktopConnection &conn, Database *db);
    void refreshConnectionList();

    // Event filter for drag and drop
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void collectionNeedsRefresh();
    void appListNeedsRefresh();
    void statusMessageRequested(const QString &message);

private slots:
    void onSearchTextChanged(const QString &text);
    void onAddConnection();
    void onEditConnection();
    void onDeleteConnection();
    void onConnect();
    void onTestConnection();
    void onImportConnections();
    void onExportConnections();
    void onConnectionSelectionChanged();
    void onToggleFavorite();
    void onTableContextMenuRequested(const QPoint &pos);
    void onAddToAppList();
    void onAddToCollection();
    void onMoveUp();
    void onMoveDown();
    void onSelectAll();
    void onSelectInverse();
    void onBatchDelete();
    void onBatchFavorite();
    void onBatchChangeCategory(const QString &category);
    QList<int> getSelectedConnectionIds();
    void updateSelectionLabel();
    void onTableCellChanged(int row, int column);
    void onCategoryComboBoxChanged(int row, int column, int previousIndex);
    void onCheckBoxStateChanged(int row, int state);
    void checkHostStatus(int row, const QString &hostAddress, int port);
    void checkHostByAddress(int row, const QHostAddress &address, int port);
    void onHostStatusChecked(int row, bool isOnline);
    void startBatchStatusCheck();
    void importFromRDPFile(const QString &filePath);
    void importFromJSONFile(const QString &filePath);

    // FRPC相关槽函数
    void onFRPCStart();
    void onFRPCStop();
    void onFRPCQuickSetup();
    void onFRPCExportRDP();
    void onFRPCSettings();
    void onFRPCAddToList();
    void onFRPCStatusChanged(FRPCManager::ConnectionStatus status);
    void onFRPCError(const QString &error);
    void onFRPCPortChanged(int port);
    void onFRPCStopped();
    void updateFRPCStatus();
    void setupFRPCUI(QVBoxLayout *mainLayout);

private:
    void setupUI();
    void loadConnections(const QList<RemoteDesktopConnection> &connections);
    RemoteDesktopConnection getSelectedConnection();
    void updateConnectionButtons();
    RDPConnectionInfo parseRDPFile(const QString &filePath);
    void parseFullAddress(const QString &fullAddress, RDPConnectionInfo &info);
    RemoteDesktopConnection rdpInfoToConnection(const RDPConnectionInfo &rdpInfo);

    Database *db;

    QTableWidget *connectionTable;
    QLineEdit *searchEdit;
    QPushButton *addButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    QPushButton *connectButton;
    QPushButton *testButton;
    QPushButton *favoriteButton;
    QPushButton *importButton;
    QPushButton *exportButton;
    QPushButton *moveUpButton;
    QPushButton *moveDownButton;
    QComboBox *categoryFilter;

    // 新增控件
    QToolBar *toolBar;
    QLabel *selectionLabel;
    QPushButton *selectAllButton;
    QPushButton *selectInverseButton;
    QPushButton *batchDeleteButton;

    // 状态检测
    QNetworkAccessManager *networkManager;
    QTimer *statusCheckTimer;
    bool isCheckingStatus;  // 防止并发检测
    QMap<int, bool> hostStatusMap;  // row -> isOnline

    // 拖拽排序
    int draggedRow;

    // Event filter for drag and drop
    void handleRowDrop(int sourceRow, int targetRow);
    void saveAllRowOrders();

    // FRPC相关控件
    QGroupBox *frpcGroupBox;
    QLineEdit *frpcDeviceNameEdit;
    QLabel *frpcStatusLabel;
    QLabel *frpcPortLabel;
    QPushButton *frpcQuickSetupButton;
    QPushButton *frpcStartButton;
    QPushButton *frpcStopButton;
    QPushButton *frpcExportButton;
    QPushButton *frpcSettingsButton;
    QPushButton *frpcAddToListButton;

    FRPCManager *frpcManager;
};

class RemoteDesktopDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteDesktopDialog(Database *db, QWidget *parent = nullptr);
    explicit RemoteDesktopDialog(Database *db, const RemoteDesktopConnection &connection, QWidget *parent = nullptr);

    RemoteDesktopConnection getConnection() const;

private slots:
    void validateForm();
    void onSave();

private:
    void setupUI();
    void loadConnection(const RemoteDesktopConnection &connection);

    Database *db;
    RemoteDesktopConnection m_connection;
    bool m_isEditMode;

    QLineEdit *nameEdit;
    QLineEdit *hostEdit;
    QSpinBox *portSpin;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *domainEdit;
    QSpinBox *widthSpin;
    QSpinBox *heightSpin;
    QCheckBox *fullScreenCheck;
    QCheckBox *allMonitorsCheck;
    QCheckBox *audioCheck;
    QCheckBox *clipboardCheck;
    QCheckBox *printerCheck;
    QCheckBox *driveCheck;
    QComboBox *categoryCombo;
    QTextEdit *notesEdit;
    QDialogButtonBox *buttonBox;
};

#endif
