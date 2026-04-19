#ifndef RECOMMENDAPPWIDGET_H
#define RECOMMENDAPPWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include "../core/recommendedappscache.h"

namespace Ui {
class RecommendAppWidget;
}

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

    Ui::RecommendAppWidget *ui;
    QList<RecommendedApp> m_allApps;
    QGridLayout* m_cardsLayout;
};

#endif // RECOMMENDAPPWIDGET_H