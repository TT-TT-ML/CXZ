#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cxzcli.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSettings>
#include <QSharedPointer>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>

#include <cmath>

static QString humanSize(qint64 n)
{
    if (n < 1024) return QString::number(n) + " B";
    if (n < 1024 * 1024) return QString::number(n / 1024.0, 'f', 1) + " KB";
    if (n < 1024LL * 1024 * 1024) return QString::number(n / 1048576.0, 'f', 1) + " MB";
    return QString::number(n / 1073741824.0, 'f', 2) + " GB";
}

// 加密等级下拉项：显示名 / 传给 --cipher 的名字
static const struct { const char *label; const char *arg; } kCiphers[] = {
    {"AES-256（默认，推荐）", "aes256"},
    {"AES-512（双 AES-256，最慢）", "aes512"},
    {"AES-192", "aes192"},
    {"AES-128", "aes128"},
    {"AES-64（仅 64 位强度，兼容用）", "aes64"},
    {"ChaCha20（旧版兼容）", "chacha20"},
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_cli(new CxzCli(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("QingXi · CXZ 压缩包"));
    setupCombos();
    setupTree();
    setupActions();

    ui->lineEditForCmd->setReadOnly(true);
    ui->lineEditForCmd_2->setReadOnly(true);
    ui->lineEditPass->setEchoMode(QLineEdit::Password);
    ui->lineEditPass->setPlaceholderText(QStringLiteral("留空 = 不加密"));
    ui->progressBar->setRange(0, 1000);
    ui->progressBar->setValue(0);
    ui->label_4->setText(QStringLiteral("就绪"));
    ui->pushButton_2->setEnabled(false);

    connect(ui->pushButton, &QPushButton::clicked, this, [this] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("压缩"));
        box.setText(QStringLiteral("要把什么压成 CXZ？"));
        QPushButton *f = box.addButton(QStringLiteral("压缩文件…"), QMessageBox::AcceptRole);
        QPushButton *d = box.addButton(QStringLiteral("压缩文件夹…"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == f) chooseCompressSource(false);
        else if (box.clickedButton() == d) chooseCompressSource(true);
    });
    connect(ui->pushButton_2, &QPushButton::clicked, this, [this] { extractTo(false); });
    connect(ui->tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) { previewEntry(it); });
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateCommandPreviews(); });
    connect(ui->comboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateCommandPreviews(); });
    connect(ui->comboBoxCipher, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateCommandPreviews(); });
    connect(ui->lineEditPass, &QLineEdit::textChanged, this, [this](const QString &t) {
        int kinds = 0;
        if (t.size() >= 8) ++kinds;
        if (t.contains(QRegularExpression(QStringLiteral("[0-9]"))) && t.size() >= 8) ++kinds;
        if (t.contains(QRegularExpression(QStringLiteral("[A-Za-z]"))) && t.size() >= 8) ++kinds;
        if (t.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9]")))) ++kinds;
        static const char *tips[] = {"很弱（太短，容易被穷举）", "弱", "一般", "较强", "强"};
        ui->lineEditPass->setToolTip(QStringLiteral("强度：") + QString::fromUtf8(tips[qBound(0, kinds, 4)]) +
                                     QStringLiteral("\n口令只经 stdin 传给 CXZ.exe，不进命令行，也不进日志"));
        updateCommandPreviews();
    });

    connect(m_cli, &CxzCli::line, this, [this](const QString &s) { ui->logView->appendPlainText(s); });
    connect(m_cli, &CxzCli::progress, this, &MainWindow::progressSet);

    if (!QFileInfo::exists(m_cli->exePath()))
        ui->labelInfo->setText(QStringLiteral("找不到 CXZ.exe。把 CXZ.exe 放到本程序同目录，"
                                             "或设置环境变量 CXZ_CLI 指向它。"));
    else
        ui->labelInfo->setText(QStringLiteral("把 .cxz / .zip 拖进来，或点「打开」；双击条目可直接预览文件内容。\n引擎：") +
                               QDir::toNativeSeparators(m_cli->exePath()));
    updateCommandPreviews();
}

MainWindow::~MainWindow()
{
    if (m_cli->running()) m_cli->cancel();
    delete ui;
}

void MainWindow::setupCombos()
{
    for (int i = 1; i <= 9; ++i)
        ui->comboBox->addItem(QStringLiteral("-%1  %2").arg(i)
                                  .arg(i <= 2 ? QStringLiteral("最快") : i >= 8 ? QStringLiteral("最小")
                                                                             : QStringLiteral("均衡")),
                              i);
    ui->comboBox->setCurrentIndex(4);                 // -5
    for (int t = 1; t <= 32; ++t) ui->comboBox_2->addItem(QString::number(t), t);
    ui->comboBox_2->setCurrentIndex(3);               // 4 线程
    for (const auto &c : kCiphers) ui->comboBoxCipher->addItem(QString::fromUtf8(c.label), QString::fromUtf8(c.arg));
    ui->comboBoxCipher->setCurrentIndex(0);
    ui->comboBoxCipher->setToolTip(QStringLiteral("AES-64 只有 64 位强度；AES-512 是两把 AES-256 串联，"
                                                   "不是 NIST 标准名；两者都谈不上\"国家机密级合规\"。"));
}

void MainWindow::setupTree()
{
    ui->tree->setColumnCount(3);
    ui->tree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("原始大小"), QStringLiteral("类型")});
    ui->tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tree->setSortingEnabled(true);
    ui->tree->sortByColumn(0, Qt::AscendingOrder);
}

void MainWindow::setupActions()
{
    auto addAct = [this](const QString &text, const QString &tip, const QKeySequence &key,
                         std::function<void()> fn) {
        QAction *a = new QAction(text, this);
        a->setToolTip(tip);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, fn);
        return a;
    };

    ui->toolBar->setIconSize(QSize(18, 18));
    ui->toolBar->addAction(addAct(QStringLiteral("打开"), QStringLiteral("打开 .cxz / .zip 预览"),
                                 QKeySequence::Open, [this] { openFileDialog(); }));
    ui->toolBar->addAction(addAct(QStringLiteral("新建"), QStringLiteral("新建空 CXZ 压缩包"),
                                 QKeySequence::New, [this] { newArchiveDialog(); }));
    ui->toolBar->addAction(addAct(QStringLiteral("压缩文件"), QStringLiteral("把单个文件压成 .cxz"), {},
                                 [this] { chooseCompressSource(false); }));
    ui->toolBar->addAction(addAct(QStringLiteral("压缩文件夹"), QStringLiteral("把整个文件夹压成 .cxz"), {},
                                 [this] { chooseCompressSource(true); }));
    ui->toolBar->addAction(addAct(QStringLiteral("解压到…"), QStringLiteral("全部解压到指定目录"),
                                 QKeySequence(QStringLiteral("Ctrl+E")), [this] { extractTo(false); }));
    ui->toolBar->addAction(addAct(QStringLiteral("解压所选"), QStringLiteral("只解压选中的条目"), {},
                                 [this] { extractTo(true); }));
    ui->toolBar->addAction(addAct(QStringLiteral("校验"), QStringLiteral("解密并核对 HMAC 与 CRC"),
                                 {}, [this] { runTest(); }));
    ui->toolBar->addAction(addAct(QStringLiteral("换口令"), QStringLiteral("换掉当前口令（不重压正文）"),
                                 {}, [this] { rekey(); }));
    ui->toolBar->addAction(addAct(QStringLiteral("加口令"), QStringLiteral("追加口令槽（多口令共用同一份正文）"),
                                 {}, [this] { addKey(); }));

    QMenu *file = ui->menubar->addMenu(QStringLiteral("文件"));
    file->addAction(addAct(QStringLiteral("打开…"), {}, QKeySequence::Open, [this] { openFileDialog(); }));
    file->addAction(addAct(QStringLiteral("新建空压缩包…"), {}, QKeySequence::New, [this] { newArchiveDialog(); }));
    file->addSeparator();
    file->addAction(addAct(QStringLiteral("压缩文件…"), {}, {}, [this] { chooseCompressSource(false); }));
    file->addAction(addAct(QStringLiteral("压缩文件夹…"), {}, {}, [this] { chooseCompressSource(true); }));
    file->addSeparator();
    file->addAction(addAct(QStringLiteral("退出"), {}, QKeySequence::Quit, [this] { close(); }));

    QMenu *tools = ui->menubar->addMenu(QStringLiteral("工具"));
    tools->addAction(addAct(QStringLiteral("校验归档"), {}, {}, [this] { runTest(); }));
    tools->addAction(addAct(QStringLiteral("换口令…"), {}, {}, [this] { rekey(); }));
    tools->addAction(addAct(QStringLiteral("添加口令…"), {}, {}, [this] { addKey(); }));
    tools->addSeparator();
    tools->addAction(addAct(QStringLiteral("关联 .cxz（右键「压缩为 CXZ」+ 新建菜单）"), {}, {},
                           [this] { associate(); }));

    QMenu *help = ui->menubar->addMenu(QStringLiteral("帮助"));
    help->addAction(addAct(QStringLiteral("关于 QingXi CXZ"), {}, {}, [this] { aboutDlg(); }));
}

QStringList MainWindow::compressArgs(const QString &in, const QString &out) const
{
    QStringList a;
    a << "-a" << "cxz" << "--machine";
    a << "--cipher" << cipherNameFromCombo();
    a << "-i" << QString::number(200000);
    a << QStringLiteral("-%1").arg(ui->comboBox->currentData().toInt());
    a << "-j" << QString::number(ui->comboBox_2->currentData().toInt());
    a << "-f" << "--pass-stdin";
    a << in << out;
    return a;
}

void MainWindow::updateCommandPreviews()
{
    QStringList c = {"-a", "cxz", "--machine", "--cipher", cipherNameFromCombo(), "-i", "200000",
                     QStringLiteral("-%1").arg(ui->comboBox->currentData().toInt()),
                     "-j", QString::number(ui->comboBox_2->currentData().toInt()),
                     "-f", "--pass-stdin", "{in_file}", "{out_file}"};
    ui->lineEditForCmd->setText(m_cli->commandText(c));
    QStringList d = {"-d", "--machine", "--pass-stdin", "-f", "{in_file}", "{out_dir}"};
    ui->lineEditForCmd_2->setText(m_cli->commandText(d));
    ui->lineEditForCmd->setToolTip(QStringLiteral("实际执行的就是这条命令（口令通过 stdin 传入，不出现命令行里）"));
    ui->lineEditForCmd_2->setToolTip(ui->lineEditForCmd->toolTip());
}

QString MainWindow::cipherNameFromCombo() const
{
    return ui->comboBoxCipher->currentData().toString();
}

void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    ui->pushButton->setEnabled(!busy);
    ui->pushButton_2->setEnabled(!busy && !m_archive.isEmpty());
    ui->toolBar->setEnabled(!busy);
    if (busy) QApplication::setOverrideCursor(Qt::BusyCursor);
    else QApplication::restoreOverrideCursor();
    if (!busy && ui->progressBar->value() >= 1000) ui->progressBar->setValue(0);
}

void MainWindow::setStatus(const QString &s)
{
    ui->statusbar->showMessage(s, 6000);
    ui->label_4->setText(s);
}

void MainWindow::progressReset()
{
    ui->progressBar->setValue(0);
    ui->label_4->setText(QStringLiteral("0%"));
    m_clock.start();
}

void MainWindow::progressSet(qint64 done, qint64 total, const QString &phase)
{
    const int pct = total > 0 ? (int)std::llround(100.0 * (double)done / (double)total) : 0;
    ui->progressBar->setValue(total > 0 ? qBound(0, pct * 10, 1000) : 0);
    const double sec = m_clock.elapsed() / 1000.0;
    QString text = QStringLiteral("%1%").arg(pct);
    if (sec > 0.4 && done > 0) {
        const double speed = (double)done / 1048576.0 / sec;
        text += QStringLiteral("  %1 MB/s").arg(QString::number(speed, 'f', 1));
        if (pct > 3 && pct < 100)
            text += QStringLiteral("  剩 %1s").arg((int)std::ceil(sec * (100.0 - pct) / pct));
    }
    ui->label_4->setText(text);
    if (!phase.isEmpty()) ui->statusbar->showMessage(phase);
}

QString MainWindow::defaultOutFor(const QString &in) const
{
    QFileInfo fi(in);
    QString name = fi.fileName();
    if (name.isEmpty()) name = QStringLiteral("archive");
    return fi.absolutePath() + "/" + name + ".cxz";
}

bool MainWindow::confirmPassword()
{
    if (ui->lineEditPass->text().isEmpty()) return true;
    bool ok = false;
    const QString again = QInputDialog::getText(this, QStringLiteral("确认密码"),
                                                QStringLiteral("再输一次密码（防止打错导致自己也解不开）"),
                                                QLineEdit::Password, QString(), &ok);
    if (!ok) return false;
    if (again != ui->lineEditPass->text()) {
        QMessageBox::warning(this, QStringLiteral("密码不一致"), QStringLiteral("两次输入的密码不一样，已取消。"));
        return false;
    }
    return true;
}

void MainWindow::chooseCompressSource(bool folder)
{
    if (m_busy) return;
    QString in = folder ? QFileDialog::getExistingDirectory(this, QStringLiteral("选择要压缩的文件夹"))
                        : QFileDialog::getOpenFileName(this, QStringLiteral("选择要压缩的文件"));
    if (in.isEmpty()) return;
    compressPath(in, defaultOutFor(in));
}

void MainWindow::compressPath(const QString &inPath, const QString &outPath)
{
    if (m_busy) return;
    if (!confirmPassword()) return;
    QFileInfo oi(outPath), ii(inPath);
    if (oi.exists() && oi.canonicalFilePath() == ii.canonicalFilePath()) {
        QMessageBox::warning(this, QStringLiteral("输出冲突"), QStringLiteral("输出文件和输入是同一个文件。"));
        return;
    }
    ui->logView->clear();
    progressReset();
    setStatus(QStringLiteral("正在压缩…"));
    const QString pass = ui->lineEditPass->text();
    runAsync(compressArgs(inPath, outPath), pass, [this, outPath, pass](int code, const QString &msg) {
        if (code == 0) {
            m_pass = pass;
            openArchivePath(outPath);
            setStatus(QStringLiteral("压缩完成"));
        } else {
            QMessageBox::critical(this, QStringLiteral("压缩失败"), msg.left(2000));
            setStatus(QStringLiteral("压缩失败"));
        }
    });
}

void MainWindow::runAsync(const QStringList &args, const QString &password,
                          std::function<void(int, const QString &)> onDone)
{
    if (m_busy) return;
    setBusy(true);
    auto conn = QSharedPointer<QMetaObject::Connection>::create();
    *conn = connect(m_cli, &CxzCli::done, this, [this, onDone, conn](int code, const QString &out, const QString &err) {
        disconnect(*conn);
        setBusy(false);
        const QString msg = code == 0 ? QString() : (err.isEmpty() ? out : err);
        if (!msg.isEmpty()) ui->logView->appendPlainText(msg.trimmed());
        onDone(code, msg);
    });
    if (!m_cli->start(args, password)) {
        disconnect(*conn);
        setBusy(false);
        onDone(-1, QStringLiteral("无法启动 CXZ.exe：") + m_cli->exePath());
    }
}

QString MainWindow::passForRun(QString *out) const
{
    if (m_archive.isEmpty()) return QString();
    if (!m_encrypted) return QString();
    if (!m_pass.isEmpty()) {
        if (out) *out = m_pass;
        return m_pass;
    }
    bool ok = false;
    const QString p = QInputDialog::getText(const_cast<MainWindow *>(this), QStringLiteral("需要密码"),
                                            QStringLiteral("这个归档是加密的，请输入密码"), QLineEdit::Password,
                                            QString(), &ok);
    if (!ok) return QString();
    if (out) *out = p;
    return p;
}

void MainWindow::ensureOpenOrWarn()
{
    if (m_archive.isEmpty())
        QMessageBox::information(this, QStringLiteral("没有打开压缩包"),
                                 QStringLiteral("先打开一个 .cxz / .zip，或从资源管理器双击它。"));
}

void MainWindow::openFileDialog()
{
    const QString f = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开压缩包"), QString(),
        QStringLiteral("压缩包 (*.cxz *.zip *.deflate *.bwt *.lz77 *.lzw *.ac *.huff *.rle);;所有文件 (*)"));
    if (!f.isEmpty()) openArchivePath(f);
}

void MainWindow::openArchivePath(const QString &path)
{
    m_archive = QDir::cleanPath(path);
    m_encrypted = false;
    refreshArchive(m_pass);
}

void MainWindow::refreshArchive(const QString &password)
{
    if (m_archive.isEmpty()) return;
    ui->tree->clear();
    m_entries = 0;
    m_rawTotal = m_packedTotal = 0;
    progressReset();
    setBusy(true);
    setStatus(QStringLiteral("正在读取归档…"));

    const CxzCli::Result r = m_cli->runSync({"-l", "--machine", "--pass-stdin", m_archive}, password);
    setBusy(false);

    if (r.code == 0) {
        m_pass = password;
        fillFromListing(r.out);
        return;
    }
    const QString msg = r.out.trimmed();
    if (msg.contains(QStringLiteral("wrong password"))) {
        bool ok = false;
        const QString p = QInputDialog::getText(this, QStringLiteral("密码不对"),
                                                QStringLiteral("请输入这个归档的密码"), QLineEdit::Password,
                                                QString(), &ok);
        if (ok && !p.isEmpty()) {
            m_pass = p;
            refreshArchive(p);
            return;
        }
    }
    ui->labelInfo->setText(QStringLiteral("无法读取：") + msg);
    setStatus(QStringLiteral("读取失败"));
}

void MainWindow::fillFromListing(const QString &text)
{
    ui->tree->clear();
    m_entries = 0;
    m_rawTotal = 0;
    QString title = QStringLiteral("CXZ");
    QString cipher;
    const qint64 packed = QFileInfo(m_archive).size();

    const QStringList lines = text.split('\n');
    for (const QString &raw : lines) {
        const QString s = raw.trimmed();
        if (s.isEmpty()) continue;
        if (s.startsWith(QStringLiteral("INFO|"))) {
            const QStringList f = s.split(QLatin1Char('|'));
            if (f.size() >= 3) cipher = f[2];
        } else if (s.startsWith(QStringLiteral("TITLE|"))) {
            title = s.mid(6);
        } else if (s.startsWith(QStringLiteral("ENTRY|"))) {
            const QString body = s.mid(6);
            const int bar = body.indexOf(QLatin1Char('|'));
            if (bar <= 0) continue;
            const qint64 size = body.left(bar).toLongLong();
            const QString name = body.mid(bar + 1);
            if (name.isEmpty()) continue;
            const bool isDir = name.endsWith(QLatin1Char('/'));
            QTreeWidgetItem *it = new QTreeWidgetItem(ui->tree);
            it->setText(0, name);
            it->setText(1, isDir ? QString() : humanSize(size));
            QString type = isDir ? QStringLiteral("文件夹") : QFileInfo(name).suffix().toLower();
            if (type.isEmpty()) type = QStringLiteral("文件");
            it->setText(2, type);
            it->setIcon(0, style()->standardIcon(isDir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
            it->setData(0, Qt::UserRole, size);
            m_rawTotal += size;
            ++m_entries;
        } else if (s.startsWith(QStringLiteral("TOTAL|"))) {
            const QStringList f = s.split(QLatin1Char('|'));
            if (f.size() >= 2) m_rawTotal = f[1].toLongLong();
        }
    }

    m_encrypted = !cipher.isEmpty() && cipher != QLatin1String("-");
    QString head = QDir::toNativeSeparators(m_archive) + QStringLiteral("\n");
    head += QStringLiteral("%1  |  %2  |  %3 个条目  |  原始 %4  →  归档 %5")
                .arg(title)
                .arg(m_encrypted ? cipher : QStringLiteral("未加密"))
                .arg(m_entries)
                .arg(humanSize(m_rawTotal))
                .arg(humanSize(packed));
    if (m_rawTotal > 0)
        head += QStringLiteral("  (%1%)").arg(QString::number(100.0 * packed / m_rawTotal, 'f', 2));
    head += QStringLiteral("\n把文件拖进来可追加；双击条目可预览。");
    ui->labelInfo->setText(head);
    ui->pushButton_2->setEnabled(true);
    setStatus(QStringLiteral("就绪"));
}

void MainWindow::extractTo(bool selectedOnly)
{
    if (m_busy) return;
    ensureOpenOrWarn();
    if (m_archive.isEmpty()) return;
    if (m_encrypted && m_pass.isEmpty()) {
        if (passForRun(nullptr).isEmpty()) return;
    }

    const QString def = QFileInfo(m_archive).absolutePath() + "/" + QFileInfo(m_archive).completeBaseName();
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("解压到…"), def);
    if (dir.isEmpty()) return;

    QStringList args{"-d", "--machine", "--to-dir", "--pass-stdin", "-f"};
    if (selectedOnly) {
        const QList<QTreeWidgetItem *> sel = ui->tree->selectedItems();
        if (sel.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("没有选中条目"),
                                     QStringLiteral("先在列表里选几行，或用「解压到…」解全部。"));
            return;
        }
        for (const QTreeWidgetItem *it : sel) {
            QString name = it->text(0);
            args << "--only" << name;
        }
    }
    args << m_archive << dir;
    ui->logView->clear();
    progressReset();
    setStatus(QStringLiteral("正在解压…"));
    runAsync(args, m_pass, [this, dir](int code, const QString &msg) {
        if (code == 0) {
            setStatus(QStringLiteral("解压完成"));
            if (QMessageBox::question(this, QStringLiteral("完成"),
                                      QStringLiteral("已解压到：\n%1\n\n要打开这个文件夹吗？")
                                          .arg(QDir::toNativeSeparators(dir)))
                QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        } else {
            QMessageBox::critical(this, QStringLiteral("解压失败"), msg.left(2000));
            setStatus(QStringLiteral("解压失败"));
        }
    });
}

void MainWindow::previewEntry(QTreeWidgetItem *it)
{
    if (!it || m_busy || m_archive.isEmpty()) return;
    const QString name = it->text(0);
    if (name.endsWith(QLatin1Char('/'))) return;

    const QString tmp = QDir::tempPath() + "/QingXi-CXZ-preview/" +
                        QString::number(static_cast<qulonglong>(qHash(m_archive + name)));
    QDir(tmp).mkpath(QStringLiteral("."));
    setStatus(QStringLiteral("正在提取 %1 …").arg(name));
    runAsync({"-d", "--machine", "--to-dir", "--pass-stdin", "-f", "--only", name, m_archive, tmp}, m_pass,
             [this, name, tmp](int code, const QString &msg) {
                 if (code != 0) {
                     setStatus(QStringLiteral("提取失败"));
                     ui->logView->appendPlainText(msg);
                     return;
                 }
                 const QString path = tmp + "/" + name;
                 if (!QFileInfo::exists(path)) {
                     setStatus(QStringLiteral("提取后没找到文件"));
                     return;
                 }
                 setStatus(QStringLiteral("已用系统默认程序打开"));
                 QDesktopServices::openUrl(QUrl::fromLocalFile(path));
             });
}

void MainWindow::runTest()
{
    if (m_busy) return;
    ensureOpenOrWarn();
    if (m_archive.isEmpty()) return;
    setBusy(true);
    const CxzCli::Result r = m_cli->runSync({"-l", "--machine", "--pass-stdin", m_archive}, m_pass);
    setBusy(false);
    if (r.code == 0)
        QMessageBox::information(this, QStringLiteral("校验通过"),
                                 QStringLiteral("HMAC-SHA256 认证标签与 CRC 全部一致，%1 个条目都能正常解密。")
                                     .arg(m_entries));
    else
        QMessageBox::critical(this, QStringLiteral("校验失败"), r.out.left(2000));
}

void MainWindow::rekey()
{
    if (m_busy) return;
    ensureOpenOrWarn();
    if (m_archive.isEmpty()) return;
    if (!m_encrypted) {
        QMessageBox::information(this, QStringLiteral("这个归档没有加密"),
                                 QStringLiteral("未加密的归档没有口令可换。"));
        return;
    }
    QString old = m_pass;
    if (old.isEmpty()) old = passForRun(nullptr);
    if (old.isEmpty()) return;
    bool ok = false;
    const QString np = QInputDialog::getText(this, QStringLiteral("换口令"),
                                             QStringLiteral("输入新口令（旧口令将失效；正文不会重新压缩）"),
                                             QLineEdit::Password, QString(), &ok);
    if (!ok || np.isEmpty()) return;
    ui->logView->clear();
    runAsync({"-P", np, "--machine", "--pass-stdin", "-f", m_archive}, old,
             [this, np](int code, const QString &msg) {
                 if (code == 0) {
                     m_pass = np;
                     refreshArchive(np);
                     setStatus(QStringLiteral("口令已更换"));
                 } else {
                     QMessageBox::critical(this, QStringLiteral("换口令失败"), msg.left(2000));
                 }
             });
}

void MainWindow::addKey()
{
    if (m_busy) return;
    ensureOpenOrWarn();
    if (m_archive.isEmpty()) return;
    if (!m_encrypted) {
        QMessageBox::information(this, QStringLiteral("这个归档没有加密"),
                                 QStringLiteral("先加密压缩一次，才能追加口令槽。"));
        return;
    }
    QString old = m_pass;
    if (old.isEmpty()) old = passForRun(nullptr);
    if (old.isEmpty()) return;
    bool ok = false;
    const QString np = QInputDialog::getText(this, QStringLiteral("添加口令"),
                                             QStringLiteral("再给这个归档加一个口令（两个口令都能打开）"),
                                             QLineEdit::Password, QString(), &ok);
    if (!ok || np.isEmpty()) return;
    ui->logView->clear();
    runAsync({"-A", np, "--machine", "--pass-stdin", "-f", m_archive}, old,
             [this](int code, const QString &msg) {
                 if (code == 0) {
                     refreshArchive(m_pass);
                     QMessageBox::information(this, QStringLiteral("已添加"), QStringLiteral("新口令已写入归档。"));
                 } else {
                     QMessageBox::critical(this, QStringLiteral("添加失败"), msg.left(2000));
                 }
             });
}

void MainWindow::newArchiveDialog()
{
    const QString f = QFileDialog::getSaveFileName(this, QStringLiteral("新建空 CXZ 压缩包"), QString(),
                                                   QStringLiteral("CXZ 压缩包 (*.cxz)"));
    if (f.isEmpty()) return;
    createEmptyArchive(f);
}

void MainWindow::createEmptyArchive(const QString &path)
{
    if (m_busy) return;
    const QString pass = ui->lineEditPass->text();
    ui->logView->clear();
    runAsync({"--create", path, "--machine", "-f", "--pass-stdin"}, pass, [this, path, pass](int code,
                                                                                            const QString &msg) {
        if (code == 0) {
            m_pass = pass;
            openArchivePath(path);
            setStatus(QStringLiteral("已新建空压缩包"));
        } else {
            QMessageBox::critical(this, QStringLiteral("新建失败"), msg.left(2000));
        }
    });
}

void MainWindow::shellCompress(const QStringList &paths)
{
    if (paths.isEmpty() || m_busy) return;
    ui->lineEditPass->clear();
    QString first;
    for (const QString &p : paths) {
        const QString out = defaultOutFor(QDir::cleanPath(p));
        const CxzCli::Result r = m_cli->runSync({"-a", "cxz", "--machine", "--cipher", QStringLiteral("aes256"),
                                                 "-i", "200000", "-5", "-j", "4", "-f", p, out});
        if (r.code == 0 && first.isEmpty()) first = out;
    }
    if (!first.isEmpty()) {
        m_pass.clear();
        openArchivePath(first);
        setStatus(QStringLiteral("已压缩"));
    }
}

void MainWindow::associate()
{
    if (writeShellRegistry(QCoreApplication::applicationFilePath()))
        QMessageBox::information(this, QStringLiteral("已关联"),
                                 QStringLiteral("已写入当前用户的资源管理器设置：\n"
                                                "· .cxz 双击用本程序打开\n"
                                                "· 文件 / 文件夹右键「压缩为 CXZ」\n"
                                                "· 新建菜单里的「CXZ 压缩包」"));
    else
        QMessageBox::warning(this, QStringLiteral("关联失败"),
                             QStringLiteral("写注册表失败（HKCU\\Software\\Classes）。"));
}

void MainWindow::aboutDlg()
{
    QMessageBox::about(this, QStringLiteral("关于 QingXi · CXZ"),
                       QStringLiteral("<b>QingXi · CXZ 压缩包</b>（图形前端）<br><br>"
                                      "这个程序只做界面：压缩、加密、解压、校验全部由同目录的 "
                                      "<code>CXZ.exe</code> 完成，引擎不内嵌。<br>"
                                      "加密等级：AES-64 / AES-128 / AES-192 / AES-256 / AES-512"
                                      "（另有 ChaCha20 兼容档）。<br>"
                                      "口令只经 stdin 传给引擎，不进命令行与日志。<br><br>"
                                      "诚实说明：AES-64 的有效强度只有 64 位；AES-512 是两把 AES-256 "
                                      "串联（512 位密钥材料），不是 NIST 标准算法名；无前向保密、"
                                      "无国密资质，不能称“国家机密级合规”。"));
}

bool MainWindow::isBusy() const { return m_busy; }

QStringList MainWindow::entryNames() const
{
    QStringList r;
    for (int i = 0; i < ui->tree->topLevelItemCount(); ++i) r << ui->tree->topLevelItem(i)->text(0);
    return r;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile()) paths << u.toLocalFile();
    if (paths.isEmpty()) return;
    e->acceptProposedAction();

    if (m_archive.isEmpty() && paths.size() == 1) {
        const QString suf = QFileInfo(paths[0]).suffix().toLower();
        if (suf == QLatin1String("cxz") || suf == QLatin1String("zip")) {
            openArchivePath(paths[0]);
            return;
        }
    }
    if (m_archive.isEmpty()) {
        compressPath(paths[0], defaultOutFor(paths[0]));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("追加文件"),
                              QStringLiteral("把拖入的 %1 个项目追加到当前压缩包？\n（会用当前口令重新生成归档）")
                                  .arg(paths.size())) != QMessageBox::Yes)
        return;

    QStringList args{"--append", m_archive, "--machine", "-f", "--pass-stdin"};
    args << paths;
    ui->logView->clear();
    progressReset();
    runAsync(args, m_pass, [this](int code, const QString &msg) {
        if (code == 0) {
            refreshArchive(m_pass);
            setStatus(QStringLiteral("已追加"));
        } else {
            QMessageBox::critical(this, QStringLiteral("追加失败"), msg.left(2000));
        }
    });
}
