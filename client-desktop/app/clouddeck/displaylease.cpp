#include "displaylease.h"

#include "backend/identitymanager.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>
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
    QSslConfiguration cfg = IdentityManager::get()->getSslConfig(); // pairing identity (mTLS)
    cfg.setPeerVerifyMode(QSslSocket::VerifyPeer);
    s.setSslConfiguration(cfg);
    // Pin the host certificate learned at pairing, exactly like NvHTTP does. The
    // socket lives in this thread, so this lambda runs synchronously inside waitFor*.
    QObject::connect(&s, &QSslSocket::sslErrors, [&s, this](const QList<QSslError>& errors) {
        for (const QSslError& e : errors) {
            if (e.certificate() != m_ServerCert) return;
        }
        s.ignoreSslErrors(errors);
    });

    // Blocking waits in short slices so the destructor never waits long.
    QDeadlineTimer deadline(40000);
    auto waitFor = [&](auto fn) {
        while (!isInterruptionRequested() && !deadline.hasExpired()) {
            if (fn(250)) return true;
            if (s.state() == QAbstractSocket::UnconnectedState) return false;
        }
        return false;
    };

    s.connectToHostEncrypted(m_Host, m_Port);
    if (!waitFor([&](int ms) { return s.waitForEncrypted(ms); }) || s.peerCertificate() != m_ServerCert) {
        answer(tr("could not open a secure connection to the CloudDeck (%1)").arg(s.errorString()));
        return;
    }

    const QByteArray target = QStringLiteral("/clouddeck/v1/display/set?width=%1&height=%2&fps=%3")
                                  .arg(m_Width).arg(m_Height).arg(m_Fps).toUtf8();
    s.write("GET " + target + " HTTP/1.1\r\nHost: " + m_Host.toUtf8() +
            "\r\nUpgrade: clouddeck-agent/1\r\nConnection: Upgrade\r\n\r\n");

    // Response headers, then (101) one JSON status line or (error) the body.
    QByteArray buf;
    int headerEnd = -1;
    while ((headerEnd = buf.indexOf("\r\n\r\n")) < 0) {
        if (buf.size() > 8192 || !waitFor([&](int ms) { return s.waitForReadyRead(ms); })) {
            answer(tr("the CloudDeck did not answer"));
            return;
        }
        buf += s.readAll();
    }
    const QList<QByteArray> head = buf.left(headerEnd).split('\n');
    const int status = head.value(0).trimmed().split(' ').value(1).toInt();
    int contentLength = -1;
    for (const QByteArray& l : head) {
        if (l.toLower().startsWith("content-length:")) contentLength = l.mid(15).trimmed().toInt();
    }
    buf.remove(0, headerEnd + 4);

    if (status != 101) {
        if (status == 404) {
            answer(tr("the CloudDeck host update is not installed"));
            return;
        }
        while (contentLength > 0 && buf.size() < contentLength && waitFor([&](int ms) { return s.waitForReadyRead(ms); })) {
            buf += s.readAll();
        }
        QString why = QString::fromUtf8(contentLength >= 0 ? buf.left(contentLength) : buf).trimmed();
        if (why.isEmpty()) why = tr("HTTP %1").arg(status);
        answer(why);
        return;
    }
    while (!buf.contains('\n')) {
        if (!waitFor([&](int ms) { return s.waitForReadyRead(ms); })) {
            answer(tr("the CloudDeck did not confirm the display change"));
            return;
        }
        buf += s.readAll();
    }
    const QJsonObject st = QJsonDocument::fromJson(buf.left(buf.indexOf('\n'))).object();
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
