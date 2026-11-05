#include "fileupload.h"

#include "agenttunnel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslSocket>
#include <QUrl>
#include <QtDebug>

FileUploader::FileUploader(QString host, quint16 httpsPort, QSslCertificate serverCert)
    : m_Host(std::move(host)), m_Port(httpsPort), m_ServerCert(std::move(serverCert))
{
    setObjectName("CloudDeck upload");
}

FileUploader::~FileUploader()
{
    requestInterruption();
    wait();
}

void FileUploader::enqueue(const QStringList& localPaths)
{
    bool needStart = false;
    {
        QMutexLocker l(&m_Lock);
        m_Queue += localPaths;
        m_BatchTotal += localPaths.size();
        if (!m_Active) { // decided under the lock, so a finishing run can't miss these
            m_Active = true;
            needStart = true;
        }
    }
    if (needStart) {
        wait(); // a previous run may still be emitting its summary
        start();
    }
}

void FileUploader::run()
{
    int index = 0, ok = 0;
    QStringList failures;
    for (;;) {
        QString path;
        int total;
        {
            QMutexLocker l(&m_Lock);
            if (m_Queue.isEmpty() || isInterruptionRequested()) {
                m_Queue.clear();
                m_BatchTotal = 0;
                m_Active = false;
                break;
            }
            path = m_Queue.takeFirst();
            total = m_BatchTotal;
        }
        index++;
        QString error;
        const QString saved = uploadOne(path, index, total, &error);
        if (saved.isEmpty()) {
            qWarning() << "CloudDeck upload of" << path << "failed:" << error;
            failures << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
        }
        else {
            qInfo() << "CloudDeck upload of" << path << "saved as" << saved;
            ok++;
        }
    }
    if (isInterruptionRequested() || index == 0) return;

    QString summary;
    if (failures.isEmpty()) {
        summary = ok == 1 ? tr("Uploaded to Downloads/CloudDeck on your CloudDeck")
                          : tr("Uploaded %1 files to Downloads/CloudDeck on your CloudDeck").arg(ok);
    }
    else {
        summary = tr("%1 of %2 uploads failed. %3").arg(failures.size()).arg(index).arg(failures.first());
    }
    emit batchFinished(summary, failures.isEmpty());
}

QString FileUploader::uploadOne(const QString& path, int index, int total, QString* error)
{
    const QFileInfo fi(path);
    if (!fi.isFile()) {
        *error = fi.isDir() ? tr("folders can't be uploaded (zip it first)") : tr("not a file");
        return QString();
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return QString();
    }
    const qint64 size = f.size();
    const QString name = fi.fileName();
    const QString counter = total > 1 ? QStringLiteral(" (%1 of %2)").arg(index).arg(total) : QString();
    emit progress(tr("Uploading %1%2").arg(name, counter));

    const auto interrupted = [this]() { return isInterruptionRequested(); };
    QSslSocket s;
    QByteArray rest;
    const QByteArray target = "file/upload?name=" + QUrl::toPercentEncoding(name) + "&size=" + QByteArray::number(size);
    *error = AgentTunnel::open(s, m_Host, m_Port, m_ServerCert, target, QDeadlineTimer(20000), interrupted, rest);
    if (!error->isEmpty()) return QString();

    QElapsedTimer sinceReport;
    sinceReport.start();
    qint64 sent = 0;
    QByteArray chunk;
    while (sent < size) {
        if (isInterruptionRequested() || s.state() != QAbstractSocket::ConnectedState) {
            *error = tr("cancelled");
            return QString();
        }
        chunk = f.read(256 * 1024);
        if (chunk.isEmpty()) {
            *error = tr("could not read the file");
            return QString();
        }
        s.write(chunk);
        sent += chunk.size();
        // Keep at most ~2 MiB in flight so memory stays flat for big files.
        while (s.bytesToWrite() > 2 * 1024 * 1024 && !isInterruptionRequested()) {
            if (!s.waitForBytesWritten(1000) && s.state() != QAbstractSocket::ConnectedState) break;
        }
        if (sinceReport.elapsed() > 250) {
            emit progress(tr("Uploading %1%2: %3%").arg(name, counter).arg(size ? sent * 100 / size : 100));
            sinceReport.restart();
        }
    }
    while (s.bytesToWrite() > 0 && !isInterruptionRequested()) {
        if (!s.waitForBytesWritten(5000) && s.state() != QAbstractSocket::ConnectedState) break;
    }
    emit progress(tr("Saving %1%2...").arg(name, counter));

    QByteArray line;
    QByteArray buf = rest;
    if (!AgentTunnel::readLine(s, buf, line, QDeadlineTimer(120000), interrupted)) {
        *error = tr("the CloudDeck did not confirm the upload");
        return QString();
    }
    const QJsonObject r = QJsonDocument::fromJson(line).object();
    if (r.contains("saved")) return r.value("saved").toString();
    *error = r.value("error").toString(tr("unknown error"));
    return QString();
}
