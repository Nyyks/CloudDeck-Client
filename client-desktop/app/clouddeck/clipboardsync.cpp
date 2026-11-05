#include "clipboardsync.h"

#include "agenttunnel.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSslSocket>
#include <QtDebug>

bool ClipboardSync::enabled() { return QSettings().value("clouddeck/clipboard/enabled", true).toBool(); }
void ClipboardSync::setEnabled(bool on) { QSettings().setValue("clouddeck/clipboard/enabled", on); }

Uint32 ClipboardSync::sdlEventType()
{
    static Uint32 type = SDL_RegisterEvents(1);
    return type;
}

ClipboardSync::ClipboardSync(QString host, quint16 httpsPort, QSslCertificate serverCert)
    : m_Host(std::move(host)), m_Port(httpsPort), m_ServerCert(std::move(serverCert))
{
    setObjectName("CloudDeck clipboard");
}

ClipboardSync::~ClipboardSync()
{
    requestInterruption();
    wait();
}

void ClipboardSync::offer(const QString& text)
{
    if (text.toUtf8().size() > kMaxBytes) {
        qInfo() << "CloudDeck clipboard: not sending" << text.toUtf8().size() << "bytes (limit" << kMaxBytes << ")";
        return;
    }
    QMutexLocker l(&m_Lock);
    m_Outgoing = text;
    m_HasOutgoing = true;
}

bool ClipboardSync::takeIncoming(QString& text)
{
    QMutexLocker l(&m_Lock);
    if (!m_HasIncoming) return false;
    text = m_Incoming;
    m_HasIncoming = false;
    return true;
}

void ClipboardSync::run()
{
    QSslSocket s;
    const auto interrupted = [this]() { return isInterruptionRequested(); };
    QByteArray buf;
    const QString error = AgentTunnel::open(s, m_Host, m_Port, m_ServerCert, "clipboard/sync",
                                            QDeadlineTimer(20000), interrupted, buf);
    QByteArray line;
    if (!error.isEmpty() || !AgentTunnel::readLine(s, buf, line, QDeadlineTimer(10000), interrupted)) {
        qWarning() << "CloudDeck clipboard sync unavailable:" << (error.isEmpty() ? QStringLiteral("no status") : error);
        return;
    }
    qInfo() << "CloudDeck clipboard sync active";

    QElapsedTimer sinceWrite;
    sinceWrite.start();
    while (!isInterruptionRequested()) {
        QString out;
        bool send = false;
        {
            QMutexLocker l(&m_Lock);
            if (m_HasOutgoing) {
                out = m_Outgoing;
                m_HasOutgoing = false;
                send = true;
            }
        }
        if (send || sinceWrite.elapsed() > 30000) { // a bare newline every 30 s is a keepalive
            QByteArray msg = send ? QJsonDocument(QJsonObject {{"text", out}}).toJson(QJsonDocument::Compact) : QByteArray();
            s.write(msg + '\n');
            s.waitForBytesWritten(2000);
            sinceWrite.restart();
        }

        if (s.waitForReadyRead(100)) buf += s.readAll();
        int nl;
        while ((nl = buf.indexOf('\n')) >= 0) {
            const QJsonObject m = QJsonDocument::fromJson(buf.left(nl)).object();
            buf.remove(0, nl + 1);
            if (!m.value("text").isString()) continue;
            {
                QMutexLocker l(&m_Lock);
                m_Incoming = m.value("text").toString();
                m_HasIncoming = true;
            }
            SDL_Event e = {};
            e.type = sdlEventType();
            SDL_PushEvent(&e);
        }
        if (s.state() != QAbstractSocket::ConnectedState) {
            qWarning() << "CloudDeck clipboard sync closed by the host";
            return;
        }
    }
    s.disconnectFromHost();
    if (s.state() != QAbstractSocket::UnconnectedState) s.waitForDisconnected(2000);
}
