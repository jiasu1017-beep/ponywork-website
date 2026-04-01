#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>

enum AppType {
    AppType_Executable,
    AppType_Website,
    AppType_Folder,
    AppType_Document,
    AppType_RemoteDesktop
};

enum AppSortMode {
    AppSortMode_Recent,      // 最近使用
    AppSortMode_UsageCount,  // 使用次数
    AppSortMode_Manual       // 手动排序
};

enum SnapshotType {
    SnapshotType_Folder,
    SnapshotType_Website,
    SnapshotType_Document
};

struct AppInfo {
    int id;
    QString name;
    QString path;
    QString arguments;
    QString iconPath;
    QString category;
    int useCount;
    bool isFavorite;
    int sortOrder;
    AppType type;
    int remoteDesktopId;
    QDateTime lastUsedTime;
    bool isPinned;

    AppInfo() : id(0), useCount(0), isFavorite(false), sortOrder(0),
                type(AppType_Executable), remoteDesktopId(-1), isPinned(false) {}
};

Q_DECLARE_METATYPE(AppInfo)

struct SnapshotInfo {
    int id;
    QString name;
    QString path;
    QString description;
    SnapshotType type;
    QString thumbnailPath;
    QDateTime createdTime;
    QDateTime lastAccessedTime;
    QString folderStructure;
    QString fileTypeDistribution;
    QString websiteTitle;
    QString websiteUrl;
    QString documentTitle;
    QString documentAuthor;
    QString documentModifiedDate;
    int fileCount;
    qint64 totalSize;
    QString tags;
    bool isFavorite;
    int sortOrder;

    SnapshotInfo() : id(0), type(SnapshotType_Folder), fileCount(0),
                     totalSize(0), isFavorite(false), sortOrder(0) {}
};

struct AppCollection {
    int id;
    QString name;
    QString description;
    QList<int> appIds;
    QString tag;
    int sortPriority;

    AppCollection() : id(0), sortPriority(0) {}
};

struct RemoteDesktopConnection {
    int id;
    QString name;
    QString hostAddress;
    int port;
    QString username;
    QString password;
    QString domain;
    QString displayName;
    int screenWidth;
    int screenHeight;
    bool fullScreen;
    bool useAllMonitors;
    bool enableAudio;
    bool enableClipboard;
    bool enablePrinter;
    bool enableDrive;
    QString notes;
    QString category;
    int sortOrder;
    bool isFavorite;
    QDateTime createdTime;
    QDateTime lastUsedTime;

    RemoteDesktopConnection() : id(0), port(3389), screenWidth(1920), screenHeight(1080),
                                 fullScreen(true), useAllMonitors(false), enableAudio(true),
                                 enableClipboard(true), enablePrinter(false), enableDrive(false),
                                 sortOrder(0), isFavorite(false) {}
};

struct FRPCConfig {
    int id;
    int userId;              // 用户ID，用于区分不同用户
    QString serverAddr;
    int serverPort;
    int localPort;
    int remotePort;
    bool isEnabled;
    QString deviceName;
    qint64 frpcPid;          // 记录启动的frpc进程PID，用于检测重复启动
    QDateTime createdTime;
    QDateTime lastUsedTime;

    FRPCConfig() : id(0), userId(0), serverPort(7000), localPort(3389),
                  remotePort(0), isEnabled(false), frpcPid(0) {}
};

struct ShortcutStat {
    QString shortcut;
    int useCount;
    QDateTime lastUsed;
};

enum TaskPriority {
    TaskPriority_Low,
    TaskPriority_Medium,
    TaskPriority_High
};

enum MemoType {
    MemoType_Script,
    MemoType_Key,
    MemoType_Other
};

struct Memo {
    QString id;
    QString name;
    MemoType type;
    QString content;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
    int syncStatus;  // 0: pending, 1: synced

    Memo() : type(MemoType_Other), syncStatus(0) {}
};

Q_DECLARE_METATYPE(Memo)

enum TaskStatus {
    TaskStatus_Todo,
    TaskStatus_InProgress,
    TaskStatus_Paused,
    TaskStatus_Completed
};

struct Task {
    QString id;
    QString title;
    QString description;
    int categoryId;
    TaskPriority priority;
    TaskStatus status;
    double workDuration;
    QDateTime completionTime;
    QStringList tags;
    QDateTime updatedAt;
    qint64 version;  // 同步版本号，每次修改自增

    Task() : categoryId(0), priority(TaskPriority_Medium),
             status(TaskStatus_Todo), workDuration(0), version(0) {}
};

enum SyncConflictStrategy {
    SyncConflictStrategy_Local,      // 本地优先
    SyncConflictStrategy_Cloud,        // 云端优先
    SyncConflictStrategy_Manual        // 手动选择
};

enum SyncStatus {
    SyncStatus_Pending,   // 待同步
    SyncStatus_Synced,    // 已同步
    SyncStatus_Conflict  // 冲突
};

struct SyncState {
    QString entityType;      // "task", "config", "category"
    QString entityId;
    qint64 localVersion;     // 本地版本号
    qint64 lastSyncVersion;  // 上次同步的版本
    QDateTime lastSyncTime; // 上次同步时间
    SyncStatus status;      // 同步状态

    SyncState() : localVersion(0), lastSyncVersion(0), status(SyncStatus_Pending) {}
};

struct SyncLog {
    int id;
    QString entityType;
    QString entityId;
    QString action;          // "upload", "download", "conflict_resolved"
    QString beforeData;      // JSON
    QString afterData;       // JSON
    QString resolution;      // "local_wins", "cloud_wins", "manual"
    QDateTime timestamp;

    SyncLog() : id(0) {}
};

struct Category {
    int id;
    QString name;
    QString description;
    int parentId;
    QString color;
    int sortOrder;

    Category() : id(0), parentId(0), sortOrder(0) {}
};

class Database : public QObject
{
    Q_OBJECT
signals:
    void appsChanged();
    void tasksChanged();
public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    bool init();
    void setCurrentUser(int userId);
    int getCurrentUserId() const { return currentUserId; }
    
    bool addApp(const AppInfo &app);
    bool updateApp(const AppInfo &app);
    bool deleteApp(int id);
    QList<AppInfo> getAllApps();
    QList<AppInfo> getFavoriteApps();
    AppInfo getAppById(int id);
    int getMaxSortOrder();
    
    bool addCollection(const AppCollection &collection);
    bool updateCollection(const AppCollection &collection);
    bool deleteCollection(int id);
    QList<AppCollection> getAllCollections();
    AppCollection getCollectionById(int id);
    
    bool setAutoStart(bool enabled);
    bool getAutoStart();
    
    bool setMinimizeToTray(bool enabled);
    bool getMinimizeToTray();
    
    bool setShowClosePrompt(bool show);
    bool getShowClosePrompt();
    
    bool setShowBottomAppBar(bool show);
    bool getShowBottomAppBar();
    
    bool setAutoCheckUpdate(bool enabled);
    bool getAutoCheckUpdate();

    bool setRemoteDesktopAutoStart(bool enabled);
    bool getRemoteDesktopAutoStart();

    bool setRemoteDesktopAutoStop(bool enabled);
    bool getRemoteDesktopAutoStop();

    bool setShortcutKey(const QString &key);
    QString getShortcutKey();
    
    bool recordShortcutUsage(const QString &shortcut);
    QList<ShortcutStat> getShortcutStats();
    bool clearShortcutStats();
    
    bool setIgnoredVersion(const QString &version);
    QString getIgnoredVersion();

    bool addRemoteDesktop(const RemoteDesktopConnection &connection);
    bool updateRemoteDesktop(const RemoteDesktopConnection &connection);
    bool deleteRemoteDesktop(int id);
    QList<RemoteDesktopConnection> getAllRemoteDesktops();
    QList<RemoteDesktopConnection> getFavoriteRemoteDesktops();
    RemoteDesktopConnection getRemoteDesktopById(int id);
    QList<RemoteDesktopConnection> searchRemoteDesktops(const QString &keyword);

    // FRPC配置方法
    bool saveFRPCConfig(const FRPCConfig &config);
    FRPCConfig getFRPCConfig();
    bool deleteFRPCConfig();

    bool addSnapshot(const SnapshotInfo &snapshot);
    bool updateSnapshot(const SnapshotInfo &snapshot);
    bool deleteSnapshot(int id);
    QList<SnapshotInfo> getAllSnapshots();
    QList<SnapshotInfo> getSnapshotsByType(SnapshotType type);
    QList<SnapshotInfo> getFavoriteSnapshots();
    SnapshotInfo getSnapshotById(int id);
    QList<SnapshotInfo> searchSnapshots(const QString &keyword);

    bool addTask(const Task &task);
    bool addTaskWithId(const Task &task);  // 保留原有ID添加任务，用于同步
    bool updateTask(const Task &task);
    bool deleteTask(const QString &id);

    // 同步相关方法
    qint64 getNextTaskVersion();                    // 获取下一个任务版本号
    bool updateTaskVersion(const QString& id);     // 更新任务的版本号
    QList<Task> getTasksModifiedSince(const QDateTime& since);  // 获取指定时间后修改的任务

    // 同步状态管理
    bool saveSyncState(const SyncState& state);
    SyncState getSyncState(const QString& entityType, const QString& entityId);
    bool updateLastSyncTime(const QString& entityType, const QString& entityId, qint64 version);

    // 同步日志
    bool addSyncLog(const SyncLog& log);
    QList<SyncLog> getSyncLogs(const QString& entityType = QString(), int limit = 100);
    bool clearSyncLogs();
    QList<Task> getAllTasks();
    QList<Task> getTasksForDate(const QDate &date);
    QList<Task> getTasksByStatus(TaskStatus status);
    QList<Task> getTasksByCategory(int categoryId);
    QList<Task> getTasksByDateRange(const QDateTime &startDate, const QDateTime &endDate);
    QList<Task> searchTasks(const QString &keyword);
    Task getTaskById(const QString &id);
    bool updateTaskStatus(const QString &id, TaskStatus status);
    bool updateTaskDuration(const QString &id, double duration);

    static QList<Category> getBuiltinCategories();

    QHash<QString, double> getCategoryWorkHours(const QDateTime &startDate, const QDateTime &endDate);
    QHash<QString, int> getCategoryTaskCount(const QDateTime &startDate, const QDateTime &endDate);
    double getTotalWorkHours(const QDateTime &startDate, const QDateTime &endDate);
    int getTotalTaskCount(const QDateTime &startDate, const QDateTime &endDate);

    // 公开方法供外部调用保存和转换任务数据
    bool saveTaskData();
    QJsonObject taskToJson(const Task &task);

    // 备忘录方法
    bool addMemo(const Memo &memo);
    bool updateMemo(const Memo &memo);
    bool deleteMemo(const QString &id);
    QList<Memo> getAllMemos();
    Memo getMemoById(const QString &id);
    QList<Memo> getMemosModifiedSince(const QDateTime &since);
    QString generateMemoId();
    bool updateMemoSyncStatus(const QString &id, int status);
    
    // 已删除备忘录ID管理（用于同步删除）
    QStringList getDeletedMemoIds();
    void addDeletedMemoId(const QString &id);
    void clearDeletedMemoIds();

private:
    QString dataFilePath;
    QString taskFilePath;
    int currentUserId;
    QJsonObject rootObject;
    QJsonObject taskRootObject;
    int nextAppId;
    int nextCollectionId;
    int nextRemoteDesktopId;
    int nextSnapshotId;

    bool loadData();
    bool saveData();
    bool loadTaskData();
    bool migrateTaskData();
    QJsonObject appToJson(const AppInfo &app);
    AppInfo jsonToApp(const QJsonObject &obj);
    QJsonObject collectionToJson(const AppCollection &collection);
    AppCollection jsonToCollection(const QJsonObject &obj);
    QJsonObject remoteDesktopToJson(const RemoteDesktopConnection &connection);
    RemoteDesktopConnection jsonToRemoteDesktop(const QJsonObject &obj);
    QJsonObject snapshotToJson(const SnapshotInfo &snapshot);
    SnapshotInfo jsonToSnapshot(const QJsonObject &obj);
    Task jsonToTask(const QJsonObject &obj);
    QString encryptPassword(const QString &password);
    QString decryptPassword(const QString &encrypted);
    QString generateTaskId();
    int getDailyTaskCount(const QString &dateStr);
    bool taskExists(const QString &taskId);
};

Q_DECLARE_METATYPE(RemoteDesktopConnection)
Q_DECLARE_METATYPE(SnapshotInfo)
Q_DECLARE_METATYPE(AppCollection)
Q_DECLARE_METATYPE(Task)
Q_DECLARE_METATYPE(Category)

#endif
