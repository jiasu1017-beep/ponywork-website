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