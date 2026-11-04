#include "usbtunnel.h"

#include "backend/identitymanager.h"

#include <QSslConfiguration>

static const quint16 kUsbIpPort = 3240;

UsbTunnel::UsbTunnel(QString host, quint16 port, QSslCertificate serverCert, QString op, QString busid,
                     QString localPath, QObject* parent)
    : QObject(parent), m_Host(std::move(host)), m_Port(port), m_ServerCert(std::move(serverCert)),
      m_Op(std::move(op)), m_BusId(std::move(busid)), m_LocalPath(std::move(localPath))
{
    connect(&m_LocalUnix, &QLocalSocket::connected, this, &UsbTunnel::onLocalConnected);
    connect(&m_LocalUnix, &QLocalSocket::readyRead, this, [this]() {
        if (m_State == State::Relaying) m_Tls.write(m_LocalUnix.readAll());
    });
    connect(&m_LocalUnix, &QLocalSocket::disconnected, this, [this]() { fail(QString()); });
    connect(&m_LocalUnix, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        fail(tr("USB helper not reachable: %1").arg(m_LocalUnix.errorString()));
    });

    connect(&m_Tls, &QSslSocket::encrypted, this, &UsbTunnel::onEncrypted);
    connect(&m_Tls, &QSslSocket::readyRead, this, &UsbTunnel::onTlsReadyRead);
    connect(&m_Tls, &QSslSocket::sslErrors, this, [this](const QList<QSslError>& errors) {
        // Pin the host certificate learned at pairing, exactly like NvHTTP does.
        for (const QSslError& e : errors) {
            if (e.certificate() != m_ServerCert) {
                fail(tr("TLS error: %1").arg(e.errorString()));
                return;
            }
        }
        m_Tls.ignoreSslErrors(errors);
    });
    connect(&m_Tls, &QSslSocket::disconnected, this, [this]() {
        if (m_State == State::AwaitBody && m_ReplyStatus) {
            emit reply(m_ReplyStatus, m_Buffer);
        }
        fail(m_State == State::Relaying || m_State == State::AwaitBody ? QString() : tr("host closed the connection"));
    });
    connect(&m_Tls, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError e) {
        if (e != QAbstractSocket::RemoteHostClosedError) fail(m_Tls.errorString());
    });

    connect(&m_Local, &QTcpSocket::connected, this, &UsbTunnel::onLocalConnected);
    connect(&m_Local, &QTcpSocket::readyRead, this, [this]() {
        if (m_State == State::Relaying) m_Tls.write(m_Local.readAll());
    });
    connect(&m_Local, &QTcpSocket::disconnected, this, [this]() { fail(QString()); });
    connect(&m_Local, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        fail(tr("local USB/IP server (usbipd) not reachable: %1").arg(m_Local.errorString()));
    });
}

UsbTunnel::~UsbTunnel()
{
    m_State = State::Done;
    m_Tls.abort();
    m_Local.abort();
    m_LocalUnix.abort();
}

void UsbTunnel::writeLocal(const QByteArray& data)
{
    if (m_LocalPath.isEmpty()) m_Local.write(data);
    else m_LocalUnix.write(data);
}

void UsbTunnel::start()
{
    QSslConfiguration cfg = IdentityManager::get()->getSslConfig(); // client cert + key from pairing
    cfg.setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_Tls.setSslConfiguration(cfg);
    m_State = State::Connecting;
    m_Tls.connectToHostEncrypted(m_Host, m_Port);
}

void UsbTunnel::stop()
{
    fail(QString());
}

void UsbTunnel::onEncrypted()
{
    if (m_Tls.peerCertificate() != m_ServerCert) {
        fail(tr("host certificate does not match the paired host"));
        return;
    }
    QString target = QStringLiteral("/clouddeck/v1/") + m_Op;
    if (!m_BusId.isEmpty()) target += QStringLiteral("?busid=") + m_BusId;
    QByteArray req = "GET " + target.toUtf8() + " HTTP/1.1\r\n"
                     "Host: " + m_Host.toUtf8() + "\r\n"
                     "Upgrade: clouddeck-agent/1\r\n"
                     "Connection: Upgrade\r\n\r\n";
    m_State = State::AwaitHeaders;
    m_Tls.write(req);
}

void UsbTunnel::onTlsReadyRead()
{
    if (m_State == State::Relaying) {
        writeLocal(m_Tls.readAll());
        return;
    }
    m_Buffer += m_Tls.readAll();
    if (m_State == State::AwaitHeaders) {
        int end = m_Buffer.indexOf("\r\n\r\n");
        if (end < 0) {
            if (m_Buffer.size() > 8192) fail(tr("bad response from host"));
            return;
        }
        const QByteArray head = m_Buffer.left(end);
        m_Buffer.remove(0, end + 4);
        const QList<QByteArray> lines = head.split('\n');
        const QList<QByteArray> status = lines.value(0).trimmed().split(' ');
        m_ReplyStatus = status.value(1).toInt();
        for (const QByteArray& l : lines) {
            if (l.toLower().startsWith("content-length:")) m_ContentLength = l.mid(15).trimmed().toInt();
        }
        if (m_ReplyStatus == 101 && m_Op == QLatin1String("usb/attach")) {
            m_State = State::ConnectingLocal;
            if (m_LocalPath.isEmpty()) m_Local.connectToHost(QStringLiteral("127.0.0.1"), kUsbIpPort);
            else m_LocalUnix.connectToServer(m_LocalPath);
            return;
        }
        if (m_ReplyStatus == 404) {
            emit reply(404, QByteArray());
            fail(tr("the host has no CloudDeck agent (install the CloudDeck host update)"));
            return;
        }
        m_State = State::AwaitBody;
    }
    if (m_State == State::AwaitBody && m_ContentLength >= 0 && m_Buffer.size() >= m_ContentLength) {
        emit reply(m_ReplyStatus, m_Buffer.left(m_ContentLength));
        fail(m_ReplyStatus == 200 ? QString() : QString::fromUtf8(m_Buffer.left(m_ContentLength)).trimmed());
    }
}

void UsbTunnel::onLocalConnected()
{
    m_State = State::Relaying;
    // Bytes the agent sent right after the 101 (the USB/IP import request) go first.
    if (!m_Buffer.isEmpty()) {
        writeLocal(m_Buffer);
        m_Buffer.clear();
    }
    if (m_Tls.bytesAvailable()) writeLocal(m_Tls.readAll());
    emit established();
}

void UsbTunnel::fail(const QString& error)
{
    if (m_State == State::Done) return;
    m_State = State::Done;
    m_Tls.disconnectFromHost();
    m_Local.disconnectFromHost();
    m_LocalUnix.disconnectFromServer();
    emit finished(error);
}
