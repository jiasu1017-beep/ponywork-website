# PonyWork 项目开发指南

## 架构原则

### 核心原则：高内聚、低耦合

**高内聚（High Cohesion）**：每个模块/类只负责单一职责，相关功能放在一起
- 一个类只做一件事
- 相关的数据和函数放在一起
- 类的代码行数不宜过长（建议 < 500 行）

**低耦合（Low Coupling）**：模块之间依赖尽量少，接口尽量简单
- 模块通过接口通信，不直接操作对方内部数据
- 依赖方向单一：上层依赖下层，下层不依赖上层
- 跨模块调用通过单例或依赖注入

### 违反原则的反面案例

```cpp
// ❌ 错误：职责不清，耦合严重
class UserManager {
    void login() { /* 登录逻辑 */ }
    void sendEmail() { /* 发邮件 */ }
    void updateUI() { /* 更新界面 */ }
    void writeDatabase() { /* 写库 */ }
};

// ❌ 错误：直接操作其他模块内部
class A { void doSomething(B* b) { b->internalData = 123; } };
```

### 正确示例

```cpp
// ✅ 正确：单一职责，接口清晰
class UserManager { void login(); };        // 只管登录
class MailService { void send(); };         // 只管发邮件
class UserUI { void update(); };            // 只管界面

// ✅ 正确：通过接口通信
class ApiClient { void get(endpoint, callback); };
class RecommendedAppsCache {
    ApiClient* apiClient;  // 依赖注入
    void refresh() { apiClient->get("/api/recommended-apps", ...); }
};
```

---

## 项目说明

**PonyWork** 是一个基于 Qt 5.15.2 的桌面应用程序，提供远程桌面、应用管理、云同步、应用推荐等功能。

### 主要模块

| 模块 | 功能 |
|------|------|
| `modules/core` | 核心功能：数据库、AI配置、日志、应用管理、网络监控、FRPC、推荐应用缓存 |
| `modules/user` | 用户系统：登录、菜单、密码修改 |
| `modules/widgets` | UI组件：应用管理、应用推荐、备忘录、工作日志、远程桌面、设置 |
| `modules/dialogs` | 对话框：截图、批量导入、图标选择、AI生成 |
| `modules/update` | 自动更新系统 |
| `server/` | 后端服务（Node.js/Express） |
| `server/admin-panel/` | 管理后台前端（静态HTML/CSS/JS） |

### 项目结构

```
PonyWork/
├── PonyWork.pro          # Qt 项目文件
├── mainwindow.cpp        # 主窗口
├── modules/             # Qt 功能模块
│   ├── core/           # 核心模块
│   ├── user/           # 用户模块
│   ├── widgets/        # UI组件
│   ├── dialogs/        # 对话框
│   ├── update/         # 更新模块
│   └── ui/             # Qt UI 文件
├── server/              # 后端服务
│   ├── admin-server.js  # Express 服务器入口
│   ├── admin-panel/     # 管理后台前端
│   ├── user-db.js       # 用户数据库管理
│   └── data/           # 数据库存储目录
├── resources.qrc         # Qt 资源文件
└── build.bat            # 编译脚本
```

---

## 项目信息

- **Qt 框架**: Qt 5.15.2
- **编译器**: MinGW 8.1.0 (64-bit)
- **后端**: Node.js / Express / SQLite
- **前端**: Bootstrap 5 / Vanilla JS

---

## 代码规范

### Qt 代码规范

1. **头文件保护**: `#ifndef MODULE_NAME_H` / `#define` / `#endif`
2. **命名规范**: 类名 `CamelCase`，函数/变量 `camelCase`
3. **文档**: 公共接口需添加注释说明
4. **内存管理**: Qt 对象使用父对象自动管理，原生指针注意生命周期

### 后端代码规范

1. **异步模式**: SQLite 操作使用 `db.all()` / `db.get()` / `db.run()` 回调模式
2. **API 响应格式**: `{ code: 0, data: ... }` 成功，`{ code: 非0, message: "错误信息" }` 失败
3. **字段命名**: 数据库用下划线（`icon_url`），API 响应根据路由决定

---

## 模块开发

### 新增 Qt 模块

1. 在对应目录创建文件：
   - `modules/xxx/xxx.h` - 头文件
   - `modules/xxx/xxx.cpp` - 实现文件
2. 如需 UI，创建 `modules/ui/xxx.ui`
3. 更新 `PonyWork.pro` 的 SOURCES/HEADERS/FORMS

### 新增后端 API

1. 在 `server/admin-server.js` 中添加路由
2. 使用 `authenticateAdmin` 中间件保护管理接口
3. 数据库表在 `db.serialize()` 中创建

---

## 应用推荐功能

### 数据库表

**recommended_apps** - 推荐应用表
```sql
CREATE TABLE recommended_apps (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,           -- 应用名称
    category TEXT NOT NULL,        -- 分类
    description TEXT,             -- 功能介绍
    icon_url TEXT,                -- 图标路径
    sort_order INTEGER DEFAULT 0,  -- 排序
    is_enabled INTEGER DEFAULT 1,  -- 是否启用
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

**recommended_app_downloads** - 下载地址表（一对多）
```sql
CREATE TABLE recommended_app_downloads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_id INTEGER NOT NULL,      -- 关联应用 ID
    name TEXT NOT NULL,           -- 平台名称（如 Windows、macOS）
    url TEXT NOT NULL,            -- 下载地址
    sort_order INTEGER DEFAULT 0,
    FOREIGN KEY (app_id) REFERENCES recommended_apps(id) ON DELETE CASCADE
);
```

### 后端 API

| 端点 | 方法 | 说明 | 认证 |
|------|------|------|------|
| `/api/recommended-apps` | GET | 客户端获取推荐列表 | 无 |
| `/api/admin/recommended-apps` | GET | 管理端获取列表 | admin |
| `/api/admin/recommended-apps` | POST | 新增应用 | admin |
| `/api/admin/recommended-apps/:id` | PUT | 修改应用 | admin |
| `/api/admin/recommended-apps/:id` | DELETE | 删除应用 | admin |
| `/api/admin/recommended-apps/:id/downloads` | POST | 新增下载地址 | admin |
| `/api/admin/recommended-apps/:id/downloads/:did` | DELETE | 删除下载地址 | admin |

### 客户端结构

| 文件 | 功能 |
|------|------|
| `modules/core/recommendedappscache.h/cpp` | 推荐应用缓存管理（单例） |
| `modules/widgets/recommendappwidget.h/cpp` | 应用推荐 Tab 组件 |

### 管理后台

| 文件 | 功能 |
|------|------|
| `server/admin-panel/index.html` | 管理后台入口 |
| `server/admin-panel/recommended-apps.js` | 应用推荐管理模块 |

---

## 后端服务

### 启动服务

```bash
cd server
npm install  # 首次运行需要安装依赖
node admin-server.js
```

默认端口：8080
管理后台：`http://localhost:8080/admin`
默认账号：`admin` / `admin123`

### API 测试

```bash
# 登录获取 Token
curl -X POST http://localhost:8080/api/admin/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}'

# 使用 Token 调用管理 API
curl http://localhost:8080/api/admin/recommended-apps \
  -H "Authorization: Bearer YOUR_TOKEN"
```

### 注意事项

- 8080 端口可能被其他服务占用，启动失败请检查端口
- 数据库文件位于 `server/data/admin.db`
- 用户数据库位于 `server/data/user_dbs/`

---

## 提交规范

**重要：提交前确保编译通过，不要自动提交 GitHub**

### 提交信息格式

```
type(scope): description
```

type:
- `feat`: 新功能
- `fix`: 修复 bug
- `refactor`: 重构
- `chore`: 构建/工具
- `docs`: 文档

---

## Qt 项目编译

### 编译命令

```bash
# 1. 设置环境
set PATH=D:\Qt\Tools\mingw810_64\bin;D:\Qt\5.15.2\mingw81_64\bin;%PATH%

# 2. 生成 Makefile
D:\Qt\5.15.2\mingw81_64\bin\qmake.exe PonyWork.pro

# 3. 编译
mingw32-make release -j4
```

### 快速编译脚本

- `build.bat` - Release 版本
- `build_test.bat` - 测试版本

---

## 常见问题

### Qt 编译问题

- **qmake 找不到**: 检查 Qt PATH 环境变量
- ** moc 错误**: 确保头文件包含 Q_OBJECT 宏
- **ui 文件错误**: 使用 Qt Designer 打开 .ui 文件检查

### 后端问题

- **端口占用**: `netstat -ano | grep 8080` 查看端口占用
- **模块找不到**: `npm install` 安装依赖
- **API 404**: 检查路由是否正确注册

### 调试技巧

1. **Qt**: 使用 `qDebug() << "message"` 输出调试信息
2. **Node.js**: 服务器日志输出到控制台
3. **前端**: 浏览器 F12 开发者工具 Network/Console 面板
