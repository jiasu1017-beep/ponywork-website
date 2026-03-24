#include "remotedesktopwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QApplication>
#include <QStyle>
#include <QProgressDialog>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTextStream>
#include <QPropertyAnimation>
#include <QFile>
#include <QIODevice>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QHostInfo>
#include <algorithm>
#include "modules/user/userapi.h"

void RemoteDesktopWidget::launchRemoteDesktop(const RemoteDesktopConnection &conn, Database *db)
{
    ApplicationManager::launchRemoteDesktop(conn, db);
}

RemoteDesktopWidget::RemoteDesktopWidget(Database *db, QWidget *parent)
    : QWidget(parent), db(db)
{
    setupUI();
    refreshConnectionList();
}

void RemoteDesktopWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();

    QLabel *searchLabel = new QLabel("搜索:");
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("输入名称、主机地址、用户名等进行搜索...");
    connect(searchEdit, &QLineEdit::textChanged, this, &RemoteDesktopWidget::onSearchTextChanged);

    QLabel *categoryLabel = new QLabel("分类:");
    categoryFilter = new QComboBox();
    categoryFilter->addItem("全部", "");
    categoryFilter->addItem("未分类", "未分类");
    categoryFilter->addItem("内网", "内网");
    categoryFilter->addItem("外网", "外网");
    categoryFilter->addItem("工作", "工作");
    categoryFilter->addItem("个人", "个人");
    categoryFilter->addItem("测试", "测试");
    connect(categoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshConnectionList();
    });

    topLayout->addWidget(searchLabel);
    topLayout->addWidget(searchEdit);
    topLayout->addWidget(categoryLabel);
    topLayout->addWidget(categoryFilter);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    connectionTable = new QTableWidget();
    connectionTable->setColumnCount(8);
    connectionTable->setHorizontalHeaderLabels(QStringList() << "名称" << "主机地址" << "端口" << "用户名" << "分类" << "备注" << "收藏" << "最后使用");
    connectionTable->horizontalHeader()->setStretchLastSection(false);
    connectionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    connectionTable->setColumnWidth(0, 150);
    connectionTable->setColumnWidth(1, 250);
    connectionTable->setColumnWidth(2, 60);
    connectionTable->setColumnWidth(3, 100);
    connectionTable->setColumnWidth(4, 80);
    connectionTable->setColumnWidth(5, 150);
    connectionTable->setColumnWidth(6, 50);
    connectionTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    connectionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    connectionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connectionTable->setAlternatingRowColors(true);
    connectionTable->verticalHeader()->setVisible(false);
    connectionTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connectionTable->setStyleSheet(
        "QTableWidget::item:selected {"
        "   background-color: #2196F3;"
        "   color: white;"
        "   font-weight: bold;"
        "}"
        "QTableWidget::item:selected:hover {"
        "   background-color: #1976D2;"
        "}"
        "QTableWidget::item:hover:!selected {"
        "   background-color: #E3F2FD;"
        "}"
    );
    connect(connectionTable, &QTableWidget::itemSelectionChanged, this, &RemoteDesktopWidget::onConnectionSelectionChanged);
    connect(connectionTable, &QTableWidget::doubleClicked, this, [this](const QModelIndex &index) {
        Q_UNUSED(index);
        onConnect();
    });
    connect(connectionTable, &QTableWidget::customContextMenuRequested, this, &RemoteDesktopWidget::onTableContextMenuRequested);

    mainLayout->addWidget(connectionTable);

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    addButton = new QPushButton("添加连接");
    addButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    connect(addButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onAddConnection);

    editButton = new QPushButton("编辑连接");
    editButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    editButton->setEnabled(false);
    connect(editButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onEditConnection);

    deleteButton = new QPushButton("删除连接");
    deleteButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    deleteButton->setEnabled(false);
    connect(deleteButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onDeleteConnection);

    favoriteButton = new QPushButton("收藏");
    favoriteButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogYesButton));
    favoriteButton->setEnabled(false);
    connect(favoriteButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onToggleFavorite);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(favoriteButton);
    buttonLayout->addStretch();

    connectButton = new QPushButton("连接");
    connectButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connectButton->setEnabled(false);
    connect(connectButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onConnect);

    testButton = new QPushButton("测试连接");
    testButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    testButton->setEnabled(false);
    connect(testButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onTestConnection);

    importButton = new QPushButton("导入");
    importButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton));
    connect(importButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onImportConnections);

    exportButton = new QPushButton("导出");
    exportButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(exportButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onExportConnections);

    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setStyleSheet("background-color: #ccc;");

    moveUpButton = new QPushButton("上移");
    moveUpButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
    moveUpButton->setEnabled(false);
    connect(moveUpButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onMoveUp);

    moveDownButton = new QPushButton("下移");
    moveDownButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
    moveDownButton->setEnabled(false);
    connect(moveDownButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onMoveDown);

    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(testButton);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(separator);
    buttonLayout->addWidget(moveUpButton);
    buttonLayout->addWidget(moveDownButton);

    mainLayout->addLayout(buttonLayout);

    // 添加FRPC控制区域
    setupFRPCUI(mainLayout);
}

void RemoteDesktopWidget::setupFRPCUI(QVBoxLayout *mainLayout)
{
    // 初始化FRPC管理器
    frpcManager = FRPCManager::instance();
    frpcManager->initialize(db);

    // 设置用户ID用于生成唯一端口
    FRPCConfig initConfig = frpcManager->getConfig();
    initConfig.userId = UserManager::instance()->currentUser().id;
    frpcManager->setConfig(initConfig);

    // 检测是否有已运行的 frpc.exe 进程
    frpcManager->checkExistingProcess();

    frpcGroupBox = new QGroupBox("FRPC 远程桌面中继");
    frpcGroupBox->setStyleSheet("QGroupBox { font-weight: bold; }");

    QVBoxLayout *frpcLayout = new QVBoxLayout();

    // 设备名称行
    QHBoxLayout *deviceNameLayout = new QHBoxLayout();
    QLabel *deviceNameLabel = new QLabel("设备名称:");
    frpcDeviceNameEdit = new QLineEdit();
    frpcDeviceNameEdit->setText(frpcManager->getConfig().deviceName);
    frpcDeviceNameEdit->setPlaceholderText("输入设备名称，用于标识这台电脑");
    deviceNameLayout->addWidget(deviceNameLabel);
    deviceNameLayout->addWidget(frpcDeviceNameEdit);
    frpcLayout->addLayout(deviceNameLayout);

    // 状态显示行
    QHBoxLayout *statusLayout = new QHBoxLayout();
    QLabel *statusTitleLabel = new QLabel("连接状态:");
    frpcStatusLabel = new QLabel("未连接");
    frpcStatusLabel->setStyleSheet("color: #888; font-weight: bold;");
    QLabel *portTitleLabel = new QLabel("远程端口:");
    frpcPortLabel = new QLabel("--");
    frpcPortLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    statusLayout->addWidget(statusTitleLabel);
    statusLayout->addWidget(frpcStatusLabel);
    statusLayout->addSpacing(30);
    statusLayout->addWidget(portTitleLabel);
    statusLayout->addWidget(frpcPortLabel);
    statusLayout->addStretch();
    frpcLayout->addLayout(statusLayout);

    // 按钮行
    QHBoxLayout *buttonRowLayout = new QHBoxLayout();
    frpcQuickSetupButton = new QPushButton("一键设置");
    frpcQuickSetupButton->setToolTip("启动FRPC并生成RDP文件");
    frpcQuickSetupButton->setStyleSheet("font-weight: bold;");
    connect(frpcQuickSetupButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onFRPCQuickSetup);

    frpcStartButton = new QPushButton("开启");
    frpcStartButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    connect(frpcStartButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onFRPCStart);

    frpcStopButton = new QPushButton("关闭");
    frpcStopButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
    frpcStopButton->setEnabled(false);
    connect(frpcStopButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onFRPCStop);

    frpcExportButton = new QPushButton("导出RDP文件");
    frpcExportButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(frpcExportButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onFRPCExportRDP);

    frpcSettingsButton = new QPushButton("参数设置");
    frpcSettingsButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(frpcSettingsButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onFRPCSettings);

    frpcAddToListButton = new QPushButton("添加到列表");
    frpcAddToListButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirLinkIcon));
    frpcAddToListButton->setEnabled(false);
    connect(frpcAddToListButton, &QPushButton::clicked, this, &RemoteDesktopWidget::onFRPCAddToList);

    buttonRowLayout->addWidget(frpcQuickSetupButton);
    buttonRowLayout->addWidget(frpcStartButton);
    buttonRowLayout->addWidget(frpcStopButton);
    buttonRowLayout->addWidget(frpcExportButton);
    buttonRowLayout->addWidget(frpcAddToListButton);
    buttonRowLayout->addWidget(frpcSettingsButton);
    buttonRowLayout->addStretch();
    frpcLayout->addLayout(buttonRowLayout);

    frpcGroupBox->setLayout(frpcLayout);
    mainLayout->addWidget(frpcGroupBox);

    // 连接FRPC信号
    connect(frpcManager, &FRPCManager::statusChanged, this, &RemoteDesktopWidget::onFRPCStatusChanged);
    connect(frpcManager, &FRPCManager::errorOccurred, this, &RemoteDesktopWidget::onFRPCError);
    connect(frpcManager, &FRPCManager::remotePortChanged, this, &RemoteDesktopWidget::onFRPCPortChanged);
    connect(frpcManager, &FRPCManager::stopped, this, &RemoteDesktopWidget::onFRPCStopped);

    // 连接ConfigSync信号用于FRPC配置同步
    ConfigSync *configSync = ConfigSync::instance();
    connect(configSync, &ConfigSync::frpcConfigLoaded, this, [this](const QJsonObject &frpcConfig) {
        // 从云端加载FRPC配置
        FRPCConfig config;
        config.userId = UserManager::instance()->currentUser().id;
        config.serverAddr = frpcConfig["serverAddr"].toString("8.163.37.74");
        config.serverPort = frpcConfig["serverPort"].toInt(7000);
        config.localPort = frpcConfig["localPort"].toInt(3389);
        config.remotePort = frpcConfig["remotePort"].toInt(0);
        config.deviceName = frpcConfig["deviceName"].toString();
        config.isEnabled = frpcConfig["isEnabled"].toBool(false);
        frpcManager->setConfig(config);
        frpcDeviceNameEdit->setText(config.deviceName);
    });

    // 如果已登录，自动同步FRPC配置
    if (UserManager::instance()->isLoggedIn()) {
        configSync->loadFRPCConfig();
    }

    // 更新初始状态
    updateFRPCStatus();
}

void RemoteDesktopWidget::refreshConnectionList()
{
    QList<RemoteDesktopConnection> connections;
    QString searchText = searchEdit->text().trimmed();
    QString categoryFilterText = categoryFilter->currentData().toString();

    if (!searchText.isEmpty()) {
        connections = db->searchRemoteDesktops(searchText);
    } else {
        connections = db->getAllRemoteDesktops();
    }

    if (!categoryFilterText.isEmpty()) {
        QList<RemoteDesktopConnection> filtered;
        for (const RemoteDesktopConnection &conn : connections) {
            if (conn.category == categoryFilterText) {
                filtered.append(conn);
            }
        }
        connections = filtered;
    }
    
    std::sort(connections.begin(), connections.end(), [](const RemoteDesktopConnection &a, const RemoteDesktopConnection &b) {
        return a.sortOrder < b.sortOrder;
    });

    loadConnections(connections);
}

void RemoteDesktopWidget::loadConnections(const QList<RemoteDesktopConnection> &connections)
{
    connectionTable->setRowCount(0);

    for (const RemoteDesktopConnection &conn : connections) {
        int row = connectionTable->rowCount();
        connectionTable->insertRow(row);

        connectionTable->setItem(row, 0, new QTableWidgetItem(conn.name));
        connectionTable->setItem(row, 1, new QTableWidgetItem(conn.hostAddress));
        connectionTable->setItem(row, 2, new QTableWidgetItem(QString::number(conn.port)));
        connectionTable->setItem(row, 3, new QTableWidgetItem(conn.username));
        connectionTable->setItem(row, 4, new QTableWidgetItem(conn.category));
        connectionTable->setItem(row, 5, new QTableWidgetItem(conn.notes));
        connectionTable->setItem(row, 6, new QTableWidgetItem(conn.isFavorite ? "★" : ""));
        connectionTable->setItem(row, 7, new QTableWidgetItem(conn.lastUsedTime.toString("yyyy-MM-dd hh:mm")));

        connectionTable->item(row, 0)->setData(Qt::UserRole, conn.id);
    }

    updateConnectionButtons();
}

void RemoteDesktopWidget::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    refreshConnectionList();
}

void RemoteDesktopWidget::onAddConnection()
{
    RemoteDesktopDialog dialog(db, this);
    if (dialog.exec() == QDialog::Accepted) {
        RemoteDesktopConnection conn = dialog.getConnection();
        
        QList<RemoteDesktopConnection> allConnections = db->getAllRemoteDesktops();
        int maxOrder = 0;
        for (const RemoteDesktopConnection &c : allConnections) {
            if (c.sortOrder > maxOrder) {
                maxOrder = c.sortOrder;
            }
        }
        conn.sortOrder = maxOrder + 1;
        
        if (db->addRemoteDesktop(conn)) {
            refreshConnectionList();
            QMessageBox::information(this, "成功", "连接添加成功！");
        } else {
            QMessageBox::warning(this, "错误", "添加连接失败！");
        }
    }
}

void RemoteDesktopWidget::onEditConnection()
{
    RemoteDesktopConnection conn = getSelectedConnection();
    if (conn.id == -1) return;

    RemoteDesktopDialog dialog(db, conn, this);
    if (dialog.exec() == QDialog::Accepted) {
        RemoteDesktopConnection updatedConn = dialog.getConnection();
        updatedConn.id = conn.id;
        updatedConn.createdTime = conn.createdTime;
        if (db->updateRemoteDesktop(updatedConn)) {
            refreshConnectionList();
            QMessageBox::information(this, "成功", "连接更新成功！");
        } else {
            QMessageBox::warning(this, "错误", "更新连接失败！");
        }
    }
}

void RemoteDesktopWidget::onDeleteConnection()
{
    RemoteDesktopConnection conn = getSelectedConnection();
    if (conn.id == -1) return;

    auto reply = QMessageBox::question(this, "确认删除",
        QString("确定要删除连接 \"%1\" 吗？").arg(conn.name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (db->deleteRemoteDesktop(conn.id)) {
            refreshConnectionList();
            QMessageBox::information(this, "成功", "连接已删除！");
        } else {
            QMessageBox::warning(this, "错误", "删除连接失败！");
        }
    }
}

void RemoteDesktopWidget::onMoveUp()
{
    int currentRow = connectionTable->currentRow();
    if (currentRow <= 0) return;

    QTableWidgetItem *currentItem = connectionTable->item(currentRow, 0);
    QTableWidgetItem *prevItem = connectionTable->item(currentRow - 1, 0);
    
    if (!currentItem || !prevItem) return;

    int currentId = currentItem->data(Qt::UserRole).toInt();
    int prevId = prevItem->data(Qt::UserRole).toInt();

    RemoteDesktopConnection currentConn = db->getRemoteDesktopById(currentId);
    RemoteDesktopConnection prevConn = db->getRemoteDesktopById(prevId);

    int tempOrder = currentConn.sortOrder;
    currentConn.sortOrder = prevConn.sortOrder;
    prevConn.sortOrder = tempOrder;

    db->updateRemoteDesktop(currentConn);
    db->updateRemoteDesktop(prevConn);

    refreshConnectionList();
    connectionTable->selectRow(currentRow - 1);
    updateConnectionButtons();
}

void RemoteDesktopWidget::onMoveDown()
{
    int currentRow = connectionTable->currentRow();
    int totalRows = connectionTable->rowCount();
    if (currentRow < 0 || currentRow >= totalRows - 1) return;

    QTableWidgetItem *currentItem = connectionTable->item(currentRow, 0);
    QTableWidgetItem *nextItem = connectionTable->item(currentRow + 1, 0);
    
    if (!currentItem || !nextItem) return;

    int currentId = currentItem->data(Qt::UserRole).toInt();
    int nextId = nextItem->data(Qt::UserRole).toInt();

    RemoteDesktopConnection currentConn = db->getRemoteDesktopById(currentId);
    RemoteDesktopConnection nextConn = db->getRemoteDesktopById(nextId);

    int tempOrder = currentConn.sortOrder;
    currentConn.sortOrder = nextConn.sortOrder;
    nextConn.sortOrder = tempOrder;

    db->updateRemoteDesktop(currentConn);
    db->updateRemoteDesktop(nextConn);

    refreshConnectionList();
    connectionTable->selectRow(currentRow + 1);
    updateConnectionButtons();
}

void RemoteDesktopWidget::onConnect()
{
    RemoteDesktopConnection conn = getSelectedConnection();
    if (conn.id == -1) return;

    QString targetName = "TERMSRV/" + conn.hostAddress;

    if (!conn.username.isEmpty() && !conn.password.isEmpty()) {
        QString username = conn.username;
        if (!conn.domain.isEmpty()) {
            username = conn.domain + "\\" + username;
        }

        QStringList cmdkeyArgs;
        cmdkeyArgs << "/generic:" + targetName << "/user:" + username << "/pass:" + conn.password;

        QProcess::execute("cmdkey.exe", cmdkeyArgs);
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString rdpFilePath = tempDir + "/PonyWork_RDP_" + QString::number(conn.id) + ".rdp";

    QFile rdpFile(rdpFilePath);
    if (!rdpFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建临时RDP文件！");
        return;
    }

    QTextStream out(&rdpFile);
    out.setCodec("UTF-8");

    QString fullAddress = conn.hostAddress;
    if (conn.port != 3389) {
        fullAddress += ":" + QString::number(conn.port);
    }

    out << "full address:s:" << fullAddress << "\n";
    out << "screen mode id:i:" << (conn.fullScreen ? "2" : "1") << "\n";
    out << "desktopwidth:i:" << conn.screenWidth << "\n";
    out << "desktopheight:i:" << conn.screenHeight << "\n";
    out << "use multimon:i:" << (conn.useAllMonitors ? "1" : "0") << "\n";
    out << "audiomode:i:" << (conn.enableAudio ? "0" : "2") << "\n";
    out << "redirectclipboard:i:" << (conn.enableClipboard ? "1" : "0") << "\n";
    out << "redirectprinters:i:" << (conn.enablePrinter ? "1" : "0") << "\n";
    out << "redirectdrives:i:" << (conn.enableDrive ? "1" : "0") << "\n";
    out << "authentication level:i:2\n";
    out << "prompt for credentials:i:0\n";
    out << "administrative session:i:0\n";

    if (!conn.username.isEmpty()) {
        QString username = conn.username;
        if (!conn.domain.isEmpty()) {
            username = conn.domain + "\\" + username;
        }
        out << "username:s:" << username << "\n";
    }

    rdpFile.close();

    conn.lastUsedTime = QDateTime::currentDateTime();
    db->updateRemoteDesktop(conn);

    QStringList args;
    args << rdpFilePath;

    QProcess::startDetached("mstsc.exe", args);
}

void RemoteDesktopWidget::onTestConnection()
{
    RemoteDesktopConnection conn = getSelectedConnection();
    if (conn.id == -1) return;

    QProgressDialog progress("正在测试连接...", "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    QTcpSocket *socket = new QTcpSocket(this);
    bool success = false;
    QEventLoop loop;

    connect(socket, &QTcpSocket::connected, this, [&]() {
        success = true;
        socket->disconnectFromHost();
        loop.quit();
    });

    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this, [&](QAbstractSocket::SocketError err) {
        Q_UNUSED(err);
        success = false;
        loop.quit();
    });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);

    socket->connectToHost(conn.hostAddress, conn.port);
    loop.exec();

    progress.close();
    socket->deleteLater();

    if (success) {
        QMessageBox::information(this, "测试成功", "连接测试成功！主机端口可访问。");
    } else {
        QMessageBox::warning(this, "测试失败", "连接测试失败！请检查主机地址、端口和网络连接。");
    }
}

void RemoteDesktopWidget::onImportConnections()
{
    QString fileName = QFileDialog::getOpenFileName(this, "导入连接配置",
        "", "RDP 文件 (*.rdp);;JSON 文件 (*.json);;所有文件 (*.*)");

    if (fileName.isEmpty()) return;

    QString suffix = QFileInfo(fileName).suffix().toLower();

    if (suffix == "rdp") {
        importFromRDPFile(fileName);
        return;
    }

    importFromJSONFile(fileName);
}

void RemoteDesktopWidget::importFromRDPFile(const QString &filePath) {
    RDPConnectionInfo rdpInfo = parseRDPFile(filePath);

    if (!rdpInfo.isValid) {
        QMessageBox::warning(this, "导入失败",
            QString("解析RDP文件失败：\n%1").arg(rdpInfo.errorMessage));
        return;
    }

    RemoteDesktopConnection conn = rdpInfoToConnection(rdpInfo);

    // 以文件名作为远程桌面名称
    QFileInfo fileInfo(filePath);
    conn.name = fileInfo.baseName();
    
    // 分类默认为"工作"
    conn.category = "工作";
    
    // 设置排序序号
    QList<RemoteDesktopConnection> allConnections = db->getAllRemoteDesktops();
    int maxOrder = 0;
    for (const RemoteDesktopConnection &c : allConnections) {
        if (c.sortOrder > maxOrder) {
            maxOrder = c.sortOrder;
        }
    }
    conn.sortOrder = maxOrder + 1;

    RemoteDesktopDialog dialog(db, conn, this);
    dialog.setWindowTitle("导入RDP连接 - 确认并编辑");

    if (dialog.exec() == QDialog::Accepted) {
        RemoteDesktopConnection finalConn = dialog.getConnection();
        if (db->addRemoteDesktop(finalConn)) {
            refreshConnectionList();
            QMessageBox::information(this, "导入成功",
                QString("成功导入连接：%1\n服务器地址：%2\n端口：%3")
                    .arg(finalConn.name)
                    .arg(finalConn.hostAddress)
                    .arg(finalConn.port));
        } else {
            QMessageBox::warning(this, "错误", "保存连接失败！");
        }
    }
}

RDPConnectionInfo RemoteDesktopWidget::parseRDPFile(const QString &filePath) {
    RDPConnectionInfo info;
    info.isValid = false;
    info.port = 3389;
    info.screenWidth = 1920;
    info.screenHeight = 1080;
    info.colorDepth = 32;
    info.fullScreen = true;
    info.useAllMonitors = false;
    info.enableAudio = true;
    info.enableClipboard = true;
    info.enablePrinter = false;
    info.enableDrive = false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        info.errorMessage = "无法打开文件，请检查文件是否被占用或无读取权限";
        return info;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");

    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    if (lines.isEmpty()) {
        info.errorMessage = "RDP文件为空，不包含任何有效配置";
        return info;
    }

    bool hasServerAddress = false;

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();

        if (trimmedLine.isEmpty() || trimmedLine.startsWith("#") || trimmedLine.startsWith(";")) {
            continue;
        }

        // 处理 full address:s:
        if (trimmedLine.startsWith("full address:s:", Qt::CaseInsensitive)) {
            // 找到第二个冒号的位置，跳过"full address:s:"部分
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    parseFullAddress(value, info);
                    hasServerAddress = true;
                }
            }
        }
        // 处理 server port:i:
        else if (trimmedLine.startsWith("server port:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int port = value.toInt(&ok);
                    if (ok && port > 0 && port <= 65535) {
                        info.port = port;
                    }
                }
            }
        }
        // 处理 username:s:
        else if (trimmedLine.startsWith("username:s:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    info.username = trimmedLine.mid(secondColonPos + 1).trimmed();
                }
            }
        }
        // 处理 domain:s:
        else if (trimmedLine.startsWith("domain:s:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    info.domain = trimmedLine.mid(secondColonPos + 1).trimmed();
                }
            }
        }
        // 处理 desktopwidth:i:
        else if (trimmedLine.startsWith("desktopwidth:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int width = value.toInt(&ok);
                    if (ok && width > 0) {
                        info.screenWidth = width;
                    }
                }
            }
        }
        // 处理 desktopheight:i:
        else if (trimmedLine.startsWith("desktopheight:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int height = value.toInt(&ok);
                    if (ok && height > 0) {
                        info.screenHeight = height;
                    }
                }
            }
        }
        // 处理 session bpp:i:
        else if (trimmedLine.startsWith("session bpp:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int bpp = value.toInt(&ok);
                    if (ok && bpp > 0) {
                        info.colorDepth = bpp;
                    }
                }
            }
        }
        // 处理 screen mode id:i:
        else if (trimmedLine.startsWith("screen mode id:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int mode = value.toInt(&ok);
                    if (ok) {
                        info.fullScreen = (mode == 2);
                    }
                }
            }
        }
        // 处理 use multimon:i:
        else if (trimmedLine.startsWith("use multimon:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int valueInt = value.toInt(&ok);
                    if (ok) {
                        info.useAllMonitors = (valueInt == 1);
                    }
                }
            }
        }
        // 处理 audiomode:i:
        else if (trimmedLine.startsWith("audiomode:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int mode = value.toInt(&ok);
                    if (ok) {
                        info.enableAudio = (mode != 2);
                    }
                }
            }
        }
        // 处理 redirectclipboard:i:
        else if (trimmedLine.startsWith("redirectclipboard:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int valueInt = value.toInt(&ok);
                    if (ok) {
                        info.enableClipboard = (valueInt == 1);
                    }
                }
            }
        }
        // 处理 redirectprinters:i:
        else if (trimmedLine.startsWith("redirectprinters:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int valueInt = value.toInt(&ok);
                    if (ok) {
                        info.enablePrinter = (valueInt == 1);
                    }
                }
            }
        }
        // 处理 redirectdrives:i:
        else if (trimmedLine.startsWith("redirectdrives:i:", Qt::CaseInsensitive)) {
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    bool ok = false;
                    int valueInt = value.toInt(&ok);
                    if (ok) {
                        info.enableDrive = (valueInt == 1);
                    }
                }
            }
        }
        // 处理 drivestoredirect:s:
        else if (trimmedLine.startsWith("drivestoredirect:s:", Qt::CaseInsensitive)) {
            // 这个配置项表示是否重定向驱动器
            int firstColonPos = trimmedLine.indexOf(":");
            if (firstColonPos > 0) {
                int secondColonPos = trimmedLine.indexOf(":", firstColonPos + 1);
                if (secondColonPos > firstColonPos) {
                    QString value = trimmedLine.mid(secondColonPos + 1).trimmed();
                    // 如果值不为空，表示启用了驱动器重定向
                    info.enableDrive = !value.isEmpty();
                }
            }
        }
    }

    if (!hasServerAddress) {
        info.errorMessage = "未找到服务器地址（full address:s）";
        return info;
    }

    if (info.serverAddress.isEmpty()) {
        info.errorMessage = "服务器地址为空";
        return info;
    }

    info.isValid = true;
    return info;
}

void RemoteDesktopWidget::parseFullAddress(const QString &fullAddress, RDPConnectionInfo &info)
{
    QString address = fullAddress;

    int colonPos = fullAddress.lastIndexOf(':');
    if (colonPos > 0) {
        QString portStr = fullAddress.mid(colonPos + 1);
        bool ok = false;
        int port = portStr.toInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            info.port = port;
            address = fullAddress.left(colonPos);
        }
    }

    if (address.startsWith("[")) {
        int endBracket = address.indexOf(']');
        if (endBracket > 0) {
            info.serverAddress = address.mid(1, endBracket - 1);
            if (endBracket + 1 < address.length()) {
                QString suffix = address.mid(endBracket + 1);
                if (suffix.startsWith(":")) {
                    bool ok = false;
                    int port = suffix.mid(1).toInt(&ok);
                    if (ok && port > 0 && port <= 65535) {
                        info.port = port;
                    }
                }
            }
        }
    } else {
        info.serverAddress = address;
    }
}

RemoteDesktopConnection RemoteDesktopWidget::rdpInfoToConnection(const RDPConnectionInfo &rdpInfo)
{
    RemoteDesktopConnection conn;
    conn.name = rdpInfo.serverAddress;
    conn.hostAddress = rdpInfo.serverAddress;
    conn.port = rdpInfo.port;
    conn.username = rdpInfo.username;
    conn.password = "";
    conn.domain = rdpInfo.domain;
    conn.screenWidth = rdpInfo.screenWidth;
    conn.screenHeight = rdpInfo.screenHeight;
    conn.fullScreen = rdpInfo.fullScreen;
    conn.useAllMonitors = rdpInfo.useAllMonitors;
    conn.enableAudio = rdpInfo.enableAudio;
    conn.enableClipboard = rdpInfo.enableClipboard;
    conn.enablePrinter = rdpInfo.enablePrinter;
    conn.enableDrive = rdpInfo.enableDrive;
    conn.category = "未分类";
    conn.isFavorite = false;
    return conn;
}

void RemoteDesktopWidget::importFromJSONFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件！");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::warning(this, "错误", "文件格式不正确！");
        return;
    }

    QJsonArray array = doc.array();
    int imported = 0;

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();
        RemoteDesktopConnection conn;
        conn.name = obj["name"].toString();
        conn.hostAddress = obj["hostAddress"].toString();
        conn.port = obj["port"].toInt(3389);
        conn.username = obj["username"].toString();
        conn.password = obj["password"].toString();
        conn.domain = obj["domain"].toString();
        conn.category = obj["category"].toString("未分类");
        conn.notes = obj["notes"].toString();
        conn.screenWidth = obj["screenWidth"].toInt(1920);
        conn.screenHeight = obj["screenHeight"].toInt(1080);
        conn.fullScreen = obj["fullScreen"].toBool(false);
        conn.useAllMonitors = obj["useAllMonitors"].toBool(false);
        conn.enableAudio = obj["enableAudio"].toBool(true);
        conn.enableClipboard = obj["enableClipboard"].toBool(true);
        conn.enablePrinter = obj["enablePrinter"].toBool(false);
        conn.enableDrive = obj["enableDrive"].toBool(false);
        conn.isFavorite = obj["isFavorite"].toBool(false);

        if (!conn.name.isEmpty() && !conn.hostAddress.isEmpty()) {
            if (db->addRemoteDesktop(conn)) {
                imported++;
            }
        }
    }

    refreshConnectionList();
    QMessageBox::information(this, "导入完成", QString("成功导入 %1 个连接！").arg(imported));
}

void RemoteDesktopWidget::onExportConnections()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出连接配置", "", "JSON 文件 (*.json);;所有文件 (*.*)");
    if (fileName.isEmpty()) return;

    QList<RemoteDesktopConnection> connections = db->getAllRemoteDesktops();
    QJsonArray array;

    for (const RemoteDesktopConnection &conn : connections) {
        QJsonObject obj;
        obj["name"] = conn.name;
        obj["hostAddress"] = conn.hostAddress;
        obj["port"] = conn.port;
        obj["username"] = conn.username;
        obj["password"] = "";
        obj["domain"] = conn.domain;
        obj["category"] = conn.category;
        obj["notes"] = conn.notes;
        obj["screenWidth"] = conn.screenWidth;
        obj["screenHeight"] = conn.screenHeight;
        obj["fullScreen"] = conn.fullScreen;
        obj["useAllMonitors"] = conn.useAllMonitors;
        obj["enableAudio"] = conn.enableAudio;
        obj["enableClipboard"] = conn.enableClipboard;
        obj["enablePrinter"] = conn.enablePrinter;
        obj["enableDrive"] = conn.enableDrive;
        obj["isFavorite"] = conn.isFavorite;
        array.append(obj);
    }

    QJsonDocument doc(array);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        QMessageBox::information(this, "导出成功", "连接配置已成功导出！\n注意：为安全起见，密码未导出。");
    } else {
        QMessageBox::warning(this, "错误", "无法保存文件！");
    }
}

void RemoteDesktopWidget::onConnectionSelectionChanged()
{
    updateConnectionButtons();
}

void RemoteDesktopWidget::onToggleFavorite()
{
    RemoteDesktopConnection conn = getSelectedConnection();
    if (conn.id == -1) return;

    conn.isFavorite = !conn.isFavorite;
    if (db->updateRemoteDesktop(conn)) {
        refreshConnectionList();
    }
}

RemoteDesktopConnection RemoteDesktopWidget::getSelectedConnection()
{
    QList<QTableWidgetItem *> selected = connectionTable->selectedItems();
    if (selected.isEmpty()) {
        RemoteDesktopConnection empty;
        empty.id = -1;
        return empty;
    }

    int row = selected.first()->row();
    int id = connectionTable->item(row, 0)->data(Qt::UserRole).toInt();
    return db->getRemoteDesktopById(id);
}

void RemoteDesktopWidget::updateConnectionButtons()
{
    bool hasSelection = !connectionTable->selectedItems().isEmpty();
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    connectButton->setEnabled(hasSelection);
    testButton->setEnabled(hasSelection);
    favoriteButton->setEnabled(hasSelection);
    
    int currentRow = connectionTable->currentRow();
    int totalRows = connectionTable->rowCount();
    bool canMoveUp = hasSelection && currentRow > 0;
    bool canMoveDown = hasSelection && currentRow < totalRows - 1;
    moveUpButton->setEnabled(canMoveUp);
    moveDownButton->setEnabled(canMoveDown);

    if (hasSelection) {
        RemoteDesktopConnection conn = getSelectedConnection();
        favoriteButton->setText(conn.isFavorite ? "取消收藏" : "收藏");
    }
}

RemoteDesktopDialog::RemoteDesktopDialog(Database *db, QWidget *parent)
    : QDialog(parent), db(db), m_isEditMode(false)
{
    setupUI();
    setWindowTitle("添加远程桌面连接");
}

RemoteDesktopDialog::RemoteDesktopDialog(Database *db, const RemoteDesktopConnection &connection, QWidget *parent)
    : QDialog(parent), db(db), m_connection(connection), m_isEditMode(true)
{
    setupUI();
    setWindowTitle("编辑远程桌面连接");
    loadConnection(connection);
}

void RemoteDesktopDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QTabWidget *tabWidget = new QTabWidget();

    QWidget *generalTab = new QWidget();
    QFormLayout *generalLayout = new QFormLayout(generalTab);

    nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("例如：公司服务器");
    hostEdit = new QLineEdit();
    hostEdit->setPlaceholderText("例如：192.168.1.100 或 server.example.com");
    portSpin = new QSpinBox();
    portSpin->setRange(1, 65535);
    portSpin->setValue(3389);
    usernameEdit = new QLineEdit();
    usernameEdit->setPlaceholderText("登录用户名");
    passwordEdit = new QLineEdit();
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText("登录密码");
    domainEdit = new QLineEdit();
    domainEdit->setPlaceholderText("域（可选）");
    categoryCombo = new QComboBox();
    categoryCombo->addItem("未分类", "未分类");
    categoryCombo->addItem("内网", "内网");
    categoryCombo->addItem("外网", "外网");
    categoryCombo->addItem("工作", "工作");
    categoryCombo->addItem("个人", "个人");
    categoryCombo->addItem("测试", "测试");
    notesEdit = new QTextEdit();
    notesEdit->setPlaceholderText("备注信息（可选）");
    notesEdit->setMaximumHeight(80);

    generalLayout->addRow("连接名称 *:", nameEdit);
    generalLayout->addRow("主机地址 *:", hostEdit);
    generalLayout->addRow("端口:", portSpin);
    generalLayout->addRow("用户名:", usernameEdit);
    generalLayout->addRow("密码:", passwordEdit);
    generalLayout->addRow("域:", domainEdit);
    generalLayout->addRow("分类:", categoryCombo);
    generalLayout->addRow("备注:", notesEdit);

    tabWidget->addTab(generalTab, "基本信息");

    QWidget *displayTab = new QWidget();
    QFormLayout *displayLayout = new QFormLayout(displayTab);

    widthSpin = new QSpinBox();
    widthSpin->setRange(640, 7680);
    widthSpin->setValue(1920);
    heightSpin = new QSpinBox();
    heightSpin->setRange(480, 4320);
    heightSpin->setValue(1080);
    fullScreenCheck = new QCheckBox("全屏模式");
    allMonitorsCheck = new QCheckBox("使用所有显示器");

    displayLayout->addRow("屏幕宽度:", widthSpin);
    displayLayout->addRow("屏幕高度:", heightSpin);
    displayLayout->addRow("", fullScreenCheck);
    displayLayout->addRow("", allMonitorsCheck);

    tabWidget->addTab(displayTab, "显示设置");

    QWidget *resourceTab = new QWidget();
    QVBoxLayout *resourceLayout = new QVBoxLayout(resourceTab);

    audioCheck = new QCheckBox("启用音频重定向");
    audioCheck->setChecked(true);
    clipboardCheck = new QCheckBox("启用剪贴板");
    clipboardCheck->setChecked(true);
    printerCheck = new QCheckBox("启用打印机重定向");
    driveCheck = new QCheckBox("启用驱动器重定向");

    resourceLayout->addWidget(audioCheck);
    resourceLayout->addWidget(clipboardCheck);
    resourceLayout->addWidget(printerCheck);
    resourceLayout->addWidget(driveCheck);
    resourceLayout->addStretch();

    tabWidget->addTab(resourceTab, "本地资源");

    mainLayout->addWidget(tabWidget);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RemoteDesktopDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    connect(nameEdit, &QLineEdit::textChanged, this, &RemoteDesktopDialog::validateForm);
    connect(hostEdit, &QLineEdit::textChanged, this, &RemoteDesktopDialog::validateForm);

    validateForm();
}

void RemoteDesktopDialog::loadConnection(const RemoteDesktopConnection &connection)
{
    nameEdit->setText(connection.name);
    hostEdit->setText(connection.hostAddress);
    portSpin->setValue(connection.port);
    usernameEdit->setText(connection.username);
    passwordEdit->setText(connection.password);
    domainEdit->setText(connection.domain);
    widthSpin->setValue(connection.screenWidth);
    heightSpin->setValue(connection.screenHeight);
    fullScreenCheck->setChecked(connection.fullScreen);
    allMonitorsCheck->setChecked(connection.useAllMonitors);
    audioCheck->setChecked(connection.enableAudio);
    clipboardCheck->setChecked(connection.enableClipboard);
    printerCheck->setChecked(connection.enablePrinter);
    driveCheck->setChecked(connection.enableDrive);
    notesEdit->setPlainText(connection.notes);

    int categoryIndex = categoryCombo->findData(connection.category);
    if (categoryIndex >= 0) {
        categoryCombo->setCurrentIndex(categoryIndex);
    }
}

RemoteDesktopConnection RemoteDesktopDialog::getConnection() const
{
    RemoteDesktopConnection conn = m_connection;
    conn.name = nameEdit->text().trimmed();
    conn.hostAddress = hostEdit->text().trimmed();
    conn.port = portSpin->value();
    conn.username = usernameEdit->text().trimmed();
    conn.password = passwordEdit->text();
    conn.domain = domainEdit->text().trimmed();
    conn.screenWidth = widthSpin->value();
    conn.screenHeight = heightSpin->value();
    conn.fullScreen = fullScreenCheck->isChecked();
    conn.useAllMonitors = allMonitorsCheck->isChecked();
    conn.enableAudio = audioCheck->isChecked();
    conn.enableClipboard = clipboardCheck->isChecked();
    conn.enablePrinter = printerCheck->isChecked();
    conn.enableDrive = driveCheck->isChecked();
    conn.notes = notesEdit->toPlainText();
    conn.category = categoryCombo->currentData().toString();
    return conn;
}

void RemoteDesktopDialog::validateForm()
{
    bool valid = !nameEdit->text().trimmed().isEmpty() && !hostEdit->text().trimmed().isEmpty();
    buttonBox->button(QDialogButtonBox::Save)->setEnabled(valid);
}

void RemoteDesktopDialog::onSave()
{
    accept();
}

void RemoteDesktopWidget::onTableContextMenuRequested(const QPoint &pos)
{
    QMenu contextMenu(this);
    
    RemoteDesktopConnection selectedConn = getSelectedConnection();
    bool hasSelection = (selectedConn.id != -1);
    int currentRow = connectionTable->currentRow();
    
    QAction *connectAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon), "连接");
    connectAction->setEnabled(hasSelection);
    connect(connectAction, &QAction::triggered, this, &RemoteDesktopWidget::onConnect);
    
    QAction *testAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_BrowserReload), "测试连接");
    testAction->setEnabled(hasSelection);
    connect(testAction, &QAction::triggered, this, &RemoteDesktopWidget::onTestConnection);
    
    contextMenu.addSeparator();
    
    QAction *editAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView), "编辑连接");
    editAction->setEnabled(hasSelection);
    connect(editAction, &QAction::triggered, this, &RemoteDesktopWidget::onEditConnection);
    
    QAction *deleteAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_TrashIcon), "删除连接");
    deleteAction->setEnabled(hasSelection);
    connect(deleteAction, &QAction::triggered, this, &RemoteDesktopWidget::onDeleteConnection);
    
    contextMenu.addSeparator();
    
    QAction *moveUpAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_ArrowUp), "上移");
    moveUpAction->setEnabled(hasSelection && currentRow > 0);
    connect(moveUpAction, &QAction::triggered, this, &RemoteDesktopWidget::onMoveUp);
    
    QAction *moveDownAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_ArrowDown), "下移");
    int totalRows = connectionTable->rowCount();
    moveDownAction->setEnabled(hasSelection && currentRow < totalRows - 1);
    connect(moveDownAction, &QAction::triggered, this, &RemoteDesktopWidget::onMoveDown);
    
    contextMenu.addSeparator();
    
    QString favoriteText = hasSelection && selectedConn.isFavorite ? "取消收藏" : "收藏";
    QIcon favoriteIcon = hasSelection && selectedConn.isFavorite 
        ? QApplication::style()->standardIcon(QStyle::SP_DialogNoButton)
        : QApplication::style()->standardIcon(QStyle::SP_DialogYesButton);
    QAction *favoriteAction = contextMenu.addAction(favoriteIcon, favoriteText);
    favoriteAction->setEnabled(hasSelection);
    connect(favoriteAction, &QAction::triggered, this, &RemoteDesktopWidget::onToggleFavorite);
    
    contextMenu.addSeparator();
    
    QAction *addToAppListAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogListView), "添加到应用列表");
    addToAppListAction->setEnabled(hasSelection);
    connect(addToAppListAction, &QAction::triggered, this, &RemoteDesktopWidget::onAddToAppList);
    
    QMenu *addToCollectionMenu = contextMenu.addMenu(QApplication::style()->standardIcon(QStyle::SP_DirIcon), "添加到集合");
    addToCollectionMenu->setEnabled(hasSelection);
    if (hasSelection) {
        QList<AppCollection> collections = db->getAllCollections();
        for (const AppCollection &col : collections) {
            QAction *colAction = addToCollectionMenu->addAction(col.name);
            colAction->setData(col.id);
            connect(colAction, &QAction::triggered, this, [this, colAction]() {
                int collectionId = colAction->data().toInt();
                RemoteDesktopConnection conn = getSelectedConnection();
                if (conn.id == -1) return;
                
                QList<AppInfo> allApps = db->getAllApps();
                AppInfo existingApp;
                bool appExists = false;
                
                for (const AppInfo &a : allApps) {
                    if (a.type == AppType_RemoteDesktop && a.remoteDesktopId == conn.id) {
                        existingApp = a;
                        appExists = true;
                        break;
                    }
                }
                
                AppCollection col = db->getCollectionById(collectionId);
                if (col.id <= 0) return;
                
                if (appExists) {
                    if (col.appIds.contains(existingApp.id)) {
                        emit statusMessageRequested(QString("\"%1\" 已在集合 \"%2\" 中，无需重复添加！").arg(conn.name, col.name));
                        return;
                    }
                    
                    col.appIds.append(existingApp.id);
                    db->updateCollection(col);
                    emit collectionNeedsRefresh();
                    emit statusMessageRequested(QString("已将 \"%1\" 添加到集合 \"%2\"！").arg(conn.name, col.name));
                } else {
                    int maxSortOrder = db->getMaxSortOrder();
                    
                    AppInfo app;
                    app.name = conn.name;
                    app.path = "";
                    app.arguments = "";
                    app.iconPath = "";
                    app.category = conn.category;
                    app.useCount = 0;
                    app.isFavorite = false;
                    app.sortOrder = maxSortOrder + 1;
                    app.type = AppType_RemoteDesktop;
                    app.remoteDesktopId = conn.id;
                    
                    if (db->addApp(app)) {
                        AppInfo newApp = db->getAllApps().last();
                        col.appIds.append(newApp.id);
                        db->updateCollection(col);
                        emit appListNeedsRefresh();
                        emit collectionNeedsRefresh();
                        emit statusMessageRequested(QString("已将 \"%1\" 添加到集合 \"%2\"！").arg(conn.name, col.name));
                    }
                }
            });
        }
        
        if (collections.isEmpty()) {
            QAction *noColAction = addToCollectionMenu->addAction("尚无集合");
            noColAction->setEnabled(false);
        }
    }
    
    contextMenu.addSeparator();
    
    QAction *addAction = contextMenu.addAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder), "添加连接");
    connect(addAction, &QAction::triggered, this, &RemoteDesktopWidget::onAddConnection);
    
    contextMenu.exec(connectionTable->mapToGlobal(pos));
}

void RemoteDesktopWidget::onAddToAppList()
{
    RemoteDesktopConnection conn = getSelectedConnection();
    if (conn.id == -1) return;

    QList<AppInfo> allApps = db->getAllApps();

    for (const AppInfo &a : allApps) {
        if (a.type == AppType_RemoteDesktop && a.remoteDesktopId == conn.id) {
            emit statusMessageRequested(QString("\"%1\" 已在应用列表中，无需重复添加！").arg(conn.name));
            return;
        }
    }
    
    int maxSortOrder = db->getMaxSortOrder();

    AppInfo app;
    app.name = conn.name;
    app.path = "";
    app.arguments = "";
    app.iconPath = "";
    app.category = conn.category;
    app.useCount = 0;
    app.isFavorite = false;
    app.sortOrder = maxSortOrder + 1;
    app.type = AppType_RemoteDesktop;
    app.remoteDesktopId = conn.id;

    if (db->addApp(app)) {
        emit appListNeedsRefresh();
        emit statusMessageRequested(QString("已将 \"%1\" 添加到应用列表！").arg(conn.name));
    } else {
        emit statusMessageRequested("添加到应用列表失败！");
    }
}

void RemoteDesktopWidget::onAddToCollection()
{
}

// FRPC相关槽函数实现
void RemoteDesktopWidget::onFRPCQuickSetup()
{
    // 保存设备名称
    FRPCConfig config = frpcManager->getConfig();
    config.deviceName = frpcDeviceNameEdit->text();
    if (config.deviceName.isEmpty()) {
        config.deviceName = FRPCManager::getLocalUsername();
        frpcDeviceNameEdit->setText(config.deviceName);
    }
    frpcManager->setConfig(config);

    // 启动FRPC
    if (!frpcManager->isRunning()) {
        bool started = frpcManager->startFRPC();
        if (!started) {
            QMessageBox::warning(this, "启动失败",
                "无法启动远程桌面服务。\n\n"
                "请检查:\n"
                "1. frpc.exe 文件是否存在\n"
                "2. 网络连接是否正常\n"
                "3. 服务器是否可达");
            return;
        }
    }

    QMessageBox::information(this, "成功",
        QString("远程桌面服务已启动！\n\n"
                "服务器: %1\n"
                "端口: %2\n\n"
                "状态显示\"已连接\"后，您可以使用以下功能：\n"
                "• 导出RDP文件 - 在其他电脑使用\n"
                "• 添加到列表 - 同步到云端在其他设备使用")
        .arg(config.serverAddr).arg(frpcManager->getRemotePort()));
}

void RemoteDesktopWidget::onFRPCStart()
{
    // 保存设备名称
    FRPCConfig config = frpcManager->getConfig();
    config.deviceName = frpcDeviceNameEdit->text();
    if (config.deviceName.isEmpty()) {
        config.deviceName = FRPCManager::getLocalUsername();
        frpcDeviceNameEdit->setText(config.deviceName);
    }
    frpcManager->setConfig(config);

    // 同步配置到云端
    if (UserManager::instance()->isLoggedIn()) {
        QJsonObject frpcJson;
        frpcJson["serverAddr"] = config.serverAddr;
        frpcJson["serverPort"] = config.serverPort;
        frpcJson["localPort"] = config.localPort;
        frpcJson["remotePort"] = config.remotePort;
        frpcJson["deviceName"] = config.deviceName;
        frpcJson["isEnabled"] = config.isEnabled;
        ConfigSync::instance()->saveFRPCConfig(frpcJson);
    }

    if (!frpcManager->startFRPC()) {
        QMessageBox::warning(this, "启动失败",
            "无法启动远程桌面服务。\n\n"
            "请检查:\n"
            "1. frpc.exe 文件是否存在\n"
            "2. 网络连接是否正常\n"
            "3. 服务器是否可达");
    }
}

void RemoteDesktopWidget::onFRPCStop()
{
    frpcManager->stopFRPC();
}

void RemoteDesktopWidget::onFRPCExportRDP()
{
    // 获取远程端口，如果为0则让用户手动输入
    int remotePort = frpcManager->getRemotePort();

    if (remotePort == 0) {
        bool ok;
        QString portStr = QInputDialog::getText(this, "输入端口",
                                                "请输入远程桌面端口号:",
                                                QLineEdit::Normal, "", &ok);
        if (!ok || portStr.isEmpty()) {
            return;
        }
        remotePort = portStr.toInt();
        if (remotePort <= 0) {
            QMessageBox::warning(this, "端口无效", "请输入有效的端口号（20000-50000）");
            return;
        }
    }

    // 获取当前Windows用户名
    QString username = FRPCManager::getLocalUsername();

    // 弹出对话框让用户输入密码（可选）
    bool ok;
    QString password = QInputDialog::getText(this, "导出RDP文件",
                                            "请输入Windows登录密码（可选，用于自动登录）:",
                                            QLineEdit::Password, "", &ok);
    if (!ok) {
        return;
    }

    // 生成RDP文件，传入端口号
    int port = remotePort;
    QString rdpFilePath = frpcManager->generateRDPFile(username, password, port, 1920, false);
    if (rdpFilePath.isEmpty()) {
        QMessageBox::warning(this, "生成失败", "无法生成RDP文件，请重试");
        return;
    }

    // 询问用户是否打开文件
    int ret = QMessageBox::question(this, "RDP文件已生成",
                                   QString("文件已生成: %1\n\n"
                                           "使用方法:\n"
                                           "1. 将RDP文件复制到其他电脑\n"
                                           "2. 双击打开，输入Windows账号密码\n"
                                           "3. 即可远程控制本机\n\n"
                                           "提示: 在其他电脑登录同一账号，可在远程桌面列表中直接访问\n\n"
                                           "是否打开文件？").arg(rdpFilePath),
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(rdpFilePath));
    }
}

void RemoteDesktopWidget::onFRPCAddToList()
{
    // 获取FRPC配置
    FRPCConfig config = frpcManager->getConfig();
    int remotePort = frpcManager->getRemotePort();

    if (remotePort == 0) {
        QMessageBox::warning(this, "未连接",
            "远程桌面服务未启动或未连接。\n\n"
            "请先点击\"开启\"按钮启动服务，"
            "等待状态显示\"已连接\"后再尝试添加到列表。");
        return;
    }

    // 获取服务器地址（默认使用 FRPS 地址）
    QString serverAddr = config.serverAddr;
    if (serverAddr.isEmpty()) {
        serverAddr = "8.163.37.74";
    }

    // 创建设置连接
    RemoteDesktopConnection conn;
    conn.id = 0;
    conn.name = config.deviceName.isEmpty() ? QHostInfo::localHostName() : config.deviceName;
    conn.hostAddress = serverAddr;
    conn.port = remotePort;
    conn.username = FRPCManager::getLocalUsername();
    conn.password = "";
    conn.domain = "";
    conn.displayName = conn.name;
    conn.screenWidth = 1920;
    conn.screenHeight = 1080;
    conn.fullScreen = true;
    conn.useAllMonitors = false;
    conn.enableAudio = false;
    conn.enableClipboard = true;
    conn.enablePrinter = false;
    conn.enableDrive = false;
    conn.notes = "";
    conn.category = "";
    conn.isFavorite = false;

    // 检查是否已存在（根据 hostAddress + port + username 判断）
    QList<RemoteDesktopConnection> existingList = db->getAllRemoteDesktops();
    int existingIndex = -1;
    for (int i = 0; i < existingList.size(); ++i) {
        const RemoteDesktopConnection &existing = existingList[i];
        if (existing.hostAddress == conn.hostAddress &&
            existing.port == conn.port &&
            existing.username == conn.username) {
            existingIndex = i;
            break;
        }
    }

    bool success = false;
    if (existingIndex >= 0) {
        // 已存在，更新现有记录（保留ID和sortOrder）
        conn.id = existingList[existingIndex].id;
        conn.sortOrder = existingList[existingIndex].sortOrder;
        conn.createdTime = existingList[existingIndex].createdTime;
        conn.lastUsedTime = QDateTime::currentDateTime();
        success = db->updateRemoteDesktop(conn);
        qDebug() << "Updating existing remote desktop:" << conn.name << conn.hostAddress << conn.port;
    } else {
        // 不存在，添加到列表最后（sortOrder为最大值+1）
        int maxSortOrder = -1;
        for (const RemoteDesktopConnection &c : existingList) {
            if (c.sortOrder > maxSortOrder) {
                maxSortOrder = c.sortOrder;
            }
        }
        conn.sortOrder = maxSortOrder + 1;
        success = db->addRemoteDesktop(conn);
        qDebug() << "Adding new remote desktop:" << conn.name << conn.hostAddress << conn.port;
    }

    if (success) {
        // 同步到云端
        if (UserManager::instance()->isLoggedIn()) {
            // 获取所有远程桌面连接
            QList<RemoteDesktopConnection> allConnections = db->getAllRemoteDesktops();
            QJsonArray rdsArray;
            for (const RemoteDesktopConnection &c : allConnections) {
                QJsonObject obj;
                obj["id"] = c.id;
                obj["name"] = c.name;
                obj["hostAddress"] = c.hostAddress;
                obj["port"] = c.port;
                obj["username"] = c.username;
                // 密码不保存到云端
                obj["displayName"] = c.displayName;
                obj["screenWidth"] = c.screenWidth;
                obj["screenHeight"] = c.screenHeight;
                obj["fullScreen"] = c.fullScreen;
                obj["enableClipboard"] = c.enableClipboard;
                rdsArray.append(obj);
            }
            // 直接保存数组格式
            QJsonObject syncData;
            syncData["remoteDesktops"] = rdsArray;
            ConfigSync::instance()->saveAllConfig(syncData);
        }

        // 刷新列表
        refreshConnectionList();
        QMessageBox::information(this, "添加成功",
            QString("已将 [%1] 添加到远程桌面列表\n\n"
                    "服务器: %2\n端口: %3\n用户名: %4\n\n"
                    "在其他电脑登录同一账号后，远程桌面列表中将显示本机，"
                    "双击即可远程控制，无需手动输入地址和端口。")
            .arg(conn.name).arg(conn.hostAddress).arg(conn.port).arg(conn.username));
    } else {
        QMessageBox::warning(this, "添加失败",
            QString("添加到列表失败\n\n"
                    "名称: %1\n"
                    "地址: %2:%3")
            .arg(conn.name).arg(conn.hostAddress).arg(conn.port));
    }
}

void RemoteDesktopWidget::onFRPCSettings()
{
    // 弹出FRPC设置对话框
    QDialog settingsDialog(this);
    settingsDialog.setWindowTitle("FRPC 参数设置");
    settingsDialog.setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout();

    QFormLayout *form = new QFormLayout();

    QLineEdit *serverAddrEdit = new QLineEdit(frpcManager->getConfig().serverAddr);
    QSpinBox *serverPortSpin = new QSpinBox();
    serverPortSpin->setRange(1, 65535);
    serverPortSpin->setValue(frpcManager->getConfig().serverPort);
    QSpinBox *localPortSpin = new QSpinBox();
    localPortSpin->setRange(1, 65535);
    localPortSpin->setValue(frpcManager->getConfig().localPort);
    localPortSpin->setToolTip("本地RDP端口，通常为3389");

    form->addRow("FRPS服务器地址:", serverAddrEdit);
    form->addRow("FRPS服务器端口:", serverPortSpin);
    form->addRow("本地RDP端口:", localPortSpin);

    layout->addLayout(form);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton = new QPushButton("保存");
    QPushButton *cancelButton = new QPushButton("取消");
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    settingsDialog.setLayout(layout);

    connect(saveButton, &QPushButton::clicked, &settingsDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &settingsDialog, &QDialog::reject);

    if (settingsDialog.exec() == QDialog::Accepted) {
        FRPCConfig config = frpcManager->getConfig();
        config.serverAddr = serverAddrEdit->text();
        config.serverPort = serverPortSpin->value();
        config.localPort = localPortSpin->value();
        config.deviceName = frpcDeviceNameEdit->text();
        frpcManager->setConfig(config);

        // 同步配置到云端
        if (UserManager::instance()->isLoggedIn()) {
            QJsonObject frpcJson;
            frpcJson["serverAddr"] = config.serverAddr;
            frpcJson["serverPort"] = config.serverPort;
            frpcJson["localPort"] = config.localPort;
            frpcJson["remotePort"] = config.remotePort;
            frpcJson["deviceName"] = config.deviceName;
            frpcJson["isEnabled"] = config.isEnabled;
            ConfigSync::instance()->saveFRPCConfig(frpcJson);
        }
    }
}

void RemoteDesktopWidget::onFRPCStatusChanged(FRPCManager::ConnectionStatus status)
{
    updateFRPCStatus();
}

void RemoteDesktopWidget::onFRPCError(const QString &error)
{
    QMessageBox::warning(this, "连接错误",
        QString("远程桌面连接出错: %1\n\n"
                "请检查:\n"
                "1. 网络连接是否正常\n"
                "2. 服务器是否可达\n"
                "3. 可以尝试关闭后重新开启").arg(error));
    updateFRPCStatus();
}

void RemoteDesktopWidget::onFRPCPortChanged(int port)
{
    frpcPortLabel->setText(port > 0 ? QString::number(port) : "--");
    frpcExportButton->setEnabled(port > 0);
}

void RemoteDesktopWidget::onFRPCStopped()
{
    // 关闭成功，提示用户
    QMessageBox::information(this, "已关闭", "远程桌面服务已停止");
    updateFRPCStatus();
}

void RemoteDesktopWidget::updateFRPCStatus()
{
    bool isRunning = frpcManager->isRunning();
    int port = frpcManager->getRemotePort();

    if (isRunning) {
        if (port > 0) {
            frpcStatusLabel->setText("已连接");
            frpcStatusLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
        } else {
            frpcStatusLabel->setText("连接中...");
            frpcStatusLabel->setStyleSheet("color: #FF9800; font-weight: bold;");
        }
    } else {
        frpcStatusLabel->setText("未连接");
        frpcStatusLabel->setStyleSheet("color: #888; font-weight: bold;");
    }

    frpcPortLabel->setText(port > 0 ? QString::number(port) : "--");
    frpcStartButton->setEnabled(!isRunning);
    frpcStopButton->setEnabled(isRunning);
    frpcExportButton->setEnabled(port > 0);
    frpcAddToListButton->setEnabled(port > 0);
}

