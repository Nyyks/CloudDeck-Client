#include "displaylease.h"

#include "agenttunnel.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslSocket>
#include <QtDebug>

DisplayLease::DisplayLease(QString host, quint16 httpsPort, QSslCertificate serverCert, int width, int height, int fps)
    : m_Host(std::move(host)), m_Port(httpsPort), m_ServerCert(std::move(serverCert)),
      m_Width(width), m_Height(height), m_Fps(fps)
{
}

DisplayLease::~DisplayLease()
{
    requestInterruption();
    wait();
}

QString DisplayLease::acquire(int timeoutMs)
{
    start();
    if (!m_Answered.tryAcquire(1, timeoutMs)) {
        requestInterruption(); // the thread gives up and closes; nothing stays changed for long
        return tr("the CloudDeck did not answer in time");
    }
    QMutexLocker l(&m_Lock);
    return m_Error;
}

QString DisplayLease::mode() const
{
    QMutexLocker l(&m_Lock);
    return m_Mode;
}

void DisplayLease::answer(const QString& error, const QString& mode)
{
    if (m_AnswerGiven.fetchAndStoreOrdered(true)) return;
    {
        QMutexLocker l(&m_Lock);
        m_Error = error;
        m_Mode = mode;
    }
    m_Answered.release();
}

void DisplayLease::run()
{
    QSslSocket s;
    const QDeadlineTimer deadline(40000);
    const auto interrupted = [this]() { return isInterruptionRequested(); };

    const QByteArray target = QStringLiteral("display/set?width=%1&height=%2&fps=%3")
                                  .arg(m_Width).arg(m_Height).arg(m_Fps).toUtf8();
    QByteArray buf;
    const QString error = AgentTunnel::open(s, m_Host, m_Port, m_ServerCert, target, deadline, interrupted, buf);
    if (!error.isEmpty()) {
        answer(error);
        return;
    }
    QByteArray line;
    if (!AgentTunnel::readLine(s, buf, line, deadline, interrupted)) {
        answer(tr("the CloudDeck did not confirm the display change"));
        return;
    }
    const QJsonObject st = QJsonDocument::fromJson(line).object();
    const QString mode = st.value("mode").toString();
    qInfo() << "CloudDeck display set to" << mode << "- restores" << st.value("restores").toString() << "when the stream ends";
    answer(QString(), mode);

    // Hold the lease. A newline every 30 s keeps idle-timeouts on the path away.
    QElapsedTimer sinceKeepalive;
    sinceKeepalive.start();
    while (!isInterruptionRequested()) {
        if (s.waitForReadyRead(250)) s.readAll();
        if (s.state() != QAbstractSocket::ConnectedState) {
            qWarning() << "CloudDeck display lease closed by the host";
            return;
        }
        if (sinceKeepalive.elapsed() > 30000) {
            s.write("\n");
            s.waitForBytesWritten(1000);
            sinceKeepalive.restart();
        }
    }
    s.disconnectFromHost();
    if (s.state() != QAbstractSocket::UnconnectedState) s.waitForDisconnected(2000);
    qInfo() << "CloudDeck display lease released";
}
