const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');

const USER_DB_DIR = path.join(__dirname, 'data', 'user_dbs');

// 连接池配置
const MAX_CACHE_SIZE = 50;           // 最大缓存连接数
const IDLE_TIMEOUT_MS = 30 * 60 * 1000;  // 空闲超时时间：30分钟
const CLEANUP_INTERVAL_MS = 5 * 60 * 1000;  // 清理间隔：5分钟

// 确保用户数据库目录存在
function ensureUserDbDir() {
    console.log(`[UserDB] 检查目录: ${USER_DB_DIR}`);
    if (!fs.existsSync(USER_DB_DIR)) {
        console.log(`[UserDB] 创建目录: ${USER_DB_DIR}`);
        fs.mkdirSync(USER_DB_DIR, { recursive: true });
    }
}

// 获取用户数据库文件路径
function getUserDbPath(userId) {
    ensureUserDbDir();
    return path.join(USER_DB_DIR, `user_${userId}.db`);
}

// 用户数据库连接缓存（包含连接和最后使用时间）
const userDbCache = new Map();

// 定时清理器
let cleanupTimer = null;

// 更新连接的最后使用时间
function touchConnection(userId) {
    const cached = userDbCache.get(userId);
    if (cached) {
        cached.lastUsed = Date.now();
    }
}

// 清理空闲连接
function cleanupIdleConnections() {
    const now = Date.now();
    let cleaned = 0;

    for (const [userId, cached] of userDbCache.entries()) {
        if (now - cached.lastUsed > IDLE_TIMEOUT_MS) {
            console.log(`[UserDB] 清理空闲连接: user_${userId}.db (空闲 ${Math.round((now - cached.lastUsed) / 60000)} 分钟)`);
            try {
                cached.db.close((err) => {
                    if (err) {
                        console.error(`关闭用户数据库 user_${userId}.db 失败:`, err);
                    }
                });
            } catch (e) {
                console.error(`关闭用户数据库 user_${userId}.db 异常:`, e);
            }
            userDbCache.delete(userId);
            cleaned++;
        }
    }

    if (cleaned > 0) {
        console.log(`[UserDB] 清理了 ${cleaned} 个空闲连接，当前缓存: ${userDbCache.size}`);
    }
}

// 当缓存满时，移除最久未使用的连接
function evictLeastRecentlyUsed() {
    let oldestUserId = null;
    let oldestTime = Date.now();

    for (const [userId, cached] of userDbCache.entries()) {
        if (cached.lastUsed < oldestTime) {
            oldestTime = cached.lastUsed;
            oldestUserId = userId;
        }
    }

    if (oldestUserId !== null) {
        console.log(`[UserDB] 缓存已满，移除最久未使用的连接: user_${oldestUserId}.db`);
        const cached = userDbCache.get(oldestUserId);
        try {
            cached.db.close((err) => {
                if (err) {
                    console.error(`关闭用户数据库 user_${oldestUserId}.db 失败:`, err);
                }
            });
        } catch (e) {
            console.error(`关闭用户数据库 user_${oldestUserId}.db 异常:`, e);
        }
        userDbCache.delete(oldestUserId);
    }
}

// 启动定时清理
function startCleanupTimer() {
    if (cleanupTimer) {
        clearInterval(cleanupTimer);
    }
    cleanupTimer = setInterval(cleanupIdleConnections, CLEANUP_INTERVAL_MS);
    console.log(`[UserDB] 启动连接清理定时器 (间隔: ${CLEANUP_INTERVAL_MS / 60000} 分钟)`);
}

// 停止定时清理
function stopCleanupTimer() {
    if (cleanupTimer) {
        clearInterval(cleanupTimer);
        cleanupTimer = null;
        console.log('[UserDB] 停止连接清理定时器');
    }
}

// 获取或创建用户数据库连接
function getUserDb(userId) {
    // 如果已缓存，更新使用时间并返回
    if (userDbCache.has(userId)) {
        touchConnection(userId);
        return userDbCache.get(userId).db;
    }

    // 如果缓存已满，移除最久未使用的连接
    if (userDbCache.size >= MAX_CACHE_SIZE) {
        evictLeastRecentlyUsed();
    }

    const dbPath = getUserDbPath(userId);
    console.log(`[UserDB] 创建用户数据库连接: ${dbPath}`);

    const userDb = new sqlite3.Database(dbPath, sqlite3.OPEN_READWRITE | sqlite3.OPEN_CREATE, (err) => {
        if (err) {
            console.error(`[UserDB] 打开数据库失败: ${err.message}`);
        } else {
            console.log(`[UserDB] 数据库打开成功: ${dbPath}`);
        }
    });

    // 启用 WAL 模式提高并发性能
    userDb.run('PRAGMA journal_mode = WAL');
    userDb.run('PRAGMA busy_timeout = 5000');  // 等待锁超时 5 秒

    // 初始化用户数据库表结构
    userDb.serialize(() => {
        userDb.run(`CREATE TABLE IF NOT EXISTS user_configs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            key TEXT NOT NULL UNIQUE,
            value TEXT,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )`);

        userDb.run(`CREATE TABLE IF NOT EXISTS user_config_profiles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            config_name TEXT NOT NULL UNIQUE,
            configs TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )`);

        userDb.run(`CREATE TABLE IF NOT EXISTS user_tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id TEXT NOT NULL UNIQUE,
            title TEXT,
            description TEXT,
            category_id INTEGER DEFAULT 0,
            priority INTEGER DEFAULT 0,
            status INTEGER DEFAULT 0,
            work_duration REAL DEFAULT 0,
            completion_time TEXT,
            tags TEXT,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )`);

        userDb.run(`CREATE TABLE IF NOT EXISTS user_memos (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            memo_id TEXT NOT NULL UNIQUE,
            name TEXT,
            type INTEGER DEFAULT 2,
            content TEXT,
            description TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )`);

        userDb.run(`CREATE TABLE IF NOT EXISTS sync_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            entity_type TEXT,
            entity_id TEXT,
            action TEXT,
            data TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )`);
    });

    // 缓存连接和最后使用时间
    userDbCache.set(userId, {
        db: userDb,
        lastUsed: Date.now()
    });

    return userDb;
}

// 关闭用户数据库连接
function closeUserDb(userId) {
    const cached = userDbCache.get(userId);
    if (cached) {
        try {
            cached.db.close((err) => {
                if (err) {
                    console.error(`关闭用户数据库 user_${userId}.db 失败:`, err);
                }
            });
        } catch (e) {
            console.error(`关闭用户数据库 user_${userId}.db 异常:`, e);
        }
        userDbCache.delete(userId);
    }
}

// 关闭所有用户数据库连接
function closeAllUserDbs() {
    console.log(`[UserDB] 关闭所有数据库连接 (${userDbCache.size} 个)`);
    for (const userId of userDbCache.keys()) {
        closeUserDb(userId);
    }
    stopCleanupTimer();
}

// 检查用户数据库是否存在
function userDbExists(userId) {
    return fs.existsSync(getUserDbPath(userId));
}

// 获取用户数据库列表
function getAllUserDbs() {
    ensureUserDbDir();
    const files = fs.readdirSync(USER_DB_DIR);
    return files.filter(f => f.startsWith('user_') && f.endsWith('.db'));
}

// 获取缓存状态
function getCacheStatus() {
    const status = {
        totalConnections: userDbCache.size,
        maxCacheSize: MAX_CACHE_SIZE,
        idleTimeoutMs: IDLE_TIMEOUT_MS,
        connections: []
    };

    const now = Date.now();
    for (const [userId, cached] of userDbCache.entries()) {
        status.connections.push({
            userId,
            idleMinutes: Math.round((now - cached.lastUsed) / 60000)
        });
    }

    return status;
}

// 迁移现有数据到用户独立数据库
function migrateDataFromMainDb(mainDb) {
    return new Promise((resolve, reject) => {
        ensureUserDbDir();

        mainDb.all("SELECT DISTINCT user_id FROM user_configs", [], (err, users) => {
            if (err) {
                console.error('获取用户列表失败:', err);
                return reject(err);
            }

            if (!users || users.length === 0) {
                console.log('没有需要迁移的用户配置数据');
                return resolve();
            }

            let migrated = 0;
            let total = users.length;

            users.forEach(({ user_id }) => {
                mainDb.all("SELECT key, value, updated_at FROM user_configs WHERE user_id = ?", [user_id], (err, configs) => {
                    if (err) {
                        console.error(`获取用户 ${user_id} 配置失败:`, err);
                        total--;
                        if (total === 0) resolve();
                        return;
                    }

                    const userDb = getUserDb(user_id);

                    configs.forEach(config => {
                        userDb.run(`INSERT OR REPLACE INTO user_configs (key, value, updated_at) VALUES (?, ?, ?)`,
                            [config.key, config.value, config.updated_at]);
                    });

                    mainDb.all("SELECT config_name, configs, created_at, updated_at FROM user_config_profiles WHERE user_id = ?", [user_id], (err, profiles) => {
                        if (err) {
                            console.error(`获取用户 ${user_id} profiles 失败:`, err);
                        } else if (profiles) {
                            profiles.forEach(profile => {
                                userDb.run(`INSERT OR REPLACE INTO user_config_profiles (config_name, configs, created_at, updated_at) VALUES (?, ?, ?, ?)`,
                                    [profile.config_name, profile.configs, profile.created_at, profile.updated_at]);
                            });
                        }

                        migrated++;
                        console.log(`迁移用户 ${user_id} 的配置完成 (${migrated}/${users.length})`);

                        if (migrated === users.length) {
                            console.log('所有用户配置数据迁移完成');
                            resolve();
                        }
                    });
                });
            });
        });
    });
}

// 初始化用户数据库模块
function initUserDb(mainDb) {
    ensureUserDbDir();
    console.log(`用户数据库目录: ${USER_DB_DIR}`);
    console.log(`[UserDB] 配置: 最大缓存=${MAX_CACHE_SIZE}, 空闲超时=${IDLE_TIMEOUT_MS / 60000}分钟`);

    startCleanupTimer();

    migrateDataFromMainDb(mainDb).then(() => {
        console.log('用户数据库模块初始化完成');
    }).catch(err => {
        console.error('用户数据迁移失败:', err);
    });
}

module.exports = {
    ensureUserDbDir,
    getUserDbPath,
    getUserDb,
    closeUserDb,
    closeAllUserDbs,
    userDbExists,
    getAllUserDbs,
    migrateDataFromMainDb,
    initUserDb,
    getCacheStatus,
    startCleanupTimer,
    stopCleanupTimer,
    USER_DB_DIR
};
