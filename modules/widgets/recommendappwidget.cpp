#include "recommendappwidget.h"
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
#include <QDialog>
#include <QTextEdit>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QUrl>
#include <userapi.h>

RecommendAppWidget::RecommendAppWidget(QWidget *parent)
    : QWidget(parent)
    , m_categoryComboBox(nullptr)
    , m_refreshBtn(nullptr)
    , m_scrollAreaWidgetContents(nullptr)
    , m_cardsLayout(nullptr)
    , m_networkManager(nullptr)
{
    m_networkManager = new QNetworkAccessManager(this);

    setupUI();
    loadApps();

    // 连接信号：数据更新后刷新显示
    connect(RecommendedAppsCache::instance(), &RecommendedAppsCache::appsUpdated,
            this, &RecommendAppWidget::loadApps);

    // 首次加载时从服务器刷新
    RecommendedAppsCache::instance()->refreshFromServer();
}

RecommendAppWidget::~RecommendAppWidget()
{
}

void RecommendAppWidget::loadIcon(const QString& iconUrl, QLabel* iconLabel)
{
    if (iconUrl.isEmpty()) {
        iconLabel->setText("📦");
        return;
    }

    QString fullUrl = iconUrl;
    if (iconUrl.startsWith("/")) {
        // 添加 /public 前缀，因为服务器静态文件在 /public 目录下
        fullUrl = QString(CLOUD_API_URL) + "/public" + iconUrl;
    } else if (!iconUrl.startsWith("http")) {
        fullUrl = QString(CLOUD_API_URL) + "/public/" + iconUrl;
    }

    QUrl url(fullUrl);
    QNetworkRequest netRequest;
    netRequest.setUrl(url);
    QNetworkReply* reply = m_networkManager->get(netRequest);
    m_pendingIconRequests[fullUrl] = iconLabel;

    connect(reply, &QNetworkReply::finished, this, [this, reply, fullUrl]() {
        QLabel* label = m_pendingIconRequests.value(fullUrl);
        if (label && reply->error() == QNetworkReply::NoError) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reply->readAll())) {
                QPixmap scaled = pixmap.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                label->setPixmap(scaled);
                label->setText("");
            } else {
                label->setText("📦");
            }
        } else if (label) {
            label->setText("📦");
        }
        m_pendingIconRequests.remove(fullUrl);
        reply->deleteLater();
    });
}

void RecommendAppWidget::loadIconLarge(const QString& iconUrl, QLabel* iconLabel)
{
    if (iconUrl.isEmpty()) {
        iconLabel->setText("📦");
        return;
    }

    QString fullUrl = iconUrl;
    if (iconUrl.startsWith("/")) {
        // 添加 /public 前缀，因为服务器静态文件在 /public 目录下
        fullUrl = QString(CLOUD_API_URL) + "/public" + iconUrl;
    } else if (!iconUrl.startsWith("http")) {
        fullUrl = QString(CLOUD_API_URL) + "/public/" + iconUrl;
    }

    QUrl url(fullUrl);
    QNetworkRequest netRequest;
    netRequest.setUrl(url);
    QNetworkReply* reply = m_networkManager->get(netRequest);

    connect(reply, &QNetworkReply::finished, this, [this, reply, iconLabel]() {
        if (reply->error() == QNetworkReply::NoError) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reply->readAll())) {
                // 详情对话框使用更大的图标
                QPixmap scaled = pixmap.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                iconLabel->setPixmap(scaled);
                iconLabel->setText("");
            } else {
                iconLabel->setText("📦");
            }
        } else {
            iconLabel->setText("📦");
        }
        reply->deleteLater();
    });
}

void RecommendAppWidget::onIconLoaded(QNetworkReply* reply)
{
    reply->deleteLater();
}

void RecommendAppWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout;

    QLabel* categoryLabel = new QLabel("分类:");
    toolbarLayout->addWidget(categoryLabel);

    m_categoryComboBox = new QComboBox;
    m_categoryComboBox->setMinimumWidth(150);
    toolbarLayout->addWidget(m_categoryComboBox);

    toolbarLayout->addStretch();

    m_refreshBtn = new QPushButton("刷新");
    toolbarLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(toolbarLayout);

    // 滚动区域
    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");
    scrollArea->setAlignment(Qt::AlignCenter);

    m_scrollAreaWidgetContents = new QWidget;
    m_cardsLayout = new QGridLayout(m_scrollAreaWidgetContents);
    m_cardsLayout->setSpacing(20);
    m_cardsLayout->setContentsMargins(10, 10, 10, 10);

    scrollArea->setWidget(m_scrollAreaWidgetContents);
    mainLayout->addWidget(scrollArea);

    // 连接信号
    connect(m_refreshBtn, &QPushButton::clicked, this, &RecommendAppWidget::onRefreshClicked);
    connect(m_categoryComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecommendAppWidget::onCategoryChanged);
}

void RecommendAppWidget::loadApps()
{
    m_allApps = RecommendedAppsCache::instance()->getApps();

    // 更新分类下拉框
    m_categoryComboBox->clear();
    m_categoryComboBox->addItem("全部");
    for (const QString& cat : RecommendedAppsCache::instance()->getCategories()) {
        m_categoryComboBox->addItem(cat);
    }

    refreshCards(m_allApps);
}

void RecommendAppWidget::onRefreshClicked()
{
    RecommendedAppsCache::instance()->refreshFromServer();
}

void RecommendAppWidget::onCategoryChanged(int index)
{
    QString category = m_categoryComboBox->itemText(index);
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

    // 加载图标
    if (!app.iconUrl.isEmpty()) {
        loadIconLarge(app.iconUrl, iconLabel);
    }

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

    // 如果没有应用，显示提示信息
    if (apps.isEmpty()) {
        QLabel* emptyLabel = new QLabel("暂无推荐应用，请稍后刷新或联系管理员添加");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #888; font-size: 14px; padding: 50px;");
        m_cardsLayout->addWidget(emptyLabel, 0, 0, 1, 3);
        return;
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

    // 异步加载图标
    if (!app.iconUrl.isEmpty()) {
        loadIcon(app.iconUrl, iconLabel);
    }

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
