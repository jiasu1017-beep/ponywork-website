#include "settingswidget.h"
#include "mainwindow.h"
#include "modules/dialogs/shortcutdialog.h"
#include "modules/dialogs/aisettingsdialog.h"
#include "modules/dialogs/chattestdialog.h"
#include "modules/dialogs/aicongeneratordialog.h"
#include "modules/user/userlogindialog.h"
#include "modules/user/userapi.h"
#include "modules/core/aiconfig.h"
#include "modules/core/database.h"
#include <QApplication>
#include <QStyle>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPixmap>
#include <QScrollArea>
#include <QMessageBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QCryptographicHash>
#include <QHostInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QTimer>
#include <QMap>
#include <QUrl>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include "modules/update/updatedialog.h"
#include "modules/update/updateprogressdialog.h"
#include "modules/core/ailogger.h"

SettingsWidget::SettingsWidget(Database *db, QWidget *parent)
    : QWidget(parent), db(db), mainWindow(nullptr), updateManager(nullptr), progressDialog(nullptr), networkManager(nullptr)
{
    setupUI();
}

SettingsWidget::~SettingsWidget()
{
}

void SettingsWidget::setUpdateManager(UpdateManager *manager)
{
    updateManager = manager;
    
    if (updateManager) {
        connect(updateManager, &UpdateManager::noUpdateAvailable, this, &SettingsWidget::onNoUpdateAvailable);
        connect(updateManager, &UpdateManager::updateCheckFailed, this, &SettingsWidget::onUpdateCheckFailed);
    }
}

void SettingsWidget::setMainWindow(MainWindow *mw)
{
    mainWindow = mw;
}

void SettingsWidget::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget *leftWidget = new QWidget(this);
    leftWidget->setFixedWidth(220);
    leftWidget->setStyleSheet("background-color: #f5f5f5;");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(10, 10, 10, 10);  // 减少边距
    leftLayout->setSpacing(2);  // 减少间距

    QLabel *titleLabel = new QLabel("设置", leftWidget);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; padding: 5px 15px; color: #333;");
    leftLayout->addWidget(titleLabel);

    QListWidget *listWidget = new QListWidget(leftWidget);
    listWidget->setFrameShape(QListWidget::NoFrame);
    listWidget->setBackgroundRole(QPalette::NoRole);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // 禁用滚动条
    listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 自动扩展
    listWidget->setStyleSheet(
        "QListWidget { border: none; background: transparent; } "
        "QListWidget::item { padding: 10px 15px; border-radius: 6px; margin: 1px 5px; color: #555; } "
        "QListWidget::item:selected { background-color: #e3f2fd; color: #1976d2; font-weight: bold; } "
        "QListWidget::item:hover { background-color: #eeeeee; }"
    );

    QListWidgetItem *generalItem = new QListWidgetItem("🔧 通用设置", listWidget);
    generalItem->setData(Qt::UserRole, 0);
    
    QListWidgetItem *shortcutItem = new QListWidgetItem("⌨️ 快捷键", listWidget);
    shortcutItem->setData(Qt::UserRole, 1);
    
    QListWidgetItem *aiItem = new QListWidgetItem("🤖 AI设置", listWidget);
    aiItem->setData(Qt::UserRole, 2);
    
    QListWidgetItem *startupItem = new QListWidgetItem("🚀 开机启动", listWidget);
    startupItem->setData(Qt::UserRole, 3);
    
    QListWidgetItem *updateItem = new QListWidgetItem("🔄 检查更新", listWidget);
    updateItem->setData(Qt::UserRole, 4);

    QListWidgetItem *remoteDesktopItem = new QListWidgetItem("🖥️ 远程桌面", listWidget);
    remoteDesktopItem->setData(Qt::UserRole, 5);

    QListWidgetItem *aboutItem = new QListWidgetItem("ℹ️ 关于", listWidget);
    aboutItem->setData(Qt::UserRole, 6);

    listWidget->setCurrentRow(0);
    leftLayout->addWidget(listWidget, 1);  // stretch factor = 1，让listWidget自动扩展

    QStackedWidget *stackWidget = new QStackedWidget(this);
    stackWidget->setStyleSheet("background-color: white;");

    QWidget *generalPage = createGeneralPage();
    QWidget *shortcutPage = createShortcutPage();
    QWidget *aiPage = createAIPage();
    QWidget *startupPage = createStartupPage();
    QWidget *updatePage = createUpdatePage();
    QWidget *remoteDesktopPage = createRemoteDesktopPage();
    QWidget *aboutPage = createAboutPage();

    stackWidget->addWidget(generalPage);
    stackWidget->addWidget(shortcutPage);
    stackWidget->addWidget(aiPage);
    stackWidget->addWidget(startupPage);
    stackWidget->addWidget(updatePage);
    stackWidget->addWidget(remoteDesktopPage);
    stackWidget->addWidget(aboutPage);

    connect(listWidget, &QListWidget::currentRowChanged, stackWidget, &QStackedWidget::setCurrentIndex);

    mainLayout->addWidget(leftWidget);
    mainLayout->addWidget(stackWidget, 1);

    QFrame *splitter = new QFrame(this);
    splitter->setFrameShape(QFrame::VLine);
    splitter->setStyleSheet("color: #e0e0e0;");
    mainLayout->addWidget(splitter);
}

QWidget *SettingsWidget::createGeneralPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("通用设置", page);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QGroupBox *closeGroup = new QGroupBox("关闭行为", page);
    QVBoxLayout *closeLayout = new QVBoxLayout(closeGroup);
    closeLayout->setSpacing(10);

    minimizeToTrayCheck = new QCheckBox("启用最小化到系统托盘", page);
    minimizeToTrayCheck->setChecked(db->getMinimizeToTray());
    connect(minimizeToTrayCheck, &QCheckBox::stateChanged, this, &SettingsWidget::onMinimizeToTrayToggled);
    closeLayout->addWidget(minimizeToTrayCheck);

    showClosePromptCheck = new QCheckBox("关闭窗口时显示提示", page);
    showClosePromptCheck->setChecked(db->getShowClosePrompt());
    connect(showClosePromptCheck, &QCheckBox::stateChanged, this, &SettingsWidget::onShowClosePromptToggled);
    closeLayout->addWidget(showClosePromptCheck);

    QLabel *closeHint = new QLabel("当前关闭行为: " + QString(db->getMinimizeToTray() ? "最小化到系统托盘" : "直接退出程序"), page);
    closeHint->setStyleSheet("color: #666; font-size: 12px; padding: 5px;");
    closeLayout->addWidget(closeHint);

    layout->addWidget(closeGroup);

    QGroupBox *appearanceGroup = new QGroupBox("外观", page);
    QVBoxLayout *appearLayout = new QVBoxLayout(appearanceGroup);
    appearLayout->setSpacing(10);

    showBottomAppBarCheck = new QCheckBox("显示快捷应用条", page);
    showBottomAppBarCheck->setChecked(db->getShowBottomAppBar());
    showBottomAppBarCheck->setToolTip("控制是否在窗口底部显示快捷应用条");
    showBottomAppBarCheck->setAccessibleName("显示快捷应用条开关");
    showBottomAppBarCheck->setAccessibleDescription("开启或关闭底部快捷应用条的显示");
    connect(showBottomAppBarCheck, &QCheckBox::stateChanged, this, &SettingsWidget::onShowBottomAppBarToggled);
    appearLayout->addWidget(showBottomAppBarCheck);

    QHBoxLayout *themeLayout = new QHBoxLayout();
    QLabel *themeLabel = new QLabel("主题:", page);
    QComboBox *themeCombo = new QComboBox(page);
    themeCombo->addItems({"浅色", "深色", "跟随系统"});
    themeCombo->setCurrentText("浅色");
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(themeCombo);
    themeLayout->addStretch();
    appearLayout->addLayout(themeLayout);

    QLabel *themeHint = new QLabel("主题切换功能开发中...", page);
    themeHint->setStyleSheet("color: #999; font-size: 12px; padding: 5px;");
    appearLayout->addWidget(themeHint);

    layout->addWidget(appearanceGroup);

    QGroupBox *trayGroup = new QGroupBox("系统托盘", page);
    QVBoxLayout *trayLayout = new QVBoxLayout(trayGroup);
    trayLayout->setSpacing(10);

    QCheckBox *showTrayIcon = new QCheckBox("显示托盘图标", trayGroup);
    showTrayIcon->setChecked(true);
    trayLayout->addWidget(showTrayIcon);

    QCheckBox *showTrayTooltip = new QCheckBox("显示托盘提示", trayGroup);
    showTrayTooltip->setChecked(true);
    trayLayout->addWidget(showTrayTooltip);

    layout->addWidget(trayGroup);

    layout->addStretch();
    return page;
}

QWidget *SettingsWidget::createShortcutPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("快捷键设置", page);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QGroupBox *globalShortcutGroup = new QGroupBox("全局快捷键", page);
    QVBoxLayout *shortcutLayout = new QVBoxLayout(globalShortcutGroup);
    shortcutLayout->setSpacing(15);

    QLabel *descLabel = new QLabel("设置用于调出窗口的全局快捷键，软件在后台运行时也能响应。", page);
    descLabel->setStyleSheet("color: #666; font-size: 13px; padding: 5px;");
    descLabel->setWordWrap(true);
    shortcutLayout->addWidget(descLabel);

    QHBoxLayout *shortcutInputLayout = new QHBoxLayout();
    QLabel *shortcutLabel = new QLabel("快捷键:", page);
    QLabel *currentShortcutLabel = new QLabel(db->getShortcutKey(), page);
    currentShortcutLabel->setStyleSheet(
        "padding: 8px 15px; border: 1px solid #ddd; border-radius: 5px; "
        "background-color: #f9f9f9; font-weight: bold; min-width: 100px;"
    );

    QPushButton *changeShortcutBtn = new QPushButton("修改", page);
    changeShortcutBtn->setStyleSheet(
        "QPushButton { background-color: #2196f3; color: white; padding: 8px 20px; border-radius: 5px; } "
        "QPushButton:hover { background-color: #1976d2; }"
    );
    connect(changeShortcutBtn, &QPushButton::clicked, this, [this, currentShortcutLabel]() {
        ShortcutDialog dialog(db, this);
        dialog.setShortcut(QKeySequence(db->getShortcutKey()));
        
        if (mainWindow && !mainWindow->isVisible()) {
            mainWindow->show();
            mainWindow->activateWindow();
        }
        
        if (dialog.exec() == QDialog::Accepted) {
            QKeySequence newShortcut = dialog.getShortcut();
            if (!newShortcut.isEmpty()) {
                QString shortcutStr = newShortcut.toString();
                if (db->setShortcutKey(shortcutStr)) {
                    currentShortcutLabel->setText(shortcutStr);
                    QMessageBox::information(this, "成功", "快捷键已更新: " + shortcutStr);
                    
                    if (mainWindow) {
                        mainWindow->refreshGlobalShortcut();
                    }
                } else {
                    QMessageBox::warning(this, "错误", "保存快捷键失败！");
                }
            }
        }
    });

    QPushButton *resetShortcutBtn = new QPushButton("重置为默认", page);
    resetShortcutBtn->setStyleSheet(
        "QPushButton { background-color: #9e9e9e; color: white; padding: 8px 20px; border-radius: 5px; } "
        "QPushButton:hover { background-color: #757575; }"
    );
    connect(resetShortcutBtn, &QPushButton::clicked, this, [this, currentShortcutLabel]() {
        if (db->setShortcutKey("Ctrl+W")) {
            currentShortcutLabel->setText("Ctrl+W");
            QMessageBox::information(this, "成功", "快捷键已重置为默认: Ctrl+W");
            
            if (mainWindow) {
                mainWindow->refreshGlobalShortcut();
            }
        }
    });

    shortcutInputLayout->addWidget(shortcutLabel);
    shortcutInputLayout->addWidget(currentShortcutLabel);
    shortcutInputLayout->addWidget(changeShortcutBtn);
    shortcutInputLayout->addWidget(resetShortcutBtn);
    shortcutInputLayout->addStretch();
    shortcutLayout->addLayout(shortcutInputLayout);

    QLabel *hintLabel = new QLabel("💡 提示: 快捷键在软件最小化或处于后台时仍然有效。窗口激活时按快捷键会最小化窗口。", page);
    hintLabel->setStyleSheet("color: #888; font-size: 12px; padding: 10px; background-color: #f5f5f5; border-radius: 5px;");
    hintLabel->setWordWrap(true);
    shortcutLayout->addWidget(hintLabel);

    layout->addWidget(globalShortcutGroup);

    layout->addStretch();
    return page;
}

QWidget *SettingsWidget::createAIPage()
{
    QWidget *page = new QWidget();
    QScrollArea *scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *contentWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("AI设置", contentWidget);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(contentWidget);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QTabWidget *aiTabWidget = new QTabWidget(contentWidget);
    aiTabWidget->setDocumentMode(true);

    QWidget *textAiTab = new QWidget();
    QVBoxLayout *textAiLayout = new QVBoxLayout(textAiTab);
    textAiLayout->setSpacing(10);

    QLabel *textAiDesc = new QLabel("管理多个文本AI模型的API密钥，支持同时配置和使用多个不同的大模型服务", textAiTab);
    textAiDesc->setStyleSheet("color: #666; font-size: 13px; padding: 5px;");
    textAiDesc->setWordWrap(true);
    textAiLayout->addWidget(textAiDesc);

    aiKeysTable = new QTableWidget(textAiTab);
    aiKeysTable->setColumnCount(7);
    aiKeysTable->setHorizontalHeaderLabels({"默认", "名称", "服务商", "模型", "API Key", "API地址", "状态"});
    aiKeysTable->horizontalHeader()->setStretchLastSection(true);
    aiKeysTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    aiKeysTable->setSelectionMode(QAbstractItemView::SingleSelection);
    aiKeysTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    aiKeysTable->setAlternatingRowColors(true);
    aiKeysTable->setMinimumHeight(250);
    connect(aiKeysTable, &QTableWidget::itemSelectionChanged, this, &SettingsWidget::onAIKeyTableSelectionChanged);
    textAiLayout->addWidget(aiKeysTable);

    QWidget *textAiButtonContainer = new QWidget(textAiTab);
    QHBoxLayout *textAiButtonLayout = new QHBoxLayout(textAiButtonContainer);
    textAiButtonContainer->setStyleSheet("QWidget { margin-top: 10px; }");
    textAiButtonLayout->setSpacing(10);

    addAIKeyBtn = new QPushButton("➕ 添加", textAiButtonContainer);
    addAIKeyBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #2980b9; }"
    );
    connect(addAIKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onAddAIKey);
    textAiButtonLayout->addWidget(addAIKeyBtn);

    editAIKeyBtn = new QPushButton("✏️ 编辑", textAiButtonContainer);
    editAIKeyBtn->setStyleSheet(
        "QPushButton { background-color: #f39c12; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #e67e22; }"
    );
    editAIKeyBtn->setEnabled(false);
    connect(editAIKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onEditAIKey);
    textAiButtonLayout->addWidget(editAIKeyBtn);

    deleteAIKeyBtn = new QPushButton("🗑️ 删除", textAiButtonContainer);
    deleteAIKeyBtn->setStyleSheet(
        "QPushButton { background-color: #e74c3c; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #c0392b; }"
    );
    deleteAIKeyBtn->setEnabled(false);
    connect(deleteAIKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onDeleteAIKey);
    textAiButtonLayout->addWidget(deleteAIKeyBtn);

    setDefaultAIKeyBtn = new QPushButton("⭐ 设为默认", textAiButtonContainer);
    setDefaultAIKeyBtn->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #8e44ad; }"
    );
    setDefaultAIKeyBtn->setEnabled(false);
    connect(setDefaultAIKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onSetDefaultAIKey);
    textAiButtonLayout->addWidget(setDefaultAIKeyBtn);

    QPushButton *openAISettingsBtn = new QPushButton("📖 AI配置指南", textAiButtonContainer);
    openAISettingsBtn->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #8e44ad; }"
    );
    openAISettingsBtn->setToolTip("打开完整的AI设置对话框，包含图像生成配置");
    connect(openAISettingsBtn, &QPushButton::clicked, this, &SettingsWidget::onOpenAISettings);
    textAiButtonLayout->addWidget(openAISettingsBtn);

    chatTestBtn = new QPushButton("💬 AI对话测试", textAiButtonContainer);
    chatTestBtn->setStyleSheet(
        "QPushButton { background-color: #e67e22; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #d35400; }"
    );
    connect(chatTestBtn, &QPushButton::clicked, this, &SettingsWidget::onChatTestClicked);
    textAiButtonLayout->addWidget(chatTestBtn);

    textAiButtonLayout->addStretch();
    textAiLayout->addWidget(textAiButtonContainer);

    QWidget *imageAiTab = new QWidget();
    QVBoxLayout *imageAiLayout = new QVBoxLayout(imageAiTab);
    imageAiLayout->setSpacing(10);

    QLabel *imageAiDesc = new QLabel("管理图像生成API密钥，用于AI图标生成功能", imageAiTab);
    imageAiDesc->setStyleSheet("color: #666; font-size: 13px; padding: 5px;");
    imageAiDesc->setWordWrap(true);
    imageAiLayout->addWidget(imageAiDesc);

    aiImageKeysTable = new QTableWidget(imageAiTab);
    aiImageKeysTable->setColumnCount(7);
    aiImageKeysTable->setHorizontalHeaderLabels({"默认", "名称", "服务商", "模型", "API Key", "API地址", "状态"});
    aiImageKeysTable->horizontalHeader()->setStretchLastSection(true);
    aiImageKeysTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    aiImageKeysTable->setSelectionMode(QAbstractItemView::SingleSelection);
    aiImageKeysTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    aiImageKeysTable->setAlternatingRowColors(true);
    aiImageKeysTable->setMinimumHeight(250);
    connect(aiImageKeysTable, &QTableWidget::itemSelectionChanged, this, &SettingsWidget::onAIImageKeyTableSelectionChanged);
    imageAiLayout->addWidget(aiImageKeysTable);

    QWidget *imageAiButtonContainer = new QWidget(imageAiTab);
    QHBoxLayout *imageAiButtonLayout = new QHBoxLayout(imageAiButtonContainer);
    imageAiButtonContainer->setStyleSheet("QWidget { margin-top: 10px; }");
    imageAiButtonLayout->setSpacing(10);

    addImageKeyBtn = new QPushButton("➕ 添加", imageAiButtonContainer);
    addImageKeyBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #2980b9; }"
    );
    connect(addImageKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onAddImageKey);
    imageAiButtonLayout->addWidget(addImageKeyBtn);

    editImageKeyBtn = new QPushButton("✏️ 编辑", imageAiButtonContainer);
    editImageKeyBtn->setStyleSheet(
        "QPushButton { background-color: #f39c12; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #e67e22; }"
    );
    editImageKeyBtn->setEnabled(false);
    connect(editImageKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onEditImageKey);
    imageAiButtonLayout->addWidget(editImageKeyBtn);

    deleteImageKeyBtn = new QPushButton("🗑️ 删除", imageAiButtonContainer);
    deleteImageKeyBtn->setStyleSheet(
        "QPushButton { background-color: #e74c3c; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #c0392b; }"
    );
    deleteImageKeyBtn->setEnabled(false);
    connect(deleteImageKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onDeleteImageKey);
    imageAiButtonLayout->addWidget(deleteImageKeyBtn);

    setDefaultImageKeyBtn = new QPushButton("⭐ 设为默认", imageAiButtonContainer);
    setDefaultImageKeyBtn->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #8e44ad; }"
    );
    setDefaultImageKeyBtn->setEnabled(false);
    connect(setDefaultImageKeyBtn, &QPushButton::clicked, this, &SettingsWidget::onSetDefaultImageKey);
    imageAiButtonLayout->addWidget(setDefaultImageKeyBtn);

    QPushButton *iconGenTestBtn = new QPushButton("🎨 图标生成测试", imageAiButtonContainer);
    iconGenTestBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; padding: 8px 16px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #229954; }"
    );
    connect(iconGenTestBtn, &QPushButton::clicked, this, &SettingsWidget::onIconGenTestClicked);
    imageAiButtonLayout->addWidget(iconGenTestBtn);

    imageAiButtonLayout->addStretch();
    imageAiLayout->addWidget(imageAiButtonContainer);

    aiTabWidget->addTab(textAiTab, "📝 文本AI");
    aiTabWidget->addTab(imageAiTab, "🎨 图像AI");
    layout->addWidget(aiTabWidget);

    QGroupBox *aiFeaturesGroup = new QGroupBox("AI功能开关", contentWidget);
    QVBoxLayout *aiFeaturesLayout = new QVBoxLayout(aiFeaturesGroup);
    aiFeaturesLayout->setSpacing(10);

    QCheckBox *taskAnalysisCheck = new QCheckBox("启用智能任务分析", aiFeaturesGroup);
    taskAnalysisCheck->setChecked(AIConfig::instance().isEnabled("task_analysis"));
    taskAnalysisCheck->setToolTip("在新建任务时启用AI分析功能");
    connect(taskAnalysisCheck, &QCheckBox::toggled, [](bool checked) {
        AIConfig::instance().setEnabled("task_analysis", checked);
    });
    aiFeaturesLayout->addWidget(taskAnalysisCheck);

    QCheckBox *reportGenCheck = new QCheckBox("启用AI报告生成", aiFeaturesGroup);
    reportGenCheck->setChecked(AIConfig::instance().isEnabled("report_generation"));
    reportGenCheck->setToolTip("在生成周报/月报/季报时启用AI内容生成");
    connect(reportGenCheck, &QCheckBox::toggled, [](bool checked) {
        AIConfig::instance().setEnabled("report_generation", checked);
    });
    aiFeaturesLayout->addWidget(reportGenCheck);

    QCheckBox *autoSuggestCheck = new QCheckBox("启用智能建议", aiFeaturesGroup);
    autoSuggestCheck->setChecked(AIConfig::instance().isEnabled("auto_suggest"));
    autoSuggestCheck->setToolTip("根据工作日志智能推荐下一步操作");
    connect(autoSuggestCheck, &QCheckBox::toggled, [](bool checked) {
        AIConfig::instance().setEnabled("auto_suggest", checked);
    });
    aiFeaturesLayout->addWidget(autoSuggestCheck);

    layout->addWidget(aiFeaturesGroup);

    QGroupBox *aiTimeoutGroup = new QGroupBox("AI超时设置", contentWidget);
    QHBoxLayout *aiTimeoutLayout = new QHBoxLayout(aiTimeoutGroup);

    QLabel *timeoutLabel = new QLabel("响应超时时间(秒):", aiTimeoutGroup);
    aiTimeoutLayout->addWidget(timeoutLabel);

    QSpinBox *timeoutSpinBox = new QSpinBox(aiTimeoutGroup);
    timeoutSpinBox->setRange(10, 300);
    timeoutSpinBox->setValue(AIConfig::instance().getTimeout());
    timeoutSpinBox->setSuffix(" 秒");
    timeoutSpinBox->setToolTip("AI请求的最大等待时间，超时后将取消请求");
    connect(timeoutSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [](int value) {
        AIConfig::instance().setTimeout(value);
    });
    aiTimeoutLayout->addWidget(timeoutSpinBox);

    aiTimeoutLayout->addStretch();
    layout->addWidget(aiTimeoutGroup);

    aiStatusLabel = new QLabel(contentWidget);
    aiStatusLabel->setAlignment(Qt::AlignCenter);
    aiStatusLabel->setStyleSheet("padding: 8px; border-radius: 4px;");
    aiStatusLabel->setVisible(false);
    layout->addWidget(aiStatusLabel);

    QLabel *aiHelpLabel = new QLabel(
        "📖 API Key获取指南:\n"
        "• MiniMax: https://platform.minimaxi.com\n"
        "• DeepSeek: https://siliconflow.cn (推荐，免费100万tokens)\n"
        "• 通义千问: https://dashscope.aliyun.com\n"
        "• 讯飞星火: https://console.xfyun.cn\n"
        "• OpenAI: https://platform.openai.com (需代理)\n"
        "• Claude: https://console.anthropic.com (需代理)\n"
        "• Gemini: https://aistudio.google.com/app/apikey (需代理)\n\n"
        "💡 安全提示: API Key将加密存储在本地配置文件中"
    );
    aiHelpLabel->setStyleSheet("padding: 12px; color: #666; font-size: 11px; background-color: #f5f5f5; border-radius: 5px;");
    aiHelpLabel->setWordWrap(true);
    layout->addWidget(aiHelpLabel);

    layout->addStretch();

    scrollArea->setWidget(contentWidget);

    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);

    loadAISettings();

    return page;
}

void SettingsWidget::loadAISettings()
{
    aiKeysTable->setRowCount(0);

    QList<AIKeyConfig> keys = AIConfig::instance().getAllKeys();
    for (const AIKeyConfig &key : keys) {
        int row = aiKeysTable->rowCount();
        aiKeysTable->insertRow(row);

        QTableWidgetItem *defaultItem = new QTableWidgetItem(key.isDefault ? "⭐ 默认" : "");
        defaultItem->setTextAlignment(Qt::AlignCenter);
        defaultItem->setData(Qt::UserRole, key.id);
        aiKeysTable->setItem(row, 0, defaultItem);

        QTableWidgetItem *nameItem = new QTableWidgetItem(key.name);
        nameItem->setData(Qt::UserRole, key.id);
        aiKeysTable->setItem(row, 1, nameItem);

        QString providerDisplay = key.provider;
        QList<AIProviderInfo> providers = AIConfig::instance().getProviders();
        for (const AIProviderInfo &p : providers) {
            if (p.id == key.provider) {
                providerDisplay = p.displayName;
                break;
            }
        }
        QTableWidgetItem *providerItem = new QTableWidgetItem(providerDisplay);
        providerItem->setData(Qt::UserRole, key.id);
        aiKeysTable->setItem(row, 2, providerItem);

        AIModelInfo modelInfo = AIConfig::instance().getModelInfo(key.model);
        QString modelDisplay = modelInfo.name.isEmpty() ? key.model : modelInfo.name;
        QTableWidgetItem *modelItem = new QTableWidgetItem(modelDisplay);
        modelItem->setData(Qt::UserRole, key.id);
        aiKeysTable->setItem(row, 3, modelItem);

        QString maskedKey = key.apiKey.isEmpty() ? "未配置" : AIConfig::instance().maskAPIKey(key.apiKey);
        QTableWidgetItem *apiKeyItem = new QTableWidgetItem(maskedKey);
        apiKeyItem->setData(Qt::UserRole, key.id);
        if (!key.apiKey.isEmpty()) {
            apiKeyItem->setForeground(QColor("#7f8c8d"));
            apiKeyItem->setToolTip("点击查看完整密钥");
        }
        aiKeysTable->setItem(row, 4, apiKeyItem);

        QString endpointDisplay = key.endpoint.isEmpty() ? "默认" : key.endpoint;
        QTableWidgetItem *endpointItem = new QTableWidgetItem(endpointDisplay);
        endpointItem->setData(Qt::UserRole, key.id);
        if (!key.endpoint.isEmpty()) {
            endpointItem->setForeground(QColor("#7f8c8d"));
        }
        aiKeysTable->setItem(row, 5, endpointItem);

        QString status = key.apiKey.isEmpty() ? "未配置" : "已配置";
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        statusItem->setData(Qt::UserRole, key.id);
        if (key.apiKey.isEmpty()) {
            statusItem->setForeground(QColor("#e74c3c"));
        } else {
            statusItem->setForeground(QColor("#27ae60"));
        }
        aiKeysTable->setItem(row, 6, statusItem);
    }

    aiImageKeysTable->setRowCount(0);

    QList<AIImageConfig> imageKeys = AIConfig::instance().getAllImageKeys();
    for (const AIImageConfig &key : imageKeys) {
        int row = aiImageKeysTable->rowCount();
        aiImageKeysTable->insertRow(row);

        QTableWidgetItem *defaultItem = new QTableWidgetItem(key.isDefault ? "⭐ 默认" : "");
        defaultItem->setTextAlignment(Qt::AlignCenter);
        defaultItem->setData(Qt::UserRole, key.id);
        aiImageKeysTable->setItem(row, 0, defaultItem);

        QTableWidgetItem *nameItem = new QTableWidgetItem(key.name);
        nameItem->setData(Qt::UserRole, key.id);
        aiImageKeysTable->setItem(row, 1, nameItem);

        QString providerDisplay = key.provider;
        QList<AIImageModelInfo> models = AIConfig::instance().getAllImageModels();
        for (const AIImageModelInfo &m : models) {
            if (m.provider == key.provider) {
                AIProviderInfo providerInfo = AIConfig::instance().getProviderInfo(m.provider);
                providerDisplay = providerInfo.displayName.isEmpty() ? key.provider : providerInfo.displayName;
                break;
            }
        }
        QTableWidgetItem *providerItem = new QTableWidgetItem(providerDisplay);
        providerItem->setData(Qt::UserRole, key.id);
        aiImageKeysTable->setItem(row, 2, providerItem);

        QTableWidgetItem *modelItem = new QTableWidgetItem(key.model);
        modelItem->setData(Qt::UserRole, key.id);
        aiImageKeysTable->setItem(row, 3, modelItem);

        QString maskedKey = key.apiKey.isEmpty() ? "未配置" : AIConfig::instance().maskAPIKey(key.apiKey);
        QTableWidgetItem *apiKeyItem = new QTableWidgetItem(maskedKey);
        apiKeyItem->setData(Qt::UserRole, key.id);
        if (!key.apiKey.isEmpty()) {
            apiKeyItem->setForeground(QColor("#7f8c8d"));
            apiKeyItem->setToolTip("点击查看完整密钥");
        }
        aiImageKeysTable->setItem(row, 4, apiKeyItem);

        QString endpointDisplay = key.endpoint.isEmpty() ? "默认" : key.endpoint;
        QTableWidgetItem *endpointItem = new QTableWidgetItem(endpointDisplay);
        endpointItem->setData(Qt::UserRole, key.id);
        if (!key.endpoint.isEmpty()) {
            endpointItem->setForeground(QColor("#7f8c8d"));
        }
        aiImageKeysTable->setItem(row, 5, endpointItem);

        QString status = key.apiKey.isEmpty() ? "未配置" : "已配置";
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        statusItem->setData(Qt::UserRole, key.id);
        if (key.apiKey.isEmpty()) {
            statusItem->setForeground(QColor("#e74c3c"));
        } else {
            statusItem->setForeground(QColor("#27ae60"));
        }
        aiImageKeysTable->setItem(row, 6, statusItem);
    }

    onAIKeyTableSelectionChanged();
    onAIImageKeyTableSelectionChanged();
}

QString SettingsWidget::loadSavedAPIKey()
{
    QSettings settings("PonyWork", "WorkLog");
    QString encryptedKey = settings.value("ai_api_key", "").toString();
    if (encryptedKey.isEmpty()) {
        return "";
    }
    QByteArray data = QByteArray::fromBase64(encryptedKey.toUtf8());
    QByteArray key = QCryptographicHash::hash(QByteArray("PonyWorkAI").append(QHostInfo::localHostName().toUtf8()), QCryptographicHash::Sha256);
    QByteArray decrypted;
    for (int i = 0; i < data.size(); ++i) {
        decrypted.append(data.at(i) ^ key.at(i % key.size()));
    }
    return QString::fromUtf8(decrypted);
}

void SettingsWidget::onAISettingsChanged()
{
    QString model = aiModelCombo->currentData().toString();
    QString endpoint = getDefaultEndpoint(model);
    apiEndpointEdit->setPlaceholderText(endpoint.isEmpty() ? "留空使用默认地址" : endpoint);
}

void SettingsWidget::onSaveAIConfig()
{
    QString model = aiModelCombo->currentData().toString();
    QString apiKey = apiKeyEdit->text().trimmed();
    QString endpoint = apiEndpointEdit->text().trimmed();

    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入API Key");
        return;
    }

    QSettings settings("PonyWork", "WorkLog");
    settings.setValue("ai_model", model);

    QByteArray key = QCryptographicHash::hash(QByteArray("PonyWorkAI").append(QHostInfo::localHostName().toUtf8()), QCryptographicHash::Sha256);
    QByteArray data = apiKey.toUtf8();
    QByteArray encrypted;
    for (int i = 0; i < data.size(); ++i) {
        encrypted.append(data.at(i) ^ key.at(i % key.size()));
    }
    settings.setValue("ai_api_key", QString::fromUtf8(encrypted.toBase64()));

    settings.setValue("ai_endpoint", endpoint);

    aiStatusLabel->setText("✅ 配置已保存");
    aiStatusLabel->setStyleSheet("padding: 8px; color: #27ae60; background-color: #e8f5e9; border-radius: 4px;");
    QTimer::singleShot(2000, this, [this]() {
        aiStatusLabel->setText("");
    });
}

void SettingsWidget::onTestAIConnection()
{
    QString model = aiModelCombo->currentData().toString();
    QString apiKey = apiKeyEdit->text().trimmed();
    QString endpoint = apiEndpointEdit->text().trimmed();

    if (apiKey.isEmpty()) {
        aiStatusLabel->setText("❌ 请先输入API Key");
        aiStatusLabel->setStyleSheet("padding: 8px; color: #e74c3c; background-color: #ffebee; border-radius: 4px;");
        return;
    }

    if (model == "local") {
        aiStatusLabel->setText("ℹ️ 使用本地关键词匹配，无需测试");
        aiStatusLabel->setStyleSheet("padding: 8px; color: #3498db; background-color: #e3f2fd; border-radius: 4px;");
        return;
    }

    if (endpoint.isEmpty()) {
        endpoint = getDefaultEndpoint(model);
    }

    if (endpoint.isEmpty()) {
        aiStatusLabel->setText("❌ 未配置API地址");
        aiStatusLabel->setStyleSheet("padding: 8px; color: #e74c3c; background-color: #ffebee; border-radius: 4px;");
        return;
    }

    testAiBtn->setEnabled(false);
    aiStatusLabel->setText("🔄 测试连接中...");
    aiStatusLabel->setStyleSheet("padding: 8px; color: #3498db; background-color: #e3f2fd; border-radius: 4px;");

    if (!networkManager) {
        networkManager = new QNetworkAccessManager(this);
    }

    QNetworkRequest request;
    QUrl url(endpoint);
    if (!url.isValid()) {
        aiStatusLabel->setText("❌ API地址无效");
        aiStatusLabel->setStyleSheet("padding: 8px; color: #e74c3c; background-color: #ffebee; border-radius: 4px;");
        testAiBtn->setEnabled(true);
        return;
    }
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QJsonObject json;
    json["model"] = getModelName(model);
    QJsonArray messages;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = "Hello";
    messages.append(msg);
    json["messages"] = messages;
    json["max_tokens"] = 10;
    json["stream"] = false;

    if (model == "qwen") {
        QJsonObject input;
        input["messages"] = json.take("messages");
        json["input"] = input;
        json.remove("max_tokens");
        json.remove("stream");
        json["parameters"] = QJsonObject({
            {"temperature", 0.7},
            {"max_tokens", 10},
            {"result_format", "message"}
        });
    }

    QJsonDocument doc(json);
    qDebug() << "AI Test Request JSON:" << doc.toJson(QJsonDocument::Indented);

    QPointer<QNetworkReply> reply = networkManager->post(request, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint, apiKey, model]() {
        if (!reply) return;

        testAiBtn->setEnabled(true);

        qDebug() << "AI Test connection response:";
        qDebug() << "  URL:" << endpoint;
        qDebug() << "  Error:" << reply->error();
        qDebug() << "  ErrorString:" << reply->errorString();

        if (reply->error() != QNetworkReply::NoError) {
            QString errorMsg = reply->errorString();
            aiStatusLabel->setText(QString("❌ 连接失败: %1").arg(errorMsg));
            aiStatusLabel->setStyleSheet("padding: 8px; color: #e74c3c; background-color: #ffebee; border-radius: 4px;");
        } else {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qDebug() << "  StatusCode:" << statusCode;

            if (statusCode == 200) {
                aiStatusLabel->setText("✅ 连接成功!");
                aiStatusLabel->setStyleSheet("padding: 8px; color: #27ae60; background-color: #e8f5e9; border-radius: 4px;");
            } else {
                QByteArray data = reply->readAll();
                qDebug() << "  Response:" << data;
                aiStatusLabel->setText(QString("❌ 错误码: %1").arg(statusCode));
                aiStatusLabel->setStyleSheet("padding: 8px; color: #e74c3c; background-color: #ffebee; border-radius: 4px;");
            }
        }
        reply->deleteLater();
    });

    QTimer::singleShot(10000, this, [this, reply]() {
        if (reply && reply->isRunning()) {
            reply->abort();
            testAiBtn->setEnabled(true);
            aiStatusLabel->setText("❌ 连接超时");
            aiStatusLabel->setStyleSheet("padding: 8px; color: #e74c3c; background-color: #ffebee; border-radius: 4px;");
        }
    });
}

void SettingsWidget::onChatTestClicked()
{
    AIKeyConfig defaultKey = AIConfig::instance().getDefaultKey();
    if (defaultKey.apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先添加并配置API Key");
        return;
    }

    ChatTestDialog *chatDialog = new ChatTestDialog(this);
    chatDialog->exec();
    delete chatDialog;
}

void SettingsWidget::onIconGenTestClicked()
{
    AIImageConfig defaultKey = AIConfig::instance().getDefaultImageKey();
    if (defaultKey.apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先添加并配置图像AI API Key");
        return;
    }

    AIIconGeneratorDialog *iconGenDialog = new AIIconGeneratorDialog(this);
    if (iconGenDialog->exec() == QDialog::Accepted) {
        QString iconPath = iconGenDialog->getGeneratedIconPath();
        if (!iconPath.isEmpty()) {
            QMessageBox::information(this, "成功", "图标已生成并保存到:\n" + iconPath);
        }
    }
    delete iconGenDialog;
}

QString SettingsWidget::getDefaultEndpoint(const QString &model)
{
    static QMap<QString, QString> endpoints = {
        {"minimax", "https://api.minimax.chat/v1/text/chatcompletion_v2"},
        {"gpt35", "https://api.openai.com/v1/chat/completions"},
        {"gpt4", "https://api.openai.com/v1/chat/completions"},
        {"claude", "https://api.anthropic.com/v1/messages"},
        {"gemini", "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent"},
        {"qwen", "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation"},
        {"spark", "https://spark-api.xfyun.com/v3.5/chat"},
        {"deepseek", "https://api.siliconflow.cn/v1/chat/completions"}
    };
    return endpoints.value(model, "");
}

QString SettingsWidget::getModelName(const QString &model)
{
    static QMap<QString, QString> models = {
        {"minimax", "abab6.5s-chat"},
        {"gpt35", "gpt-3.5-turbo"},
        {"gpt4", "gpt-4"},
        {"claude", "claude-3-opus-20240229"},
        {"gemini", "gemini-pro"},
        {"qwen", "qwen-turbo"},
        {"spark", "generalv3.5"},
        {"deepseek", "deepseek-ai/DeepSeek-V3.2"}
    };
    return models.value(model, "");
}

QString SettingsWidget::getModelDisplayName(const QString &model)
{
    static QMap<QString, QString> displayNames = {
        {"minimax", "MiniMax"},
        {"gpt35", "OpenAI GPT-3.5"},
        {"gpt4", "OpenAI GPT-4"},
        {"claude", "Claude-3"},
        {"gemini", "Google Gemini"},
        {"qwen", "通义千问"},
        {"spark", "讯飞星火"},
        {"deepseek", "DeepSeek"},
        {"local", "本地关键词匹配"}
    };
    return displayNames.value(model, model);
}

void SettingsWidget::onAIKeyTableSelectionChanged()
{
    bool hasSelection = !aiKeysTable->selectedItems().isEmpty();
    editAIKeyBtn->setEnabled(hasSelection);
    deleteAIKeyBtn->setEnabled(hasSelection);
    setDefaultAIKeyBtn->setEnabled(hasSelection);
}

void SettingsWidget::onAddAIKey()
{
    QDialog dialog(this);
    dialog.setWindowTitle("添加AI模型");
    dialog.setMinimumWidth(550);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(15);

    QGroupBox *formGroup = new QGroupBox("模型配置", &dialog);
    QFormLayout *formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight);

    QLabel *providerLabel = new QLabel("服务商:", formGroup);
    QComboBox *providerCombo = new QComboBox(formGroup);
    providerCombo->setEditable(true);
    providerCombo->setInsertPolicy(QComboBox::NoInsert);
    providerCombo->setMinimumWidth(250);
    providerCombo->lineEdit()->setPlaceholderText("搜索服务商...");

    QList<AIProviderInfo> providers = AIConfig::instance().getProviders();
    for (const AIProviderInfo &provider : providers) {
        providerCombo->addItem(provider.displayName, provider.id);
    }
    formLayout->addRow(providerLabel, providerCombo);

    QLabel *modelLabel = new QLabel("模型:", formGroup);
    QComboBox *modelCombo = new QComboBox(formGroup);
    modelCombo->setEditable(true);
    modelCombo->setInsertPolicy(QComboBox::NoInsert);
    modelCombo->setMinimumWidth(250);
    modelCombo->lineEdit()->setPlaceholderText("选择或搜索模型...");

    auto updateModels = [&](const QString &providerId) {
        modelCombo->clear();
        QList<AIModelInfo> models = AIConfig::instance().getModelsByProvider(providerId);
        for (const AIModelInfo &model : models) {
            modelCombo->addItem(model.id, model.id);
        }
    };
    updateModels(providerCombo->currentData().toString());
    connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
        if (index >= 0) {
            updateModels(providerCombo->itemData(index).toString());
        }
    });

    formLayout->addRow(modelLabel, modelCombo);

    QLabel *nameLabel = new QLabel("配置名称:", formGroup);
    QLineEdit *nameEdit = new QLineEdit(formGroup);
    nameEdit->setPlaceholderText("自定义名称（可选）");
    formLayout->addRow(nameLabel, nameEdit);

    QLabel *apiKeyLabel = new QLabel("API Key:", formGroup);
    QWidget *apiKeyContainer = new QWidget(formGroup);
    QHBoxLayout *apiKeyLayout = new QHBoxLayout(apiKeyContainer);
    apiKeyLayout->setContentsMargins(0, 0, 0, 0);
    apiKeyLayout->setSpacing(8);

    QLineEdit *apiKeyEdit = new QLineEdit(apiKeyContainer);
    apiKeyEdit->setPlaceholderText("请输入API Key");
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setMinimumWidth(200);

    QPushButton *togglePasswordBtn = new QPushButton("👁", apiKeyContainer);
    togglePasswordBtn->setFixedWidth(35);
    togglePasswordBtn->setToolTip("显示/隐藏密码");
    togglePasswordBtn->setStyleSheet("QPushButton { border: 1px solid #ccc; border-radius: 3px; padding: 4px; }");

    connect(togglePasswordBtn, &QPushButton::clicked, [&]() {
        if (apiKeyEdit->echoMode() == QLineEdit::Password) {
            apiKeyEdit->setEchoMode(QLineEdit::Normal);
            togglePasswordBtn->setText("🔒");
        } else {
            apiKeyEdit->setEchoMode(QLineEdit::Password);
            togglePasswordBtn->setText("👁");
        }
    });

    apiKeyLayout->addWidget(apiKeyEdit);
    apiKeyLayout->addWidget(togglePasswordBtn);
    formLayout->addRow(apiKeyLabel, apiKeyContainer);

    QLabel *endpointLabel = new QLabel("API地址:", formGroup);
    QLineEdit *endpointEdit = new QLineEdit(formGroup);
    endpointEdit->setPlaceholderText("留空使用默认地址（可选）");
    formLayout->addRow(endpointLabel, endpointEdit);

    mainLayout->addWidget(formGroup);

    QFrame *verifyFrame = new QFrame(&dialog);
    verifyFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    verifyFrame->setStyleSheet("QFrame { background-color: #f8f9fa; border-radius: 8px; padding: 15px; }");
    QVBoxLayout *verifyLayout = new QVBoxLayout(verifyFrame);
    verifyLayout->setSpacing(10);

    QHBoxLayout *verifyBtnLayout = new QHBoxLayout();
    verifyBtnLayout->setSpacing(10);

    QPushButton *verifyBtn = new QPushButton("🔐 验证连接", &dialog);
    verifyBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
        "QPushButton:hover { background-color: #2980b9; } "
        "QPushButton:disabled { background-color: #bdc3c7; }"
    );

    QLabel *verifyStatusLabel = new QLabel("", verifyFrame);
    verifyStatusLabel->setAlignment(Qt::AlignCenter);
    verifyStatusLabel->setStyleSheet("padding: 10px; border-radius: 4px; font-size: 13px;");
    verifyStatusLabel->setVisible(false);

    verifyBtnLayout->addStretch();
    verifyBtnLayout->addWidget(verifyBtn);
    verifyBtnLayout->addStretch();

    verifyLayout->addLayout(verifyBtnLayout);
    verifyLayout->addWidget(verifyStatusLabel);

    mainLayout->addWidget(verifyFrame);

    QCheckBox *defaultCheck = new QCheckBox("设为默认模型", &dialog);
    defaultCheck->setStyleSheet("QCheckBox { font-size: 14px; padding: 5px; }");
    mainLayout->addWidget(defaultCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);

    QPushButton *okBtn = new QPushButton("✅ 添加", &dialog);
    okBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; padding: 10px 25px; border-radius: 5px; font-weight: bold; font-size: 14px; } "
        "QPushButton:hover { background-color: #219a52; }"
    );

    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #95a5a6; color: white; padding: 10px 25px; border-radius: 5px; font-size: 14px; } "
        "QPushButton:hover { background-color: #7f8c8d; }"
    );

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    QString verifyLogFileName;
    {
        QString logDir = QCoreApplication::applicationDirPath() + "/logs";
        QDir dir(logDir);
        if (!dir.exists()) {
            dir.mkpath(logDir);
        }
        verifyLogFileName = logDir + "/ai_image_verify_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    }

    QNetworkAccessManager *verifyManager = new QNetworkAccessManager(&dialog);

    connect(verifyBtn, &QPushButton::clicked, [&]() {
        QString apiKey = apiKeyEdit->text().trimmed();
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        if (apiKey.isEmpty()) {
            verifyStatusLabel->setText("❌ 没有可验证的API Key");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            return;
        }

        if (modelId == "local") {
            verifyStatusLabel->setText("ℹ️ 本地模式无需验证");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #2980b9; background-color: #d4e6f1; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            return;
        }

        verifyBtn->setEnabled(false);
        verifyBtn->setText("🔄 验证中...");
        verifyBtn->setStyleSheet(
            "QPushButton { background-color: #f39c12; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
            "QPushButton:disabled { background-color: #f39c12; }"
        );
        verifyStatusLabel->setVisible(false);

        AIModelInfo modelInfo = AIConfig::instance().getModelInfo(modelId);
        QString actualEndpoint = endpoint.isEmpty() ? modelInfo.defaultEndpoint : endpoint;
        QString actualModelName = modelInfo.name.isEmpty() ? modelId : modelInfo.name;

        if (actualEndpoint.isEmpty()) {
            verifyStatusLabel->setText("❌ 未配置API地址");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );
            return;
        }

        QUrl url(actualEndpoint);
        QNetworkRequest request;
        request.setUrl(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

        QJsonObject json;
        json["model"] = actualModelName;
        QJsonArray messages;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "Hi";
        messages.append(msg);
        json["messages"] = messages;
        json["max_tokens"] = 10;
        json["stream"] = false;

        if (providerId == "aliyun" || providerId == "minimax") {
            QJsonObject input;
            input["messages"] = json.take("messages");
            json["input"] = input;
            json.remove("max_tokens");
            json.remove("stream");
            json["parameters"] = QJsonObject({
                {"temperature", 0.7},
                {"max_tokens", 10},
                {"result_format", "message"}
            });
        }

        QJsonDocument doc(json);
        QPointer<QNetworkReply> reply = verifyManager->post(request, doc.toJson());

        connect(reply, &QNetworkReply::finished, [=]() {
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );

            if (!reply || reply->error() != QNetworkReply::NoError) {
                QString errorMsg = reply ? reply->errorString() : "网络错误";
                verifyStatusLabel->setText(QString("❌ 连接失败: %1").arg(errorMsg));
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
                return;
            }

            QByteArray responseData = reply->readAll();
            QJsonParseError parseError;
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);

            if (parseError.error != QJsonParseError::NoError) {
                verifyStatusLabel->setText("⚠️ 响应格式异常");
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #d35400; background-color: #fdebd0; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
                return;
            }

            QJsonObject responseObj = responseDoc.object();
            if (responseObj.contains("error")) {
                QString errorMsg = responseObj["error"].toObject()["message"].toString("未知错误");
                verifyStatusLabel->setText(QString("❌ API错误: %1").arg(errorMsg));
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
            } else {
                verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #196f3d; background-color: #d4edda; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
            }
        });
    });

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString apiKey = apiKeyEdit->text().trimmed();
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        if (modelId.isEmpty()) {
            QMessageBox::warning(this, "警告", "请选择或输入模型名称");
            return;
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this, "警告", "API Key不能为空");
            return;
        }

        AIKeyConfig newKey;
        newKey.provider = providerId;
        newKey.model = modelId;

        QString customName = nameEdit->text().trimmed();
        if (!customName.isEmpty()) {
            newKey.name = customName;
        } else {
            AIModelInfo modelInfo = AIConfig::instance().getModelInfo(modelId);
            newKey.name = modelInfo.name.isEmpty() ? modelId : modelInfo.name;
        }

        newKey.apiKey = apiKey;
        newKey.endpoint = endpoint;
        newKey.isDefault = defaultCheck->isChecked();

        AIConfig::instance().addKey(newKey);
        loadAISettings();

        aiStatusLabel->setText("✅ 模型添加成功");
        aiStatusLabel->setStyleSheet("padding: 8px; color: #196f3d; background-color: #d4edda; border-radius: 4px;");
        QTimer::singleShot(2000, this, [this]() {
            aiStatusLabel->setText("");
        });
    }
}

void SettingsWidget::onEditAIKey()
{
    int currentRow = aiKeysTable->currentRow();
    if (currentRow < 0) return;

    QString keyId = aiKeysTable->item(currentRow, 0)->data(Qt::UserRole).toString();
    AIKeyConfig key = AIConfig::instance().getKey(keyId);
    if (key.id.isEmpty()) return;

    QDialog dialog(this);
    dialog.setWindowTitle("编辑AI模型");
    dialog.setMinimumWidth(550);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(15);

    QGroupBox *formGroup = new QGroupBox("模型配置", &dialog);
    QFormLayout *formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight);

    QLabel *providerLabel = new QLabel("服务商:", formGroup);
    QComboBox *providerCombo = new QComboBox(formGroup);
    providerCombo->setEditable(true);
    providerCombo->setInsertPolicy(QComboBox::NoInsert);
    providerCombo->setMinimumWidth(250);
    providerCombo->lineEdit()->setPlaceholderText("搜索服务商...");

    QList<AIProviderInfo> providers = AIConfig::instance().getProviders();
    for (const AIProviderInfo &provider : providers) {
        providerCombo->addItem(provider.displayName, provider.id);
    }
    int providerIndex = providerCombo->findData(key.provider);
    if (providerIndex >= 0) providerCombo->setCurrentIndex(providerIndex);

    formLayout->addRow(providerLabel, providerCombo);

    QLabel *modelLabel = new QLabel("模型:", formGroup);
    QComboBox *modelCombo = new QComboBox(formGroup);
    modelCombo->setEditable(true);
    modelCombo->setInsertPolicy(QComboBox::NoInsert);
    modelCombo->setMinimumWidth(250);
    modelCombo->lineEdit()->setPlaceholderText("选择或搜索模型...");

    auto updateModels = [&](const QString &providerId) {
        modelCombo->clear();
        QList<AIModelInfo> models = AIConfig::instance().getModelsByProvider(providerId);
        for (const AIModelInfo &model : models) {
            modelCombo->addItem(model.id, model.id);
        }
    };
    updateModels(key.provider);
    int modelIndex = modelCombo->findData(key.model);
    if (modelIndex >= 0) {
        modelCombo->setCurrentIndex(modelIndex);
    } else {
        modelCombo->setCurrentText(key.model);
    }

    connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
        if (index >= 0) {
            updateModels(providerCombo->itemData(index).toString());
        }
    });

    formLayout->addRow(modelLabel, modelCombo);

    QLabel *nameLabel = new QLabel("配置名称:", formGroup);
    QLineEdit *nameEdit = new QLineEdit(key.name, formGroup);
    nameEdit->setPlaceholderText("自定义名称（可选）");
    formLayout->addRow(nameLabel, nameEdit);

    QLabel *apiKeyLabel = new QLabel("API Key:", formGroup);
    QWidget *apiKeyContainer = new QWidget(formGroup);
    QHBoxLayout *apiKeyLayout = new QHBoxLayout(apiKeyContainer);
    apiKeyLayout->setContentsMargins(0, 0, 0, 0);
    apiKeyLayout->setSpacing(8);

    QLineEdit *apiKeyEdit = new QLineEdit(apiKeyContainer);
    apiKeyEdit->setPlaceholderText("留空保持原值");
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setMinimumWidth(200);

    QPushButton *togglePasswordBtn = new QPushButton("👁", apiKeyContainer);
    togglePasswordBtn->setFixedWidth(35);
    togglePasswordBtn->setToolTip("显示/隐藏密码");
    togglePasswordBtn->setStyleSheet("QPushButton { border: 1px solid #ccc; border-radius: 3px; padding: 4px; }");

    connect(togglePasswordBtn, &QPushButton::clicked, [&]() {
        if (apiKeyEdit->echoMode() == QLineEdit::Password) {
            apiKeyEdit->setEchoMode(QLineEdit::Normal);
            togglePasswordBtn->setText("🔒");
        } else {
            apiKeyEdit->setEchoMode(QLineEdit::Password);
            togglePasswordBtn->setText("👁");
        }
    });

    apiKeyLayout->addWidget(apiKeyEdit);
    apiKeyLayout->addWidget(togglePasswordBtn);
    formLayout->addRow(apiKeyLabel, apiKeyContainer);

    QLabel *endpointLabel = new QLabel("API地址:", formGroup);
    QLineEdit *endpointEdit = new QLineEdit(key.endpoint, formGroup);
    endpointEdit->setPlaceholderText("留空使用默认地址（可选）");
    formLayout->addRow(endpointLabel, endpointEdit);

    mainLayout->addWidget(formGroup);

    QFrame *verifyFrame = new QFrame(&dialog);
    verifyFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    verifyFrame->setStyleSheet("QFrame { background-color: #f8f9fa; border-radius: 8px; padding: 15px; }");
    QVBoxLayout *verifyLayout = new QVBoxLayout(verifyFrame);
    verifyLayout->setSpacing(10);

    QHBoxLayout *verifyBtnLayout = new QHBoxLayout();
    verifyBtnLayout->setSpacing(10);

    QPushButton *verifyBtn = new QPushButton("🔐 验证连接", &dialog);
    verifyBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
        "QPushButton:hover { background-color: #2980b9; } "
        "QPushButton:disabled { background-color: #bdc3c7; }"
    );

    QLabel *verifyStatusLabel = new QLabel("", verifyFrame);
    verifyStatusLabel->setAlignment(Qt::AlignCenter);
    verifyStatusLabel->setStyleSheet("padding: 10px; border-radius: 4px; font-size: 13px;");
    verifyStatusLabel->setVisible(false);

    verifyBtnLayout->addStretch();
    verifyBtnLayout->addWidget(verifyBtn);
    verifyBtnLayout->addStretch();

    verifyLayout->addLayout(verifyBtnLayout);
    verifyLayout->addWidget(verifyStatusLabel);

    mainLayout->addWidget(verifyFrame);

    QCheckBox *defaultCheck = new QCheckBox("设为默认模型", &dialog);
    defaultCheck->setChecked(key.isDefault);
    defaultCheck->setStyleSheet("QCheckBox { font-size: 14px; padding: 5px; }");
    mainLayout->addWidget(defaultCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);

    QPushButton *okBtn = new QPushButton("✅ 保存", &dialog);
    okBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; padding: 10px 25px; border-radius: 5px; font-weight: bold; font-size: 14px; } "
        "QPushButton:hover { background-color: #219a52; }"
    );

    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #95a5a6; color: white; padding: 10px 25px; border-radius: 5px; font-size: 14px; } "
        "QPushButton:hover { background-color: #7f8c8d; }"
    );

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    QString verifyLogFileName;
    {
        QString logDir = QCoreApplication::applicationDirPath() + "/logs";
        QDir dir(logDir);
        if (!dir.exists()) {
            dir.mkpath(logDir);
        }
        verifyLogFileName = logDir + "/ai_image_verify_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    }

    QNetworkAccessManager *verifyManager = new QNetworkAccessManager(&dialog);

    connect(verifyBtn, &QPushButton::clicked, [&]() {
        QString apiKey = apiKeyEdit->text().trimmed();
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        if (apiKey.isEmpty()) {
            verifyStatusLabel->setText("❌ 没有可验证的API Key");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            return;
        }

        if (modelId == "local") {
            verifyStatusLabel->setText("ℹ️ 本地模式无需验证");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #2980b9; background-color: #d4e6f1; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            return;
        }

        verifyBtn->setEnabled(false);
        verifyBtn->setText("🔄 验证中...");
        verifyBtn->setStyleSheet(
            "QPushButton { background-color: #f39c12; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
            "QPushButton:disabled { background-color: #f39c12; }"
        );
        verifyStatusLabel->setVisible(false);

        AIModelInfo modelInfo = AIConfig::instance().getModelInfo(modelId);
        QString actualEndpoint = endpoint.isEmpty() ? modelInfo.defaultEndpoint : endpoint;
        QString actualModelName = modelInfo.name.isEmpty() ? modelId : modelInfo.name;

        if (actualEndpoint.isEmpty()) {
            verifyStatusLabel->setText("❌ 未配置API地址");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );
            return;
        }

        QUrl url(actualEndpoint);
        QNetworkRequest request;
        request.setUrl(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

        QJsonObject json;
        json["model"] = actualModelName;
        QJsonArray messages;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "Hi";
        messages.append(msg);
        json["messages"] = messages;
        json["max_tokens"] = 10;
        json["stream"] = false;

        if (providerId == "aliyun" || providerId == "minimax") {
            QJsonObject input;
            input["messages"] = json.take("messages");
            json["input"] = input;
            json.remove("max_tokens");
            json.remove("stream");
            json["parameters"] = QJsonObject({
                {"temperature", 0.7},
                {"max_tokens", 10},
                {"result_format", "message"}
            });
        }

        QJsonDocument doc(json);
        QPointer<QNetworkReply> reply = verifyManager->post(request, doc.toJson());

        connect(reply, &QNetworkReply::finished, [=]() {
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );

            if (!reply || reply->error() != QNetworkReply::NoError) {
                QString errorMsg = reply ? reply->errorString() : "网络错误";
                verifyStatusLabel->setText(QString("❌ 连接失败: %1").arg(errorMsg));
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
                return;
            }

            QByteArray responseData = reply->readAll();
            QJsonParseError parseError;
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);

            if (parseError.error != QJsonParseError::NoError) {
                verifyStatusLabel->setText("⚠️ 响应格式异常");
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #d35400; background-color: #fdebd0; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
                return;
            }

            QJsonObject responseObj = responseDoc.object();
            if (responseObj.contains("error")) {
                QString errorMsg = responseObj["error"].toObject()["message"].toString("未知错误");
                verifyStatusLabel->setText(QString("❌ API错误: %1").arg(errorMsg));
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
            } else {
                verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #196f3d; background-color: #d4edda; border-radius: 4px;");
                verifyStatusLabel->setVisible(true);
            }
        });
    });

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        if (modelId.isEmpty()) {
            QMessageBox::warning(this, "警告", "请选择或输入模型名称");
            return;
        }

        key.provider = providerCombo->currentData().toString();
        key.model = modelId;

        if (!apiKeyEdit->text().trimmed().isEmpty()) {
            key.apiKey = apiKeyEdit->text().trimmed();
        }
        key.endpoint = endpointEdit->text().trimmed();
        key.isDefault = defaultCheck->isChecked();

        QString customName = nameEdit->text().trimmed();
        if (!customName.isEmpty()) {
            key.name = customName;
        } else {
            AIModelInfo modelInfo = AIConfig::instance().getModelInfo(key.model);
            key.name = modelInfo.name.isEmpty() ? modelId : modelInfo.name;
        }

        AIConfig::instance().updateKey(key);
        loadAISettings();

        aiStatusLabel->setText("✅ 模型更新成功");
        aiStatusLabel->setStyleSheet("padding: 8px; color: #196f3d; background-color: #d4edda; border-radius: 4px;");
        aiStatusLabel->setVisible(true);
        QTimer::singleShot(2000, this, [this]() {
            aiStatusLabel->setText("");
        });
    }
}

void SettingsWidget::onDeleteAIKey()
{
    int currentRow = aiKeysTable->currentRow();
    if (currentRow < 0) return;

    QString keyId = aiKeysTable->item(currentRow, 0)->data(Qt::UserRole).toString();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", "确定要删除此AI配置吗？",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        AIConfig::instance().deleteKey(keyId);
        loadAISettings();
    }
}

void SettingsWidget::onSetDefaultAIKey()
{
    int currentRow = aiKeysTable->currentRow();
    if (currentRow < 0) return;

    QString keyId = aiKeysTable->item(currentRow, 0)->data(Qt::UserRole).toString();
    AIConfig::instance().setDefaultKey(keyId);
    loadAISettings();
}

void SettingsWidget::onAIImageKeyTableSelectionChanged()
{
    bool hasSelection = aiImageKeysTable->selectedItems().size() > 0;
    editImageKeyBtn->setEnabled(hasSelection);
    deleteImageKeyBtn->setEnabled(hasSelection);
    setDefaultImageKeyBtn->setEnabled(hasSelection);
}

void SettingsWidget::onAddImageKey()
{
    QDialog dialog(this);
    dialog.setWindowTitle("添加图像生成API");
    dialog.setMinimumWidth(550);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(15);

    QGroupBox *formGroup = new QGroupBox("图像生成配置", &dialog);
    QFormLayout *formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight);

    QLabel *providerLabel = new QLabel("服务商:", formGroup);
    QComboBox *providerCombo = new QComboBox(formGroup);
    providerCombo->setEditable(true);
    providerCombo->setInsertPolicy(QComboBox::NoInsert);
    providerCombo->setMinimumWidth(250);

    QList<AIImageModelInfo> imageModels = AIConfig::instance().getAllImageModels();
    QSet<QString> addedProviders;
    for (const AIImageModelInfo &model : imageModels) {
        if (!addedProviders.contains(model.provider)) {
            QString displayName = model.provider;
            if (model.provider == "siliconflow") displayName = "🚀 硅基流动";
            else if (model.provider == "openai") displayName = "🔵 OpenAI DALL-E";
            else if (model.provider == "stability") displayName = "🎭 Stability AI";
            providerCombo->addItem(displayName, model.provider);
            addedProviders.insert(model.provider);
        }
    }
    formLayout->addRow(providerLabel, providerCombo);

    QLabel *modelLabel = new QLabel("模型:", formGroup);
    QComboBox *modelCombo = new QComboBox(formGroup);
    modelCombo->setEditable(true);
    modelCombo->setInsertPolicy(QComboBox::NoInsert);
    modelCombo->setMinimumWidth(250);

    auto updateImageModels = [&](const QString &providerId) {
        modelCombo->clear();
        QList<AIImageModelInfo> models = AIConfig::instance().getImageModelsByProvider(providerId);
        for (const AIImageModelInfo &model : models) {
            modelCombo->addItem(model.name.isEmpty() ? model.id : model.name, model.id);
        }
    };
    updateImageModels(providerCombo->currentData().toString());
    connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
        if (index >= 0) {
            updateImageModels(providerCombo->itemData(index).toString());
        }
    });

    formLayout->addRow(modelLabel, modelCombo);

    QLabel *nameLabel = new QLabel("配置名称:", formGroup);
    QLineEdit *nameEdit = new QLineEdit(formGroup);
    nameEdit->setPlaceholderText("自定义名称（可选）");
    formLayout->addRow(nameLabel, nameEdit);

    QLabel *apiKeyLabel = new QLabel("API Key:", formGroup);
    QWidget *apiKeyContainer = new QWidget(formGroup);
    QHBoxLayout *apiKeyLayout = new QHBoxLayout(apiKeyContainer);
    apiKeyLayout->setContentsMargins(0, 0, 0, 0);
    apiKeyLayout->setSpacing(8);

    QLineEdit *apiKeyEdit = new QLineEdit(apiKeyContainer);
    apiKeyEdit->setPlaceholderText("请输入API Key");
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setMinimumWidth(200);

    QPushButton *togglePasswordBtn = new QPushButton("👁", apiKeyContainer);
    togglePasswordBtn->setFixedWidth(35);
    togglePasswordBtn->setToolTip("显示/隐藏密码");
    togglePasswordBtn->setStyleSheet("QPushButton { border: 1px solid #ccc; border-radius: 3px; padding: 4px; }");

    connect(togglePasswordBtn, &QPushButton::clicked, [&]() {
        if (apiKeyEdit->echoMode() == QLineEdit::Password) {
            apiKeyEdit->setEchoMode(QLineEdit::Normal);
            togglePasswordBtn->setText("🔒");
        } else {
            apiKeyEdit->setEchoMode(QLineEdit::Password);
            togglePasswordBtn->setText("👁");
        }
    });

    apiKeyLayout->addWidget(apiKeyEdit);
    apiKeyLayout->addWidget(togglePasswordBtn);
    formLayout->addRow(apiKeyLabel, apiKeyContainer);

    QLabel *endpointLabel = new QLabel("API地址:", formGroup);
    QLineEdit *endpointEdit = new QLineEdit(formGroup);
    endpointEdit->setPlaceholderText("留空使用默认地址（可选）");
    formLayout->addRow(endpointLabel, endpointEdit);

    mainLayout->addWidget(formGroup);

    QFrame *verifyFrame = new QFrame(&dialog);
    verifyFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    verifyFrame->setStyleSheet("QFrame { background-color: #f8f9fa; border-radius: 8px; padding: 15px; }");
    QVBoxLayout *verifyLayout = new QVBoxLayout(verifyFrame);
    verifyLayout->setSpacing(10);

    QHBoxLayout *verifyBtnLayout = new QHBoxLayout();
    verifyBtnLayout->setSpacing(10);

    QPushButton *verifyBtn = new QPushButton("🔐 验证连接", &dialog);
    verifyBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
        "QPushButton:hover { background-color: #2980b9; } "
        "QPushButton:disabled { background-color: #bdc3c7; }"
    );

    QLabel *verifyStatusLabel = new QLabel("", verifyFrame);
    verifyStatusLabel->setAlignment(Qt::AlignCenter);
    verifyStatusLabel->setStyleSheet("padding: 10px; border-radius: 4px; font-size: 13px;");
    verifyStatusLabel->setVisible(false);

    verifyBtnLayout->addStretch();
    verifyBtnLayout->addWidget(verifyBtn);
    verifyBtnLayout->addStretch();

    verifyLayout->addLayout(verifyBtnLayout);
    verifyLayout->addWidget(verifyStatusLabel);

    mainLayout->addWidget(verifyFrame);

    QCheckBox *defaultCheck = new QCheckBox("设为默认", &dialog);
    defaultCheck->setStyleSheet("QCheckBox { font-size: 14px; padding: 5px; }");
    mainLayout->addWidget(defaultCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);

    QPushButton *okBtn = new QPushButton("✅ 添加", &dialog);
    okBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; padding: 10px 25px; border-radius: 5px; font-weight: bold; font-size: 14px; } "
        "QPushButton:hover { background-color: #219a52; }"
    );

    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #95a5a6; color: white; padding: 10px 25px; border-radius: 5px; font-size: 14px; } "
        "QPushButton:hover { background-color: #7f8c8d; }"
    );

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    QString verifyLogFileName;
    {
        verifyLogFileName = AILogger::initLogFile(AILogger::ImageVerify);
    }

    QNetworkAccessManager *verifyManager = new QNetworkAccessManager(&dialog);

    connect(verifyBtn, &QPushButton::clicked, [&]() {
        QString apiKey = apiKeyEdit->text().trimmed();
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        if (apiKey.isEmpty()) {
            verifyStatusLabel->setText("❌ 没有可验证的API Key");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            return;
        }

        verifyBtn->setEnabled(false);
        verifyBtn->setText("🔄 验证中...");
        verifyBtn->setStyleSheet(
            "QPushButton { background-color: #f39c12; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
            "QPushButton:disabled { background-color: #f39c12; }"
        );
        verifyStatusLabel->setVisible(false);

        QList<AIImageModelInfo> models = AIConfig::instance().getAllImageModels();
        QString actualEndpoint;
        for (const AIImageModelInfo &model : models) {
            if (model.provider == providerId && model.id == modelId) {
                actualEndpoint = model.defaultEndpoint;
                break;
            }
        }
        if (actualEndpoint.isEmpty()) {
            actualEndpoint = endpoint;
        }

        if (actualEndpoint.isEmpty()) {
            verifyStatusLabel->setText("❌ 未配置API地址");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );
            return;
        }

        QUrl url(actualEndpoint);
        QNetworkRequest request;
        request.setUrl(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

        QJsonObject json;
        json["model"] = modelId;
        json["prompt"] = "test";
        json["n"] = 1;
        json["size"] = "1024x1024";

        QJsonDocument doc(json);

        AILogger::logVerifyRequest(verifyLogFileName, providerId, modelId, actualEndpoint, json);

        QPointer<QNetworkReply> reply = verifyManager->post(request, doc.toJson());

        connect(reply, &QNetworkReply::finished, [=]() {
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );

            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonParseError parseError;
                QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);

                AILogger::logVerifyResponse(verifyLogFileName, reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                                           responseDoc.object());

                if (parseError.error == QJsonParseError::NoError && !responseDoc.isNull()) {
                    if (providerId == "openai" || providerId == "siliconflow") {
                        if (responseDoc.object().contains("data") || responseDoc.object().contains("url")) {
                            verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #27ae60; background-color: #d5f4e6; border-radius: 4px;");
                        } else if (responseDoc.object().contains("error")) {
                            QString errorMsg = responseDoc.object()["error"].toObject()["message"].toString();
                            verifyStatusLabel->setText("❌ API错误: " + errorMsg);
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                        } else {
                            verifyStatusLabel->setText("⚠️ 连接成功，但响应格式异常");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #f39c12; background-color: #fdebd0; border-radius: 4px;");
                        }
                    } else if (providerId == "stability") {
                        if (responseDoc.object().contains("artifacts")) {
                            verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #27ae60; background-color: #d5f4e6; border-radius: 4px;");
                        } else if (responseDoc.object().contains("errors")) {
                            QString errorMsg = responseDoc.object()["errors"].toArray().first().toObject()["message"].toString();
                            verifyStatusLabel->setText("❌ API错误: " + errorMsg);
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                        } else {
                            verifyStatusLabel->setText("⚠️ 连接成功，但响应格式异常");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #f39c12; background-color: #fdebd0; border-radius: 4px;");
                        }
                    } else {
                        verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                        verifyStatusLabel->setStyleSheet("padding: 10px; color: #27ae60; background-color: #d5f4e6; border-radius: 4px;");
                    }
                } else {
                    verifyStatusLabel->setText("⚠️ 连接成功，但响应解析失败");
                    verifyStatusLabel->setStyleSheet("padding: 10px; color: #f39c12; background-color: #fdebd0; border-radius: 4px;");
                }
            } else {
                QString errorMsg = reply->errorString();
                QByteArray errorData = reply->readAll();
                QString detailedError = errorMsg;

                if (!errorData.isEmpty()) {
                    QJsonParseError parseError;
                    QJsonDocument errorDoc = QJsonDocument::fromJson(errorData, &parseError);
                    if (parseError.error == QJsonParseError::NoError) {
                        if (errorDoc.object().contains("error")) {
                            detailedError = errorDoc.object()["error"].toObject()["message"].toString();
                        } else if (errorDoc.object().contains("errors")) {
                            detailedError = errorDoc.object()["errors"].toArray().first().toObject()["message"].toString();
                        }
                    }
                }

                AILogger::logVerifyError(verifyLogFileName, detailedError, errorData);

                verifyStatusLabel->setText("❌ 连接失败: " + detailedError);
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            }
            verifyStatusLabel->setVisible(true);
            reply->deleteLater();
        });
    });

    connect(okBtn, &QPushButton::clicked, [&]() {
        QString apiKey = apiKeyEdit->text().trimmed();
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(&dialog, "警告", "请输入API Key");
            return;
        }

        AIImageConfig config;
        config.id = QUuid::createUuid().toString();
        config.name = nameEdit->text().trimmed().isEmpty() ? providerCombo->currentText() : nameEdit->text().trimmed();
        config.provider = providerId;
        config.model = modelId;
        config.apiKey = apiKey;
        config.endpoint = endpoint;
        config.isDefault = defaultCheck->isChecked();

        AIConfig::instance().addImageKey(config);
        loadAISettings();
        dialog.accept();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void SettingsWidget::onEditImageKey()
{
    int currentRow = aiImageKeysTable->currentRow();
    if (currentRow < 0) return;

    QString keyId = aiImageKeysTable->item(currentRow, 0)->data(Qt::UserRole).toString();
    AIImageConfig config = AIConfig::instance().getImageKey(keyId);

    QDialog dialog(this);
    dialog.setWindowTitle("编辑图像生成API");
    dialog.setMinimumWidth(550);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(15);

    QGroupBox *formGroup = new QGroupBox("图像生成配置", &dialog);
    QFormLayout *formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight);

    QLabel *providerLabel = new QLabel("服务商:", formGroup);
    QComboBox *providerCombo = new QComboBox(formGroup);
    providerCombo->setEditable(true);
    providerCombo->setInsertPolicy(QComboBox::NoInsert);
    providerCombo->setMinimumWidth(250);

    QList<AIImageModelInfo> imageModels = AIConfig::instance().getAllImageModels();
    QSet<QString> addedProviders;
    for (const AIImageModelInfo &model : imageModels) {
        if (!addedProviders.contains(model.provider)) {
            QString displayName = model.provider;
            if (model.provider == "siliconflow") displayName = "🚀 硅基流动";
            else if (model.provider == "openai") displayName = "🔵 OpenAI DALL-E";
            else if (model.provider == "stability") displayName = "🎭 Stability AI";
            providerCombo->addItem(displayName, model.provider);
            addedProviders.insert(model.provider);
        }
    }

    int providerIndex = providerCombo->findData(config.provider);
    if (providerIndex >= 0) providerCombo->setCurrentIndex(providerIndex);
    formLayout->addRow(providerLabel, providerCombo);

    QLabel *modelLabel = new QLabel("模型:", formGroup);
    QComboBox *modelCombo = new QComboBox(formGroup);
    modelCombo->setEditable(true);
    modelCombo->setInsertPolicy(QComboBox::NoInsert);
    modelCombo->setMinimumWidth(250);

    auto updateImageModels = [&](const QString &providerId) {
        modelCombo->clear();
        QList<AIImageModelInfo> models = AIConfig::instance().getImageModelsByProvider(providerId);
        for (const AIImageModelInfo &model : models) {
            modelCombo->addItem(model.name.isEmpty() ? model.id : model.name, model.id);
        }
    };
    updateImageModels(config.provider);
    int modelIndex = modelCombo->findData(config.model);
    if (modelIndex >= 0) modelCombo->setCurrentIndex(modelIndex);

    connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
        if (index >= 0) {
            updateImageModels(providerCombo->itemData(index).toString());
        }
    });

    formLayout->addRow(modelLabel, modelCombo);

    QLabel *nameLabel = new QLabel("配置名称:", formGroup);
    QLineEdit *nameEdit = new QLineEdit(config.name, formGroup);
    nameEdit->setPlaceholderText("自定义名称（可选）");
    formLayout->addRow(nameLabel, nameEdit);

    QLabel *apiKeyLabel = new QLabel("API Key:", formGroup);
    QWidget *apiKeyContainer = new QWidget(formGroup);
    QHBoxLayout *apiKeyLayout = new QHBoxLayout(apiKeyContainer);
    apiKeyLayout->setContentsMargins(0, 0, 0, 0);
    apiKeyLayout->setSpacing(8);

    QLineEdit *apiKeyEdit = new QLineEdit(apiKeyContainer);
    apiKeyEdit->setPlaceholderText("留空保持原值");
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setMinimumWidth(200);

    QPushButton *togglePasswordBtn = new QPushButton("👁", apiKeyContainer);
    togglePasswordBtn->setFixedWidth(35);
    togglePasswordBtn->setToolTip("显示/隐藏密码");
    togglePasswordBtn->setStyleSheet("QPushButton { border: 1px solid #ccc; border-radius: 3px; padding: 4px; }");

    connect(togglePasswordBtn, &QPushButton::clicked, [&]() {
        if (apiKeyEdit->echoMode() == QLineEdit::Password) {
            apiKeyEdit->setEchoMode(QLineEdit::Normal);
            togglePasswordBtn->setText("🔒");
        } else {
            apiKeyEdit->setEchoMode(QLineEdit::Password);
            togglePasswordBtn->setText("👁");
        }
    });

    apiKeyLayout->addWidget(apiKeyEdit);
    apiKeyLayout->addWidget(togglePasswordBtn);
    formLayout->addRow(apiKeyLabel, apiKeyContainer);

    QLabel *endpointLabel = new QLabel("API地址:", formGroup);
    QLineEdit *endpointEdit = new QLineEdit(formGroup);
    endpointEdit->setPlaceholderText("留空使用默认值");
    endpointEdit->setText(config.endpoint);
    formLayout->addRow(endpointLabel, endpointEdit);

    mainLayout->addWidget(formGroup);

    QFrame *verifyFrame = new QFrame(&dialog);
    verifyFrame->setFrameShape(QFrame::StyledPanel);
    verifyFrame->setStyleSheet("QFrame { background-color: #f8f9fa; border-radius: 8px; padding: 10px; }");
    QVBoxLayout *verifyLayout = new QVBoxLayout(verifyFrame);
    verifyLayout->setSpacing(10);

    QHBoxLayout *verifyBtnLayout = new QHBoxLayout();
    verifyBtnLayout->setSpacing(10);

    QPushButton *verifyBtn = new QPushButton("🔐 验证连接", &dialog);
    verifyBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
        "QPushButton:hover { background-color: #2980b9; } "
        "QPushButton:disabled { background-color: #bdc3c7; }"
    );

    QLabel *verifyStatusLabel = new QLabel("", verifyFrame);
    verifyStatusLabel->setAlignment(Qt::AlignCenter);
    verifyStatusLabel->setStyleSheet("padding: 10px; border-radius: 4px; font-size: 13px;");
    verifyStatusLabel->setVisible(false);

    verifyBtnLayout->addStretch();
    verifyBtnLayout->addWidget(verifyBtn);
    verifyBtnLayout->addStretch();

    verifyLayout->addLayout(verifyBtnLayout);
    verifyLayout->addWidget(verifyStatusLabel);

    mainLayout->addWidget(verifyFrame);

    QCheckBox *defaultCheck = new QCheckBox("设为默认", &dialog);
    defaultCheck->setChecked(config.isDefault);
    defaultCheck->setStyleSheet("QCheckBox { font-size: 14px; padding: 5px; }");
    mainLayout->addWidget(defaultCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);

    QPushButton *okBtn = new QPushButton("✅ 保存", &dialog);
    okBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; padding: 10px 25px; border-radius: 5px; font-weight: bold; font-size: 14px; } "
        "QPushButton:hover { background-color: #219a52; }"
    );

    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #95a5a6; color: white; padding: 10px 25px; border-radius: 5px; font-size: 14px; } "
        "QPushButton:hover { background-color: #7f8c8d; }"
    );

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    QString verifyLogFileName;
    {
        QString logDir = QCoreApplication::applicationDirPath() + "/logs";
        QDir dir(logDir);
        if (!dir.exists()) {
            dir.mkpath(logDir);
        }
        verifyLogFileName = logDir + "/ai_image_verify_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    }

    QString originalApiKey = config.apiKey;
    QNetworkAccessManager *verifyManager = new QNetworkAccessManager(&dialog);

    connect(verifyBtn, &QPushButton::clicked, [&]() {
        QString apiKey = apiKeyEdit->text().trimmed();
        if (apiKey.isEmpty()) {
            apiKey = originalApiKey;
        }
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        if (apiKey.isEmpty()) {
            verifyStatusLabel->setText("❌ 没有可验证的API Key");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            return;
        }

        verifyBtn->setEnabled(false);
        verifyBtn->setText("🔄 验证中...");
        verifyBtn->setStyleSheet(
            "QPushButton { background-color: #f39c12; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
            "QPushButton:disabled { background-color: #f39c12; }"
        );
        verifyStatusLabel->setVisible(false);

        QList<AIImageModelInfo> models = AIConfig::instance().getAllImageModels();
        QString actualEndpoint;
        for (const AIImageModelInfo &model : models) {
            if (model.provider == providerId && model.id == modelId) {
                actualEndpoint = model.defaultEndpoint;
                break;
            }
        }
        if (actualEndpoint.isEmpty()) {
            actualEndpoint = endpoint;
        }

        if (actualEndpoint.isEmpty()) {
            verifyStatusLabel->setText("❌ 未配置API地址");
            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            verifyStatusLabel->setVisible(true);
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );
            return;
        }

        QUrl url(actualEndpoint);
        QNetworkRequest request;
        request.setUrl(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

        QJsonObject json;
        json["model"] = modelId;
        json["prompt"] = "test";
        json["n"] = 1;
        json["size"] = "1024x1024";

        QJsonDocument doc(json);

        AILogger::logVerifyRequest(verifyLogFileName, providerId, modelId, actualEndpoint, json);

        QPointer<QNetworkReply> reply = verifyManager->post(request, doc.toJson());

        connect(reply, &QNetworkReply::finished, [=]() {
            verifyBtn->setEnabled(true);
            verifyBtn->setText("🔐 验证连接");
            verifyBtn->setStyleSheet(
                "QPushButton { background-color: #3498db; color: white; padding: 10px 20px; border-radius: 5px; font-weight: bold; } "
                "QPushButton:hover { background-color: #2980b9; }"
            );

            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonParseError parseError;
                QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);

                AILogger::logVerifyResponse(verifyLogFileName, reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                                           responseDoc.object());

                if (parseError.error == QJsonParseError::NoError && !responseDoc.isNull()) {
                    if (providerId == "openai" || providerId == "siliconflow") {
                        if (responseDoc.object().contains("data") || responseDoc.object().contains("url")) {
                            verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #27ae60; background-color: #d5f4e6; border-radius: 4px;");
                        } else if (responseDoc.object().contains("error")) {
                            QString errorMsg = responseDoc.object()["error"].toObject()["message"].toString();
                            verifyStatusLabel->setText("❌ API错误: " + errorMsg);
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                        } else {
                            verifyStatusLabel->setText("⚠️ 连接成功，但响应格式异常");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #f39c12; background-color: #fdebd0; border-radius: 4px;");
                        }
                    } else if (providerId == "stability") {
                        if (responseDoc.object().contains("artifacts")) {
                            verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #27ae60; background-color: #d5f4e6; border-radius: 4px;");
                        } else if (responseDoc.object().contains("errors")) {
                            QString errorMsg = responseDoc.object()["errors"].toArray().first().toObject()["message"].toString();
                            verifyStatusLabel->setText("❌ API错误: " + errorMsg);
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
                        } else {
                            verifyStatusLabel->setText("⚠️ 连接成功，但响应格式异常");
                            verifyStatusLabel->setStyleSheet("padding: 10px; color: #f39c12; background-color: #fdebd0; border-radius: 4px;");
                        }
                    } else {
                        verifyStatusLabel->setText("✅ 连接成功！API Key有效");
                        verifyStatusLabel->setStyleSheet("padding: 10px; color: #27ae60; background-color: #d5f4e6; border-radius: 4px;");
                    }
                } else {
                    verifyStatusLabel->setText("⚠️ 连接成功，但响应解析失败");
                    verifyStatusLabel->setStyleSheet("padding: 10px; color: #f39c12; background-color: #fdebd0; border-radius: 4px;");
                }
            } else {
                QString errorMsg = reply->errorString();
                QByteArray errorData = reply->readAll();
                QString detailedError = errorMsg;

                if (!errorData.isEmpty()) {
                    QJsonParseError parseError;
                    QJsonDocument errorDoc = QJsonDocument::fromJson(errorData, &parseError);
                    if (parseError.error == QJsonParseError::NoError) {
                        if (errorDoc.object().contains("error")) {
                            detailedError = errorDoc.object()["error"].toObject()["message"].toString();
                        } else if (errorDoc.object().contains("errors")) {
                            detailedError = errorDoc.object()["errors"].toArray().first().toObject()["message"].toString();
                        }
                    }
                }

                AILogger::logVerifyError(verifyLogFileName, detailedError, errorData);

                verifyStatusLabel->setText("❌ 连接失败: " + detailedError);
                verifyStatusLabel->setStyleSheet("padding: 10px; color: #c0392b; background-color: #fadbd8; border-radius: 4px;");
            }
            verifyStatusLabel->setVisible(true);
            reply->deleteLater();
        });
    });

    connect(okBtn, &QPushButton::clicked, [&]() {
        QString apiKey = apiKeyEdit->text().trimmed();
        QString modelId = modelCombo->currentData().toString();
        if (modelId.isEmpty()) {
            modelId = modelCombo->currentText().trimmed();
        }
        QString providerId = providerCombo->currentData().toString();
        QString endpoint = endpointEdit->text().trimmed();

        QString originalApiKey = config.apiKey;
        if (apiKey.isEmpty()) {
            apiKey = originalApiKey;
        }

        if (apiKey.isEmpty()) {
            QMessageBox::warning(&dialog, "警告", "请输入API Key");
            return;
        }

        config.name = nameEdit->text().trimmed();
        config.provider = providerId;
        config.model = modelId;

        if (!apiKeyEdit->text().trimmed().isEmpty()) {
            config.apiKey = apiKey;
        }

        config.endpoint = endpoint;
        config.isDefault = defaultCheck->isChecked();

        AIConfig::instance().updateImageKey(config);
        loadAISettings();
        dialog.accept();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void SettingsWidget::onDeleteImageKey()
{
    int currentRow = aiImageKeysTable->currentRow();
    if (currentRow < 0) return;

    QString keyId = aiImageKeysTable->item(currentRow, 0)->data(Qt::UserRole).toString();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", "确定要删除此图像生成API配置吗？",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        AIConfig::instance().deleteImageKey(keyId);
        loadAISettings();
    }
}

void SettingsWidget::onSetDefaultImageKey()
{
    int currentRow = aiImageKeysTable->currentRow();
    if (currentRow < 0) return;

    QString keyId = aiImageKeysTable->item(currentRow, 0)->data(Qt::UserRole).toString();
    AIConfig::instance().setDefaultImageKey(keyId);
    loadAISettings();
}

QWidget *SettingsWidget::createStartupPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("开机启动", page);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QGroupBox *startupGroup = new QGroupBox("开机启动设置", page);
    QVBoxLayout *startupLayout = new QVBoxLayout(startupGroup);
    startupLayout->setSpacing(15);

    autoStartCheck = new QCheckBox("开机自动启动小马办公", page);
    autoStartCheck->setChecked(db->getAutoStart());
    connect(autoStartCheck, &QCheckBox::stateChanged, this, &SettingsWidget::onAutoStartToggled);
    startupLayout->addWidget(autoStartCheck);

    statusLabel = new QLabel();
    if (db->getAutoStart()) {
        statusLabel->setText("✓ 当前状态: 已启用");
        statusLabel->setStyleSheet("color: #4caf50; font-size: 13px; padding: 5px;");
    } else {
        statusLabel->setText("✗ 当前状态: 已禁用");
        statusLabel->setStyleSheet("color: #f44336; font-size: 13px; padding: 5px;");
    }
    startupLayout->addWidget(statusLabel);

    QLabel *startupHint = new QLabel("启用后，每次电脑开机时小马办公将自动启动并运行在系统托盘区域。", page);
    startupHint->setStyleSheet("color: #666; font-size: 12px; padding: 10px; background-color: #f5f5f5; border-radius: 5px;");
    startupHint->setWordWrap(true);
    startupLayout->addWidget(startupHint);

    layout->addWidget(startupGroup);

    QGroupBox *startupBehaviorGroup = new QGroupBox("启动行为", page);
    QVBoxLayout *behaviorLayout = new QVBoxLayout(startupBehaviorGroup);
    behaviorLayout->setSpacing(10);

    QRadioButton *startMinimized = new QRadioButton("启动时最小化到托盘", startupBehaviorGroup);
    startMinimized->setChecked(true);
    behaviorLayout->addWidget(startMinimized);

    QRadioButton *startNormal = new QRadioButton("启动时正常显示窗口", startupBehaviorGroup);
    behaviorLayout->addWidget(startNormal);

    layout->addWidget(startupBehaviorGroup);

    layout->addStretch();
    return page;
}

QWidget *SettingsWidget::createUpdatePage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("检查更新", page);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QGroupBox *autoUpdateGroup = new QGroupBox("自动更新", page);
    QVBoxLayout *autoUpdateLayout = new QVBoxLayout(autoUpdateGroup);
    autoUpdateLayout->setSpacing(10);

    autoCheckUpdateCheck = new QCheckBox("自动检查更新", page);
    autoCheckUpdateCheck->setChecked(db->getAutoCheckUpdate());
    connect(autoCheckUpdateCheck, &QCheckBox::stateChanged, this, &SettingsWidget::onAutoCheckUpdateToggled);
    autoUpdateLayout->addWidget(autoCheckUpdateCheck);

    QLabel *updateInfoLabel = new QLabel("启用后，软件启动时和后台每24小时会自动检查更新。", page);
    updateInfoLabel->setStyleSheet("color: #666; font-size: 12px; padding: 5px;");
    updateInfoLabel->setWordWrap(true);
    autoUpdateLayout->addWidget(updateInfoLabel);

    layout->addWidget(autoUpdateGroup);

    QGroupBox *checkUpdateGroup = new QGroupBox("手动检查更新", page);
    QVBoxLayout *checkLayout = new QVBoxLayout(checkUpdateGroup);
    checkLayout->setSpacing(15);

    QLabel *versionLabel = new QLabel("当前版本: v1.0.3", page);
    versionLabel->setStyleSheet("font-size: 14px; color: #333; padding: 5px;");
    checkLayout->addWidget(versionLabel);

    checkUpdateButton = new QPushButton("🔄 检查更新", page);
    checkUpdateButton->setStyleSheet(
        "QPushButton { background-color: #2196f3; color: white; padding: 12px 30px; border-radius: 5px; font-size: 14px; font-weight: bold; } "
        "QPushButton:hover { background-color: #1976d2; } "
        "QPushButton:pressed { background-color: #1565c0; }"
    );
    connect(checkUpdateButton, &QPushButton::clicked, this, &SettingsWidget::onCheckUpdateClicked);
    checkLayout->addWidget(checkUpdateButton);

    layout->addWidget(checkUpdateGroup);

    QGroupBox *cloudGroup = new QGroupBox("云端同步", page);
    QVBoxLayout *cloudLayout = new QVBoxLayout(cloudGroup);
    QHBoxLayout *cloudBtnLayout = new QHBoxLayout();
    cloudLoginBtn = new QPushButton("☁️ 登录云端", page);
    cloudSyncBtn = new QPushButton("🔄 同步配置", page);
    cloudChangePasswordBtn = new QPushButton("🔐 修改密码", page);
    cloudBtnLayout->addWidget(cloudLoginBtn);
    cloudBtnLayout->addWidget(cloudSyncBtn);
    cloudBtnLayout->addWidget(cloudChangePasswordBtn);
    cloudStatusLabel = new QLabel("未登录", page);
    cloudStatusLabel->setStyleSheet("color: #888;");
    cloudLayout->addLayout(cloudBtnLayout);
    cloudLayout->addWidget(cloudStatusLabel);

    // 冲突策略选择
    QHBoxLayout* conflictLayout = new QHBoxLayout();
    QLabel* conflictLabel = new QLabel("冲突处理策略:", page);
    conflictStrategyCombo = new QComboBox(page);
    conflictStrategyCombo->addItem("本地优先", static_cast<int>(SyncConflictStrategy_Local));
    conflictStrategyCombo->addItem("云端优先", static_cast<int>(SyncConflictStrategy_Cloud));
    conflictStrategyCombo->addItem("每次手动选择", static_cast<int>(SyncConflictStrategy_Manual));
    conflictLayout->addWidget(conflictLabel);
    conflictLayout->addWidget(conflictStrategyCombo);
    conflictLayout->addStretch();
    cloudLayout->addLayout(conflictLayout);

    // 加载保存的冲突策略
    QSettings settings;
    int savedStrategy = settings.value("sync/conflictStrategy", 0).toInt();
    conflictStrategyCombo->setCurrentIndex(conflictStrategyCombo->findData(savedStrategy));
    connect(conflictStrategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                QSettings settings;
                settings.setValue("sync/conflictStrategy", conflictStrategyCombo->itemData(index).toInt());
            });

    layout->addWidget(cloudGroup);

    connect(cloudLoginBtn, &QPushButton::clicked, this, &SettingsWidget::onCloudLoginClicked);
    connect(cloudSyncBtn, &QPushButton::clicked, this, &SettingsWidget::onCloudSyncClicked);
    connect(cloudChangePasswordBtn, &QPushButton::clicked, this, &SettingsWidget::onCloudChangePasswordClicked);
    connect(UserManager::instance(), &UserManager::loginSuccess, this, &SettingsWidget::onCloudLoginSuccess);
    connect(UserManager::instance(), &UserManager::logoutComplete, this, [this]() {
        cloudStatusLabel->setText("未登录");
        cloudStatusLabel->setStyleSheet("color: #888;");
        cloudLoginBtn->setText("☁️ 登录云端");
    });

    if (UserManager::instance()->isLoggedIn()) {
        cloudStatusLabel->setText("已登录: " + UserManager::instance()->currentUser().email);
        cloudStatusLabel->setStyleSheet("color: green;");
        cloudLoginBtn->setText("退出登录");
    }

    layout->addStretch();

    QGroupBox *updateChannelGroup = new QGroupBox("更新通道", page);
    QVBoxLayout *channelLayout = new QVBoxLayout(updateChannelGroup);
    channelLayout->setSpacing(10);

    QRadioButton *stableChannel = new QRadioButton("稳定版 (推荐)", updateChannelGroup);
    stableChannel->setChecked(true);
    channelLayout->addWidget(stableChannel);

    QRadioButton *betaChannel = new QRadioButton("测试版", updateChannelGroup);
    betaChannel->setChecked(false);
    channelLayout->addWidget(betaChannel);

    QLabel *channelHint = new QLabel("测试版可能包含最新功能但可能不稳定", updateChannelGroup);
    channelHint->setStyleSheet("color: #999; font-size: 11px; padding: 5px;");
    channelLayout->addWidget(channelHint);

    layout->addWidget(updateChannelGroup);

    layout->addStretch();
    return page;
}

QWidget *SettingsWidget::createRemoteDesktopPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("远程桌面", page);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QGroupBox *autoControlGroup = new QGroupBox("自动控制", page);
    QVBoxLayout *autoControlLayout = new QVBoxLayout(autoControlGroup);
    autoControlLayout->setSpacing(15);

    remoteDesktopAutoStartCheck = new QCheckBox("自动启动", page);
    remoteDesktopAutoStartCheck->setChecked(db->getRemoteDesktopAutoStart());
    connect(remoteDesktopAutoStartCheck, &QCheckBox::stateChanged, this, [this](int state) {
        db->setRemoteDesktopAutoStart(state == Qt::Checked);
    });

    QLabel *autoStartLabel = new QLabel("随软件启动自动连接远程桌面", page);
    autoStartLabel->setStyleSheet("color: #666; font-size: 12px; padding-left: 24px;");
    autoControlLayout->addWidget(remoteDesktopAutoStartCheck);
    autoControlLayout->addWidget(autoStartLabel);

    remoteDesktopAutoStopCheck = new QCheckBox("自动停止", page);
    remoteDesktopAutoStopCheck->setChecked(db->getRemoteDesktopAutoStop());
    connect(remoteDesktopAutoStopCheck, &QCheckBox::stateChanged, this, [this](int state) {
        db->setRemoteDesktopAutoStop(state == Qt::Checked);
    });

    QLabel *autoStopLabel = new QLabel("随软件关闭自动断开远程桌面", page);
    autoStopLabel->setStyleSheet("color: #666; font-size: 12px; padding-left: 24px;");
    autoControlLayout->addWidget(remoteDesktopAutoStopCheck);
    autoControlLayout->addWidget(autoStopLabel);

    layout->addWidget(autoControlGroup);

    layout->addStretch();
    return page;
}

QWidget *SettingsWidget::createAboutPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QLabel *title = new QLabel("关于", page);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    layout->addWidget(title);

    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    QGroupBox *appInfoGroup = new QGroupBox("应用信息", page);
    QVBoxLayout *infoLayout = new QVBoxLayout(appInfoGroup);
    infoLayout->setSpacing(15);

    QLabel *appNameLabel = new QLabel("小马办公", page);
    appNameLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #6200ea;");
    infoLayout->addWidget(appNameLabel);

    QLabel *versionLabel = new QLabel("版本: v1.0.3", page);
    versionLabel->setStyleSheet("font-size: 14px; color: #666;");
    infoLayout->addWidget(versionLabel);

    QLabel *descLabel = new QLabel("一个功能完善的桌面办公助手应用", page);
    descLabel->setStyleSheet("font-size: 13px; color: #888; padding: 10px 0;");
    infoLayout->addWidget(descLabel);

    layout->addWidget(appInfoGroup);

    QGroupBox *featuresGroup = new QGroupBox("主要功能", page);
    QVBoxLayout *featuresLayout = new QVBoxLayout(featuresGroup);
    featuresLayout->setSpacing(8);

    QStringList features = {
        "📱 应用管理 - 管理和快速启动常用应用",
        "📁 集合管理 - 自定义应用分组和批量启动",
        "⏰ 定时关机 - 定时关机/重启/休眠",
        "🤖 AI智能 - 任务分析和报告生成",
        "📊 工作日志 - 记录和分析工作情况"
    };

    for (const QString &feature : features) {
        QLabel *featureLabel = new QLabel(feature, page);
        featureLabel->setStyleSheet("font-size: 13px; color: #555; padding: 5px;");
        featuresLayout->addWidget(featureLabel);
    }

    layout->addWidget(featuresGroup);

    QGroupBox *techGroup = new QGroupBox("技术信息", page);
    QVBoxLayout *techLayout = new QVBoxLayout(techGroup);
    techLayout->setSpacing(8);

    QLabel *techLabel1 = new QLabel("• Qt 5.15.2", techGroup);
    techLabel1->setStyleSheet("font-size: 12px; color: #666;");
    techLayout->addWidget(techLabel1);

    QLabel *techLabel2 = new QLabel("• MinGW 8.1.0", techGroup);
    techLabel2->setStyleSheet("font-size: 12px; color: #666;");
    techLayout->addWidget(techLabel2);

    QLabel *techLabel3 = new QLabel("• Windows COM 自动化", techGroup);
    techLabel3->setStyleSheet("font-size: 12px; color: #666;");
    techLayout->addWidget(techLabel3);

    layout->addWidget(techGroup);

    layout->addStretch();

    QLabel *copyrightLabel = new QLabel("© 2026 小马办公. All rights reserved.", page);
    copyrightLabel->setStyleSheet("color: #bbb; font-size: 12px; padding: 20px; text-align: center;");
    copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyrightLabel);

    return page;
}

void SettingsWidget::onAutoStartToggled(int state)
{
    bool enabled = (state == Qt::Checked);
    
    if (db->setAutoStart(enabled)) {
        if (enabled) {
            statusLabel->setText("✓ 当前状态: 已启用");
            statusLabel->setStyleSheet("color: #4caf50; font-size: 13px; padding: 5px;");
            QMessageBox::information(this, "成功", "开机自动启动已启用！");
        } else {
            statusLabel->setText("✗ 当前状态: 已禁用");
            statusLabel->setStyleSheet("color: #f44336; font-size: 13px; padding: 5px;");
            QMessageBox::information(this, "成功", "开机自动启动已禁用！");
        }
    } else {
        QMessageBox::warning(this, "错误", "设置开机自动启动失败！");
        autoStartCheck->setChecked(!enabled);
    }
}

void SettingsWidget::onMinimizeToTrayToggled(int state)
{
    bool enabled = (state == Qt::Checked);
    
    if (db->setMinimizeToTray(enabled)) {
        QMessageBox::information(this, "成功", QString("最小化到系统托盘已%1！").arg(enabled ? "启用" : "禁用"));
    } else {
        QMessageBox::warning(this, "错误", "设置失败！");
        minimizeToTrayCheck->setChecked(!enabled);
    }
}

void SettingsWidget::onShowClosePromptToggled(int state)
{
    bool show = (state == Qt::Checked);
    
    if (db->setShowClosePrompt(show)) {
        QMessageBox::information(this, "成功", QString("关闭提示已%1！").arg(show ? "启用" : "禁用"));
    } else {
        QMessageBox::warning(this, "错误", "设置失败！");
        showClosePromptCheck->setChecked(!show);
    }
}

void SettingsWidget::onShowBottomAppBarToggled(int state)
{
    bool show = (state == Qt::Checked);
    
    if (db->setShowBottomAppBar(show)) {
        // 设置已保存，强制刷新底部应用条的显示状态
        if (mainWindow) {
            mainWindow->refreshBottomAppBarVisibility();
        }
    } else {
        QMessageBox::warning(this, "错误", "设置失败！");
        showBottomAppBarCheck->setChecked(!show);
    }
}

void SettingsWidget::onAutoCheckUpdateToggled(int state)
{
    bool enabled = (state == Qt::Checked);
    
    if (db->setAutoCheckUpdate(enabled)) {
        QMessageBox::information(this, "成功", QString("自动检查更新已%1！").arg(enabled ? "启用" : "禁用"));
    } else {
        QMessageBox::warning(this, "错误", "设置失败！");
        autoCheckUpdateCheck->setChecked(!enabled);
    }
}

void SettingsWidget::onAboutClicked()
{
    // 关于页面已集成到左侧导航中，无需额外处理
}

void SettingsWidget::onCheckUpdateClicked()
{
    if (!updateManager) {
        QMessageBox::warning(this, "错误", "更新管理器未初始化");
        return;
    }
    
    checkUpdateButton->setEnabled(false);
    checkUpdateButton->setText("检查中...");
    updateManager->checkForUpdates();
}

void SettingsWidget::onUpdateAvailable(const UpdateInfo &info)
{
    Q_UNUSED(info);
    checkUpdateButton->setEnabled(true);
    checkUpdateButton->setText("🔄 检查更新");
}

void SettingsWidget::onNoUpdateAvailable()
{
    checkUpdateButton->setEnabled(true);
    checkUpdateButton->setText("🔄 检查更新");
    statusLabel->setText("✅ 当前已是最新版本");
    QTimer::singleShot(3000, this, [this]() {
        statusLabel->setText("");
    });
}

void SettingsWidget::onUpdateCheckFailed(const QString &error)
{
    Q_UNUSED(error);
    checkUpdateButton->setEnabled(true);
    checkUpdateButton->setText("🔄 检查更新");
    QMessageBox::warning(this, "检查更新", "检查更新失败，请检查网络连接！");
}

void SettingsWidget::onOpenAISettings()
{
    AISettingsDialog dialog(this);
    dialog.exec();
}

bool SettingsWidget::isShortcutConflict(const QString &shortcut)
{
    QStringList conflictShortcuts = {
        "Ctrl+A", "Ctrl+Z", "Ctrl+Y", "F1", "F12", "Win+Tab"
    };
    
    QString normalizedShortcut = shortcut.toUpper().replace(" ", "");
    for (const QString &conflict : conflictShortcuts) {
        if (normalizedShortcut == conflict.toUpper().replace(" ", "")) {
            return true;
        }
    }
    
    if (shortcut.contains("Ctrl+Alt+", Qt::CaseInsensitive) || 
        shortcut.contains("Ctrl+Shift+Alt+", Qt::CaseInsensitive) ||
        shortcut.contains("Win+Ctrl", Qt::CaseInsensitive) ||
        shortcut.contains("Win+Alt", Qt::CaseInsensitive)) {
        return true;
    }
    
    if (shortcut.contains("Ctrl+Shift", Qt::CaseInsensitive) && 
        (shortcut.endsWith("T", Qt::CaseInsensitive) || 
         shortcut.endsWith("N", Qt::CaseInsensitive) ||
         shortcut.endsWith("W", Qt::CaseInsensitive))) {
        return true;
    }
    
    return false;
}

void SettingsWidget::updateCloudLoginStatus(const UserInfo& user) {
    qDebug() << "[SettingsWidget] 更新云端登录状态:" << user.email;
    cloudStatusLabel->setText("已登录：" + user.email);
    cloudStatusLabel->setStyleSheet("color: green;");
    cloudLoginBtn->setText("退出登录");
}
