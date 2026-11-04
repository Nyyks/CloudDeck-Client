// CloudDeck-Client: one USB/IP tunnel (docs/adr/0006-usb-passthrough.md).
// TLS to Sunshine:47984 with the pairing identity, HTTP Upgrade to the host agent,
// then a byte relay to the local USB/IP server (usbipd-win) on 127.0.0.1:3240.
// The agent speaks the USB/IP protocol; this side only copies bytes.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QSslCertificate>
#include <QSslSocket>
#include <QTcpSocket>

class UsbTunnel : public QObject
{
    Q_OBJECT
public:
    // op: "usb/attach" (relay) or "hello" (single JSON reply)
    UsbTunnel(QString host, quint16 port, QSslCertificate serverCert, QString op, QString busid, QObject* parent = nullptr);
    ~UsbTunnel() override;

    void start();
    void stop();
    QString busid() const { return m_BusId; }

signals:
    void established();                          // 101 received, relaying
    void reply(int httpStatus, QByteArray body); // non-upgrade answer (hello, errors)
    void finished(QString error);                // tunnel closed; error empty on clean close

private:
    void onEncrypted();
    void onTlsReadyRead();
    void onLocalConnected();
    void fail(const QString& error);

    QString m_Host;
    quint16 m_Port;
    QSslCertificate m_ServerCert;
    QString m_Op;
    QString m_BusId;
    QSslSocket m_Tls;
    QTcpSocket m_Local;
    QByteArray m_Buffer;
    enum class State { Idle, Connecting, AwaitHeaders, AwaitBody, ConnectingLocal, Relaying, Done } m_State = State::Idle;
    int m_ReplyStatus = 0;
    int m_ContentLength = -1;
};
