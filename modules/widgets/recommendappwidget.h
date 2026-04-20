#ifndef RECOMMENDAPPWIDGET_H
#define RECOMMENDAPPWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include "../core/recommendedappscache.h"

class RecommendAppWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecommendAppWidget(QWidget *parent = nullptr);
    ~RecommendAppWidget();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onRefreshClicked();
    void onCategoryChanged(int index);
    void onAppCardClicked(int appId);
    void onIconLoaded(QNetworkReply* reply);

private:
    void setupUI();
    void loadApps();
    void refreshCards(const QList<RecommendedApp>& apps);
    QWidget* createAppCard(const RecommendedApp& app);
    void loadIcon(const QString& iconUrl, QLabel* iconLabel);
    void loadIconLarge(const QString& iconUrl, QLabel* iconLabel);

    QComboBox* m_categoryComboBox;
    QPushButton* m_refreshBtn;
    QWidget* m_scrollAreaWidgetContents;
    QGridLayout* m_cardsLayout;
    QList<RecommendedApp> m_allApps;
    QNetworkAccessManager* m_networkManager;
    QMap<QString, QLabel*> m_pendingIconRequests;
};

#endif // RECOMMENDAPPWIDGET_H
