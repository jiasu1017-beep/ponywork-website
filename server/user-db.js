const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');

const USER_DB_DIR = path.join(__dirname, 'data', 'user_dbs');

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
    ensureUserDbDir();  // 确保目录存在
    return path.join(USER_DB_DIR, `user_${userId}.db`);
}

// 获取或创建用户数据库连接
const userDbCache = new Map();

function getUserDb(userId) {
    if (userDbCache.has(userId)) {
        return userDbCache.get(userId);
    }

    const dbPath = getUserDbPath(userId);
    console.log(`[UserDB] 创建用户数据库: ${dbPath}`);

    const userDb = new sqlite3.Database(dbPath, (err) => {
        if (err) {
            console.error(`[UserDB] 打开数据库失败: ${err.message}`);
        } else {
            console.log(`[UserDB] 数据库打开成功: ${dbPath}`);
        }
    });

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

        // 工作日志自动同步表（跟随账号）
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

        // 备忘录同步表（跟随账号）
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

        // 同步日志表
        userDb.run(`CREATE TABLE IF NOT EXISTS sync_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            entity_type TEXT,
            entity_id TEXT,
            action TEXT,
            data TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )`);
    });

    userDbCache.set(userId, userDb);
    return userDb;
}

// 关闭用户数据库连接
function closeUserDb(userId) {
    if (userDbCache.has(userId)) {
        const userDb = userDbCache.get(userId);
        userDb.close((err) => {
            if (err) {
                console.error(`关闭用户数据库 user_${userId}.db 失败:`, err);
            }
        });
        userDbCache.delete(userId);
    }
}

// 关闭所有用户数据库连接
function closeAllUserDbs() {
    for (const userId of userDbCache.keys()) {
        closeUserDb(userId);
    }
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

// 迁移现有数据到用户独立数据库
function migrateDataFromMainDb(mainDb) {
    return new Promise((resolve, reject) => {
        ensureUserDbDir();

        // 获取所有有配置的用户
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
                // 获取该用户的配置
                mainDb.all("SELECT key, value, updated_at FROM user_configs WHERE user_id = ?", [user_id], (err, configs) => {
                    if (err) {
                        console.error(`获取用户 ${user_id} 配置失败:`, err);
                        total--;
                        if (total === 0) resolve();
                        return;
                    }

                    const userDb = getUserDb(user_id);

                    // 迁移 user_configs
                    configs.forEach(config => {
                        userDb.run(`INSERT OR REPLACE INTO user_configs (key, value, updated_at) VALUES (?, ?, ?)`,
                            [config.key, config.value, config.updated_at]);
                    });

                    // 获取该用户的 profiles
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
    ensureUserDbDir();  // 确保目录存在
    console.log(`用户数据库目录: ${USER_DB_DIR}`);

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
    USER_DB_DIR
};
