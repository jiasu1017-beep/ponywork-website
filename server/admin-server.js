const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');
const userDb = require('./user-db');

const app = express();
const PORT = process.env.PORT || 8080;

app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

const DB_PATH = process.env.DB_PATH || path.join(__dirname, 'data', 'admin.db');
const dataDir = path.dirname(DB_PATH);
if (!fs.existsSync(dataDir)) {
    fs.mkdirSync(dataDir, { recursive: true });
}

const db = new sqlite3.Database(DB_PATH);

db.serialize(() => {
    db.run(`CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        email TEXT UNIQUE NOT NULL,
        password TEXT NOT NULL,
        avatar TEXT,
        role TEXT DEFAULT 'user',
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        last_login DATETIME,
        status TEXT DEFAULT 'active'
    )`);
    
    // 迁移：为现有表添加avatar字段（如果不存在）
    db.all("PRAGMA table_info(users)", [], (err, columns) => {
        if (!err && columns) {
            const hasAvatar = columns.some(col => col.name === 'avatar');
            if (!hasAvatar) {
                db.run("ALTER TABLE users ADD COLUMN avatar TEXT", (err) => {
                    if (!err) {
                        console.log('[数据库迁移] 已添加 avatar 字段到 users 表');
                    }
                });
            }
        }
    });

    db.run(`CREATE TABLE IF NOT EXISTS user_sessions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER,
        token TEXT UNIQUE,
        ip_address TEXT,
        user_agent TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        expires_at DATETIME,
        FOREIGN KEY (user_id) REFERENCES users(id)
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS login_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER,
        username TEXT,
        ip_address TEXT,
        user_agent TEXT,
        action TEXT,
        status TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS operation_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER,
        username TEXT,
        action TEXT,
        details TEXT,
        ip_address TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS app_collections (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        description TEXT,
        apps TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        created_by INTEGER,
        status TEXT DEFAULT 'active'
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS system_config (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        key TEXT UNIQUE NOT NULL,
        value TEXT,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);
    
    // 用户配置表（当前生效的配置）
    db.run(`CREATE TABLE IF NOT EXISTS user_configs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        key TEXT NOT NULL,
        value TEXT,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        UNIQUE(user_id, key),
        FOREIGN KEY (user_id) REFERENCES users(id)
    )`);

    // 用户配置表（支持多个配置，如不同设备的配置）
    db.run(`CREATE TABLE IF NOT EXISTS user_config_profiles (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        config_name TEXT NOT NULL,
        configs TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users(id),
        UNIQUE(user_id, config_name)
    )`);

    // 创建索引
    db.run(`CREATE INDEX IF NOT EXISTS idx_user_config_profiles_user ON user_config_profiles(user_id)`);

    db.run(`CREATE TABLE IF NOT EXISTS admin_users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        password TEXT NOT NULL,
        role TEXT DEFAULT 'admin',
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        last_login DATETIME
    )`);
    
    // 密码重置 token 表
    db.run(`CREATE TABLE IF NOT EXISTS password_reset_tokens (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        token TEXT UNIQUE NOT NULL,
        expires_at DATETIME NOT NULL,
        used INTEGER DEFAULT 0,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users(id)
    )`);

    const stmt = db.prepare("SELECT * FROM admin_users WHERE username = 'admin'");
    stmt.get((err, row) => {
        if (!row) {
            const bcrypt = require('bcryptjs');
            const hashedPassword = bcrypt.hashSync('admin123', 10);
            db.run("INSERT INTO admin_users (username, password, role) VALUES (?, ?, ?)", 
                ['admin', hashedPassword, 'super_admin']);
            console.log('Default admin user created: admin / admin123');
        }
    });
    stmt.finalize();
});

app.use('/admin', express.static(path.join(__dirname, 'admin-panel')));

app.post('/api/admin/login', (req, res) => {
    const { username, password } = req.body;
    
    if (!username || !password) {
        return res.status(400).json({ success: false, message: '用户名和密码不能为空' });
    }

    const bcrypt = require('bcryptjs');
    db.get("SELECT * FROM admin_users WHERE username = ?", [username], (err, admin) => {
        if (err || !admin) {
            return res.status(401).json({ success: false, message: '用户名或密码错误' });
        }

        if (!bcrypt.compareSync(password, admin.password)) {
            return res.status(401).json({ success: false, message: '用户名或密码错误' });
        }

        const token = require('crypto').randomBytes(32).toString('hex');
        const expiresAt = new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString();
        
        db.run("INSERT INTO user_sessions (user_id, token, ip_address, user_agent, expires_at) VALUES (?, ?, ?, ?, ?)",
            [admin.id, token, req.ip, req.headers['user-agent'], expiresAt]);
        
        db.run("UPDATE admin_users SET last_login = CURRENT_TIMESTAMP WHERE id = ?", [admin.id]);

        res.json({
            success: true,
            message: '登录成功',
            token: token,
            user: {
                id: admin.id,
                username: admin.username,
                role: admin.role
            }
        });
    });
});

const authenticateAdmin = (req, res, next) => {
    const token = req.headers['authorization']?.replace('Bearer ', '');
    
    if (!token) {
        return res.status(401).json({ success: false, message: '未授权访问' });
    }

    db.get("SELECT * FROM user_sessions WHERE token = ? AND expires_at > datetime('now')", [token], (err, session) => {
        if (err || !session) {
            return res.status(401).json({ success: false, message: '登录已过期，请重新登录' });
        }
        req.adminId = session.user_id;
        next();
    });
};

// 验证普通用户 token
const authenticateToken = (req, res, next) => {
    const token = req.headers['authorization']?.replace('Bearer ', '');
    
    if (!token) {
        return res.status(401).json({ success: false, message: '未授权访问' });
    }

    db.get("SELECT user_id FROM user_sessions WHERE token = ? AND expires_at > datetime('now')", [token], (err, session) => {
        if (err || !session) {
            return res.status(401).json({ success: false, message: '登录已过期，请重新登录' });
        }
        req.userId = session.user_id;
        next();
    });
};

app.get('/api/admin/stats', authenticateAdmin, (req, res) => {
    const stats = {};

    db.get("SELECT COUNT(*) as count FROM users", (err, row) => {
        stats.totalUsers = row?.count || 0;

        db.get("SELECT COUNT(*) as count FROM users WHERE status = 'active'", (err, row) => {
            stats.activeUsers = row?.count || 0;

            db.get("SELECT COUNT(*) as count FROM login_logs WHERE created_at > datetime('now', '-24 hours')", (err, row) => {
                stats.logins24h = row?.count || 0;

                // 修复：统计有效的在线用户（未过期的 session）
                db.get("SELECT COUNT(DISTINCT user_id) as count FROM user_sessions WHERE expires_at > datetime('now')", (err, row) => {
                    stats.onlineUsers = row?.count || 0;

                    db.get("SELECT COUNT(*) as count FROM app_collections", (err, row) => {
                        stats.totalCollections = row?.count || 0;

                        res.json({ success: true, stats });
                    });
                });
            });
        });
    });
});

// 获取所有用户的FRPC端口使用情况
app.get('/api/admin/frpc/ports', authenticateAdmin, (req, res) => {
    // 先获取所有用户
    db.all("SELECT id, username, email FROM users WHERE status = 'active'", [], (err, users) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取用户列表失败: ' + err.message });
        }

        if (!users || users.length === 0) {
            return res.json({ success: true, ports: [] });
        }

        // 遍历每个用户，查询其数据库中的FRPC配置
        let processed = 0;
        const frpcPorts = [];

        users.forEach(user => {
            try {
                const userDbConn = userDb.getUserDb(user.id);
                userDbConn.get("SELECT value, updated_at FROM user_configs WHERE key = 'frpc'", [], (err, row) => {
                    let frpcConfig = {};
                    let updatedAt = null;

                    if (row) {
                        try {
                            if (row.value) {
                                frpcConfig = JSON.parse(row.value);
                            }
                            updatedAt = row.updated_at;
                        } catch (e) {
                            console.error('Parse FRPC config error:', e);
                        }
                    }

                    frpcPorts.push({
                        user_id: user.id,
                        username: user.username,
                        email: user.email,
                        remote_port: frpcConfig.remotePort || null,
                        device_name: frpcConfig.deviceName || null,
                        server_addr: frpcConfig.serverAddr || '8.163.37.74',
                        is_enabled: frpcConfig.isEnabled || false,
                        last_updated: updatedAt
                    });

                    processed++;
                    if (processed === users.length) {
                        // 按最后更新时间排序
                        frpcPorts.sort((a, b) => {
                            if (!a.last_updated) return 1;
                            if (!b.last_updated) return -1;
                            return new Date(b.last_updated) - new Date(a.last_updated);
                        });
                        res.json({ success: true, ports: frpcPorts });
                    }
                });
            } catch (e) {
                console.error('Error getting user db for user', user.id, e);
                frpcPorts.push({
                    user_id: user.id,
                    username: user.username,
                    email: user.email,
                    remote_port: null,
                    device_name: null,
                    server_addr: '8.163.37.74',
                    is_enabled: false,
                    last_updated: null
                });
                processed++;
                if (processed === users.length) {
                    res.json({ success: true, ports: frpcPorts });
                }
            }
        });
    });
});

// 获取单个用户的 FRPC 端口信息（优化：避免获取所有用户）
app.get('/api/admin/users/:id/frpc', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);

    // 检查用户是否存在
    db.get("SELECT id, username, email FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        try {
            const userDbConn = userDb.getUserDb(userId);
            userDbConn.get("SELECT value, updated_at FROM user_configs WHERE key = 'frpc'", [], (err, row) => {
                let frpcConfig = {};
                let updatedAt = null;

                if (row) {
                    try {
                        if (row.value) {
                            frpcConfig = JSON.parse(row.value);
                        }
                        updatedAt = row.updated_at;
                    } catch (e) {
                        console.error('Parse FRPC config error:', e);
                    }
                }

                res.json({
                    success: true,
                    frpc: {
                        user_id: user.id,
                        username: user.username,
                        email: user.email,
                        remote_port: frpcConfig.remotePort || null,
                        device_name: frpcConfig.deviceName || null,
                        server_addr: frpcConfig.serverAddr || '8.163.37.74',
                        is_enabled: frpcConfig.isEnabled || false,
                        last_updated: updatedAt
                    }
                });
            });
        } catch (e) {
            console.error('Error getting user db for user', userId, e);
            res.json({
                success: true,
                frpc: {
                    user_id: user.id,
                    username: user.username,
                    email: user.email,
                    remote_port: null,
                    device_name: null,
                    server_addr: '8.163.37.74',
                    is_enabled: false,
                    last_updated: null
                }
            });
        }
    });
});

app.get('/api/admin/users/:id', authenticateAdmin, (req, res) => {
    const { id } = req.params;

    db.get("SELECT id, username, email, role, created_at, last_login, status FROM users WHERE id = ?", [id], (err, user) => {
        if (err) {
            return res.status(500).json({ success: false, message: '获取用户信息失败' });
        }

        if (!user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        // 获取用户的登录次数
        db.get("SELECT COUNT(*) as count FROM login_logs WHERE user_id = ?", [id], (err, row) => {
            user.loginCount = row?.count || 0;

            // 获取用户的操作记录数
            db.get("SELECT COUNT(*) as count FROM operation_logs WHERE user_id = ?", [id], (err, row) => {
                user.operationCount = row?.count || 0;

                // 获取当前是否在线（是否有未过期的 session）
                db.get("SELECT COUNT(*) as count FROM user_sessions WHERE user_id = ? AND expires_at > datetime('now')", [id], (err, row) => {
                    user.isOnline = row?.count > 0;

                    // 获取该用户的收藏应用数量
                    db.get("SELECT COUNT(*) as count FROM app_collections WHERE created_by = ?", [id], (err, row) => {
                        user.collectionsCount = row?.count || 0;

                        res.json({ success: true, user });
                    });
                });
            });
        });
    });
});

// 获取单个用户的所有日志
app.get('/api/admin/users/:id/logs', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const { page = 1, limit = 50, type = 'all' } = req.query;
    const offset = (page - 1) * limit;

    // 首先检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        let loginQuery, operationQuery;
        const params = [id];

        if (type === 'login') {
            // 只获取登录日志
            loginQuery = `SELECT * FROM login_logs WHERE user_id = ? ORDER BY created_at DESC LIMIT ? OFFSET ?`;
            db.get("SELECT COUNT(*) as total FROM login_logs WHERE user_id = ?", [id], (err, row) => {
                const total = row?.total || 0;
                db.all(loginQuery, [...params, parseInt(limit), parseInt(offset)], (err, logs) => {
                    if (err) {
                        return res.status(500).json({ success: false, message: '获取日志失败' });
                    }
                    res.json({
                        success: true,
                        logs: logs,
                        user: user,
                        pagination: {
                            page: parseInt(page),
                            limit: parseInt(limit),
                            total: total,
                            pages: Math.ceil(total / limit)
                        }
                    });
                });
            });
        } else if (type === 'operation') {
            // 只获取操作日志
            operationQuery = `SELECT * FROM operation_logs WHERE user_id = ? ORDER BY created_at DESC LIMIT ? OFFSET ?`;
            db.get("SELECT COUNT(*) as total FROM operation_logs WHERE user_id = ?", [id], (err, row) => {
                const total = row?.total || 0;
                db.all(operationQuery, [...params, parseInt(limit), parseInt(offset)], (err, logs) => {
                    if (err) {
                        return res.status(500).json({ success: false, message: '获取日志失败' });
                    }
                    res.json({
                        success: true,
                        logs: logs,
                        user: user,
                        pagination: {
                            page: parseInt(page),
                            limit: parseInt(limit),
                            total: total,
                            pages: Math.ceil(total / limit)
                        }
                    });
                });
            });
        } else {
            // 获取所有日志（登录日志 + 操作日志）
            const loginCountQuery = "SELECT COUNT(*) as count FROM login_logs WHERE user_id = ?";
            const operationCountQuery = "SELECT COUNT(*) as count FROM operation_logs WHERE user_id = ?";

            db.get(loginCountQuery, [id], (err, loginRow) => {
                db.get(operationCountQuery, [id], (err, operationRow) => {
                    const total = (loginRow?.count || 0) + (operationRow?.count || 0);

                    // 合并两个日志表并按时间排序
                    db.all(`SELECT 'login' as log_type, id, user_id, username, ip_address, user_agent, action, status, created_at
                            FROM login_logs WHERE user_id = ?
                            UNION ALL
                            SELECT 'operation' as log_type, id, user_id, username, ip_address, null as user_agent, action, details as status, created_at
                            FROM operation_logs WHERE user_id = ?
                            ORDER BY created_at DESC LIMIT ? OFFSET ?`,
                        [id, id, parseInt(limit), parseInt(offset)], (err, logs) => {
                        if (err) {
                            return res.status(500).json({ success: false, message: '获取日志失败: ' + err.message });
                        }

                        res.json({
                            success: true,
                            logs: logs,
                            user: user,
                            pagination: {
                                page: parseInt(page),
                                limit: parseInt(limit),
                                total: total,
                                pages: Math.ceil(total / limit)
                            }
                        });
                    });
                });
            });
        }
    });
});

// 获取单个用户的同步配置列表（管理后台）
app.get('/api/admin/users/:id/configs', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);

    // 首先检查用户是否存在
    db.get("SELECT id, username, email FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        // 使用用户独立数据库
        const userDbConn = userDb.getUserDb(userId);
        userDbConn.all("SELECT key, value, updated_at FROM user_configs ORDER BY updated_at DESC", [], (err, configs) => {
            if (err) {
                return res.status(500).json({ success: false, message: '获取配置失败' });
            }

            // 解析每个配置的值
            const parsedConfigs = configs.map(config => {
                let parsedValue = config.value;
                try {
                    parsedValue = JSON.parse(config.value);
                } catch (e) {
                    // 保持原值
                }

                // 计算值的大小
                const valueStr = typeof config.value === 'string' ? config.value : JSON.stringify(config.value);
                const sizeKB = (valueStr.length / 1024).toFixed(2);

                return {
                    key: config.key,
                    value: parsedValue,
                    valueSize: sizeKB + ' KB',
                    updated_at: config.updated_at
                };
            });

            res.json({
                success: true,
                user: user,
                configs: parsedConfigs,
                totalCount: parsedConfigs.length
            });
        });
    });
});

// 获取单个用户的同步配置详情（管理后台）
app.get('/api/admin/users/:id/configs/:key', authenticateAdmin, (req, res) => {
    const { id, key } = req.params;
    const userId = parseInt(id);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(userId);
    userDbConn.get("SELECT key, value, updated_at FROM user_configs WHERE key = ?", [key], (err, config) => {
        if (err || !config) {
            return res.status(404).json({ success: false, message: '配置不存在' });
        }

        let parsedValue = config.value;
        try {
            parsedValue = JSON.parse(config.value);
        } catch (e) {
            // 保持原值
        }

        res.json({
            success: true,
            config: {
                key: config.key,
                value: parsedValue,
                updated_at: config.updated_at
            }
        });
    });
});

// 获取用户的所有配置列表（管理后台）
app.get('/api/admin/users/:id/config-profiles', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(userId);
    userDbConn.all("SELECT config_name, created_at, updated_at FROM user_config_profiles ORDER BY updated_at DESC", [], (err, profiles) => {
        if (err) {
            return res.status(500).json({ success: false, message: '获取配置列表失败' });
        }

        res.json({
            success: true,
            profiles: profiles
        });
    });
});

// 获取指定配置的详细内容（管理后台）
app.get('/api/admin/users/:id/config-profiles/:config_name', authenticateAdmin, (req, res) => {
    const { id, config_name } = req.params;
    const userId = parseInt(id);
    const decodedName = decodeURIComponent(config_name);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(userId);
    userDbConn.get("SELECT config_name, configs, created_at, updated_at FROM user_config_profiles WHERE config_name = ?", [decodedName], (err, profile) => {
        if (err || !profile) {
            return res.status(404).json({ success: false, message: '配置不存在' });
        }

        try {
            const configs = JSON.parse(profile.configs);
            res.json({
                success: true,
                profile: {
                    config_name: profile.config_name,
                    configs: configs,
                    created_at: profile.created_at,
                    updated_at: profile.updated_at
                }
            });
        } catch (e) {
            res.status(500).json({ success: false, message: '解析配置失败' });
        }
    });
});

// 获取用户的工作日志（管理后台）
app.get('/api/admin/users/:id/tasks', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);
    const { page = 1, limit = 50 } = req.query;
    const offset = (page - 1) * limit;

    // 首先检查用户是否存在
    db.get("SELECT id, username, email FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        // 使用用户独立数据库
        const userDbConn = userDb.getUserDb(userId);

        // 获取总数
        userDbConn.get("SELECT COUNT(*) as total FROM user_tasks", [], (errCount, row) => {
            const total = row?.total || 0;

            // 获取工作日志列表
            userDbConn.all("SELECT * FROM user_tasks ORDER BY updated_at DESC LIMIT ? OFFSET ?",
                [parseInt(limit), parseInt(offset)],
                (err, tasks) => {
                    if (err) {
                        return res.status(500).json({ success: false, message: '获取工作日志失败' });
                    }

                    // 转换为前端需要的格式
                    const parsedTasks = tasks.map(task => {
                        let tags = [];
                        try {
                            tags = JSON.parse(task.tags || '[]');
                        } catch (e) {
                            console.error(`解析任务 ${task.task_id} 的 tags 失败:`, e);
                        }
                        return {
                            id: task.task_id,
                            title: task.title,
                            description: task.description,
                            categoryId: task.category_id,
                            priority: task.priority,
                            status: task.status,
                            workDuration: task.work_duration,
                            completionTime: task.completion_time,
                            tags: tags,
                            updatedAt: task.updated_at
                        };
                    });

                    res.json({
                        success: true,
                        tasks: parsedTasks,
                        user: user,
                        pagination: {
                            page: parseInt(page),
                            limit: parseInt(limit),
                            total: total,
                            pages: Math.ceil(total / limit)
                        }
                    });
                });
        });
    });
});

// 删除指定用户的工作日志（管理后台）
app.delete('/api/admin/users/:id/tasks/:task_id', authenticateAdmin, (req, res) => {
    const { id, task_id } = req.params;
    const userId = parseInt(id);

    // 检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        // 使用用户独立数据库删除任务
        const userDbConn = userDb.getUserDb(userId);
        userDbConn.run("DELETE FROM user_tasks WHERE task_id = ?", [task_id], function(err) {
            if (err) {
                return res.status(500).json({ success: false, message: '删除工作日志失败' });
            }

            // 记录操作日志
            db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                [req.adminId, 'delete_user_task', `删除用户 ${user.username} 的工作日志: ${task_id}`, req.ip]);

            res.json({ success: true, message: '工作日志删除成功' });
        });
    });
});

// 获取指定用户的备忘录（管理后台）
app.get('/api/admin/users/:id/memos', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);

    // 检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        const userDbConn = userDb.getUserDb(userId);
        userDbConn.all("SELECT * FROM user_memos ORDER BY updated_at DESC", [], (err, rows) => {
            if (err) {
                return res.status(500).json({ success: false, message: '获取备忘录失败' });
            }

            const memos = rows.map(row => ({
                id: row.memo_id,
                name: row.name,
                type: row.type,
                typeName: ['', '脚本', '密钥', '其他'][row.type] || '其他',
                content: row.content,
                description: row.description,
                createdAt: row.created_at,
                updatedAt: row.updated_at
            }));

            res.json({ success: true, username: user.username, memos: memos });
        });
    });
});

// 删除指定用户的备忘录（管理后台）
app.delete('/api/admin/users/:id/memos/:memo_id', authenticateAdmin, (req, res) => {
    const { id, memo_id } = req.params;
    const userId = parseInt(id);

    // 检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        const userDbConn = userDb.getUserDb(userId);
        userDbConn.run("DELETE FROM user_memos WHERE memo_id = ?", [memo_id], function(err) {
            if (err) {
                return res.status(500).json({ success: false, message: '删除备忘录失败' });
            }

            // 记录操作日志
            db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                [req.adminId, 'delete_user_memo', `删除用户 ${user.username} 的备忘录: ${memo_id}`, req.ip]);

            res.json({ success: true, message: '备忘录删除成功' });
        });
    });
});

// 批量删除用户日志（管理后台）
app.delete('/api/admin/users/:id/logs', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const { logIds, type = 'login' } = req.body; // logIds: 日志ID数组, type: login/operation/all

    // 检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        if (logIds && Array.isArray(logIds) && logIds.length > 0) {
            // 批量删除指定日志
            const placeholders = logIds.map(() => '?').join(',');
            let tableName = type === 'operation' ? 'operation_logs' : 'login_logs';

            db.run(`DELETE FROM ${tableName} WHERE id IN (${placeholders}) AND user_id = ?`,
                [...logIds, id],
                function(err) {
                    if (err) {
                        return res.status(500).json({ success: false, message: '删除日志失败' });
                    }

                    db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                        [req.adminId, 'batch_delete_logs', `批量删除用户 ${user.username} 的 ${logIds.length} 条日志`, req.ip]);

                    res.json({ success: true, message: `成功删除 ${this.changes} 条日志` });
                });
        } else {
            // 删除所有日志
            const { deleteType = 'login' } = req.body; // login/operation/all
            let deletedCount = 0;
            let tasks = [];

            if (deleteType === 'login' || deleteType === 'all') {
                tasks.push(new Promise((resolve, reject) => {
                    db.run("DELETE FROM login_logs WHERE user_id = ?", [id], function(err) {
                        if (err) reject(err);
                        else {
                            deletedCount += this.changes;
                            resolve();
                        }
                    });
                }));
            }

            if (deleteType === 'operation' || deleteType === 'all') {
                tasks.push(new Promise((resolve, reject) => {
                    db.run("DELETE FROM operation_logs WHERE user_id = ?", [id], function(err) {
                        if (err) reject(err);
                        else {
                            deletedCount += this.changes;
                            resolve();
                        }
                    });
                }));
            }

            Promise.all(tasks).then(() => {
                db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                    [req.adminId, 'delete_all_logs', `清空用户 ${user.username} 的所有日志 (${deleteType})`, req.ip]);

                res.json({ success: true, message: `成功删除 ${deletedCount} 条日志` });
            }).catch(err => {
                res.status(500).json({ success: false, message: '删除日志失败: ' + err.message });
            });
        }
    });
});

// 批量删除用户配置（管理后台）
app.delete('/api/admin/users/:id/configs', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);
    const { configKeys } = req.body; // 配置键数组

    // 检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        const userDbConn = userDb.getUserDb(userId);

        if (configKeys && Array.isArray(configKeys) && configKeys.length > 0) {
            // 批量删除指定配置
            const placeholders = configKeys.map(() => '?').join(',');
            userDbConn.run(`DELETE FROM user_configs WHERE key IN (${placeholders})`,
                configKeys,
                function(err) {
                    if (err) {
                        return res.status(500).json({ success: false, message: '删除配置失败' });
                    }

                    db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                        [req.adminId, 'batch_delete_configs', `批量删除用户 ${user.username} 的 ${configKeys.length} 个配置`, req.ip]);

                    res.json({ success: true, message: `成功删除 ${this.changes} 个配置` });
                });
        } else {
            // 删除所有配置
            userDbConn.run("DELETE FROM user_configs", [], function(err) {
                if (err) {
                    return res.status(500).json({ success: false, message: '删除配置失败' });
                }

                const deletedCount = this.changes;

                db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                    [req.adminId, 'delete_all_configs', `清空用户 ${user.username} 的所有配置`, req.ip]);

                res.json({ success: true, message: `成功删除 ${deletedCount} 个配置` });
            });
        }
    });
});

// 批量删除用户设备配置（管理后台）
app.delete('/api/admin/users/:id/config-profiles', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const userId = parseInt(id);
    const { profileNames } = req.body; // 配置名称数组

    // 检查用户是否存在
    db.get("SELECT id, username FROM users WHERE id = ?", [id], (err, user) => {
        if (err || !user) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }

        const userDbConn = userDb.getUserDb(userId);

        if (profileNames && Array.isArray(profileNames) && profileNames.length > 0) {
            // 批量删除指定配置
            const placeholders = profileNames.map(() => '?').join(',');
            userDbConn.run(`DELETE FROM user_config_profiles WHERE config_name IN (${placeholders})`,
                profileNames,
                function(err) {
                    if (err) {
                        return res.status(500).json({ success: false, message: '删除设备配置失败' });
                    }

                    db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                        [req.adminId, 'batch_delete_profiles', `批量删除用户 ${user.username} 的 ${profileNames.length} 个设备配置`, req.ip]);

                    res.json({ success: true, message: `成功删除 ${this.changes} 个设备配置` });
                });
        } else {
            // 删除所有设备配置
            userDbConn.run("DELETE FROM user_config_profiles", [], function(err) {
                if (err) {
                    return res.status(500).json({ success: false, message: '删除设备配置失败' });
                }

                const deletedCount = this.changes;

                db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                    [req.adminId, 'delete_all_profiles', `清空用户 ${user.username} 的所有设备配置`, req.ip]);

                res.json({ success: true, message: `成功删除 ${deletedCount} 个设备配置` });
            });
        }
    });
});

// 用户列表 API（保持原有功能）
app.get('/api/admin/users', authenticateAdmin, (req, res) => {
    const { page = 1, limit = 20, search = '', status = '' } = req.query;
    const offset = (page - 1) * limit;
    
    let whereClause = '1=1';
    const params = [];
    
    if (search) {
        whereClause += ' AND (username LIKE ? OR email LIKE ?)';
        params.push(`%${search}%`, `%${search}%`);
    }
    
    if (status) {
        whereClause += ' AND status = ?';
        params.push(status);
    }

    db.get(`SELECT COUNT(*) as total FROM users WHERE ${whereClause}`, params, (err, row) => {
        const total = row?.total || 0;
        
        db.all(`SELECT id, username, email, role, created_at, last_login, status 
                FROM users WHERE ${whereClause} 
                ORDER BY created_at DESC LIMIT ? OFFSET ?`, 
            [...params, parseInt(limit), parseInt(offset)], (err, users) => {
            if (err) {
                return res.status(500).json({ success: false, message: '获取用户列表失败' });
            }
            
            res.json({
                success: true,
                users: users,
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total: total,
                    pages: Math.ceil(total / limit)
                }
            });
        });
    });
});

app.put('/api/admin/users/:id', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const { email, role, status } = req.body;
    
    const updates = [];
    const params = [];
    
    if (email !== undefined) {
        updates.push('email = ?');
        params.push(email);
    }
    if (role !== undefined) {
        updates.push('role = ?');
        params.push(role);
    }
    if (status !== undefined) {
        updates.push('status = ?');
        params.push(status);
    }
    
    if (updates.length === 0) {
        return res.status(400).json({ success: false, message: '没有要更新的字段' });
    }
    
    params.push(id);
    
    db.run(`UPDATE users SET ${updates.join(', ')} WHERE id = ?`, params, function(err) {
        if (err) {
            return res.status(500).json({ success: false, message: '更新用户失败: ' + err.message });
        }
        
        db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
            [req.adminId, 'update_user', `更新用户 ID: ${id}`, req.ip]);
        
        res.json({ success: true, message: '用户更新成功' });
    });
});

app.delete('/api/admin/users/:id', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    
    db.run("DELETE FROM users WHERE id = ?", [id], function(err) {
        if (err) {
            return res.status(500).json({ success: false, message: '删除用户失败' });
        }
        
        db.run("DELETE FROM user_sessions WHERE user_id = ?", [id]);
        
        db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
            [req.adminId, 'delete_user', `删除用户 ID: ${id}`, req.ip]);
        
        res.json({ success: true, message: '用户删除成功' });
    });
});

// 重置用户密码
app.post('/api/admin/users/:id/reset-password', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const newPassword = '666666';
    
    console.log(`\n[重置密码] 管理员 ID: ${req.adminId}, 用户 ID: ${id}`);
    
    const crypto = require('crypto');
    const hashedPassword = crypto.createHash('sha256').update(newPassword).digest('hex');
    console.log(`  - 新密码哈希: ${hashedPassword}`);
    
    db.run("UPDATE users SET password = ? WHERE id = ?", [hashedPassword, id], function(err) {
        if (err) {
            console.log(`  - 失败：${err.message}`);
            return res.status(500).json({ success: false, message: '重置密码失败：' + err.message });
        }
        
        if (this.changes === 0) {
            return res.status(404).json({ success: false, message: '用户不存在' });
        }
        
        // 使所有 session 失效（强制重新登录）
        db.run("DELETE FROM user_sessions WHERE user_id = ?", [id]);
        
        // 记录操作日志
        db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
            [req.adminId, 'reset_password', `重置用户 ID: ${id} 的密码`, req.ip]);
        
        // 记录密码重置日志
        db.run("INSERT INTO login_logs (user_id, action, status, ip_address) VALUES (?, ?, ?, ?)",
            [parseInt(id), 'password_reset', 'admin_reset', req.ip]);
        
        console.log(`  - 成功：用户 ID: ${id} 密码已重置为 ${newPassword}`);
        
        res.json({ 
            success: true, 
            message: `密码已重置为 ${newPassword}`,
            newPassword: newPassword
        });
    });
});

app.get('/api/admin/logs', authenticateAdmin, (req, res) => {
    const { page = 1, limit = 50, type = 'login', startDate = '', endDate = '' } = req.query;
    const offset = (page - 1) * limit;
    
    let table = type === 'login' ? 'login_logs' : 'operation_logs';
    let whereClause = '1=1';
    const params = [];
    
    if (startDate) {
        whereClause += ' AND created_at >= ?';
        params.push(startDate);
    }
    if (endDate) {
        whereClause += ' AND created_at <= ?';
        params.push(endDate);
    }

    db.get(`SELECT COUNT(*) as total FROM ${table} WHERE ${whereClause}`, params, (err, row) => {
        const total = row?.total || 0;
        
        db.all(`SELECT * FROM ${table} WHERE ${whereClause} 
                ORDER BY created_at DESC LIMIT ? OFFSET ?`, 
            [...params, parseInt(limit), parseInt(offset)], (err, logs) => {
            if (err) {
                return res.status(500).json({ success: false, message: '获取日志失败' });
            }
            
            res.json({
                success: true,
                logs: logs,
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total: total,
                    pages: Math.ceil(total / limit)
                }
            });
        });
    });
});

app.get('/api/admin/collections', authenticateAdmin, (req, res) => {
    const { page = 1, limit = 20, search = '' } = req.query;
    const offset = (page - 1) * limit;
    
    let whereClause = '1=1';
    const params = [];
    
    if (search) {
        whereClause += ' AND (name LIKE ? OR description LIKE ?)';
        params.push(`%${search}%`, `%${search}%`);
    }

    db.get(`SELECT COUNT(*) as total FROM app_collections WHERE ${whereClause}`, params, (err, row) => {
        const total = row?.total || 0;
        
        db.all(`SELECT * FROM app_collections WHERE ${whereClause} 
                ORDER BY created_at DESC LIMIT ? OFFSET ?`, 
            [...params, parseInt(limit), parseInt(offset)], (err, collections) => {
            if (err) {
                return res.status(500).json({ success: false, message: '获取应用集合列表失败' });
            }
            
            res.json({
                success: true,
                collections: collections,
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total: total,
                    pages: Math.ceil(total / limit)
                }
            });
        });
    });
});

app.put('/api/admin/collections/:id', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    const { name, description, apps, status } = req.body;
    
    const updates = ['updated_at = CURRENT_TIMESTAMP'];
    const params = [];
    
    if (name !== undefined) {
        updates.push('name = ?');
        params.push(name);
    }
    if (description !== undefined) {
        updates.push('description = ?');
        params.push(description);
    }
    if (apps !== undefined) {
        updates.push('apps = ?');
        params.push(typeof apps === 'string' ? apps : JSON.stringify(apps));
    }
    if (status !== undefined) {
        updates.push('status = ?');
        params.push(status);
    }
    
    params.push(id);
    
    db.run(`UPDATE app_collections SET ${updates.join(', ')} WHERE id = ?`, params, function(err) {
        if (err) {
            return res.status(500).json({ success: false, message: '更新应用集合失败' });
        }
        
        db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
            [req.adminId, 'update_collection', `更新应用集合 ID: ${id}`, req.ip]);
        
        res.json({ success: true, message: '应用集合更新成功' });
    });
});

app.delete('/api/admin/collections/:id', authenticateAdmin, (req, res) => {
    const { id } = req.params;
    
    db.run("DELETE FROM app_collections WHERE id = ?", [id], function(err) {
        if (err) {
            return res.status(500).json({ success: false, message: '删除应用集合失败' });
        }
        
        db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
            [req.adminId, 'delete_collection', `删除应用集合 ID: ${id}`, req.ip]);
        
        res.json({ success: true, message: '应用集合删除成功' });
    });
});

app.get('/api/admin/config', authenticateAdmin, (req, res) => {
    db.all("SELECT * FROM system_config", (err, configs) => {
        if (err) {
            return res.status(500).json({ success: false, message: '获取配置失败' });
        }
        res.json({ success: true, configs });
    });
});

// 用户获取自己的配置
app.get('/api/config/get', authenticateToken, (req, res) => {
    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.all("SELECT * FROM user_configs", [], (err, configs) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取配置失败' });
        }

        // 转换为对象格式
        const configObj = {};
        configs.forEach(config => {
            try {
                configObj[config.key] = JSON.parse(config.value);
            } catch (e) {
                configObj[config.key] = config.value;
            }
        });

        res.json({ success: true, configs: configObj });
    });
});

// 用户保存配置到默认配置（保持向后兼容）
app.post('/api/config/save', authenticateToken, (req, res) => {
    const { configs } = req.body;

    if (!configs) {
        return res.status(400).json({ success: false, error: '配置数据不能为空' });
    }

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.serialize(() => {
        for (const [key, value] of Object.entries(configs)) {
            const valueStr = typeof value === 'object' ? JSON.stringify(value) : String(value);
            userDbConn.run(`INSERT OR REPLACE INTO user_configs (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP)`,
                [key, valueStr]);
        }

        res.json({ success: true, message: '配置保存成功' });
    });
});

// ===== FRPC配置API =====

// 获取FRPC配置
app.get('/api/config/frpc/get', authenticateToken, (req, res) => {
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.get("SELECT value FROM user_configs WHERE key = 'frpc'", [], (err, row) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取FRPC配置失败' });
        }

        if (row && row.value) {
            try {
                const frpcConfig = JSON.parse(row.value);
                res.json({ success: true, frpc: frpcConfig });
            } catch (e) {
                res.json({ success: true, frpc: {} });
            }
        } else {
            res.json({ success: true, frpc: {} });
        }
    });
});

// 保存FRPC配置
app.post('/api/config/frpc/save', authenticateToken, (req, res) => {
    const { frpc } = req.body;

    if (!frpc) {
        return res.status(400).json({ success: false, error: 'FRPC配置不能为空' });
    }

    const userDbConn = userDb.getUserDb(req.userId);
    const frpcStr = JSON.stringify(frpc);

    userDbConn.run(`INSERT OR REPLACE INTO user_configs (key, value, updated_at) VALUES ('frpc', ?, CURRENT_TIMESTAMP)`,
        [frpcStr], (err) => {
            if (err) {
                return res.status(500).json({ success: false, error: '保存FRPC配置失败' });
            }
            res.json({ success: true, message: 'FRPC配置保存成功' });
        });
});

// 用户获取配置列表（不含详细数据，用于列表展示）
app.get('/api/config/list', authenticateToken, (req, res) => {
    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.all("SELECT key, updated_at, LENGTH(value) as value_length FROM user_configs ORDER BY updated_at DESC", [], (err, configs) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取配置列表失败' });
        }

        res.json({ success: true, configs: configs });
    });
});

// ===== 多个配置管理 API =====

// 获取用户所有配置列表
app.get('/api/config/profiles', authenticateToken, (req, res) => {
    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.all("SELECT config_name, created_at, updated_at FROM user_config_profiles ORDER BY updated_at DESC", [], (err, profiles) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取配置列表失败' });
        }

        res.json({ success: true, profiles: profiles });
    });
});

// 创建新配置
app.post('/api/config/profiles', authenticateToken, (req, res) => {
    const { config_name, configs } = req.body;

    if (!config_name) {
        return res.status(400).json({ success: false, error: '请指定配置名称（如：台式机、笔记本）' });
    }

    if (!configs) {
        return res.status(400).json({ success: false, error: '配置数据不能为空' });
    }

    const configsStr = JSON.stringify(configs);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.run(`INSERT OR REPLACE INTO user_config_profiles (config_name, configs, created_at, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)`,
        [config_name, configsStr],
        function(err) {
            if (err) {
                return res.status(500).json({ success: false, error: '创建配置失败: ' + err.message });
            }

            res.json({ success: true, message: '配置创建成功', config_name: config_name });
        });
});

// 获取指定配置
app.get('/api/config/profiles/:config_name', authenticateToken, (req, res) => {
    const { config_name } = req.params;
    const decodedName = decodeURIComponent(config_name);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.get("SELECT config_name, configs, created_at, updated_at FROM user_config_profiles WHERE config_name = ?", [decodedName], (err, profile) => {
        if (err || !profile) {
            return res.status(404).json({ success: false, error: '配置不存在' });
        }

        try {
            const configs = JSON.parse(profile.configs);
            res.json({ success: true, configs: configs, config_name: profile.config_name, created_at: profile.created_at, updated_at: profile.updated_at });
        } catch (e) {
            res.status(500).json({ success: false, error: '解析配置失败' });
        }
    });
});

// 更新指定配置
app.put('/api/config/profiles/:config_name', authenticateToken, (req, res) => {
    const { config_name } = req.params;
    const decodedName = decodeURIComponent(config_name);
    const { configs } = req.body;

    if (!configs) {
        return res.status(400).json({ success: false, error: '配置数据不能为空' });
    }

    const configsStr = JSON.stringify(configs);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.run(`UPDATE user_config_profiles SET configs = ?, updated_at = CURRENT_TIMESTAMP WHERE config_name = ?`,
        [configsStr, decodedName],
        function(err) {
            if (err) {
                return res.status(500).json({ success: false, error: '更新配置失败' });
            }

            res.json({ success: true, message: '配置更新成功', config_name: decodedName });
        });
});

// 删除指定配置
app.delete('/api/config/profiles/:config_name', authenticateToken, (req, res) => {
    const { config_name } = req.params;
    const decodedName = decodeURIComponent(config_name);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.run("DELETE FROM user_config_profiles WHERE config_name = ?", [decodedName], function(err) {
        if (err) {
            return res.status(500).json({ success: false, error: '删除配置失败' });
        }

        res.json({ success: true, message: '配置删除成功', config_name: decodedName });
    });
});

// 上传配置（兼容旧接口）
app.post('/api/config/upload', authenticateToken, (req, res) => {
    const { configs, config_name } = req.body;

    if (!configs) {
        return res.status(400).json({ success: false, error: '配置数据不能为空' });
    }

    // 如果没有指定配置名称，提示用户必须提供
    if (!config_name) {
        return res.status(400).json({ success: false, error: '请指定配置名称（如：台式机、笔记本）' });
    }

    const configsStr = JSON.stringify(configs);

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.run(`INSERT OR REPLACE INTO user_config_profiles (config_name, configs, created_at, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)`,
        [config_name, configsStr],
        function(err) {
            if (err) {
                return res.status(500).json({ success: false, error: '上传配置失败: ' + err.message });
            }

            res.json({ success: true, message: '配置上传成功', config_name: config_name });
        });
});

// 工作日志自动同步 - 上传/同步工作日志（跟随账号）
app.post('/api/config/tasks/sync', authenticateToken, (req, res) => {
    const { tasks, deletedTaskIds } = req.body;

    if (!tasks || !Array.isArray(tasks)) {
        return res.status(400).json({ success: false, error: '工作日志数据格式错误' });
    }

    const userDbConn = userDb.getUserDb(req.userId);

    // 处理删除的任务
    let deleted = 0;
    if (deletedTaskIds && Array.isArray(deletedTaskIds) && deletedTaskIds.length > 0) {
        const deleteStmt = userDbConn.prepare("DELETE FROM user_tasks WHERE task_id = ?");
        deletedTaskIds.forEach(taskId => {
            deleteStmt.run(taskId, function(err) {
                if (!err) deleted++;
            });
        });
        deleteStmt.finalize();
    }

    // 批量插入或更新工作日志
    // 使用客户端发送的 updatedAt，如果为空则使用服务器时间
    const stmt = userDbConn.prepare(`INSERT OR REPLACE INTO user_tasks
        (task_id, title, description, category_id, priority, status, work_duration, completion_time, tags, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`);

    let inserted = 0;
    tasks.forEach(task => {
        // 优先使用客户端发送的 updatedAt，否则使用当前时间
        const updatedAt = task.updatedAt || new Date().toISOString();
        stmt.run(
            task.id || '',
            task.title || '',
            task.description || '',
            task.categoryId || 0,
            task.priority || 0,
            task.status || 0,
            task.workDuration || 0,
            task.completionTime || '',
            JSON.stringify(task.tags || []),
            updatedAt,
            function(err) {
                if (!err) inserted++;
            }
        );
    });

    stmt.finalize((err) => {
        if (err) {
            return res.status(500).json({ success: false, error: '同步工作日志失败: ' + err.message });
        }

        res.json({ success: true, message: '工作日志同步成功', count: inserted, deleted: deleted });
    });
});

// 获取工作日志（跟随账号）
app.get('/api/config/tasks/get', authenticateToken, (req, res) => {
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.all("SELECT * FROM user_tasks ORDER BY updated_at DESC", [], (err, rows) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取工作日志失败: ' + err.message });
        }

        // 转换为前端需要的格式
        const tasks = rows.map(row => {
            let tags = [];
            try {
                tags = JSON.parse(row.tags || '[]');
            } catch (e) {
                console.error(`解析任务 ${row.task_id} 的 tags 失败:`, e);
            }
            return {
                id: row.task_id,
                title: row.title,
                description: row.description,
                categoryId: row.category_id,
                priority: row.priority,
                status: row.status,
                workDuration: row.work_duration,
                completionTime: row.completion_time,
                tags: tags,
                updatedAt: row.updated_at
            };
        });

        res.json({ success: true, tasks: tasks });
    });
});

// 增量同步：上传任务
app.post('/api/config/tasks/incremental', authenticateToken, (req, res) => {
    const { tasks, lastSyncTime, incremental } = req.body;

    if (!tasks || !Array.isArray(tasks)) {
        return res.status(400).json({ success: false, error: '工作日志数据格式错误' });
    }

    const userDbConn = userDb.getUserDb(req.userId);

    // 批量插入或更新工作日志（增量模式）
    const stmt = userDbConn.prepare(`INSERT OR REPLACE INTO user_tasks
        (task_id, title, description, category_id, priority, status, work_duration, completion_time, tags, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`);

    let inserted = 0;
    tasks.forEach(task => {
        const updatedAt = task.updatedAt || new Date().toISOString();
        const tags = typeof task.tags === 'string' ? task.tags : JSON.stringify(task.tags || []);

        stmt.run(
            task.id,
            task.title,
            task.description,
            task.categoryId,
            task.priority,
            task.status,
            task.workDuration,
            task.completionTime,
            tags,
            updatedAt,
            function(err) {
                if (!err) inserted++;
            }
        );
    });
    stmt.finalize();

    // 记录同步日志
    const syncLogStmt = userDbConn.prepare(`INSERT INTO sync_logs (entity_type, entity_id, action, data, created_at) VALUES (?, ?, ?, ?, ?)`);
    syncLogStmt.run('task', 'batch', 'upload', JSON.stringify({ count: tasks.length, lastSyncTime }), new Date().toISOString());
    syncLogStmt.finalize();

    res.json({ success: true, inserted: inserted });
});

// 增量同步：下载任务
app.get('/api/config/tasks/incremental', authenticateToken, (req, res) => {
    const lastSyncTime = req.query.lastSyncTime;

    const userDbConn = userDb.getUserDb(req.userId);

    // 只获取上次同步时间之后修改的任务
    let query = "SELECT * FROM user_tasks";
    let params = [];

    if (lastSyncTime) {
        query += " WHERE updated_at > ?";
        params.push(lastSyncTime);
    }

    query += " ORDER BY updated_at DESC";

    userDbConn.all(query, params, (err, rows) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取工作日志失败: ' + err.message });
        }

        const tasks = rows.map(row => {
            let tags = [];
            try {
                tags = JSON.parse(row.tags || '[]');
            } catch (e) {
                console.error(`解析任务 ${row.task_id} 的 tags 失败:`, e);
            }
            return {
                id: row.task_id,
                title: row.title,
                description: row.description,
                categoryId: row.category_id,
                priority: row.priority,
                status: row.status,
                workDuration: row.work_duration,
                completionTime: row.completion_time,
                tags: tags,
                updatedAt: row.updated_at
            };
        });

        res.json({ success: true, tasks: tasks });
    });
});

// ========== 备忘录同步 API ==========

// 同步备忘录（上传/更新）
app.post('/api/memos/sync', authenticateToken, (req, res) => {
    const { memos } = req.body;

    if (!memos || !Array.isArray(memos)) {
        return res.status(400).json({ success: false, error: '备忘录数据格式错误' });
    }

    const userDbConn = userDb.getUserDb(req.userId);

    // 批量插入或更新备忘录
    const stmt = userDbConn.prepare(`INSERT OR REPLACE INTO user_memos
        (memo_id, name, type, content, description, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)`);

    let inserted = 0;
    memos.forEach(memo => {
        const updatedAt = memo.updatedAt || new Date().toISOString();
        const createdAt = memo.createdAt || updatedAt;
        stmt.run(
            memo.id || '',
            memo.name || '',
            memo.type || 2,
            memo.content || '',
            memo.description || '',
            createdAt,
            updatedAt,
            function(err) {
                if (!err) inserted++;
            }
        );
    });

    stmt.finalize((err) => {
        if (err) {
            return res.status(500).json({ success: false, error: '同步备忘录失败: ' + err.message });
        }

        res.json({ success: true, message: '备忘录同步成功', count: inserted });
    });
});

// 获取用户备忘录
app.get('/api/memos/get', authenticateToken, (req, res) => {
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.all("SELECT * FROM user_memos ORDER BY updated_at DESC", [], (err, rows) => {
        if (err) {
            return res.status(500).json({ success: false, error: '获取备忘录失败: ' + err.message });
        }

        const memos = rows.map(row => ({
            id: row.memo_id,
            name: row.name,
            type: row.type,
            content: row.content,
            description: row.description,
            createdAt: row.created_at,
            updatedAt: row.updated_at
        }));

        res.json({ success: true, memos: memos });
    });
});

// 增量同步备忘录
app.post('/api/memos/incremental', authenticateToken, (req, res) => {
    const { memos, lastSyncTime } = req.body;

    if (!memos || !Array.isArray(memos)) {
        return res.status(400).json({ success: false, error: '备忘录数据格式错误' });
    }

    const userDbConn = userDb.getUserDb(req.userId);

    // 批量插入或更新备忘录
    const stmt = userDbConn.prepare(`INSERT OR REPLACE INTO user_memos
        (memo_id, name, type, content, description, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)`);

    let inserted = 0;
    memos.forEach(memo => {
        const updatedAt = memo.updatedAt || new Date().toISOString();
        const createdAt = memo.createdAt || updatedAt;
        stmt.run(
            memo.id || '',
            memo.name || '',
            memo.type || 2,
            memo.content || '',
            memo.description || '',
            createdAt,
            updatedAt,
            function(err) {
                if (!err) inserted++;
            }
        );
    });

    stmt.finalize((err) => {
        if (err) {
            return res.status(500).json({ success: false, error: '增量同步备忘录失败: ' + err.message });
        }

        // 获取上次同步时间之后修改的备忘录
        let query = "SELECT * FROM user_memos";
        let params = [];

        if (lastSyncTime) {
            query += " WHERE updated_at > ?";
            params.push(lastSyncTime);
        }

        query += " ORDER BY updated_at DESC";

        userDbConn.all(query, params, (err, rows) => {
            if (err) {
                return res.status(500).json({ success: false, error: '获取备忘录失败: ' + err.message });
            }

            const memosResult = rows.map(row => ({
                id: row.memo_id,
                name: row.name,
                type: row.type,
                content: row.content,
                description: row.description,
                createdAt: row.created_at,
                updatedAt: row.updated_at
            }));

            res.json({ success: true, message: '增量同步成功', count: inserted, memos: memosResult });
        });
    });
});

// 删除备忘录（用户端）
app.post('/api/memos/delete', authenticateToken, (req, res) => {
    const { memoIds } = req.body;

    if (!memoIds || !Array.isArray(memoIds) || memoIds.length === 0) {
        return res.status(400).json({ success: false, error: '备忘录ID列表格式错误' });
    }

    const userDbConn = userDb.getUserDb(req.userId);

    // 批量删除备忘录
    const placeholders = memoIds.map(() => '?').join(',');
    userDbConn.run(`DELETE FROM user_memos WHERE memo_id IN (${placeholders})`, memoIds, function(err) {
        if (err) {
            return res.status(500).json({ success: false, error: '删除备忘录失败: ' + err.message });
        }

        res.json({ success: true, message: '备忘录删除成功', deletedCount: this.changes });
    });
});

// 删除用户配置项
app.delete('/api/config/:key', authenticateToken, (req, res) => {
    const { key } = req.params;

    // 使用用户独立数据库
    const userDbConn = userDb.getUserDb(req.userId);

    userDbConn.run("DELETE FROM user_configs WHERE key = ?", [key], function(err) {
        if (err) {
            return res.status(500).json({ success: false, error: '删除配置失败' });
        }

        res.json({ success: true, message: '配置删除成功' });
    });
});

app.put('/api/admin/config/:key', authenticateAdmin, (req, res) => {
    const { key } = req.params;
    const { value } = req.body;
    
    db.run(`INSERT OR REPLACE INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP)`,
        [key, value], function(err) {
        if (err) {
            return res.status(500).json({ success: false, message: '更新配置失败' });
        }
        
        db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
            [req.adminId, 'update_config', `更新配置: ${key}`, req.ip]);
        
        res.json({ success: true, message: '配置更新成功' });
    });
});

app.post('/api/admin/sync-user', (req, res) => {
    const { username, email, password, action } = req.body;
    
    if (!username || !email) {
        return res.status(400).json({ success: false, message: '用户名和邮箱必填' });
    }
    
    if (action === 'register') {
        const bcrypt = require('bcryptjs');
        const hashedPassword = bcrypt.hashSync(password || 'default123', 10);
        
        db.run("INSERT INTO users (username, email, password) VALUES (?, ?, ?)",
            [username, email, hashedPassword], function(err) {
            if (err) {
                return res.status(500).json({ success: false, message: '注册失败: ' + err.message });
            }
            
            db.run("INSERT INTO login_logs (username, action, status, ip_address) VALUES (?, ?, ?, ?)",
                [username, 'register', 'success', req.ip]);
            
            res.json({ success: true, message: '用户注册成功', userId: this.lastID });
        });
    } else if (action === 'login') {
        db.get("SELECT * FROM users WHERE username = ? OR email = ?", [username, username], (err, user) => {
            if (err || !user) {
                db.run("INSERT INTO login_logs (username, action, status, ip_address) VALUES (?, ?, ?, ?)",
                    [username, 'login', 'failed_not_found', req.ip]);
                return res.status(401).json({ success: false, message: '用户不存在' });
            }
            
            const bcrypt = require('bcryptjs');
            if (!bcrypt.compareSync(password, user.password)) {
                db.run("INSERT INTO login_logs (user_id, username, action, status, ip_address) VALUES (?, ?, ?, ?, ?)",
                    [user.id, username, 'login', 'failed_wrong_password', req.ip]);
                return res.status(401).json({ success: false, message: '密码错误' });
            }
            
            const token = require('crypto').randomBytes(32).toString('hex');
            const expiresAt = new Date(Date.now() + 7 * 24 * 60 * 60 * 1000).toISOString();
            
            db.run("INSERT INTO user_sessions (user_id, token, ip_address, user_agent, expires_at) VALUES (?, ?, ?, ?, ?)",
                [user.id, token, req.ip, req.headers['user-agent'], expiresAt]);
            
            db.run("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = ?", [user.id]);
            
            db.run("INSERT INTO login_logs (user_id, username, action, status, ip_address) VALUES (?, ?, ?, ?, ?)",
                [user.id, username, 'login', 'success', req.ip]);
            
            res.json({
                success: true,
                message: '登录成功',
                token: token,
                user: {
                    id: user.id,
                    username: user.username,
                    email: user.email,
                    role: user.role
                }
            });
        });
    } else {
        res.status(400).json({ success: false, message: '无效的操作' });
    }
});

app.post('/api/admin/validate-token', (req, res) => {
    const { token } = req.body;
    
    if (!token) {
        return res.status(400).json({ success: false, message: 'Token required' });
    }
    
    db.get("SELECT u.* FROM users u JOIN user_sessions s ON u.id = s.user_id WHERE s.token = ? AND s.expires_at > datetime('now')", 
        [token], (err, user) => {
        if (err || !user) {
            return res.status(401).json({ success: false, message: 'Invalid or expired token' });
        }
        
        res.json({
            success: true,
            user: {
                id: user.id,
                username: user.username,
                email: user.email,
                role: user.role
            }
        });
    });
});

// ============================================
// 客户端用户系统 API（新增）
// ============================================

// 用户注册
app.post('/api/auth/register', (req, res) => {
    const { username, email, password } = req.body;
    
    console.log(`\n[注册请求] ${new Date().toISOString()}`);
    console.log(`  - Username: ${username}`);
    console.log(`  - Email: ${email}`);
    console.log(`  - IP: ${req.ip}`);
    
    if (!username || !email || !password) {
        console.log(`  - 失败：缺少必要参数`);
        return res.status(400).json({ success: false, error: '用户名、邮箱和密码必填' });
    }
    
    if (password.length < 6) {
        console.log(`  - 失败：密码长度不足`);
        return res.status(400).json({ success: false, error: '密码长度至少 6 位' });
    }
    
    // 检查客户端是否已经发送了 SHA256 哈希值（64位十六进制）
    const crypto = require('crypto');
    let hashedPassword;

    if (/^[a-f0-9]{64}$/i.test(password)) {
        // 客户端已经发送了哈希值，直接使用
        hashedPassword = password;
    } else {
        // 客户端发送的是明文，进行 SHA-256 哈希
        hashedPassword = crypto.createHash('sha256').update(password).digest('hex');
    }

    db.run("INSERT INTO users (username, email, password, role) VALUES (?, ?, ?, 'user')",
        [username, email, hashedPassword], function(err) {
        if (err) {
            console.log(`  - 失败：${err.message}`);
            if (err.message.includes('username')) {
                return res.status(400).json({ success: false, error: '用户名已存在' });
            }
            if (err.message.includes('email')) {
                return res.status(400).json({ success: false, error: '邮箱已被注册' });
            }
            return res.status(500).json({ success: false, error: '注册失败：' + err.message });
        }
        
        const userId = this.lastID;
        console.log(`  - 成功：用户 ID=${userId}`);
        
        db.run("INSERT INTO login_logs (username, action, status, ip_address) VALUES (?, ?, ?, ?)",
            [username, 'register', 'success', req.ip]);
        
        res.json({ success: true, userId: userId, message: '注册成功' });
    });
});

// 用户登录
app.post('/api/auth/login', (req, res) => {
    const { email, username, password } = req.body;
    
    console.log(`\n[登录请求] ${new Date().toISOString()}`);
    console.log(`  - Email: ${email || 'N/A'}`);
    console.log(`  - Username: ${username || 'N/A'}`);
    console.log(`  - IP: ${req.ip}`);
    
    if ((!email && !username) || !password) {
        console.log(`  - 失败：缺少必要参数`);
        return res.status(400).json({ success: false, error: '邮箱/用户名和密码必填' });
    }
    
    const identifier = email || username;
    const isEmail = email ? true : false;
    
    const query = isEmail ? "SELECT * FROM users WHERE email = ?" : "SELECT * FROM users WHERE username = ?";
    
    db.get(query, [identifier], (err, user) => {
        if (err || !user) {
            console.log(`  - 失败：用户不存在`);
            db.run("INSERT INTO login_logs (username, action, status, ip_address) VALUES (?, ?, ?, ?)",
                [identifier, 'login', 'failed_not_found', req.ip]);
            return res.status(401).json({ success: false, error: '邮箱/用户名或密码错误' });
        }
        
        // 修复：支持 SHA-256 和 bcrypt 两种密码格式
        const crypto = require('crypto');
        const sha256Hash = crypto.createHash('sha256').update(password).digest('hex');
        
        console.log(`  - 密码验证调试:`);
        console.log(`    收到密码长度：${password.length}`);
        console.log(`    收到密码前 20 位：${password.substring(0, 20)}...`);
        console.log(`    数据库密码长度：${user.password.length}`);
        console.log(`    数据库密码前 20 位：${user.password.substring(0, 20)}...`);
        
        // 检查客户端是否已经发送了 SHA256 哈希值（64位十六进制）
        const isClientHash = /^[a-f0-9]{64}$/i.test(password);
        
        let passwordValid = false;
        
        // 检查数据库密码格式并验证
        if (user.password.length === 64 && /^[a-f0-9]{64}$/i.test(user.password)) {
            // 数据库密码是 SHA-256 格式
            console.log(`    检测到数据库密码为 SHA-256 格式`);
            
            if (isClientHash) {
                // 客户端已经发送了哈希值，直接比较
                console.log(`    客户端发送的是哈希值，直接比较`);
                passwordValid = (user.password === password);
            } else {
                // 客户端发送的是明文，计算哈希后比较
                console.log(`    客户端发送的是明文，计算哈希后比较`);
                passwordValid = (user.password === sha256Hash);
            }
            console.log(`    SHA-256 验证结果：${passwordValid ? '成功' : '失败'}`);
        } else if (user.password.startsWith('$2a$') || user.password.startsWith('$2b$')) {
            // 数据库密码是 bcrypt 格式
            console.log(`    检测到数据库密码为 bcrypt 格式`);
            
            // 尝试 1: 直接用收到的密码（可能是 SHA-256）进行 bcrypt 验证
            const bcrypt = require('bcryptjs');
            try {
                passwordValid = bcrypt.compareSync(password, user.password);
                if (passwordValid) {
                    console.log(`    bcrypt 验证（使用收到的密码）结果：成功`);
                } else {
                    console.log(`    bcrypt 验证（使用收到的密码）结果：失败`);
                    // 尝试 2: 如果失败，说明收到的是 SHA-256，需要用明文验证
                    // 但客户端已经发送哈希，所以这种情况无法处理
                    console.log(`    提示：客户端发送的是 SHA-256 哈希，但数据库需要 bcrypt 验证`);
                    console.log(`    建议：重新注册该用户或使用明文密码登录`);
                }
            } catch (e) {
                console.log(`    bcrypt 验证异常：${e.message}`);
                passwordValid = false;
            }
        } else {
            console.log(`    未知密码格式，尝试验证`);
            // 其他格式，直接比较
            passwordValid = (user.password === password || user.password === sha256Hash);
        }
        
        if (!passwordValid) {
            console.log(`  - 失败：密码错误`);
            db.run("INSERT INTO login_logs (user_id, username, action, status, ip_address) VALUES (?, ?, ?, ?, ?)",
                [user.id, user.username, 'login', 'failed_wrong_password', req.ip]);
            return res.status(401).json({ success: false, error: '邮箱/用户名或密码错误' });
        }
        
        console.log(`  - 密码验证通过`);
        
        const token = require('crypto').randomBytes(32).toString('hex');
        const expiresAt = new Date(Date.now() + 7 * 24 * 60 * 60 * 1000).toISOString();
        
        db.run("INSERT INTO user_sessions (user_id, token, ip_address, expires_at) VALUES (?, ?, ?, ?)",
            [user.id, token, req.ip, expiresAt]);
        
        // 修复：使用 ISO 格式记录登录时间
        const loginTime = new Date().toISOString();
        db.run("UPDATE users SET last_login = ? WHERE id = ?", [loginTime, user.id]);
        
        db.run("INSERT INTO login_logs (user_id, username, action, status, ip_address) VALUES (?, ?, ?, ?, ?)",
            [user.id, user.username, 'login', 'success', req.ip]);
        
        console.log(`  - 成功：用户 ID=${user.id}, Token=${token.substring(0, 20)}...`);

        res.json({
            success: true,
            token: token,
            user: {
                id: user.id,
                username: user.username,
                email: user.email,
                vipLevel: 0,
                lastLogin: user.last_login,
                createdAt: user.created_at
            }
        });
    });
});

// 检查邮箱是否存在
app.get('/api/auth/check-email', (req, res) => {
    const email = req.query.email;
    
    if (!email) {
        return res.status(400).json({ success: false, error: '邮箱必填' });
    }
    
    db.get("SELECT id FROM users WHERE email = ?", [email], (err, user) => {
        if (err) {
            return res.status(500).json({ success: false, error: '查询失败' });
        }
        
        res.json({ exists: !!user });
    });
});

// 检查用户名是否存在
app.get('/api/auth/check-username', (req, res) => {
    const username = req.query.username;
    
    if (!username) {
        return res.status(400).json({ success: false, error: '用户名必填' });
    }
    
    db.get("SELECT id FROM users WHERE username = ?", [username], (err, user) => {
        if (err) {
            return res.status(500).json({ success: false, error: '查询失败' });
        }
        
        res.json({ exists: !!user });
    });
});

// 请求密码重置
app.post('/api/auth/request-password-reset', (req, res) => {
    const { email } = req.body;
    
    if (!email) {
        return res.status(400).json({ success: false, error: '邮箱必填' });
    }
    
    // 检查邮箱是否存在
    db.get("SELECT id, username FROM users WHERE email = ?", [email], (err, user) => {
        if (err || !user) {
            // 为了安全，即使邮箱不存在也返回成功
            return res.json({ success: true, message: '如果邮箱已注册，您将收到重置邮件' });
        }
        
        // 生成重置 token（64 位随机字符串）
        const crypto = require('crypto');
        const token = crypto.randomBytes(32).toString('hex');
        const expiresAt = new Date(Date.now() + 30 * 60 * 1000); // 30 分钟有效
        
        // 保存 token 到数据库
        db.run(`INSERT INTO password_reset_tokens (user_id, token, expires_at, used) 
                VALUES (?, ?, ?, 0)`, 
            [user.id, token, expiresAt.toISOString().replace('T', ' ').replace('Z', '')], 
            function(err) {
                if (err) {
                    console.error('[密码重置] 保存 token 失败:', err);
                    return res.status(500).json({ success: false, error: '系统错误' });
                }
                
                console.log('[密码重置] 为用户', user.username, '生成重置 token');
                // TODO: 发送邮件（实际部署时实现）
                // 暂时只返回 token 用于测试
                res.json({ 
                    success: true, 
                    message: '如果邮箱已注册，您将收到重置邮件',
                    debug_token: token // 生产环境需移除
                });
            }
        );
    });
});

// 重置密码
app.post('/api/auth/reset-password', (req, res) => {
    const { token, new_password } = req.body;
    
    if (!token || !new_password) {
        return res.status(400).json({ success: false, error: '参数不完整' });
    }
    
    // 验证 token
    db.get("SELECT * FROM password_reset_tokens WHERE token = ? AND used = 0 AND expires_at > datetime('now')", 
        [token], (err, resetToken) => {
            if (err || !resetToken) {
                return res.status(400).json({ success: false, error: '无效或已过期的重置链接' });
            }
            
            // 检查密码格式
            if (new_password.length < 6) {
                return res.status(400).json({ success: false, error: '密码长度至少 6 位' });
            }
            
            // 检查是否为 SHA-256 格式（客户端已哈希）
            const crypto = require('crypto');
            const isSHA256 = /^[a-f0-9]{64}$/i.test(new_password);
            let hashedPassword;
            
            if (isSHA256) {
                // 客户端已经发送了 SHA-256 哈希值，直接使用
                hashedPassword = new_password;
            } else {
                // 客户端发送的是明文，进行 SHA-256 哈希
                hashedPassword = crypto.createHash('sha256').update(new_password).digest('hex');
            }
            
            console.log('[密码重置] 新密码格式: SHA-256');
            
            // 更新密码（统一使用 SHA-256 格式）
            db.run("UPDATE users SET password = ? WHERE id = ?", [hashedPassword, resetToken.user_id], function(err) {
                if (err) {
                    return res.status(500).json({ success: false, error: '更新密码失败' });
                }
                
                // 标记 token 为已使用
                db.run("UPDATE password_reset_tokens SET used = 1 WHERE token = ?", [token], function(err) {
                    if (err) {
                        console.error('[密码重置] 标记 token 失败:', err);
                    }
                    
                    // 删除该用户的所有其他 session
                    db.run("DELETE FROM user_sessions WHERE user_id = ?", [resetToken.user_id], function(err) {
                        if (err) {
                            console.error('[密码重置] 清除 session 失败:', err);
                        }
                        
                        console.log('[密码重置] 用户 ID', resetToken.user_id, '密码已重置');
                        
                        // 记录操作日志
                        db.run(`INSERT INTO operation_logs (user_id, action, details, ip_address) 
                                VALUES (?, 'password_reset', '用户自助重置密码', '')`, 
                            [resetToken.user_id]);
                        
                        res.json({ success: true, message: '密码重置成功，请使用新密码登录' });
                    });
                });
            });
        }
    );
});

// 获取用户资料
app.get('/api/auth/profile', (req, res) => {
    const token = req.headers['authorization']?.replace('Bearer ', '');
    
    if (!token) {
        return res.status(401).json({ success: false, error: '未授权访问' });
    }
    
    db.get("SELECT u.* FROM users u JOIN user_sessions s ON u.id = s.user_id WHERE s.token = ? AND s.expires_at > datetime('now')", 
        [token], (err, user) => {
        if (err || !user) {
            return res.status(401).json({ success: false, error: '登录已过期' });
        }
        
        res.json({
            success: true,
            user: {
                id: user.id,
                email: user.email,
                username: user.username,
                avatar: user.avatar,
                vip_level: 0,
                created_at: user.created_at,
                last_login: user.last_login
            }
        });
    });
});

// 用户登出
app.post('/api/auth/logout', (req, res) => {
    const token = req.headers['authorization']?.replace('Bearer ', '');
    
    if (!token) {
        return res.status(401).json({ success: false, error: '未授权访问' });
    }
    
    // 获取用户信息
    db.get("SELECT user_id, username FROM user_sessions WHERE token = ? AND expires_at > datetime('now')", 
        [token], (err, session) => {
        if (err || !session) {
            return res.status(401).json({ success: false, error: '无效的会话' });
        }
        
        // 删除会话
        db.run("DELETE FROM user_sessions WHERE token = ?", [token], function(err) {
            if (err) {
                console.error('[用户登出] 删除会话失败:', err);
                return res.status(500).json({ success: false, error: '登出失败' });
            }
            
            // 记录登出日志
            db.run("INSERT INTO login_logs (user_id, username, action, status, ip_address) VALUES (?, ?, ?, ?, ?)",
                [session.user_id, session.username, 'logout', 'success', req.ip], (err) => {
                if (err) {
                    console.error('[用户登出] 记录日志失败:', err);
                }
                
                console.log('[用户登出] 用户:', session.username, '已登出');
                
                res.json({ success: true, message: '登出成功' });
            });
        });
    });
});

// 修改密码
app.post('/api/auth/change-password', authenticateToken, (req, res) => {
    const { old_password, new_password } = req.body;
    const userId = req.userId;
    
    console.log(`\n[修改密码] 用户 ID: ${userId}`);
    
    if (!old_password || !new_password) {
        console.log(`  - 失败：缺少参数`);
        return res.status(400).json({ success: false, error: '请提供旧密码和新密码' });
    }
    
    if (new_password.length < 6) {
        console.log(`  - 失败：新密码长度不足`);
        return res.status(400).json({ success: false, error: '新密码长度不能少于6位' });
    }
    
    // 获取用户当前密码
    db.get("SELECT password FROM users WHERE id = ?", [userId], (err, user) => {
        if (err || !user) {
            console.log(`  - 失败：用户不存在`);
            return res.status(404).json({ success: false, error: '用户不存在' });
        }
        
        const crypto = require('crypto');
        
        // 检查旧密码
        const isOldPasswordHash = /^[a-f0-9]{64}$/i.test(old_password);
        
        let passwordValid = false;
        
        if (user.password.length === 64 && /^[a-f0-9]{64}$/i.test(user.password)) {
            // 数据库密码是 SHA-256 格式
            if (isOldPasswordHash) {
                passwordValid = (user.password === old_password);
            } else {
                const oldHash = crypto.createHash('sha256').update(old_password).digest('hex');
                passwordValid = (user.password === oldHash);
            }
        } else if (user.password.startsWith('$2a$') || user.password.startsWith('$2b$')) {
            const bcrypt = require('bcryptjs');
            passwordValid = bcrypt.compareSync(old_password, user.password);
        } else {
            passwordValid = (user.password === old_password || user.password === old_password);
        }
        
        console.log(`  - 旧密码验证结果: ${passwordValid ? '成功' : '失败'}`);
        
        if (!passwordValid) {
            console.log(`  - 失败：旧密码错误`);
            return res.status(401).json({ success: false, error: '旧密码错误' });
        }
        
        // 客户端已经发送了 SHA256 哈希值，直接使用
        const newHash = new_password;
        
        // 更新密码
        db.run("UPDATE users SET password = ? WHERE id = ?", [newHash, userId], function(err) {
            if (err) {
                console.log(`  - 失败：${err.message}`);
                return res.status(500).json({ success: false, error: '修改密码失败' });
            }
            
            console.log(`  - 成功：新密码已更新`);
            
            // 使所有现有会话失效（强制重新登录）
            db.run("DELETE FROM user_sessions WHERE user_id = ?", [userId]);
            
            // 记录操作日志
            db.run("INSERT INTO operation_logs (user_id, action, details, ip_address) VALUES (?, ?, ?, ?)",
                [userId, 'change_password', '用户修改密码', req.ip]);
            
            res.json({ success: true, message: '密码修改成功，请重新登录' });
        });
    });
});

// 更新用户资料
app.post('/api/user/update-profile', authenticateToken, (req, res) => {
    const { username, avatar } = req.body;
    const userId = req.userId;
    
    console.log(`\n[更新资料] 用户 ID: ${userId}`);
    
    // 构建更新语句
    let updates = [];
    let params = [];
    
    if (username) {
        // 检查用户名是否已被其他用户使用
        db.get("SELECT id FROM users WHERE username = ? AND id != ?", [username, userId], (err, existingUser) => {
            if (err) {
                return res.status(500).json({ success: false, error: '检查用户名失败' });
            }
            if (existingUser) {
                return res.status(400).json({ success: false, error: '用户名已被使用' });
            }
        });
        updates.push("username = ?");
        params.push(username);
    }
    
    if (avatar !== undefined) {
        updates.push("avatar = ?");
        params.push(avatar);
    }
    
    if (updates.length === 0) {
        return res.status(400).json({ success: false, error: '没有要更新的内容' });
    }
    
    params.push(userId);
    
    db.run(`UPDATE users SET ${updates.join(', ')} WHERE id = ?`, params, function(err) {
        if (err) {
            console.log(`  - 失败：${err.message}`);
            return res.status(500).json({ success: false, error: '更新资料失败' });
        }
        
        // 返回更新后的用户信息
        db.get("SELECT id, email, username, avatar, created_at, last_login FROM users WHERE id = ?", [userId], (err, user) => {
            if (err || !user) {
                return res.status(500).json({ success: false, error: '获取用户信息失败' });
            }
            
            res.json({
                success: true,
                message: '资料更新成功',
                user: {
                    id: user.id,
                    email: user.email,
                    username: user.username,
                    avatar: user.avatar,
                    vip_level: 0,
                    created_at: user.created_at,
                    last_login: user.last_login
                }
            });
        });
    });
});

app.get('/admin/*', (req, res) => {
    res.sendFile(path.join(__dirname, 'admin-panel', 'index.html'));
});

// 初始化用户数据库模块（迁移现有数据）
userDb.initUserDb(db);

// 优雅关闭 - 关闭所有数据库连接
process.on('SIGINT', () => {
    console.log('\n正在关闭服务器...');
    userDb.closeAllUserDbs();
    db.close(() => {
        console.log('数据库连接已关闭');
        process.exit(0);
    });
});

process.on('SIGTERM', () => {
    console.log('\n正在关闭服务器...');
    userDb.closeAllUserDbs();
    db.close(() => {
        console.log('数据库连接已关闭');
        process.exit(0);
    });
});

app.listen(PORT, '0.0.0.0', () => {
    console.log(`====================================`);
    console.log(`   Admin Server Running`);
    console.log(`   Port: ${PORT}`);
    console.log(`   Admin Panel: http://localhost:${PORT}/admin`);
    console.log(`   Default Login: admin / admin123`);
    console.log(`   用户数据库目录: ${userDb.USER_DB_DIR}`);
    console.log(`====================================`);
});
