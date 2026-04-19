# 应用推荐功能实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 PonyWork 客户端新增「应用推荐」Tab，后端管理员可配置推荐应用列表（含多个下载地址），客户端用户可查看和获取下载链接。

**Architecture:** 后端采用 Express + SQLite，新增 `recommended_apps` 和 `recommended_app_downloads` 两张表，提供 RESTful API。客户端采用 Qt Widgets，新建 RecommendAppWidget 独立 Tab，本地缓存机制实现离线可用。

**Tech Stack:** Node.js/Express (后端), Qt 5.15.2/C++ (客户端), SQLite (数据库)

---

## 文件结构

### 后端
- `server/admin-server.js` - 新增 API 端点和数据库初始化
- `server/admin-panel/index.html` - 新增应用推荐管理页面

### 客户端
- `modules/core/recommendedappscache.h` - 推荐应用缓存管理头文件
- `modules/core/recommendedappscache.cpp` - 推荐应用缓存管理实现
- `modules/widgets/recommendappwidget.h` - 推荐应用 Widget 头文件
- `modules/widgets/recommendappwidget.cpp` - 推荐应用 Widget 实现
- `modules/ui/recommendappwidget.ui` - Qt Designer UI 文件
- `modules/widgets/mainwindow.h` - 新增 Tab 声明
- `modules/widgets/mainwindow.cpp` - 新增 Tab 实现
- `PonyWork.pro` - 添加新文件到项目

---

## Task 1: 后端数据库和 API 端点

**Files:**
- Modify: `server/admin-server.js` - 新增表初始化和 API 路由

- [ ] **Step 1: 添加数据库表初始化代码**

在 `admin-server.js` 中找到数据库初始化位置（`initDatabase` 函数），添加：

```javascript
// 推荐应用表
db.run(`
    CREATE TABLE IF NOT EXISTS recommended_apps (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        category TEXT NOT NULL,
        description TEXT,
        icon_url TEXT,
        sort_order INTEGER DEFAULT 0,
        is_enabled INTEGER DEFAULT 1,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
`);

// 推荐应用下载地址表
db.run(`
    CREATE TABLE IF NOT EXISTS recommended_app_downloads (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        app_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        url TEXT NOT NULL,
        sort_order INTEGER DEFAULT 0,
        FOREIGN KEY (app_id) REFERENCES recommended_apps(id) ON DELETE CASCADE
    )
`);
```

- [ ] **Step 2: 添加客户端获取推荐应用 API**

在 `admin-server.js` 中添加（放在用户相关路由之后）：

```javascript
// 客户端获取推荐应用列表（公开）
app.get('/api/recommended-apps', (req, res) => {
    const apps = db.prepare(`
        SELECT id, name, category, description, icon_url as iconUrl, sort_order as sortOrder
        FROM recommended_apps
        WHERE is_enabled = 1
        ORDER BY sort_order ASC, id ASC
    `).all();

    // 获取每个应用的下载地址
    const downloads = db.prepare(`
        SELECT id, app_id as appId, name, url, sort_order as sortOrder
        FROM recommended_app_downloads
        WHERE app_id IN (SELECT id FROM recommended_apps WHERE is_enabled = 1)
        ORDER BY sort_order ASC
    `).all();

    // 合并数据
    const appsWithDownloads = apps.map(app => {
        app.downloads = downloads.filter(d => d.appId === app.id);
        return app;
    });

    res.json({ code: 0, data: appsWithDownloads });
});
```

- [ ] **Step 3: 添加管理员 API 路由**

```javascript
// 管理员获取推荐应用列表
app.get('/api/admin/recommended-apps', requireAdmin, (req, res) => {
    const apps = db.prepare(`
        SELECT id, name, category, description, icon_url, sort_order, is_enabled,
               created_at, updated_at
        FROM recommended_apps
        ORDER BY sort_order ASC, id ASC
    `).all();

    const downloads = db.prepare(`
        SELECT id, app_id, name, url, sort_order
        FROM recommended_app_downloads
        ORDER BY sort_order ASC
    `).all();

    const appsWithDownloads = apps.map(app => {
        app.downloads = downloads.filter(d => d.app_id === app.id);
        return app;
    });

    res.json({ code: 0, data: appsWithDownloads });
});

// 新增推荐应用
app.post('/api/admin/recommended-apps', requireAdmin, (req, res) => {
    const { name, category, description, iconUrl, sortOrder = 0, isEnabled = 1 } = req.body;
    if (!name || !category) {
        return res.status(400).json({ code: 1, message: 'name and category are required' });
    }

    const result = db.prepare(`
        INSERT INTO recommended_apps (name, category, description, icon_url, sort_order, is_enabled)
        VALUES (?, ?, ?, ?, ?, ?)
    `).run(name, category, description || '', iconUrl || '', sortOrder, isEnabled);

    res.json({ code: 0, data: { id: result.lastInsertRowid } });
});

// 修改推荐应用
app.put('/api/admin/recommended-apps/:id', requireAdmin, (req, res) => {
    const { id } = req.params;
    const { name, category, description, iconUrl, sortOrder, isEnabled } = req.body;

    const fields = [];
    const values = [];
    if (name) { fields.push('name = ?'); values.push(name); }
    if (category) { fields.push('category = ?'); values.push(category); }
    if (description !== undefined) { fields.push('description = ?'); values.push(description); }
    if (iconUrl !== undefined) { fields.push('icon_url = ?'); values.push(iconUrl); }
    if (sortOrder !== undefined) { fields.push('sort_order = ?'); values.push(sortOrder); }
    if (isEnabled !== undefined) { fields.push('is_enabled = ?'); values.push(isEnabled); }

    if (fields.length === 0) {
        return res.status(400).json({ code: 1, message: 'no fields to update' });
    }

    fields.push('updated_at = CURRENT_TIMESTAMP');
    values.push(id);

    db.prepare(`UPDATE recommended_apps SET ${fields.join(', ')} WHERE id = ?`).run(...values);
    res.json({ code: 0 });
});

// 删除推荐应用
app.delete('/api/admin/recommended-apps/:id', requireAdmin, (req, res) => {
    const { id } = req.params;
    db.prepare('DELETE FROM recommended_apps WHERE id = ?').run(id);
    res.json({ code: 0 });
});

// 新增下载地址
app.post('/api/admin/recommended-apps/:id/downloads', requireAdmin, (req, res) => {
    const { id } = req.params;
    const { name, url, sortOrder = 0 } = req.body;
    if (!name || !url) {
        return res.status(400).json({ code: 1, message: 'name and url are required' });
    }

    const result = db.prepare(`
        INSERT INTO recommended_app_downloads (app_id, name, url, sort_order)
        VALUES (?, ?, ?, ?)
    `).run(id, name, url, sortOrder);

    res.json({ code: 0, data: { id: result.lastInsertRowid } });
});

// 删除下载地址
app.delete('/api/admin/recommended-apps/:id/downloads/:did', requireAdmin, (req, res) => {
    const { did } = req.params;
    db.prepare('DELETE FROM recommended_app_downloads WHERE id = ?').run(did);
    res.json({ code: 0 });
});
```

- [ ] **Step 4: 提交后端代码**

```bash
cd f:/00AI/PonyWork
git add server/admin-server.js
git commit -m "feat(admin): 添加推荐应用管理 API"
```

---

## Task 2: 管理员后台前端页面

**Files:**
- Modify: `server/admin-panel/index.html` - 新增应用推荐管理 Tab

- [ ] **Step 1: 添加应用推荐管理 Tab HTML**

在 `server/admin-panel/index.html` 中找到现有 Tab 容器（如用户管理 Tab），添加新的 Tab 内容和对应的面板。

在 Tab 列表中添加：
```html
<button class="tab-btn" data-tab="recommended-apps">应用推荐</button>
```

添加 Tab 面板：
```html
<div id="recommended-apps-panel" class="tab-panel" style="display:none;">
    <div class="panel-header">
        <h2>应用推荐管理</h2>
        <button id="add-app-btn" class="btn-primary">新增应用</button>
    </div>
    <table id="apps-table" class="data-table">
        <thead>
            <tr>
                <th>ID</th>
                <th>名称</th>
                <th>分类</th>
                <th>下载地址数</th>
                <th>启用状态</th>
                <th>排序</th>
                <th>操作</th>
            </tr>
        </thead>
        <tbody></tbody>
    </table>
</div>
```

- [ ] **Step 2: 添加应用推荐管理 JavaScript**

添加管理页面的 JS 代码（新增/编辑/删除表单和逻辑）。由于这是已有管理后台，建议在 `admin-panel/` 下新增 `recommended-apps.js` 并在 index.html 中引入。

新建 `server/admin-panel/recommended-apps.js`：
```javascript
// 应用推荐管理模块
(function() {
    let apps = [];
    let editingAppId = null;

    // 加载应用列表
    async function loadApps() {
        const resp = await fetch('/api/admin/recommended-apps', {
            headers: { 'Authorization': `Bearer ${localStorage.getItem('admin_token')}` }
        });
        const data = await resp.json();
        if (data.code === 0) {
            apps = data.data;
            renderTable();
        }
    }

    // 渲染表格
    function renderTable() {
        const tbody = document.querySelector('#apps-table tbody');
        tbody.innerHTML = apps.map(app => `
            <tr data-id="${app.id}">
                <td>${app.id}</td>
                <td>${app.name}</td>
                <td>${app.category}</td>
                <td>${app.downloads ? app.downloads.length : 0}</td>
                <td>${app.is_enabled ? '是' : '否'}</td>
                <td>${app.sort_order}</td>
                <td>
                    <button onclick="RecommendedApps.edit(${app.id})">编辑</button>
                    <button onclick="RecommendedApps.delete(${app.id})">删除</button>
                    <button onclick="RecommendedApps.manageDownloads(${app.id})">下载地址</button>
                </td>
            </tr>
        `).join('');
    }

    // 暴露全局接口
    window.RecommendedApps = {
        loadApps,
        edit: (id) => { editingAppId = id; showEditModal(id); },
        delete: async (id) => { if (confirm('确认删除？')) { /* 调用 DELETE API */ loadApps(); } },
        manageDownloads: (id) => { showDownloadsModal(id); }
    };

    // 初始化时加载数据
    document.addEventListener('DOMContentLoaded', loadApps);
})();
```

- [ ] **Step 3: 提交管理后台代码**

```bash
git add server/admin-panel/index.html server/admin-panel/recommended-apps.js
git commit -m "feat(admin-panel): 添加应用推荐管理页面"
```

---

## Task 3: 客户端数据结构

**Files:**
- Create: `modules/core/recommendedappscache.h`
- Create: `modules/core/recommendedappscache.cpp`

- [ ] **Step 1: 创建 recommendedappscache.h**

```cpp
#ifndef RECOMMENDEDAPPSCACHE_H
#define RECOMMENDEDAPPSCACHE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonArray>

struct RecommendedAppDownload {
    int id;
    int appId;
    QString name;
    QString url;
    int sortOrder;
};

struct RecommendedApp {
    int id;
    QString name;
    QString category;
    QString description;
    QString iconUrl;
    int sortOrder;
    QList<RecommendedAppDownload> downloads;
};

class RecommendedAppsCache : public QObject
{
    Q_OBJECT
public:
    static RecommendedAppsCache* instance();

    QList<RecommendedApp> getApps();
    QList<QString> getCategories();
    void refreshFromServer();
    RecommendedApp getAppById(int id);

signals:
    void appsUpdated(const QList<RecommendedApp>& apps);
    void refreshFailed(const QString& error);

private:
    explicit RecommendedAppsCache(QObject *parent = nullptr);
    void loadFromDatabase();
    void saveToDatabase(const QList<RecommendedApp>& apps);
    RecommendedAppsCache(const RecommendedAppsCache&) = delete;
    RecommendedAppsCache& operator=(const RecommendedAppsCache&) = delete;

    QList<RecommendedApp> m_apps;
};

#endif // RECOMMENDEDAPPSCACHE_H
```

- [ ] **Step 2: 创建 recommendedappscache.cpp**

```cpp
#include "recommendedappscache.h"
#include "database.h"
#include "userapi.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlRecord>

RecommendedAppsCache* RecommendedAppsCache::instance()
{
    static RecommendedAppsCache inst;
    return &inst;
}

RecommendedAppsCache::RecommendedAppsCache(QObject *parent)
    : QObject(parent)
{
    loadFromDatabase();
}

QList<RecommendedApp> RecommendedAppsCache::getApps()
{
    return m_apps;
}

QList<QString> RecommendedAppsCache::getCategories()
{
    QList<QString> categories;
    for (const auto& app : m_apps) {
        if (!categories.contains(app.category)) {
            categories.append(app.category);
        }
    }
    return categories;
}

RecommendedApp RecommendedAppsCache::getAppById(int id)
{
    for (const auto& app : m_apps) {
        if (app.id == id) return app;
    }
    return RecommendedApp();
}

void RecommendedAppsCache::refreshFromServer()
{
    ApiClient::instance()->get("/api/recommended-apps");
    connect(ApiClient::instance(), &ApiClient::requestSuccess,
            this, [this](const QString& endpoint, const QJsonDocument& doc) {
        if (endpoint != "/api/recommended-apps") return;
        Q_UNUSED(endpoint);

        QJsonArray arr = doc.object()["data"].toArray();
        QList<RecommendedApp> apps;
        for (const auto& item : arr) {
            QJsonObject obj = item.toObject();
            RecommendedApp app;
            app.id = obj["id"].toInt();
            app.name = obj["name"].toString();
            app.category = obj["category"].toString();
            app.description = obj["description"].toString();
            app.iconUrl = obj["iconUrl"].toString();
            app.sortOrder = obj["sortOrder"].toInt();

            QJsonArray downloadsArr = obj["downloads"].toArray();
            for (const auto& d : downloadsArr) {
                QJsonObject dob = d.toObject();
                RecommendedAppDownload dl;
                dl.id = dob["id"].toInt();
                dl.appId = app.id;
                dl.name = dob["name"].toString();
                dl.url = dob["url"].toString();
                dl.sortOrder = dob["sortOrder"].toInt();
                app.downloads.append(dl);
            }
            apps.append(app);
        }

        m_apps = apps;
        saveToDatabase(apps);
        emit appsUpdated(apps);
    });

    connect(ApiClient::instance(), &ApiClient::requestFailed,
            this, [this](const QString& endpoint, int code, const QString& err) {
        if (endpoint != "/api/recommended-apps") return;
        Q_UNUSED(endpoint);
        Q_UNUSED(code);
        emit refreshFailed(err);
    });
}

void RecommendedAppsCache::loadFromDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT id, name, category, description, icon_url, sort_order FROM recommended_apps_cache");
    while (query.next()) {
        RecommendedApp app;
        app.id = query.value(0).toInt();
        app.name = query.value(1).toString();
        app.category = query.value(2).toString();
        app.description = query.value(3).toString();
        app.iconUrl = query.value(4).toString();
        app.sortOrder = query.value(5).toInt();

        QSqlQuery dlQuery(db);
        dlQuery.exec(QString("SELECT id, name, url, sort_order FROM recommended_app_downloads_cache WHERE app_id = %1").arg(app.id));
        while (dlQuery.next()) {
            RecommendedAppDownload dl;
            dl.id = dlQuery.value(0).toInt();
            dl.appId = app.id;
            dl.name = dlQuery.value(1).toString();
            dl.url = dlQuery.value(2).toString();
            dl.sortOrder = dlQuery.value(3).toInt();
            app.downloads.append(dl);
        }
        m_apps.append(app);
    }
}

void RecommendedAppsCache::saveToDatabase(const QList<RecommendedApp>& apps)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery delQuery(db);
    delQuery.exec("DELETE FROM recommended_apps_cache");
    delQuery.exec("DELETE FROM recommended_app_downloads_cache");

    QSqlQuery appQuery(db);
    for (const auto& app : apps) {
        appQuery.prepare("INSERT INTO recommended_apps_cache (id, name, category, description, icon_url, sort_order) VALUES (?, ?, ?, ?, ?, ?)");
        appQuery.addBindValue(app.id);
        appQuery.addBindValue(app.name);
        appQuery.addBindValue(app.category);
        appQuery.addBindValue(app.description);
        appQuery.addBindValue(app.iconUrl);
        appQuery.addBindValue(app.sortOrder);
        appQuery.exec();

        QSqlQuery dlQuery(db);
        for (const auto& dl : app.downloads) {
            dlQuery.prepare("INSERT INTO recommended_app_downloads_cache (id, app_id, name, url, sort_order) VALUES (?, ?, ?, ?, ?)");
            dlQuery.addBindValue(dl.id);
            dlQuery.addBindValue(app.id);
            dlQuery.addBindValue(dl.name);
            dlQuery.addBindValue(dl.url);
            dlQuery.addBindValue(dl.sortOrder);
            dlQuery.exec();
        }
    }
    db.commit();
}
```

**注意：** 客户端数据库需新增缓存表。在 `modules/core/database.cpp` 的 `initDatabase` 函数中添加：

```cpp
// 推荐应用缓存表
db.run("CREATE TABLE IF NOT EXISTS recommended_apps_cache ("
       "id INTEGER PRIMARY KEY, name TEXT, category TEXT, "
       "description TEXT, icon_url TEXT, sort_order INTEGER)");

db.run("CREATE TABLE IF NOT EXISTS recommended_app_downloads_cache ("
       "id INTEGER PRIMARY KEY, app_id INTEGER, name TEXT, "
       "url TEXT, sort_order INTEGER)");
```

- [ ] **Step 3: 提交客户端缓存代码**

```bash
git add modules/core/recommendedappscache.h modules/core/recommendedappscache.cpp
git add modules/core/database.cpp  # 如有修改
git commit -m "feat(client): 添加推荐应用缓存管理"
```

---

## Task 4: 客户端 UI 文件

**Files:**
- Create: `modules/ui/recommendappwidget.ui`

- [ ] **Step 1: 创建 recommendappwidget.ui**

使用 Qt Designer 创建 UI 文件，布局如下：

```
QVBoxLayout
├── HBoxLayout (工具栏)
│   ├── QLabel "分类:"
│   ├── QComboBox (categoryComboBox)
│   ├── QSpacer
│   └── QPushButton "刷新" (refreshBtn)
└── QScrollArea
    └── QWidget (containerWidget)
        └── QGridLayout (cardsLayout)
            [卡片 widgets]
```

卡片 Widget 内容：
```
QVBoxLayout (card)
├── QLabel (应用图标 80x80)
├── QLabel (应用名称)
├── QLabel (分类标签)
└── QLabel (简介，1行...)
```

详情弹窗 RecommendAppDetailDialog：
```
QVBoxLayout
├── QLabel (应用图标 120x120)
├── QLabel (应用名称)
├── QLabel (分类标签)
├── QTextEdit (description, readOnly)
├── QLabel "下载地址"
├── QVBoxLayout (downloadsList)
│   └── [重复: QLabel(平台) + QPushButton(下载) + QPushButton(复制)]
└── QPushButton "关闭"
```

- [ ] **Step 2: 提交 UI 文件**

```bash
git add modules/ui/recommendappwidget.ui
git commit -m "feat(client): 添加推荐应用 Widget UI"
```

---

## Task 5: 客户端 RecommendAppWidget

**Files:**
- Create: `modules/widgets/recommendappwidget.h`
- Create: `modules/widgets/recommendappwidget.cpp`

- [ ] **Step 1: 创建 recommendappwidget.h**

参考现有 `AppManagerWidget` 的结构，创建：

```cpp
#ifndef RECOMMENDAPPWIDGET_H
#define RECOMMENDAPPWIDGET_H

#include <QWidget>
#include "recommendedappscache.h"

namespace Ui {
class RecommendAppWidget;
}

class RecommendAppWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecommendAppWidget(QWidget *parent = nullptr);
    ~RecommendAppWidget();

private slots:
    void onRefreshClicked();
    void onCategoryChanged(int index);
    void onAppCardClicked(int appId);

private:
    void setupUI();
    void loadApps();
    void refreshCards(const QList<RecommendedApp>& apps);

    Ui::RecommendAppWidget *ui;
    QList<RecommendedApp> m_allApps;
};

#endif // RECOMMENDAPPWIDGET_H
```

- [ ] **Step 2: 创建 recommendappwidget.cpp**

```cpp
#include "recommendappwidget.h"
#include "ui_recommendappwidget.h"
#include "recommendedappscache.h"
#include "appdetaildialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QJsonDocument>

RecommendAppWidget::RecommendAppWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RecommendAppWidget)
{
    ui->setupUi(this);
    setupUI();
    loadApps();

    connect(RecommendedAppsCache::instance(), &RecommendedAppsCache::appsUpdated,
            this, &RecommendAppWidget::loadApps);
}

RecommendAppWidget::~RecommendAppWidget()
{
    delete ui;
}

void RecommendAppWidget::setupUI()
{
    // 设置分类下拉框
    ui->categoryComboBox->addItem("全部");
    connect(ui->refreshBtn, &QPushButton::clicked, this, &RecommendAppWidget::onRefreshClicked);
    connect(ui->categoryComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecommendAppWidget::onCategoryChanged);
}

void RecommendAppWidget::loadApps()
{
    m_allApps = RecommendedAppsCache::instance()->getApps();

    // 更新分类下拉框
    QString current = ui->categoryComboBox->currentText();
    ui->categoryComboBox->clear();
    ui->categoryComboBox->addItem("全部");
    for (const QString& cat : RecommendedAppsCache::instance()->getCategories()) {
        ui->categoryComboBox->addItem(cat);
    }

    refreshCards(m_allApps);
}

void RecommendAppWidget::onRefreshClicked()
{
    RecommendedAppsCache::instance()->refreshFromServer();
}

void RecommendAppWidget::onCategoryChanged(int index)
{
    QString category = ui->categoryComboBox->itemText(index);
    if (category == "全部") {
        refreshCards(m_allApps);
    } else {
        QList<RecommendedApp> filtered;
        for (const auto& app : m_allApps) {
            if (app.category == category) filtered.append(app);
        }
        refreshCards(filtered);
    }
}

void RecommendAppWidget::onAppCardClicked(int appId)
{
    RecommendedApp app = RecommendedAppsCache::instance()->getAppById(appId);
    if (app.id == 0) return;

    AppDetailDialog dlg(app, this);
    dlg.exec();
}

void RecommendAppWidget::refreshCards(const QList<RecommendedApp>& apps)
{
    // 清除现有卡片
    QLayoutItem* child;
    while ((child = ui->cardsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    // 创建新卡片（网格布局，每行3个）
    int row = 0, col = 0;
    const int maxCols = 3;
    for (const auto& app : apps) {
        QWidget* card = createAppCard(app);
        ui->cardsLayout->addWidget(card, row, col);
        col++;
        if (col >= maxCols) { col = 0; row++; }
    }
}

QWidget* RecommendAppWidget::createAppCard(const RecommendedApp& app)
{
    QWidget* card = new QWidget;
    card->setFixedSize(200, 180);
    card->setStyleSheet("QWidget { border: 1px solid #ddd; border-radius: 8px; }");

    QVBoxLayout* layout = new QVBoxLayout(card);

    // 图标
    QLabel* iconLabel = new QLabel;
    iconLabel->setFixedSize(80, 80);
    iconLabel->setAlignment(Qt::AlignCenter);
    if (!app.iconUrl.isEmpty()) {
        // 使用 QNetworkAccessManager 加载远程图标
        // 使用 QPixmap 和 QUrl 异步加载，设置默认图标作为占位
        QPixmap pixmap;
        if (pixmap.loadFromData(QByteArray())) { // 占位，实际需要网络请求
            iconLabel->setPixmap(pixmap.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } else {
        iconLabel->setText("📦");
    }
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);

    // 名称
    QLabel* nameLabel = new QLabel(app.name);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(nameLabel);

    // 分类
    QLabel* catLabel = new QLabel(app.category);
    catLabel->setAlignment(Qt::AlignCenter);
    catLabel->setStyleSheet("color: #666; font-size: 12px;");
    layout->addWidget(catLabel);

    // 简介
    QLabel* descLabel = new QLabel(app.description);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 11px; color: #888;");
    descLabel->setFixedHeight(20);
    layout->addWidget(descLabel);

    // 点击事件
    card->setCursor(Qt::PointingHandCursor);
    connect(card, &QWidget::clicked, [this, app]() { onAppCardClicked(app.id); });

    return card;
}
```

- [ ] **Step 3: 创建 AppDetailDialog**

```cpp
// 新建 appdetaildialog.h/cpp 或在 recommendappwidget.cpp 中内联
class AppDetailDialog : public QDialog
{
    Q_OBJECT
public:
    AppDetailDialog(const RecommendedApp& app, QWidget* parent = nullptr);

private slots:
    void onDownloadClicked(const QString& url);
    void onCopyUrlClicked(const QString& url);

private:
    RecommendedApp m_app;
};
```

- [ ] **Step 4: 提交 Widget 代码**

```bash
git add modules/widgets/recommendappwidget.h modules/widgets/recommendappwidget.cpp
git commit -m "feat(client): 添加推荐应用 Widget"
```

---

## Task 6: 主窗口集成

**Files:**
- Modify: `modules/widgets/mainwindow.h`
- Modify: `modules/widgets/mainwindow.cpp`

- [ ] **Step 1: 在 mainwindow.h 中添加 Tab 声明**

```cpp
#include "recommendappwidget.h"  // 添加 include

private:
    RecommendAppWidget *m_recommendAppWidget;  // 添加成员变量
```

- [ ] **Step 2: 在 mainwindow.cpp 中集成 Tab**

在 setupTabWidget 或类似函数中添加：

```cpp
m_recommendAppWidget = new RecommendAppWidget(this);
ui->tabWidget->addTab(m_recommendAppWidget, QString::fromUtf8("应用推荐"));
```

- [ ] **Step 3: 提交主窗口修改**

```bash
git add modules/widgets/mainwindow.h modules/widgets/mainwindow.cpp
git commit -m "feat(client): 集成推荐应用 Tab 到主窗口"
```

---

## Task 7: 项目文件更新

**Files:**
- Modify: `PonyWork.pro`

- [ ] **Step 1: 更新 PonyWork.pro**

在 SOURCES、HEADERS、FORMS 中添加新文件：

```qmake
SOURCES += \
    ...
    modules/core/recommendedappscache.cpp \
    modules/widgets/recommendappwidget.cpp

HEADERS += \
    ...
    modules/core/recommendedappscache.h \
    modules/widgets/recommendappwidget.h

FORMS += \
    ...
    modules/ui/recommendappwidget.ui
```

- [ ] **Step 2: 提交项目文件**

```bash
git add PonyWork.pro
git commit -m "chore: 添加推荐应用模块到项目"
```

---

## 验证方案

1. **后端 API 测试**：
```bash
# 启动后端
cd server && node admin-server.js

# 测试获取推荐应用（无需认证）
curl http://localhost:8080/api/recommended-apps

# 测试管理员接口（需先登录获取 token）
curl -H "Authorization: Bearer <admin_token>" http://localhost:8080/api/admin/recommended-apps
```

2. **客户端测试**：
```bash
# 编译
cd f:/00AI/PonyWork
./build.bat

# 运行客户端，验证：
# 1. 主窗口出现「应用推荐」Tab
# 2. 点击 Tab 显示推荐应用列表（卡片网格）
# 3. 分类筛选下拉框可用
# 4. 刷新按钮可触发更新
# 5. 点击卡片弹出详情对话框
# 6. 详情对话框显示多个下载地址
```
