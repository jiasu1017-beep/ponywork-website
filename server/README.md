# PonyWork Admin Server 后端管理系统

## 📋 目录

- [项目简介](#-项目简介)
- [技术栈](#-技术栈)
- [核心文件](#-核心文件)
- [系统架构](#-系统架构)
- [数据结构](#-数据结构)
- [API 接口文档](#-api-接口文档)
- [部署指南](#-部署指南)
- [使用说明](#-使用说明)
- [运维管理](#-运维管理)
- [故障排查](#-故障排查)
- [安全建议](#-安全建议)

---

## 📝 项目简介

PonyWork Admin Server 是一个基于 Node.js + Express + SQLite 的后端管理系统，提供用户管理、日志监控、应用集合管理等功能。系统包含完整的 RESTful API 和现代化的管理后台界面。

**主要功能**：
- ✅ 用户认证与授权（JWT Token）
- ✅ 用户管理（CRUD、角色管理、状态控制）
- ✅ 日志管理（登录日志、操作日志）
- ✅ 应用集合管理
- ✅ 系统配置管理
- ✅ 实时统计仪表盘
- ✅ 客户端集成接口

---

## 🛠️ 技术栈

### 后端技术

| 技术 | 版本 | 用途 |
|------|------|------|
| **Node.js** | 18.x | 运行环境 |
| **Express.js** | 4.x | Web 框架 |
| **SQLite3** | 5.x | 数据库 |
| **JWT** | - | Token 认证 |
| **bcryptjs** | 2.x | 密码加密 |
| **CORS** | - | 跨域支持 |

### 前端技术

| 技术 | 用途 |
|------|------|
| **HTML5/CSS3** | 页面结构与样式 |
| **Bootstrap 5** | UI 框架 |
| **Vanilla JavaScript** | 交互逻辑 |
| **Bootstrap Icons** | 图标库 |
| **Fetch API** | HTTP 请求 |

### 开发工具

- **PM2** - 进程管理
- **Node.js npm** - 包管理
- **SQLite3** - 数据库管理

---

## 📦 核心文件

```
server/
├── admin-server.js          # 后端服务主程序（800+ 行）
├── admin-panel/             # 管理后台前端
│   └── index.html          # 单页面应用（1200+ 行）
├── package.json            # Node.js 依赖配置
├── README.md               # 本文档
└── data/                   # 数据库目录（运行时创建）
    └── admin.db            # SQLite 数据库
```

**文件大小**：
- `admin-server.js`: ~850 行
- `admin-panel/index.html`: ~1200 行
- `package.json`: ~20 行

---

## 🏗️ 系统架构

### 架构图

```
┌─────────────┐         ┌──────────────────┐         ┌──────────────┐
│   客户端    │ ──────> │  Express Server  │ ─────> │   SQLite3    │
│  (Qt/C++)   │  HTTP   │   (Node.js)      │  SQL   │   Database   │
└─────────────┘         └──────────────────┘         └──────────────┘
                              │
                              ▼
                       ┌──────────────┐
                       │ admin-panel  │
                       │  (Frontend)  │
                       └──────────────┘
```

### 认证流程

```
1. 用户登录
   ↓
2. 验证用户名密码
   ↓
3. 生成 JWT Token
   ↓
4. 保存到 user_sessions
   ↓
5. 返回 Token 给客户端
   ↓
6. 客户端后续请求携带 Token
   ↓
7. 中间件验证 Token 有效性
```

---

## 📊 数据结构

### 数据库位置

```
/var/www/ponywork-admin/data/admin.db
```

### 数据表结构

#### 1. users - 用户信息表

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,      -- 用户名
    email TEXT UNIQUE NOT NULL,         -- 邮箱
    password TEXT NOT NULL,             -- 密码（bcrypt 加密）
    role TEXT DEFAULT 'user',           -- 角色：user/admin
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 注册时间
    last_login DATETIME,                -- 最后登录时间
    status TEXT DEFAULT 'active'        -- 状态：active/inactive
);
```

#### 2. user_sessions - 用户会话表

```sql
CREATE TABLE user_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,                    -- 用户 ID
    token TEXT UNIQUE,                  -- JWT Token
    ip_address TEXT,                    -- 登录 IP
    user_agent TEXT,                    -- 浏览器标识
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 创建时间
    expires_at DATETIME,                -- 过期时间
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

#### 3. login_logs - 登录日志表

```sql
CREATE TABLE login_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,                    -- 用户 ID
    username TEXT,                      -- 用户名
    action TEXT,                        -- 操作：login/logout
    status TEXT,                        -- 状态：success/failed
    ip_address TEXT,                    -- IP 地址
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP   -- 时间
);
```

#### 4. operation_logs - 操作日志表

```sql
CREATE TABLE operation_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,                    -- 用户 ID
    action TEXT,                        -- 操作类型
    details TEXT,                       -- 详细信息
    ip_address TEXT,                    -- IP 地址
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP   -- 时间
);
```

#### 5. app_collections - 应用集合表

```sql
CREATE TABLE app_collections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,                 -- 名称
    description TEXT,                   -- 描述
    status TEXT DEFAULT 'active',       -- 状态：active/inactive
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 创建时间
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP   -- 更新时间
);
```

#### 6. system_config - 系统配置表

```sql
CREATE TABLE system_config (
    key TEXT PRIMARY KEY,               -- 配置键
    value TEXT,                         -- 配置值
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP   -- 更新时间
);
```

#### 7. admin_users - 管理员表

```sql
CREATE TABLE admin_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,      -- 管理员用户名
    password TEXT NOT NULL,             -- 密码（bcrypt 加密）
    role TEXT DEFAULT 'admin',          -- 角色
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP   -- 创建时间
);
```

#### 8. user_configs - 用户配置表（新增）

```sql
CREATE TABLE user_configs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,           -- 用户 ID
    key TEXT NOT NULL,                  -- 配置键
    value TEXT,                         -- 配置值（JSON 格式）
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 更新时间
    UNIQUE(user_id, key),
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

---

## 🌐 API 接口文档

### 基础信息

- **基础 URL**: `http://8.163.37.74:8080`
- **认证方式**: Bearer Token
- **数据格式**: JSON

### 认证接口

#### 1. 管理员登录
```http
POST /api/admin/login
Content-Type: application/json

{
  "username": "admin",
  "password": "admin123"
}

Response:
{
  "success": true,
  "token": "abc123...",
  "admin": {
    "id": 1,
    "username": "admin"
  }
}
```

#### 2. 验证 Token
```http
POST /api/admin/validate-token
Authorization: Bearer <token>

Response:
{
  "success": true,
  "adminId": 1
}
```

### 用户管理接口

#### 3. 获取用户列表
```http
GET /api/admin/users?page=1&limit=20
Authorization: Bearer <token>

Response:
{
  "success": true,
  "users": [...],
  "pagination": {
    "page": 1,
    "limit": 20,
    "total": 100,
    "pages": 5
  }
}
```

#### 4. 更新用户
```http
PUT /api/admin/users/:id
Authorization: Bearer <token>
Content-Type: application/json

{
  "email": "user@example.com",
  "role": "admin",
  "status": "active"
}
```

#### 5. 删除用户
```http
DELETE /api/admin/users/:id
Authorization: Bearer <token>
```

#### 6. 重置用户密码
```http
POST /api/admin/users/:id/reset-password
Authorization: Bearer <token>
Content-Type: application/json

{
  "newPassword": "666666"
}
```

### 日志管理接口

#### 7. 获取日志列表
```http
GET /api/admin/logs?page=1&limit=50&type=login|operation&startDate=2026-03-01&endDate=2026-03-15
Authorization: Bearer <token>

Response:
{
  "success": true,
  "logs": [...],
  "pagination": {...}
}
```

### 应用集合接口

#### 8. 获取应用集合列表
```http
GET /api/admin/collections?page=1&limit=20&search=keyword
Authorization: Bearer <token>
```

#### 9. 创建应用集合
```http
POST /api/admin/collections
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "应用名称",
  "description": "应用描述"
}
```

#### 10. 更新应用集合
```http
PUT /api/admin/collections/:id
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "新名称",
  "status": "active"
}
```

#### 11. 删除应用集合
```http
DELETE /api/admin/collections/:id
Authorization: Bearer <token>
```

### 系统配置接口

#### 12. 获取系统配置
```http
GET /api/admin/config
Authorization: Bearer <token>

Response:
{
  "success": true,
  "configs": [...]
}
```

#### 13. 更新配置
```http
PUT /api/admin/config/:key
Authorization: Bearer <token>
Content-Type: application/json

{
  "value": "新配置值"
}
```

### 统计接口

#### 14. 获取系统统计
```http
GET /api/admin/stats
Authorization: Bearer <token>

Response:
{
  "success": true,
  "stats": {
    "totalUsers": 100,
    "activeUsers": 80,
    "todayLogins": 25,
    "totalLogins": 500
  }
}
```

### 客户端集成接口

#### 15. 用户注册
```http
POST /api/auth/register
Content-Type: application/json

{
  "username": "newuser",
  "email": "user@example.com",
  "password": "sha256_hash"
}
```

#### 16. 用户登录
```http
POST /api/auth/login
Content-Type: application/json

{
  "identifier": "username 或 email",
  "password": "sha256_hash"
}
```

#### 17. 检查邮箱是否存在
```http
GET /api/auth/check-email?email=user@example.com

Response:
{
  "exists": false
}
```

#### 18. 获取用户配置
```http
GET /api/config/get
Authorization: Bearer <token>

Response:
{
  "success": true,
  "configs": {
    "theme": "dark",
    "language": "zh-CN"
  }
}
```

#### 19. 保存用户配置
```http
POST /api/config/save
Authorization: Bearer <token>
Content-Type: application/json

{
  "configs": {
    "theme": "light",
    "language": "en-US"
  }
}
```

---

## 🚀 部署指南

### 📖 极简部署教程（5 步搞定）

#### 1️⃣ 连接服务器
```bash
ssh root@8.163.37.74
# 密码：UxZp28Kv9m3Lq1Wn4Rt6
```

#### 2️⃣ 进入目录
```bash
cd /var/www/ponywork-admin
```

#### 3️⃣ 启动服务
```bash
pm2 start admin-server.js --name ponywork-backend
```

#### 4️⃣ 重启服务（更新代码后）
```bash
pm2 restart ponywork-backend
```

#### 5️⃣ 查看日志
```bash
pm2 logs ponywork-backend --lines 0
```

**完成！** 🎉 访问 `http://8.163.37.74:8080/admin/` 管理后台

---

### 环境要求

- **操作系统**: Linux (Ubuntu/CentOS) 或 Windows
- **Node.js**: 18.x 或更高版本
- **npm**: 9.x 或更高版本
- **内存**: 至少 512MB
- **磁盘**: 至少 1GB 可用空间

### 首次部署

#### 步骤 1: SSH 连接服务器

```bash
ssh root@8.163.37.74
# 密码：UxZp28Kv9m3Lq1Wn4Rt6
```

#### 步骤 2: 创建目录

```bash
mkdir -p /var/www/ponywork-admin
cd /var/www/ponywork-admin
mkdir -p admin-panel data
```

#### 步骤 3: 上传文件

**方法 A: 使用 SCP**
```bash
# 在本地 PowerShell 执行
scp F:\00AI\PonyWork\server\admin-server.js root@8.163.37.74:/var/www/ponywork-admin/
scp F:\00AI\PonyWork\server\package.json root@8.163.37.74:/var/www/ponywork-admin/
scp -r F:\00AI\PonyWork\server\admin-panel root@8.163.37.74:/var/www/ponywork-admin/
```

**方法 B: 使用 WinSCP**
1. 打开 WinSCP
2. 连接服务器（主机：8.163.37.74，用户：root，密码：UxZp28Kv9m3Lq1Wn4Rt6）
3. 拖拽文件到 `/var/www/ponywork-admin/`

#### 步骤 4: 安装依赖

```bash
cd /var/www/ponywork-admin
npm install
```

**依赖包**：
```json
{
  "express": "^4.18.2",
  "sqlite3": "^5.1.6",
  "bcryptjs": "^2.4.3",
  "cors": "^2.8.5",
  "jsonwebtoken": "^9.0.0"
}
```

#### 步骤 5: 启动服务

**方法 A: 使用 PM2（推荐）**
```bash
# 安装 PM2
npm install -g pm2

# 启动服务
pm2 start admin-server.js --name ponywork-backend

# 设置开机自启
pm2 startup
pm2 save
```

**方法 B: 使用 nohup**
```bash
nohup node admin-server.js > /var/log/ponywork-admin.log 2>&1 &
```

#### 步骤 6: 验证服务

```bash
# 检查进程
pm2 status
# 或
ps aux | grep admin-server.js

# 检查端口
netstat -tlnp | grep 8080

# 查看日志
pm2 logs ponywork-backend
# 或
tail -f /var/log/ponywork-admin.log

# 测试 API
curl http://localhost:8080/api/admin/stats
```

### 重新部署（更新代码）

#### 方法 1: 使用 PM2（推荐）

```bash
# 1. 上传新文件（使用 WinSCP 或 SCP）
scp F:\00AI\PonyWork\server\admin-server.js root@8.163.37.74:/var/www/ponywork-admin/

# 2. SSH 连接服务器
ssh root@8.163.37.74

# 3. 重启服务
cd /var/www/ponywork-admin
pm2 restart ponywork-backend

# 4. 查看日志确认
pm2 logs ponywork-backend --lines 30
```

#### 方法 2: 手动重启

```bash
# 1. 停止服务
pm2 stop ponywork-backend
# 或
pkill -f admin-server.js

# 2. 上传新文件

# 3. 启动服务
cd /var/www/ponywork-admin
pm2 start admin-server.js --name ponywork-backend
# 或
nohup node admin-server.js > /var/log/ponywork-admin.log 2>&1 &
```

#### 方法 3: 使用部署脚本

创建 `deploy.sh`：
```bash
#!/bin/bash
echo "开始部署..."
cd /var/www/ponywork-admin
pm2 restart ponywork-backend
pm2 logs ponywork-backend --lines 20
echo "部署完成！"
```

执行：
```bash
chmod +x deploy.sh
./deploy.sh
```

---

## 📖 使用说明

### 访问管理后台

1. **打开浏览器**
2. **访问**: `http://8.163.37.74:8080/admin/`
3. **登录**:
   - 用户名：`admin`
   - 密码：`admin123`

### 功能模块

#### 1. 仪表盘

- 查看系统统计信息
- 用户总数、活跃用户数
- 今日登录次数、总登录次数
- 在线用户数

#### 2. 用户管理

- 查看用户列表（分页）
- 搜索用户（用户名/邮箱）
- 编辑用户信息（角色、状态）
- 重置用户密码
- 删除用户

**操作流程**：
```
用户管理 → 搜索用户 → 点击编辑 → 修改信息 → 保存
```

#### 3. 日志管理

- 查看登录日志
- 查看操作日志
- 按日期筛选
- 分页浏览

**操作流程**：
```
日志管理 → 选择日志类型 → 选择日期范围 → 查询
```

#### 4. 应用集合

- 创建应用集合
- 编辑应用信息
- 启用/禁用应用
- 删除应用

**操作流程**：
```
应用集合 → 新建 → 填写信息 → 保存
```

#### 5. 系统配置

- 查看系统配置
- 修改配置项
- 保存配置

---

## 🔧 运维管理

### 常用命令

#### 服务管理

```bash
# 启动服务
pm2 start ponywork-backend

# 停止服务
pm2 stop ponywork-backend

# 重启服务
pm2 restart ponywork-backend

# 查看状态
pm2 status

# 查看日志
pm2 logs ponywork-backend

# 实时监控
pm2 monit

# 删除服务
pm2 delete ponywork-backend
```

#### 日志管理

```bash
# 查看实时日志
pm2 logs ponywork-backend --lines 0

# 查看最近 100 行
pm2 logs ponywork-backend --lines 100

# 仅查看错误
pm2 logs ponywork-backend --err

# 清空日志
pm2 flush
```

#### 数据库管理

```bash
# 进入 SQLite 命令行
sqlite3 /var/www/ponywork-admin/data/admin.db

# 查看表
.tables

# 查看用户
SELECT * FROM users LIMIT 10;

# 查看登录日志
SELECT * FROM login_logs ORDER BY created_at DESC LIMIT 20;

# 备份数据库
cp /var/www/ponywork-admin/data/admin.db /backup/admin.db.$(date +%Y%m%d)

# 退出
.exit
```

#### 系统监控

```bash
# 查看进程
ps aux | grep node

# 查看端口
netstat -tlnp | grep 8080

# 查看内存使用
free -h

# 查看磁盘使用
df -h

# 查看 CPU 使用
top
```

### 性能优化

#### 1. 数据库优化

```sql
-- 添加索引
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_login_logs_created ON login_logs(created_at);
CREATE INDEX idx_operation_logs_created ON operation_logs(created_at);

-- 定期清理旧日志
DELETE FROM login_logs WHERE created_at < datetime('now', '-90 days');
DELETE FROM operation_logs WHERE created_at < datetime('now', '-90 days');

-- 清理过期会话
DELETE FROM user_sessions WHERE expires_at < datetime('now');
```

#### 2. 日志轮转

创建 `/etc/logrotate.d/ponywork-admin`：
```
/var/log/ponywork-admin.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 0640 root root
}
```

#### 3. PM2 配置

创建 `ecosystem.config.js`：
```javascript
module.exports = {
  apps: [{
    name: 'ponywork-backend',
    script: './admin-server.js',
    instances: 1,
    exec_mode: 'fork',
    env: {
      NODE_ENV: 'production',
      PORT: 8080
    },
    error_file: './logs/error.log',
    out_file: './logs/out.log',
    log_date_format: 'YYYY-MM-DD HH:mm:ss',
    max_memory_restart: '500M',
    watch: false
  }]
};
```

启动：
```bash
pm2 start ecosystem.config.js
```

---

## 🆘 故障排查

### 常见问题

#### 1. 无法访问 8080 端口

**症状**: 浏览器无法打开 `http://8.163.37.74:8080`

**排查步骤**:
```bash
# 检查服务是否运行
pm2 status

# 检查端口监听
netstat -tlnp | grep 8080

# 检查防火墙
ufw status
ufw allow 8080/tcp

# 检查阿里云安全组
# 登录阿里云控制台 → 安全组 → 添加规则：端口 8080
```

#### 2. 服务启动失败

**症状**: `pm2 start` 失败或立即退出

**排查步骤**:
```bash
# 查看详细错误
pm2 logs ponywork-backend --err

# 检查 Node.js 版本
node -v

# 检查依赖
cd /var/www/ponywork-admin
npm install

# 检查语法错误
node --check admin-server.js

# 手动启动查看错误
node admin-server.js
```

**常见错误**:
- `Error: Cannot find module 'express'` → 运行 `npm install`
- `Error: listen EADDRINUSE: address already in use :::8080` → 端口被占用，修改端口或停止占用进程
- `SyntaxError: Unexpected token` → 代码语法错误，检查最近修改

#### 3. 数据库错误

**症状**: API 返回数据库相关错误

**排查步骤**:
```bash
# 检查数据库文件
ls -lh /var/www/ponywork-admin/data/admin.db

# 检查权限
chmod 644 /var/www/ponywork-admin/data/admin.db

# 测试数据库
sqlite3 /var/www/ponywork-admin/data/admin.db ".tables"

# 修复数据库
sqlite3 /var/www/ponywork-admin/data/admin.db "VACUUM;"
```

#### 4. 登录失败

**症状**: 管理员无法登录

**排查步骤**:
```bash
# 检查管理员账号
sqlite3 /var/www/ponywork-admin/data/admin.db "SELECT * FROM admin_users;"

# 重置管理员密码
cd /var/www/ponywork-admin
node
> const bcrypt = require('bcryptjs');
> bcrypt.hash('admin123', 10).then(hash => console.log(hash));
# 复制哈希值
> .exit

sqlite3 /var/www/ponywork-admin/data/admin.db
UPDATE admin_users SET password='复制的哈希值' WHERE username='admin';
```

#### 5. 前端页面空白

**症状**: 访问 `/admin/` 页面空白

**排查步骤**:
```bash
# 检查文件是否存在
ls -lh /var/www/ponywork-admin/admin-panel/index.html

# 检查文件内容
head -20 /var/www/ponywork-admin/admin-panel/index.html

# 检查浏览器控制台错误
# F12 → Console 查看错误信息

# 清除浏览器缓存
# Ctrl+F5 强制刷新
```

### 调试模式

#### 开启调试日志

修改 `admin-server.js`，在文件开头添加：
```javascript
process.env.DEBUG = 'true';
```

重启服务：
```bash
pm2 restart ponywork-backend
```

查看详细日志：
```bash
pm2 logs ponywork-backend --lines 200
```

### 性能问题

#### CPU 使用率高

```bash
# 查看进程资源
top -p $(pgrep -f admin-server.js)

# 检查慢查询
sqlite3 /var/www/ponywork-admin/data/admin.db "PRAGMA query_only = ON;"

# 优化数据库
sqlite3 /var/www/ponywork-admin/data/admin.db "VACUUM; ANALYZE;"
```

#### 内存泄漏

```bash
# 监控内存使用
pm2 monit

# 重启服务释放内存
pm2 restart ponywork-backend

# 设置内存限制
pm2 restart ponywork-backend --max-memory 500M
```

---

## 🔐 安全建议

### 1. 修改默认密码

**立即修改 admin 密码**：
```bash
sqlite3 /var/www/ponywork-admin/data/admin.db

# 生成新密码哈希（在 Node.js 中）
> const bcrypt = require('bcryptjs');
> bcrypt.hash('你的新密码', 10).then(hash => console.log(hash));

# 更新数据库
UPDATE admin_users SET password='哈希值' WHERE username='admin';
```

### 2. 配置防火墙

**Ubuntu/Debian**:
```bash
ufw allow 8080/tcp
ufw enable
ufw status
```

**CentOS/RHEL**:
```bash
firewall-cmd --zone=public --add-port=8080/tcp --permanent
firewall-cmd --reload
firewall-cmd --list-ports
```

### 3. 配置 HTTPS（推荐）

**使用 Nginx 反向代理**：

安装 Nginx：
```bash
apt-get install nginx  # Ubuntu/Debian
# 或
yum install nginx      # CentOS/RHEL
```

配置 `/etc/nginx/sites-available/ponywork-admin`：
```nginx
server {
    listen 80;
    server_name 8.163.37.74;
    
    location / {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }
}
```

启用配置：
```bash
ln -s /etc/nginx/sites-available/ponywork-admin /etc/nginx/sites-enabled/
nginx -t
systemctl restart nginx
```

**申请 SSL 证书**（使用 Let's Encrypt）：
```bash
apt-get install certbot python3-certbot-nginx
certbot --nginx -d your-domain.com
```

### 4. 定期备份

**创建备份脚本** `backup.sh`：
```bash
#!/bin/bash
BACKUP_DIR="/backup/ponywork"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# 备份数据库
cp /var/www/ponywork-admin/data/admin.db $BACKUP_DIR/admin.db.$DATE

# 备份代码
cp -r /var/www/ponywork-admin $BACKUP_DIR/code.$DATE

# 压缩
cd $BACKUP_DIR
tar -czf ponywork-backup.$DATE.tar.gz admin.db.$DATE code.$DATE

# 清理旧备份（保留 30 天）
find $BACKUP_DIR -name "*.tar.gz" -mtime +30 -delete

echo "Backup completed: $BACKUP_DIR/ponywork-backup.$DATE.tar.gz"
```

设置定时任务：
```bash
crontab -e
# 每天凌晨 2 点备份
0 2 * * * /var/www/ponywork-admin/backup.sh
```

### 5. 监控告警

**安装监控工具**：
```bash
# 安装 PM2 Plus（免费）
pm2 plus

# 或使用其他监控工具
# - Prometheus + Grafana
# - New Relic
# - DataDog
```

### 6. 输入验证

所有 API 接口已实现输入验证，但建议：
- 定期更新依赖包：`npm update`
- 审查代码中的用户输入处理
- 使用参数化查询防止 SQL 注入

---

## 📞 技术支持

### 获取帮助

**遇到问题时，请提供以下信息**：

1. **错误日志**
   ```bash
   pm2 logs ponywork-backend --lines 100
   ```

2. **服务状态**
   ```bash
   pm2 status
   ```

3. **系统信息**
   ```bash
   uname -a
   node -v
   npm -v
   ```

4. **网络状态**
   ```bash
   netstat -tlnp | grep 8080
   ```

5. **数据库状态**
   ```bash
   sqlite3 /var/www/ponywork-admin/data/admin.db ".tables"
   ```

### 联系方式

- **项目地址**: `F:\00AI\PonyWork`
- **服务器**: `8.163.37.74`
- **文档**: 本文档

---

## � 快速参考卡片

### 常用命令速查

```bash
# 🔗 连接服务器
ssh root@8.163.37.74

# 📂 进入目录
cd /var/www/ponywork-admin

# ▶️ 启动服务
pm2 start admin-server.js --name ponywork-backend

# 🔄 重启服务
pm2 restart ponywork-backend

# ⏹️ 停止服务
pm2 stop ponywork-backend

# 📊 查看状态
pm2 status

# 📝 查看日志
pm2 logs ponywork-backend --lines 0

# 🔍 实时监控
pm2 monit

# 🗑️ 删除服务
pm2 delete ponywork-backend

# 💾 备份数据库
cp /var/www/ponywork-admin/data/admin.db /backup/admin.db.$(date +%Y%m%d)

# 🌐 访问地址
# 管理后台：http://8.163.37.74:8080/admin/
# API 地址：http://8.163.37.74:8080
```

### 默认账号

```
管理员登录：
- 用户名：admin
- 密码：admin123

⚠️ 首次部署后请立即修改密码！
```

### 端口说明

| 端口 | 用途 | 访问方式 |
|------|------|----------|
| 8080 | HTTP 服务 | http://8.163.37.74:8080 |
| 22   | SSH     | ssh root@8.163.37.74 |

### 重要路径

| 路径 | 用途 |
|------|------|
| `/var/www/ponywork-admin/` | 项目根目录 |
| `/var/www/ponywork-admin/admin-server.js` | 主程序 |
| `/var/www/ponywork-admin/admin-panel/` | 前端页面 |
| `/var/www/ponywork-admin/data/admin.db` | 数据库 |

---

## �📄 许可证

本项目为内部使用，未公开许可证。

---

## 📝 更新日志

### v1.0.0 (2026-03-15)
- ✅ 初始版本发布
- ✅ 用户管理功能
- ✅ 日志管理功能
- ✅ 应用集合管理
- ✅ 系统配置管理
- ✅ 客户端集成接口
- ✅ 用户配置接口（新增）
- ✅ 时间显示修复（新增）
- ✅ 邮箱注册检查（新增）

---

**最后更新**: 2026-03-15  
**维护人员**: 开发团队
