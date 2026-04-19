#include "recommendedappscache.h"
#include "userapi.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

static void initCacheDatabase()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString dbPath = dataPath + "/recommended_apps_cache.db";

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "Failed to open recommended apps cache database:" << db.lastError().text();
        return;
    }

    // Create tables if not exist
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS recommended_apps_cache ("
               "id INTEGER PRIMARY KEY, name TEXT, category TEXT, "
               "description TEXT, icon_url TEXT, sort_order INTEGER)");

    query.exec("CREATE TABLE IF NOT EXISTS recommended_app_downloads_cache ("
               "id INTEGER PRIMARY KEY, app_id INTEGER, name TEXT, "
               "url TEXT, sort_order INTEGER)");
}

RecommendedAppsCache* RecommendedAppsCache::instance()
{
    static RecommendedAppsCache inst;
    return &inst;
}

RecommendedAppsCache::RecommendedAppsCache(QObject *parent)
    : QObject(parent)
{
    initCacheDatabase();
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
    if (!db.isOpen()) {
        return;
    }
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
    if (!db.isOpen()) {
        return;
    }
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