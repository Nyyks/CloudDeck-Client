// CloudDeck-Client: USB device forwarding (docs/adr/0006-usb-passthrough.md).
// QML singleton "CloudDeckUsb". Windows: usbipd-win exports devices locally; the
// host agent imports them through a UsbTunnel per device while a stream runs.
#pragma once

#include <QHash>
#include <QObject>
#include <QSslCertificate>
#include <QThread>
#include <QVariantList>

class UsbTunnel;

class UsbWorker : public QObject
{
    Q_OBJECT
public slots:
    void startTunnel(QString host, quint16 port, QSslCertificate cert, QString busid);
    void stopAll();
signals:
    void tunnelState(QString busid, bool attached, QString error);
private:
    QHash<QString, UsbTunnel*> m_Tunnels;
};

class UsbForwarder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool serverInstalled READ serverInstalled NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    static UsbForwarder* get();

    bool supported() const;
    bool serverInstalled() const { return !m_UsbipdPath.isEmpty(); }
    QVariantList devices() const { return m_Devices; }
    QString status() const { return m_Status; }

    Q_INVOKABLE void refresh();
    // Mark a device to be forwarded during streams (binds it in usbipd: one UAC prompt).
    Q_INVOKABLE void setForward(const QString& instanceId, bool forward);
    Q_INVOKABLE void installServer();

    // Called by Session (any thread).
    void onConnectionStarted(const QString& host, quint16 httpsPort, const QSslCertificate& serverCert);
    void onConnectionStopping();

signals:
    void devicesChanged();
    void statusChanged();

private:
    UsbForwarder();
    QStringList forwardList() const;
    void setStatus(const QString& s);
    bool bindElevated(const QString& busid);
    void onTunnelState(const QString& busid, bool attached, const QString& error);

    QString m_UsbipdPath;
    QVariantList m_Devices;
    QString m_Status;
    QHash<QString, bool> m_Attached;  // busid -> attached
    QThread m_Thread;
    UsbWorker* m_Worker;
};
