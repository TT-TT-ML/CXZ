#include "cxzcli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

CxzCli::CxzCli(QObject *parent) : QObject(parent)
{
    m_exe = locateCli();
}

QString CxzCli::locateCli()
{
    const QString env = QProcessEnvironment::systemEnvironment().value("CXZ_CLI");
    if (!env.isEmpty() && QFileInfo::exists(env)) return env;

    const QString app = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        app + "/CXZ.exe",
        app + "/../CXZ.exe",
        app + "/../../CXZ.exe",
        app + "/../../my_zip/dist/CXZ.exe",
        app + "/../../my_zip/build/CXZ.exe",
        app + "/../../../my_zip/dist/CXZ.exe",
        app + "/../../../my_zip/build/CXZ.exe",
        "E:/codes/my_zip/dist/CXZ.exe",
        "E:/codes/my_zip/build/CXZ.exe",
    };
    for (const QString &c : cands)
        if (QFileInfo::exists(c)) return QDir::cleanPath(c);
    return app + "/CXZ.exe";
}

QString CxzCli::commandText(const QStringList &args) const
{
    QString s = QString("\"") + QDir::toNativeSeparators(m_exe) + "\"";
    for (const QString &a : args) {
        s += ' ';
        if (a.isEmpty() || a.contains(' ')) s += QString("\"") + a + "\"";
        else s += a;
    }
    return s;
}

CxzCli::Result CxzCli::runSync(const QStringList &args, const QString &password, int timeoutMs)
{
    Result r;
    QProcess p;
    p.setProgram(m_exe);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForStarted(5000)) {
        r.err = QStringLiteral("无法启动 CXZ.exe：") + m_exe;
        return r;
    }
    if (!password.isEmpty()) p.write(password.toUtf8() + "\n");
    p.closeWriteChannel();
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        r.code = -2;
        r.err = QStringLiteral("命令超时或已取消");
        r.out = QString::fromUtf8(p.readAll());
        return r;
    }
    r.code = p.exitCode();
    r.out = QString::fromUtf8(p.readAll());
    r.err = r.code == 0 ? QString() : r.out;
    return r;
}

bool CxzCli::start(const QStringList &args, const QString &password)
{
    if (running()) return false;
    if (!m_p) {
        m_p = new QProcess(this);
        m_p->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_p, &QProcess::readyRead, this, &CxzCli::handleOutput);
        connect(m_p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus) {
                    handleOutput();
                    emit done(code, m_out, code == 0 ? QString() : m_err);
                });
    }
    m_out.clear();
    m_err.clear();
    m_p->setProgram(m_exe);
    m_p->setArguments(args);
    m_p->start();
    if (!m_p->waitForStarted(5000)) {
        m_err = QStringLiteral("无法启动 CXZ.exe：") + m_exe;
        emit done(-1, m_out, m_err);
        return false;
    }
    if (!password.isEmpty()) m_p->write(password.toUtf8() + "\n");
    m_p->closeWriteChannel();
    return true;
}

void CxzCli::cancel()
{
    if (!running()) return;
    m_p->kill();
}

bool CxzCli::running() const
{
    return m_p && m_p->state() != QProcess::NotRunning;
}

void CxzCli::handleOutput()
{
    if (!m_p) return;
    const QByteArray chunk = m_p->readAll();
    m_out += QString::fromUtf8(chunk);

    // 只按行解析：PROG|done|total|phase 是进度，其余原样当日志
    static const QRegularExpression reProg(QStringLiteral("^PROG\\|(\\d+)\\|(\\d+)\\|(.*)$"));
    QStringList lines = m_out.split('\n');
    m_out = lines.takeLast();            // 最后一段可能还没写完整，留到下一次
    for (const QString &raw : lines) {
        const QString s = raw.trimmed();
        if (s.isEmpty()) continue;
        const QRegularExpressionMatch m = reProg.match(s);
        if (m.hasMatch())
            emit progress(m.captured(1).toLongLong(), m.captured(2).toLongLong(), m.captured(3));
        else
            emit line(s);
    }
    if (!m_out.isEmpty() && m_out.startsWith("PROG|")) {
        // 半行进度也允许先更新一次，视觉上更连续
        const QRegularExpressionMatch m = reProg.match(m_out);
        if (m.hasMatch())
            emit progress(m.captured(1).toLongLong(), m.captured(2).toLongLong(), m.captured(3));
    }
}
