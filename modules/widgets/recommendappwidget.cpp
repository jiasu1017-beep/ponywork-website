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
    , m_scrollArea(nullptr)
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
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; } QScrollBar:vertical { width: 6px; background: transparent; } QScrollBar::handle:vertical { background: #ccc; border-radius: 3px; min-height: 20px; } QScrollBar::handle:vertical:hover { background: #aaa; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    m_scrollArea->setAlignment(Qt::AlignCenter);

    m_scrollAreaWidgetContents = new QWidget;
    m_cardsLayout = new QGridLayout(m_scrollAreaWidgetContents);
    m_cardsLayout->setSpacing(12);
    m_cardsLayout->setContentsMargins(15, 10, 15, 10);

    m_scrollArea->setWidget(m_scrollAreaWidgetContents);
    mainLayout->addWidget(m_scrollArea);

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
    dlg.setMinimumSize(420, 550);
    dlg.setStyleSheet("QDialog { background: #f0f2f5; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 顶部卡片 - 图标、名称、分类
    QWidget* headerCard = new QWidget;
    headerCard->setStyleSheet("QWidget { background: white; border-radius: 12px; border: 1px solid #dce0e8; }");
    QVBoxLayout* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setSpacing(10);
    headerLayout->setContentsMargins(20, 20, 20, 20);

    // 图标
    QLabel* iconLabel = new QLabel;
    iconLabel->setFixedSize(100, 100);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setText("📦");
    headerLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);

    // 加载图标
    if (!app.iconUrl.isEmpty()) {
        loadIconLarge(app.iconUrl, iconLabel);
    }

    // 名称
    QLabel* nameLabel = new QLabel(app.name);
    nameLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333;");
    nameLabel->setAlignment(Qt::AlignHCenter);
    headerLayout->addWidget(nameLabel);

    // 分类标签
    QLabel* catLabel = new QLabel(app.category);
    catLabel->setStyleSheet("color: white; background: #4A90D9; border-radius: 10px; padding: 4px 16px; font-size: 12px;");
    catLabel->setAlignment(Qt::AlignHCenter);
    headerLayout->addWidget(catLabel, 0, Qt::AlignHCenter);

    mainLayout->addWidget(headerCard);

    // 简介区域 - 可滚动
    QWidget* descCard = new QWidget;
    descCard->setStyleSheet("QWidget { background: white; border-radius: 12px; border: 1px solid #dce0e8; }");
    QVBoxLayout* descLayout = new QVBoxLayout(descCard);
    descLayout->setContentsMargins(15, 15, 15, 15);

    QLabel* descTitle = new QLabel("应用简介");
    descTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 8px;");
    descLayout->addWidget(descTitle);

    QScrollArea* descScrollArea = new QScrollArea;
    descScrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    descScrollArea->setWidgetResizable(true);
    descScrollArea->setFixedHeight(80);

    QLabel* descText = new QLabel(app.description);
    descText->setStyleSheet("font-size: 13px; color: #666; line-height: 1.6;");
    descText->setWordWrap(true);
    descScrollArea->setWidget(descText);
    descLayout->addWidget(descScrollArea);

    mainLayout->addWidget(descCard, 1); // stretch factor = 1 可占用剩余空间

    // 下载地址区域
    QWidget* dlCard = new QWidget;
    dlCard->setStyleSheet("QWidget { background: white; border-radius: 12px; border: 1px solid #dce0e8; }");
    QVBoxLayout* dlLayout = new QVBoxLayout(dlCard);
    dlLayout->setContentsMargins(15, 15, 15, 15);

    QLabel* dlTitle = new QLabel("下载地址");
    dlTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; margin-bottom: 10px;");
    dlLayout->addWidget(dlTitle);

    for (const auto& dl : app.downloads) {
        QWidget* dlItem = new QWidget;
        dlItem->setStyleSheet("QWidget { background: #f8f9fa; border-radius: 8px; padding: 10px; margin-bottom: 8px; }");
        QHBoxLayout* dlItemLayout = new QHBoxLayout(dlItem);
        dlItemLayout->setContentsMargins(12, 8, 12, 8);

        // 平台名称
        QLabel* platformLabel = new QLabel(dl.name);
        platformLabel->setStyleSheet("font-size: 13px; color: #333; font-weight: 500;");
        dlItemLayout->addWidget(platformLabel);

        dlItemLayout->addStretch();

        // 打开按钮
        QPushButton* openBtn = new QPushButton("打开");
        openBtn->setStyleSheet("QPushButton { background: #4A90D9; color: white; border: none; border-radius: 6px; padding: 6px 16px; font-size: 12px; } QPushButton:hover { background: #357ABD; }");
        QString url = dl.url;
        connect(openBtn, &QPushButton::clicked, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        dlItemLayout->addWidget(openBtn);

        // 复制按钮
        QPushButton* copyBtn = new QPushButton("复制");
        copyBtn->setStyleSheet("QPushButton { background: #6c757d; color: white; border: none; border-radius: 6px; padding: 6px 16px; font-size: 12px; } QPushButton:hover { background: #5a6268; }");
        connect(copyBtn, &QPushButton::clicked, [url]() {
            QApplication::clipboard()->setText(url);
        });
        dlItemLayout->addWidget(copyBtn);

        dlLayout->addWidget(dlItem);
    }

    mainLayout->addWidget(dlCard);

    // 关闭按钮
    QPushButton* closeBtn = new QPushButton("关闭");
    closeBtn->setStyleSheet("QPushButton { background: #e0e0e0; color: #333; border: none; border-radius: 8px; padding: 10px 40px; font-size: 14px; } QPushButton:hover { background: #d0d0d0; }");
    mainLayout->addWidget(closeBtn, 0, Qt::AlignHCenter);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

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
        m_cardsLayout->addWidget(emptyLabel, 0, 0, 1, 1);
        return;
    }

    // 根据滚动区域内容宽度动态计算每行显示数量
    // 卡片宽度200 + 间距12
    int cardWidth = 200;
    int spacing = 12;
    int contentsWidth = m_scrollArea->viewport()->width();
    // 减去左右边距和边距
    int availableWidth = contentsWidth > 0 ? contentsWidth - 30 : width() - 100;
    int maxCols = qMax(1, availableWidth / (cardWidth + spacing));

    // 创建新卡片
    int row = 0, col = 0;
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
    card->setFixedSize(200, 200);
    card->setStyleSheet("QWidget { border: 1px solid #e0e0e0; border-radius: 8px; background: white; } QWidget:hover { border-color: #4A90D9; background: #f8f9fa; }");
    card->setCursor(Qt::PointingHandCursor);
    card->installEventFilter(this);

    // 保存 app id
    card->setProperty("appId", app.id);

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setSpacing(6);
    layout->setContentsMargins(10, 10, 10, 10);

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
    nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    nameLabel->setWordWrap(true);
    nameLabel->setFixedHeight(20);
    layout->addWidget(nameLabel);

    // 分类
    QLabel* catLabel = new QLabel(app.category);
    catLabel->setAlignment(Qt::AlignCenter);
    catLabel->setStyleSheet("color: #4A90D9; font-size: 12px;");
    layout->addWidget(catLabel);

    // 简介 - 可滚动
    QScrollArea* descScroll = new QScrollArea;
    descScroll->setStyleSheet("QScrollArea { border: none; background: transparent; } QScrollBar:vertical { width: 4px; background: transparent; } QScrollBar::handle:vertical { background: #ccc; border-radius: 2px; min-height: 15px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    descScroll->setWidgetResizable(true);
    descScroll->setFixedHeight(36);

    QLabel* descLabel = new QLabel(app.description);
    descLabel->setStyleSheet("font-size: 12px; color: #666; background: transparent; padding: 2px;");
    descLabel->setWordWrap(true);
    descScroll->setWidget(descLabel);
    layout->addWidget(descScroll);

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

void RecommendAppWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 窗口大小变化时重新布局卡片
    if (!m_allApps.isEmpty()) {
        refreshCards(m_allApps);
    }
}
