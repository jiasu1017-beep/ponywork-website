#ifndef RECOMMENDAPPWIDGET_H
#define RECOMMENDAPPWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
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

private:
    void setupUI();
    void loadApps();
    void refreshCards(const QList<RecommendedApp>& apps);
    QWidget* createAppCard(const RecommendedApp& app);

    QComboBox* m_categoryComboBox;
    QPushButton* m_refreshBtn;
    QWidget* m_scrollAreaWidgetContents;
    QGridLayout* m_cardsLayout;
    QList<RecommendedApp> m_allApps;
};

#endif // RECOMMENDAPPWIDGET_H
