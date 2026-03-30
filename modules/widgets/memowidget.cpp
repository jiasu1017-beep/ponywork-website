#include "memowidget.h"
#include <QApplication>
#include <QTimer>
#include <QJsonArray>

// MemoDialog 实现
MemoDialog::MemoDialog(Memo *memo, QWidget *parent)
    : QDialog(parent), currentMemo(memo)
{
    setWindowTitle(memo ? "编辑备忘录" : "添加备忘录");
    setMinimumSize(400, 350);
    setModal(true);

    QFormLayout *formLayout = new QFormLayout(this);

    nameEdit = new QLineEdit(this);
    typeCombo = new QComboBox(this);
    typeCombo->addItem("脚本", 0);
    typeCombo->addItem("密钥", 1);
    typeCombo->addItem("其他", 2);

    contentEdit = new QTextEdit(this);
    contentEdit->setMinimumHeight(120);

    descEdit = new QLineEdit(this);

    formLayout->addRow("名称:", nameEdit);
    formLayout->addRow("类型:", typeCombo);
    formLayout->addRow("内容:", contentEdit);
    formLayout->addRow("描述:", descEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton("确定", this);
    QPushButton *cancelBtn = new QPushButton("取消", this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    formLayout->addRow(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &MemoDialog::onAccepted);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    if (memo) {
        nameEdit->setText(memo->name);
        typeCombo->setCurrentIndex(memo->type);
        contentEdit->setPlainText(memo->content);
        descEdit->setText(memo->description);
    }
}

void MemoDialog::onAccepted()
{
    if (nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入名称");
        return;
    }
    if (contentEdit->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入内容");
        return;
    }
    accept();
}

MemoDialog::~MemoDialog()
{
}

// MemoWidget 实现
MemoWidget::MemoWidget(Database *database, QWidget *parent)
    : QWidget(parent), db(database)
{
    setupUI();
    loadMemos();
}

MemoWidget::~MemoWidget()
{
}

void MemoWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 标题栏
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QLabel *titleIcon = new QLabel("📋", this);
    titleIcon->setStyleSheet("font-size: 18px; padding: 0 4px;");
    titleLabel = new QLabel("备忘录", this);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #333333;");

    QPushButton *addBtn = new QPushButton("+ 添加", this);
    QPushButton *syncBtn = new QPushButton("🔄 同步", this);
    addBtn->setStyleSheet("QPushButton { background: #4CAF50; color: white; border: none; padding: 5px 14px; border-radius: 4px; font-size: 13px; } QPushButton:hover { background: #45a049; }");
    syncBtn->setStyleSheet("QPushButton { background: #2196F3; color: white; border: none; padding: 5px 14px; border-radius: 4px; font-size: 13px; } QPushButton:hover { background: #1976D2; }");

    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(syncBtn);
    titleLayout->addWidget(addBtn);
    titleLayout->setSpacing(8);

    // 备忘录列表 - 左右分栏布局
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);

    // 左侧列表
    QWidget *listWidget = new QWidget(this);
    listWidget->setMinimumWidth(180);
    listWidget->setMaximumWidth(220);
    QVBoxLayout *listLayout = new QVBoxLayout(listWidget);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);

    memoList = new QListWidget(this);
    memoList->setStyleSheet("QListWidget { background: #FFFFFF; border: 1px solid #DDDDDD; border-radius: 6px; padding: 4px; color: #333333; font-size: 13px; } QListWidget::item { padding: 8px 10px; margin: 2px 0; border-radius: 4px; color: #333333; } QListWidget::item:selected { background: #4CAF50; color: white; } QListWidget::item:hover { background: #E8F5E9; } QListWidget::item:selected:hover { background: #45a049; }");
    listLayout->addWidget(memoList);

    // 右侧详情
    QWidget *detailWidget = new QWidget(this);
    detailWidget->setObjectName("detailWidget");
    detailWidget->setStyleSheet("background: #FFFFFF; border: 1px solid #DDDDDD; border-radius: 6px; padding: 12px;");
    QVBoxLayout *detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(6);

    typeLabel = new QLabel("选择一条备忘录查看详情", this);
    typeLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333333; padding: 4px 0;");

    contentLabel = new QLabel("内容:", this);
    contentLabel->setStyleSheet("color: #666666; font-size: 12px; padding-top: 6px;");

    contentEdit = new QTextEdit(this);
    contentEdit->setReadOnly(true);
    contentEdit->setPlaceholderText("选择备忘录查看内容");
    contentEdit->setStyleSheet("QTextEdit { background: #FAFAFA; border: 1px solid #DDDDDD; border-radius: 4px; padding: 8px; color: #333333; font-size: 13px; }");

    descLabel = new QLabel("选择备忘录查看详情", this);
    descLabel->setObjectName("descLabel");
    descLabel->setStyleSheet("color: #888888; font-size: 12px; padding-top: 4px;");

    detailLayout->addWidget(typeLabel);
    detailLayout->addWidget(contentLabel);
    detailLayout->addWidget(contentEdit);
    detailLayout->addWidget(descLabel);
    // 移除 stretch，减小下方空白

    splitter->addWidget(listWidget);
    splitter->addWidget(detailWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    // 操作按钮 - 复制和运行放右侧
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    copyBtn = new QPushButton("📋 复制", this);
    runBtn = new QPushButton("▶ 运行", this);
    editBtn = new QPushButton("✎ 编辑", this);
    deleteBtn = new QPushButton("🗑 删除", this);

    QString btnStyle = "QPushButton { background: #F5F5F5; color: #333333; border: 1px solid #DDDDDD; padding: 6px 14px; border-radius: 4px; font-size: 13px; } QPushButton:hover { background: #E8E8E8; } QPushButton:disabled { color: #AAAAAA; background: #F5F5F5; }";
    copyBtn->setStyleSheet(btnStyle);
    runBtn->setStyleSheet(btnStyle);
    editBtn->setStyleSheet(btnStyle);
    deleteBtn->setStyleSheet("QPushButton { background: #FFEBEE; color: #D32F2F; border: 1px solid #FFCDD2; padding: 6px 14px; border-radius: 4px; font-size: 13px; } QPushButton:hover { background: #FFCDD2; }");

    btnLayout->addStretch();
    btnLayout->addWidget(copyBtn);
    btnLayout->addWidget(runBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(deleteBtn);

    // 组装主布局
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addLayout(btnLayout);

    // 信号连接
    connect(addBtn, &QPushButton::clicked, this, &MemoWidget::onAddMemo);
    connect(syncBtn, &QPushButton::clicked, this, &MemoWidget::onSyncMemos);
    connect(copyBtn, &QPushButton::clicked, this, &MemoWidget::onCopyContent);
    connect(runBtn, &QPushButton::clicked, this, &MemoWidget::onRunCommand);
    connect(editBtn, &QPushButton::clicked, this, &MemoWidget::onEditMemo);
    connect(deleteBtn, &QPushButton::clicked, this, &MemoWidget::onDeleteMemo);
    connect(memoList, &QListWidget::itemClicked, this, &MemoWidget::onMemoSelected);

    // 同步信号连接
    connect(MemoSync::instance(), &MemoSync::memosUploadComplete, this, &MemoWidget::onSyncComplete);
    connect(MemoSync::instance(), &MemoSync::memosDownloadComplete, this, &MemoWidget::onSyncComplete);
    connect(MemoSync::instance(), &MemoSync::syncFailed, this, &MemoWidget::onSyncFailed);

    // 初始状态
    setButtonStates(false);
}

void MemoWidget::setButtonStates(bool enabled)
{
    copyBtn->setEnabled(enabled);
    runBtn->setEnabled(enabled);
    editBtn->setEnabled(enabled);
    deleteBtn->setEnabled(enabled);
}

void MemoWidget::loadMemos()
{
    updateMemoList();
}

void MemoWidget::updateMemoList()
{
    memoList->clear();
    QList<Memo> memos = db->getAllMemos();

    for (const Memo &memo : memos) {
        QListWidgetItem *item = new QListWidgetItem(memoList);
        item->setText(getTypeIcon(memo.type) + " " + memo.name);
        item->setData(Qt::UserRole, memo.id);
        item->setSizeHint(QSize(0, 40));
    }
}

void MemoWidget::onMemoSelected()
{
    QListWidgetItem *item = memoList->currentItem();
    if (!item) return;

    QString memoId = item->data(Qt::UserRole).toString();
    Memo memo = db->getMemoById(memoId);

    if (!memo.id.isEmpty()) {
        currentMemoId = memoId;
        showMemoDetail(memo);
    }
}

void MemoWidget::showMemoDetail(const Memo &memo)
{
    typeLabel->setText(getTypeIcon(memo.type) + " " + memo.name);
    contentEdit->setPlainText(memo.content);
    descLabel->setText(memo.description.isEmpty() ? "" : "描述: " + memo.description);

    setButtonStates(true);
}

QString MemoWidget::getTypeIcon(MemoType type)
{
    switch (type) {
    case MemoType_Script: return "⚡";
    case MemoType_Key: return "🗝";
    case MemoType_Other: return "📝";
    default: return "📝";
    }
}

void MemoWidget::onAddMemo()
{
    Memo memo;
    memo.id = db->generateMemoId();
    memo.createdAt = QDateTime::currentDateTime();
    memo.updatedAt = memo.createdAt;
    memo.syncStatus = 0;

    MemoDialog dialog(&memo, this);
    if (dialog.exec() == QDialog::Accepted) {
        memo.name = dialog.name();
        memo.type = dialog.type();
        memo.content = dialog.content();
        memo.description = dialog.description();

        if (db->addMemo(memo)) {
            updateMemoList();
        }
    }
}

void MemoWidget::onEditMemo()
{
    if (currentMemoId.isEmpty()) return;

    Memo memo = db->getMemoById(currentMemoId);
    if (memo.id.isEmpty()) return;

    MemoDialog dialog(&memo, this);
    if (dialog.exec() == QDialog::Accepted) {
        memo.name = dialog.name();
        memo.type = dialog.type();
        memo.content = dialog.content();
        memo.description = dialog.description();
        memo.updatedAt = QDateTime::currentDateTime();

        if (db->updateMemo(memo)) {
            updateMemoList();
            showMemoDetail(memo);
        }
    }
}

void MemoWidget::onDeleteMemo()
{
    if (currentMemoId.isEmpty()) return;

    int ret = QMessageBox::question(this, "确认删除", "确定要删除这条备忘录吗?",
                                  QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        if (db->deleteMemo(currentMemoId)) {
            currentMemoId.clear();
            updateMemoList();
            setButtonStates(false);
        }
    }
}

void MemoWidget::onCopyContent()
{
    if (currentMemoId.isEmpty()) return;

    Memo memo = db->getMemoById(currentMemoId);
    if (memo.id.isEmpty()) return;

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(memo.content);

    // 提示
    QLabel *tipLabel = new QLabel("已复制到剪贴板", this);
    tipLabel->setStyleSheet("color: #4CAF50; background: #2D2D2D; padding: 8px 16px; border-radius: 4px;");
    tipLabel->setAlignment(Qt::AlignCenter);

    // 显示临时提示
    QTimer::singleShot(2000, tipLabel, &QLabel::deleteLater);
}

void MemoWidget::onRunCommand()
{
    if (currentMemoId.isEmpty()) return;

    Memo memo = db->getMemoById(currentMemoId);
    if (memo.id.isEmpty()) return;

    runInPowerShell(memo.content);
}

bool MemoWidget::runInPowerShell(const QString &command)
{
    QString cmd = command.trimmed();

    // 判断命令类型：自动检测
    bool usePowerShell = cmd.startsWith("powershell", Qt::CaseInsensitive) ||
                        cmd.startsWith("PS", Qt::CaseInsensitive) ||
                        cmd.startsWith("pwsh", Qt::CaseInsensitive);

    if (usePowerShell) {
        QProcess::startDetached("powershell.exe", QStringList() << "-NoExit" << "-Command" << cmd);
    } else {
        QProcess::startDetached("cmd.exe", QStringList() << "/c" << "start" << "cmd" << "/k" << cmd);
    }

    return true;
}

void MemoWidget::onRefreshMemos()
{
    loadMemos();
}

void MemoWidget::onSyncMemos()
{
    QList<Memo> memos = db->getAllMemos();
    QJsonArray memosJson;
    for (const Memo &memo : memos) {
        if (memo.syncStatus == 0) {
            QJsonObject obj;
            obj["id"] = memo.id;
            obj["name"] = memo.name;
            obj["type"] = static_cast<int>(memo.type);
            obj["content"] = memo.content;
            obj["description"] = memo.description;
            obj["createdAt"] = memo.createdAt.toString(Qt::ISODate);
            obj["updatedAt"] = memo.updatedAt.toString(Qt::ISODate);
            memosJson.append(obj);
        }
    }

    if (!memosJson.isEmpty()) {
        MemoSync::instance()->uploadMemos(memosJson);
    } else {
        MemoSync::instance()->downloadMemos();
    }
}

void MemoWidget::onSyncComplete()
{
    qDebug() << "Memo sync completed";
    loadMemos();
}

void MemoWidget::onSyncFailed(const QString &error)
{
    qDebug() << "Memo sync failed:" << error;
}