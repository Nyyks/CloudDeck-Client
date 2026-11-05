#include "agenttunnel.h"

#include "backend/identitymanager.h"

#include <QCoreApplication>
#include <QSslConfiguration>

namespace AgentTunnel {

static QString tr(const char* s) { return QCoreApplication::translate("AgentTunnel", s); }

// Blocking waits in short slices so callers can be interrupted quickly.
template <typename F>
static bool waitSliced(QSslSocket& s, QDeadlineTimer deadline, const std::function<bool()>& interrupted, F fn)
{
    while (!interrupted() && !deadline.hasExpired()) {
        if (fn(250)) return true;
        if (s.state() == QAbstractSocket::UnconnectedState) return false;
    }
    return false;
}

QString open(QSslSocket& s, const QString& host, quint16 port, const QSslCertificate& serverCert,
             const QByteArray& target, QDeadlineTimer deadline, const std::function<bool()>& interrupted,
             QByteArray& rest)
{
    QSslConfiguration cfg = IdentityManager::get()->getSslConfig(); // pairing identity (mTLS)
    cfg.setPeerVerifyMode(QSslSocket::VerifyPeer);
    s.setSslConfiguration(cfg);
    // Pin the host certificate learned at pairing, exactly like NvHTTP does. The
    // socket lives on this thread, so the lambda runs synchronously inside waitFor*.
    QObject::connect(&s, &QSslSocket::sslErrors, [&s, serverCert](const QList<QSslError>& errors) {
        for (const QSslError& e : errors) {
            if (e.certificate() != serverCert) return;
        }
        s.ignoreSslErrors(errors);
    });

    s.connectToHostEncrypted(host, port);
    if (!waitSliced(s, deadline, interrupted, [&](int ms) { return s.waitForEncrypted(ms); }) ||
        s.peerCertificate() != serverCert) {
        return tr("could not open a secure connection to the CloudDeck (%1)").arg(s.errorString());
    }

    s.write("GET /clouddeck/v1/" + target + " HTTP/1.1\r\nHost: " + host.toUtf8() +
            "\r\nUpgrade: clouddeck-agent/1\r\nConnection: Upgrade\r\n\r\n");

    QByteArray buf;
    int headerEnd = -1;
    while ((headerEnd = buf.indexOf("\r\n\r\n")) < 0) {
        if (buf.size() > 8192 || !waitSliced(s, deadline, interrupted, [&](int ms) { return s.waitForReadyRead(ms); })) {
            return tr("the CloudDeck did not answer");
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

    if (status == 101) {
        rest = buf;
        return QString();
    }
    if (status == 404) {
        return tr("the CloudDeck host update is not installed");
    }
    while (contentLength > 0 && buf.size() < contentLength &&
           waitSliced(s, deadline, interrupted, [&](int ms) { return s.waitForReadyRead(ms); })) {
        buf += s.readAll();
    }
    QString why = QString::fromUtf8(contentLength >= 0 ? buf.left(contentLength) : buf).trimmed();
    return why.isEmpty() ? tr("HTTP %1").arg(status) : why;
}

bool readLine(QSslSocket& s, QByteArray& buf, QByteArray& line, QDeadlineTimer deadline,
              const std::function<bool()>& interrupted)
{
    int nl;
    while ((nl = buf.indexOf('\n')) < 0) {
        if (!waitSliced(s, deadline, interrupted, [&](int ms) { return s.waitForReadyRead(ms); })) {
            return false;
        }
        buf += s.readAll();
    }
    line = buf.left(nl);
    buf.remove(0, nl + 1);
    return true;
}

}
