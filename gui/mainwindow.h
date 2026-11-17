#pragma once

// QingXi · CXZ 压缩包（图形前端）
// 这个程序本身不压缩、不加密：任何操作都交给同目录的 CXZ.exe。
// 两个软件完全分开，命令行引擎可单独发布，前端只是它的调用方。

#include <QMainWindow>
#include <QElapsedTimer>
#include <QStringList>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTreeWidgetItem;
class CxzCli;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openArchivePath(const QString &path);           // 双击 .cxz / 命令行参数进来
    void shellCompress(const QStringList &paths);        // 资源管理器右键「压缩为 CXZ」
    void createEmptyArchive(const QString &path);        // 「新建 → CXZ 压缩包」

    static bool writeShellRegistry(const QString &exePath);   // 关联 + 右键 + 新建
    static bool removeShellRegistry();                        // 撤销关联

    QStringList entryNames() const;      // 自检用
    bool isBusy() const;

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void setupCombos();
    void setupActions();
    void setupTree();
    void refreshArchive(const QString &password);
    void fillFromListing(const QString &text);

    void chooseCompressSource(bool folder);
    void compressPath(const QString &inPath, const QString &outPath);
    void extractTo(bool selectedOnly);
    void runTest();
    void rekey();
    void addKey();
    void newArchiveDialog();
    void associate();
    void aboutDlg();
    void openFileDialog();
    void previewEntry(QTreeWidgetItem *it);

    QStringList compressArgs(const QString &in, const QString &out) const;
    QString passForRun(QString *out) const;
    QString cipherNameFromCombo() const;
    int kdfIterations() const;
    void updateCommandPreviews();
    void setBusy(bool busy);
    void setStatus(const QString &s);
    void progressReset();
    void progressSet(qint64 done, qint64 total, const QString &phase);
    void ensureOpenOrWarn();
    QString defaultOutFor(const QString &in) const;
    void runAsync(const QStringList &args, const QString &password,
                  std::function<void(int, const QString &)> onDone);
    bool confirmPassword();

    Ui::MainWindow *ui = nullptr;
    CxzCli *m_cli = nullptr;
    QElapsedTimer m_clock;

    QString m_archive;          // 当前预览的归档（空 = 没有）
    QString m_pass;             // 当前归档的口令
    int m_entries = 0;
    qint64 m_rawTotal = 0;
    qint64 m_packedTotal = 0;
    bool m_encrypted = false;
    bool m_busy = false;
};
