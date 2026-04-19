#include "recommendappwidget.h"
#include "ui_recommendappwidget.h"
#include "../core/recommendedappscache.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>
#include <QEvent>

RecommendAppWidget::RecommendAppWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RecommendAppWidget)
    , m_cardsLayout(nullptr)
{
    ui->setupUi(this);
    setupUI();
    loadApps();

    connect(RecommendedAppsCache::instance(), &RecommendedAppsCache::appsUpdated,
            this, &RecommendAppWidget::loadApps);
}

RecommendAppWidget::~RecommendAppWidget()
{
    delete ui;
}

void RecommendAppWidget::setupUI()
{
    // 初始化卡片布局
    QWidget* contents = ui->scrollAreaWidgetContents;
    m_cardsLayout = new QGridLayout(contents);
    m_cardsLayout->setSpacing(20);
    m_cardsLayout->setContentsMargins(10, 10, 10, 10);

    // 连接刷新按钮
    connect(ui->refreshBtn, &QPushButton::clicked, this, &RecommendAppWidget::onRefreshClicked);

    // 连接分类下拉框
    connect(ui->categoryComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecommendAppWidget::onCategoryChanged);
}

void RecommendAppWidget::loadApps()
{
    m_allApps = RecommendedAppsCache::instance()->getApps();

    // 更新分类下拉框
    ui->categoryComboBox->clear();
    ui->categoryComboBox->addItem("全部");
    for (const QString& cat : RecommendedAppsCache::instance()->getCategories()) {
        ui->categoryComboBox->addItem(cat);
    }

    refreshCards(m_allApps);
}

void RecommendAppWidget::onRefreshClicked()
{
    RecommendedAppsCache::instance()->refreshFromServer();
}

void RecommendAppWidget::onCategoryChanged(int index)
{
    QString category = ui->categoryComboBox->itemText(index);
    if (category == "全部") {
        refreshCards(m_allApps);
    } else {
        QList<RecommendedApp> filtered;
        for (const auto& app : m_allApps) {
            if (app.category == category) filtered.append(app);
        }
        refreshCards(filtered);
    }
}

void RecommendAppWidget::onAppCardClicked(int appId)
{
    RecommendedApp app = RecommendedAppsCache::instance()->getAppById(appId);
    if (app.id == 0) return;

    // 创建详情对话框
    QDialog dlg(this);
    dlg.setWindowTitle(app.name);
    dlg.setMinimumSize(400, 500);

    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    // 图标
    QLabel* iconLabel = new QLabel;
    iconLabel->setFixedSize(120, 120);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setText("📦");
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);

    // 名称
    QLabel* nameLabel = new QLabel(app.name);
    nameLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    // 分类
    QLabel* catLabel = new QLabel(app.category);
    catLabel->setStyleSheet("color: #666;");
    catLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(catLabel);

    // 简介
    QTextEdit* descText = new QTextEdit;
    descText->setText(app.description);
    descText->setReadOnly(true);
    layout->addWidget(descText);

    // 下载地址
    layout->addWidget(new QLabel("下载地址"));
    for (const auto& dl : app.downloads) {
        QHBoxLayout* dlLayout = new QHBoxLayout;
        dlLayout->addWidget(new QLabel(dl.name));

        QPushButton* openBtn = new QPushButton("打开");
        QPushButton* copyBtn = new QPushButton("复制");
        dlLayout->addWidget(openBtn);
        dlLayout->addWidget(copyBtn);

        QString url = dl.url;
        connect(openBtn, &QPushButton::clicked, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        connect(copyBtn, &QPushButton::clicked, [url]() {
            QApplication::clipboard()->setText(url);
        });

        layout->addLayout(dlLayout);
    }

    // 关闭按钮
    QPushButton* closeBtn = new QPushButton("关闭");
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

void RecommendAppWidget::refreshCards(const QList<RecommendedApp>& apps)
{
    if (!m_cardsLayout) return;

    // 清除现有卡片
    QLayoutItem* child;
    while ((child = m_cardsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    // 创建新卡片（网格布局，每行3个）
    int row = 0, col = 0;
    const int maxCols = 3;
    for (const auto& app : apps) {
        QWidget* card = createAppCard(app);
        m_cardsLayout->addWidget(card, row, col);
        col++;
        if (col >= maxCols) { col = 0; row++; }
    }
}

QWidget* RecommendAppWidget::createAppCard(const RecommendedApp& app)
{
    QWidget* card = new QWidget;
    card->setFixedSize(200, 180);
    card->setStyleSheet("QWidget { border: 1px solid #ddd; border-radius: 8px; background: white; }");
    card->setCursor(Qt::PointingHandCursor);
    card->installEventFilter(this);

    // 保存 app id
    card->setProperty("appId", app.id);

    QVBoxLayout* layout = new QVBoxLayout(card);

    // 图标
    QLabel* iconLabel = new QLabel;
    iconLabel->setFixedSize(80, 80);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setText("📦");
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);

    // 名称
    QLabel* nameLabel = new QLabel(app.name);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(nameLabel);

    // 分类
    QLabel* catLabel = new QLabel(app.category);
    catLabel->setAlignment(Qt::AlignCenter);
    catLabel->setStyleSheet("color: #666; font-size: 12px;");
    layout->addWidget(catLabel);

    // 简介
    QLabel* descLabel = new QLabel(app.description);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 11px; color: #888;");
    descLabel->setFixedHeight(20);
    layout->addWidget(descLabel);

    return card;
}

bool RecommendAppWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* card = qobject_cast<QWidget*>(obj);
        if (card && card->property("appId").isValid()) {
            int appId = card->property("appId").toInt();
            onAppCardClicked(appId);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
