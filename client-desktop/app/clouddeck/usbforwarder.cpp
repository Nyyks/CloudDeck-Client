#include "usbforwarder.h"
#include "usbtunnel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// ---------------------------------------------------------------- worker (own thread)

void UsbWorker::startTunnel(QString host, quint16 port, QSslCertificate cert, QString busid)
{
    if (m_Tunnels.contains(busid)) return;
    auto* t = new UsbTunnel(host, port, cert, QStringLiteral("usb/attach"), busid, this);
    m_Tunnels.insert(busid, t);
    connect(t, &UsbTunnel::established, this, [this, busid]() { emit tunnelState(busid, true, QString()); });
    connect(t, &UsbTunnel::finished, this, [this, t, busid](QString error) {
        m_Tunnels.remove(busid);
        t->deleteLater();
        emit tunnelState(busid, false, error);
    });
    t->start();
}

void UsbWorker::stopAll()
{
    const auto tunnels = m_Tunnels.values();
    for (UsbTunnel* t : tunnels) t->stop();  // finished() removes them
}

// ---------------------------------------------------------------- forwarder

UsbForwarder* UsbForwarder::get()
{
    static UsbForwarder* s = new UsbForwarder();
    return s;
}

UsbForwarder::UsbForwarder() : m_Worker(new UsbWorker())
{
    qRegisterMetaType<QSslCertificate>();
    m_Thread.setObjectName("CloudDeck USB");
    m_Worker->moveToThread(&m_Thread);
    connect(&m_Thread, &QThread::finished, m_Worker, &QObject::deleteLater);
    connect(m_Worker, &UsbWorker::tunnelState, this, &UsbForwarder::onTunnelState, Qt::QueuedConnection);
    m_Thread.start();
    refresh();
}

bool UsbForwarder::supported() const
{
#ifdef Q_OS_WIN32
    return true;
#else
    return false;  // Linux/Android: Phase 3 steps 4/5
#endif
}

QStringList UsbForwarder::forwardList() const
{
    return QSettings().value("clouddeck/usb/forward").toStringList();
}

void UsbForwarder::setStatus(const QString& s)
{
    if (s != m_Status) { m_Status = s; emit statusChanged(); }
}

static QString deviceWarning(const QString& description)
{
    static const QRegularExpression input("keyboard|mouse|tastatur|maus|touchpad", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression iso("camera|webcam|kamera|audio|headset|microphone|mikrofon|speaker", QRegularExpression::CaseInsensitiveOption);
    if (input.match(description).hasMatch())
        return UsbForwarder::tr("Input device: it stops working on this PC while forwarded.");
    if (iso.match(description).hasMatch())
        return UsbForwarder::tr("Cameras and audio devices may work poorly over the internet.");
    return QString();
}

void UsbForwarder::refresh()
{
#ifdef Q_OS_WIN32
    m_UsbipdPath = QStandardPaths::findExecutable("usbipd");
    if (m_UsbipdPath.isEmpty() && QFileInfo::exists("C:/Program Files/usbipd-win/usbipd.exe"))
        m_UsbipdPath = "C:/Program Files/usbipd-win/usbipd.exe";
    m_Devices.clear();
    if (m_UsbipdPath.isEmpty()) {
        setStatus(tr("USB forwarding needs usbipd-win (free, GPL). Install it once, then restart this app."));
        emit devicesChanged();
        return;
    }
    QProcess p;
    p.start(m_UsbipdPath, {"state"});
    if (!p.waitForFinished(10000) || p.exitCode() != 0) {
        setStatus(tr("usbipd-win did not answer: %1").arg(QString::fromLocal8Bit(p.readAllStandardError()).trimmed()));
        emit devicesChanged();
        return;
    }
    const QStringList fwd = forwardList();
    const QJsonArray devs = QJsonDocument::fromJson(p.readAllStandardOutput()).object().value("Devices").toArray();
    for (const QJsonValue& v : devs) {
        const QJsonObject d = v.toObject();
        const QString busid = d.value("BusId").toString();
        if (busid.isEmpty()) continue;  // persisted but not connected right now
        const QString instance = d.value("InstanceId").toString();
        const QString desc = d.value("Description").toString();
        QVariantMap m;
        m["busid"] = busid;
        m["instanceId"] = instance;
        m["description"] = desc;
        m["shared"] = !d.value("PersistedGuid").isNull();
        m["forward"] = fwd.contains(instance);
        m["attached"] = m_Attached.value(busid, false);
        m["warning"] = deviceWarning(desc);
        m_Devices << m;
    }
    setStatus(m_Devices.isEmpty() ? tr("No USB devices found.")
                                  : tr("Checked devices are forwarded to your CloudDeck while a stream runs."));
#else
    setStatus(tr("USB forwarding is available on Windows for now; Linux and Android follow."));
#endif
    emit devicesChanged();
}

bool UsbForwarder::bindElevated(const QString& busid)
{
#ifdef Q_OS_WIN32
    // usbipd bind needs admin rights once per device (UAC prompt).
    const std::wstring exe = QDir::toNativeSeparators(m_UsbipdPath).toStdWString();
    const std::wstring args = QStringLiteral("bind --busid %1").arg(busid).toStdWString();
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return false;
    WaitForSingleObject(sei.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return code == 0;
#else
    Q_UNUSED(busid);
    return false;
#endif
}

void UsbForwarder::setForward(const QString& instanceId, bool forward)
{
    QStringList fwd = forwardList();
    if (forward) {
        for (const QVariant& v : m_Devices) {
            const QVariantMap d = v.toMap();
            if (d["instanceId"].toString() == instanceId && !d["shared"].toBool()) {
                if (!bindElevated(d["busid"].toString())) {
                    setStatus(tr("Could not share %1 with usbipd (administrator approval needed).").arg(d["description"].toString()));
                    refresh();
                    return;
                }
            }
        }
        if (!fwd.contains(instanceId)) fwd << instanceId;
    }
    else {
        fwd.removeAll(instanceId);
    }
    QSettings().setValue("clouddeck/usb/forward", fwd);
    refresh();
}

void UsbForwarder::installServer()
{
#ifdef Q_OS_WIN32
    // winget asks for elevation itself (MSI). Fall back to the release page.
    if (!QProcess::startDetached("winget", {"install", "--exact", "--id", "dorssel.usbipd-win"}))
        QDesktopServices::openUrl(QUrl("https://github.com/dorssel/usbipd-win/releases/latest"));
#endif
}

void UsbForwarder::onConnectionStarted(const QString& host, quint16 httpsPort, const QSslCertificate& serverCert)
{
    // May run on the connection thread; hop to ours, then hand tunnels to the worker.
    QMetaObject::invokeMethod(this, [this, host, httpsPort, serverCert]() {
        refresh();
        for (const QVariant& v : m_Devices) {
            const QVariantMap d = v.toMap();
            if (d["forward"].toBool() && d["shared"].toBool()) {
                QMetaObject::invokeMethod(m_Worker, "startTunnel", Qt::QueuedConnection,
                                          Q_ARG(QString, host), Q_ARG(quint16, httpsPort),
                                          Q_ARG(QSslCertificate, serverCert), Q_ARG(QString, d["busid"].toString()));
            }
        }
    }, Qt::QueuedConnection);
}

void UsbForwarder::onConnectionStopping()
{
    QMetaObject::invokeMethod(m_Worker, "stopAll", Qt::QueuedConnection);
}

void UsbForwarder::onTunnelState(const QString& busid, bool attached, const QString& error)
{
    m_Attached[busid] = attached;
    if (!error.isEmpty()) {
        qWarning() << "USB: tunnel for" << busid << "failed:" << error;
        setStatus(tr("USB %1: %2").arg(busid, error));
    }
    else {
        qInfo() << "USB:" << busid << (attached ? "attached to the host" : "detached");
    }
    for (QVariant& v : m_Devices) {
        QVariantMap d = v.toMap();
        if (d["busid"] == busid) { d["attached"] = attached; v = d; }
    }
    emit devicesChanged();
}
