#include "mainwindow.h"
#include "cxzcli.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

// ------------------------------------------------------------------ 资源管理器集成
// 只写 HKCU（当前用户），不动系统级设置，也不需要管理员权限。
bool MainWindow::writeShellRegistry(const QString &exePath)
{
    const QString exe = QDir::toNativeSeparators(exePath);
    const QString quoted = QString("\"") + exe + "\"";
    QSettings cls(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);

    // 1) .cxz 双击用本程序打开
    cls.setValue(QStringLiteral("QingXiCXZ.CXZ/Default"), QStringLiteral("QingXi CXZ 压缩包"));
    cls.setValue(QStringLiteral("QingXiCXZ.CXZ/shell/open/command/Default"), quoted + " \"%1\"");
    cls.setValue(QStringLiteral("QingXiCXZ.CXZ/DefaultIcon/Default"), quoted + ",0");
    cls.setValue(QStringLiteral(".cxz/Default"), QStringLiteral("QingXiCXZ.CXZ"));
    cls.setValue(QStringLiteral(".cxz/Content Type"), QStringLiteral("application/x-cxz"));
    cls.setValue(QStringLiteral(".cxz/PerceivedType"), QStringLiteral("compressed"));

    // 2) 右键「压缩为 CXZ」——文件与文件夹都注册
    cls.setValue(QStringLiteral("*/shell/QingXiCXZCompress/Default"), QStringLiteral("压缩为 CXZ"));
    cls.setValue(QStringLiteral("*/shell/QingXiCXZCompress/MuiVerb"), QStringLiteral("压缩为 CXZ"));
    cls.setValue(QStringLiteral("*/shell/QingXiCXZCompress/command/Default"),
                 quoted + " --shell-compress \"%1\"");
    cls.setValue(QStringLiteral("Directory/shell/QingXiCXZCompress/Default"), QStringLiteral("压缩为 CXZ"));
    cls.setValue(QStringLiteral("Directory/shell/QingXiCXZCompress/command/Default"),
                 quoted + " --shell-compress \"%1\"");

    // 3) 新建菜单「CXZ 压缩包」：Windows 会执行 ShellNew\Command 生成文件
    cls.setValue(QStringLiteral(".cxz/ShellNew/Command/Default"), quoted + " --create \"%1\"");
    cls.setValue(QStringLiteral(".cxz/ShellNew/ItemName"), QStringLiteral("CXZ 压缩包"));
    cls.sync();
    return cls.status() == QSettings::NoError;
}

bool MainWindow::removeShellRegistry()
{
    QSettings cls(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);
    cls.remove(QStringLiteral("QingXiCXZ.CXZ"));
    cls.remove(QStringLiteral(".cxz"));
    cls.remove(QStringLiteral("*/shell/QingXiCXZCompress"));
    cls.remove(QStringLiteral("Directory/shell/QingXiCXZCompress"));
    cls.sync();
    return cls.status() == QSettings::NoError;
}

// ------------------------------------------------------------------ 无界面自检
// 用法：CXZQt.exe --selftest <日志文件>
// 全程只通过 CXZ.exe 走完整链路：建包 -> 列清单 -> 追加 -> 解压比对 -> 换口令 -> 加口令
static int runSelfTest(const QString &logPath)
{
    QFile log(logPath);
    if (!log.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 3;
    QTextStream say(&log);
    say.setEncoding(QStringConverter::Utf8);
    int fail = 0;
    auto check = [&](bool ok, const QString &what) {
        say << (ok ? "PASS  " : "FAIL  ") << what << "\n";
        if (!ok) ++fail;
    };

    CxzCli cli;
    const QString exe = cli.exePath();
    say << "引擎： " << exe << "\n";
    check(QFileInfo::exists(exe), QStringLiteral("找到 CXZ.exe"));
    if (!QFileInfo::exists(exe)) {
        log.close();
        return 3;
    }

    const QString tmp = QDir::tempPath() + "/qingxi_cxz_gui_selftest";
    QDir(tmp).removeRecursively();
    QDir().mkpath(tmp);
    const QString arc = tmp + "/a.cxz";
    const QString srcFile = tmp + "/hello.txt";
    {
        QFile f(srcFile);
        f.open(QIODevice::WriteOnly);
        f.write("QingXi CXZ selftest payload\n");
    }

    // 1) 新建空归档（加密）
    auto r1 = cli.runSync({"--create", arc, "--machine", "--cipher", "aes256", "-f", "--pass-stdin"}, "pw");
    check(r1.code == 0 && QFileInfo::exists(arc), QStringLiteral("--create 生成加密空归档"));

    // 2) 列清单
    auto r2 = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "pw");
    check(r2.code == 0 && r2.out.contains(QStringLiteral("INFO|")),
          QStringLiteral("--machine -l 输出 INFO 行"));
    check(r2.out.contains(QStringLiteral("AES-256")), QStringLiteral("清单里读出 AES-256"));

    // 3) 追加文件
    auto r3 = cli.runSync({"--append", arc, "--machine", "-f", "--pass-stdin", srcFile}, "pw");
    check(r3.code == 0, QStringLiteral("--append 追加单个文件"));
    auto r3b = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "pw");
    check(r3b.out.contains(QStringLiteral("ENTRY|")), QStringLiteral("追加后清单里有条目"));

    // 4) 解压并逐字节比对
    const QString outDir = tmp + "/out";
    auto r4 = cli.runSync({"-d", "--machine", "--to-dir", "--pass-stdin", "-f", arc, outDir}, "pw");
    check(r4.code == 0 && QFileInfo::exists(outDir + "/hello.txt"), QStringLiteral("解压出 hello.txt"));
    if (QFileInfo::exists(outDir + "/hello.txt")) {
        QFile a(outDir + "/hello.txt"), b(srcFile);
        a.open(QIODevice::ReadOnly);
        b.open(QIODevice::ReadOnly);
        check(a.readAll() == b.readAll(), QStringLiteral("解压内容与原始一致"));
    }

    // 5) 错误口令必须被拒
    auto r5 = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "bad");
    check(r5.code != 0 && r5.out.contains(QStringLiteral("wrong password")),
          QStringLiteral("错误口令被统一文案拒绝"));

    // 6) 换口令
    auto r6 = cli.runSync({"-P", "pw2", "--machine", "-f", "--pass-stdin", arc}, "pw");
    check(r6.code == 0, QStringLiteral("--rekey 换口令"));
    auto r6b = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "pw2");
    check(r6b.code == 0, QStringLiteral("新口令可用"));
    auto r6c = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "pw");
    check(r6c.code != 0, QStringLiteral("旧口令失效"));

    // 7) 追加口令槽：两个口令都能开
    auto r7 = cli.runSync({"-A", "pw3", "--machine", "-f", "--pass-stdin", arc}, "pw2");
    check(r7.code == 0, QStringLiteral("--add-key 追加口令槽"));
    auto r7a = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "pw2");
    auto r7b = cli.runSync({"-l", "--machine", "--pass-stdin", arc}, "pw3");
    check(r7a.code == 0 && r7b.code == 0, QStringLiteral("两个口令都能打开同一个归档"));
    check(r7b.out.contains(QStringLiteral("slots")) || r7b.out.contains(QStringLiteral("INFO|")),
          QStringLiteral("清单正常输出"));

    // 8) 五档加密都能往返
    const QStringList ciphers = {"aes64", "aes128", "aes192", "aes256", "aes512"};
    for (const QString &c : ciphers) {
        const QString a2 = tmp + "/" + c + ".cxz";
        auto rc = cli.runSync({"-a", "cxz", "--machine", "--cipher", c, "-f", "--pass-stdin", srcFile, a2}, "pw");
        const QString d2 = tmp + "/" + c;
        auto rd = cli.runSync({"-d", "--machine", "--to-dir", "--pass-stdin", "-f", a2, d2}, "pw");
        // 单文件归档没有条目名，解压后按归档名落盘（aes64.cxz -> aes64）
        const QString got = d2 + "/" + c;
        bool same = false;
        if (QFileInfo::exists(got)) {
            QFile a(got), b(srcFile);
            if (a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly))
                same = a.readAll() == b.readAll();
        }
        check(rc.code == 0 && rd.code == 0 && same, QStringLiteral("%1 加密往返").arg(c));
    }

    say << "\n" << (fail == 0 ? "ALL TESTS PASSED" : QStringLiteral("FAILURES: %1").arg(fail)) << "\n";
    log.close();
    QDir(tmp).removeRecursively();
    return fail == 0 ? 0 : 1;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CXZQt"));
    QApplication::setOrganizationName(QStringLiteral("QingXi"));
    QApplication::setApplicationDisplayName(QStringLiteral("QingXi · CXZ 压缩包"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icon.ico")));

    QStringList args = QApplication::arguments();
    if (args.size() >= 3 && args[1] == QLatin1String("--selftest"))
        return runSelfTest(args[2]);

    MainWindow w;
    w.show();

    // 资源管理器右键「压缩为 CXZ」
    if (args.size() >= 3 && args[1] == QLatin1String("--shell-compress")) {
        QStringList paths;
        for (int i = 2; i < args.size(); ++i) paths << args[i];
        w.shellCompress(paths);
    } else if (args.size() >= 3 && args[1] == QLatin1String("--create")) {
        w.createEmptyArchive(args[2]);
    } else if (args.size() >= 2 && QFileInfo::exists(args[1])) {
        w.openArchivePath(args[1]);
    } else if (args.size() >= 3 && args[1] == QLatin1String("--remove-assoc")) {
        MainWindow::removeShellRegistry();
        return 0;
    }

    return app.exec();
}
