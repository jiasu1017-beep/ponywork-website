#ifndef MEMOWIDGET_H
#define MEMOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QDialog>
#include <QFormLayout>
#include <QCheckBox>
#include <QMessageBox>
#include <QClipboard>
#include <QProcess>
#include <QDebug>
#include "modules/core/database.h"
#include "modules/user/userapi.h"

class MemoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MemoDialog(Memo *memo = nullptr, QWidget *parent = nullptr);
    ~MemoDialog();

    QString name() const { return nameEdit->text(); }
    MemoType type() const { return static_cast<MemoType>(typeCombo->currentIndex()); }
    QString content() const { return contentEdit->toPlainText(); }
    QString description() const { return descEdit->text(); }

private slots:
    void onAccepted();

private:
    QLineEdit *nameEdit;
    QComboBox *typeCombo;
    QTextEdit *contentEdit;
    QLineEdit *descEdit;
    Memo *currentMemo;
};

class MemoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MemoWidget(Database *db, QWidget *parent = nullptr);
    ~MemoWidget();

private slots:
    void onAddMemo();
    void onEditMemo();
    void onDeleteMemo();
    void onCopyContent();
    void onRunCommand();
    void onMemoSelected();
    void onRefreshMemos();
    void onSyncMemos();
    void onSyncComplete();
    void onSyncFailed(const QString &error);
    void onContextMenu(const QPoint &pos);

private:
    void setupUI();
    void loadMemos();
    void updateMemoList();
    void showMemoDetail(const Memo &memo);
    QString getTypeIcon(MemoType type);
    bool runInPowerShell(const QString &command);
    void setButtonStates(bool enabled);

    Database *db;
    QListWidget *memoList;
    QLabel *titleLabel;
    QLabel *typeLabel;
    QLabel *descLabel;
    QWidget *contentContainer;

    QPushButton *copyBtn;
    QPushButton *runBtn;
    QPushButton *editBtn;
    QPushButton *deleteBtn;

    QString currentMemoId;
    QList<QWidget*> contentSegments;

private:
    QWidget* createSegmentWidget(const QString &text, int index);
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // MEMOWIDGET_H