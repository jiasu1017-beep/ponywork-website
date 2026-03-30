#include "memowidget.h"
#include <QApplication>
#include <QTimer>

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
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(10);

    // 标题栏
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QLabel *titleIcon = new QLabel("📋", this);
    titleIcon->setStyleSheet("font-size: 20px;");
    titleLabel = new QLabel("备忘录", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #E0E0E0;");

    QPushButton *addBtn = new QPushButton("+ 添加", this);
    addBtn->setStyleSheet("QPushButton { background: #4CAF50; color: white; border: none; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background: #45a049; }");

    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(addBtn);

    // 备忘录列表
    memoList = new QListWidget(this);
    memoList->setStyleSheet("QListWidget { background: #2D2D2D; border: 1px solid #3D3D3D; border-radius: 8px; } QListWidget::item { padding: 12px; margin: 4px; border-radius: 6px; } QListWidget::item:selected { background: #3D3D3D; border: 1px solid #4CAF50; } QListWidget::item:hover { background: #353535; }");
    memoList->setMinimumHeight(150);

    // 详情区域
    QWidget *detailWidget = new QWidget(this);
    detailWidget->setObjectName("detailWidget");
    detailWidget->setStyleSheet("background: #2D2D2D; border-radius: 8px; padding: 16px;");
    QVBoxLayout *detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(10);

    typeLabel = new QLabel("", this);
    typeLabel->setStyleSheet("font-size: 14px; color: #9E9E9E;");

    contentLabel = new QLabel("内容:", this);
    contentLabel->setStyleSheet("color: #9E9E9E; font-size: 12px;");

    contentEdit = new QTextEdit(this);
    contentEdit->setReadOnly(true);
    contentEdit->setStyleSheet("QTextEdit { background: #1E1E1E; border: 1px solid #3D3D3D; border-radius: 4px; padding: 8px; color: #E0E0E0; }");

    descLabel = new QLabel("", this);
    descLabel->setStyleSheet("color: #9E9E9E; font-size: 12px; margin-top: 8px;");

    detailLayout->addWidget(typeLabel);
    detailLayout->addWidget(contentLabel);
    detailLayout->addWidget(contentEdit);
    detailLayout->addWidget(descLabel);
    detailLayout->addStretch();

    // 空状态提示
    emptyLabel = new QLabel("请选择或添加备忘录", this);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #666; font-size: 14px;");
    emptyLabel->setMinimumHeight(200);

    // 操作按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    copyBtn = new QPushButton("📋 复制", this);
    runBtn = new QPushButton("▶ 运行", this);
    editBtn = new QPushButton("✎ 编辑", this);
    deleteBtn = new QPushButton("🗑 删除", this);

    QString btnStyle = "QPushButton { background: #3D3D3D; color: #E0E0E0; border: none; padding: 8px 16px; border-radius: 4px; } QPushButton:hover { background: #4D4D4D; } QPushButton:disabled { color: #666; }";
    copyBtn->setStyleSheet(btnStyle);
    runBtn->setStyleSheet(btnStyle);
    editBtn->setStyleSheet(btnStyle);
    deleteBtn->setStyleSheet("QPushButton { background: #5D3A3A; color: #E0E0E0; border: none; padding: 8px 16px; border-radius: 4px; } QPushButton:hover { background: #7D4A4A; }");

    btnLayout->addWidget(copyBtn);
    btnLayout->addWidget(runBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(deleteBtn);

    // 组装主布局
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(memoList, 1);
    mainLayout->addWidget(detailWidget, 2);
    mainLayout->addWidget(emptyLabel, 2);
    mainLayout->addLayout(btnLayout);

    // 信号连接
    connect(addBtn, &QPushButton::clicked, this, &MemoWidget::onAddMemo);
    connect(copyBtn, &QPushButton::clicked, this, &MemoWidget::onCopyContent);
    connect(runBtn, &QPushButton::clicked, this, &MemoWidget::onRunCommand);
    connect(editBtn, &QPushButton::clicked, this, &MemoWidget::onEditMemo);
    connect(deleteBtn, &QPushButton::clicked, this, &MemoWidget::onDeleteMemo);
    connect(memoList, &QListWidget::itemClicked, this, &MemoWidget::onMemoSelected);

    // 初始状态
    detailWidget->hide();
    emptyLabel->show();
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
        item->setSizeHint(QSize(0, 50));
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
    emptyLabel->hide();

    // 显示详情面板
    QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets) {
        if (w->objectName() == "detailWidget") {
            w->show();
            break;
        }
    }

    typeLabel->setText(getTypeIcon(memo.type) + " " + memo.name);
    contentEdit->setPlainText(memo.content);
    descLabel->setText("描述: " + memo.description);

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