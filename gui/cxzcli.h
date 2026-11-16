#pragma once

// CXZ.exe 的唯一调用层：GUI 不内嵌任何压缩/加密代码，全部通过这个进程包装器完成。
// 口令一律走 stdin（--pass-stdin），绝不拼进命令行，避免出现在进程列表/日志里。

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class CxzCli : public QObject
{
    Q_OBJECT

public:
    explicit CxzCli(QObject *parent = nullptr);

    struct Result {
        int code = -1;
        QString out;
        QString err;
        bool ok() const { return code == 0; }
    };

    // 定位 CXZ.exe：程序目录 > 上级 > 构建/发布目录 > 环境变量 CXZ_CLI
    static QString locateCli();
    QString exePath() const { return m_exe; }
    void setExePath(const QString &p) { m_exe = p; }

    // 同步执行（短命令：列清单 / 校验 / 换口令）
    Result runSync(const QStringList &args, const QString &password = QString(), int timeoutMs = 600000);

    // 异步执行（长命令：压缩 / 解压），进度通过 progress() 信号回来
    bool start(const QStringList &args, const QString &password = QString());
    void cancel();
    bool running() const;

    // 给人看的命令行文本（真执行时用的是 args，口令不会出现在这里）
    QString commandText(const QStringList &args) const;

signals:
    void progress(qint64 done, qint64 total, const QString &phase);
    void line(const QString &text);
    void done(int code, const QString &out, const QString &err);

private:
    void handleOutput();
    QString m_exe;
    QProcess *m_p = nullptr;
    QString m_out, m_err;
};
