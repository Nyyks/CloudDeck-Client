#include "usbforwarder.h"
#include "usbtunnel.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QTimer>
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

#ifdef Q_OS_LINUX
// Root (via pkexec) can't read files inside a FUSE-mounted AppImage, so run a copy
// from the user's cache directory.
static QString prepareHelper(QString* error)
{
    QString src = qEnvironmentVariable("CLOUDDECK_USB_HELPER");
    if (src.isEmpty()) src = QCoreApplication::applicationDirPath() + "/clouddeck-usb-helper";
    if (!QFileInfo::exists(src)) {
        *error = UsbForwarder::tr("clouddeck-usb-helper is missing from this build");
        return QString();
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    const QString dst = dir + "/clouddeck-usb-helper";
    QFile::remove(dst);
    if (!QFile::copy(src, dst)) {
        *error = UsbForwarder::tr("could not prepare clouddeck-usb-helper in %1").arg(dir);
        return QString();
    }
    QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                   QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                   QFileDevice::ReadOther | QFileDevice::ExeOther);
    return dst;
}
#endif

void UsbWorker::startForwarding(QString host, quint16 port, QSslCertificate cert, QStringList busids)
{
    m_Host = host;
    m_Port = port;
    m_Cert = cert;
#ifdef Q_OS_LINUX
    QString error;
    const QString helper = prepareHelper(&error);
    if (helper.isEmpty()) {
        for (const QString& b : busids) emit tunnelState(b, false, error);
        return;
    }
    if (!m_Helper) {
        m_Helper = new QProcess(this);
        m_Helper->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_Helper, &QProcess::readyReadStandardOutput, this, &UsbWorker::onHelperOutput);
        connect(m_Helper, &QProcess::readyReadStandardError, this, [this]() {
            qInfo().noquote() << "USB helper:" << QString::fromLocal8Bit(m_Helper->readAllStandardError()).trimmed();
        });
        connect(m_Helper, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
            if (code == 126 || code == 127) {  // pkexec: authorization dismissed / failed
                for (const QString& b : m_Pending) emit tunnelState(b, false, UsbForwarder::tr("authorization was refused"));
            }
            m_Pending.clear();
            m_Helper->deleteLater();
            m_Helper = nullptr;
        });
        // pkexec shows the password prompt (polkit agent of the desktop session).
        m_Helper->start("pkexec", {helper});
        if (!m_Helper->waitForStarted(5000)) {
            for (const QString& b : busids) emit tunnelState(b, false, UsbForwarder::tr("pkexec is not available"));
            m_Helper->deleteLater();
            m_Helper = nullptr;
            return;
        }
    }
    for (const QString& b : busids) {
        m_Pending << b;
        m_Helper->write(("export " + b + "\n").toUtf8());
    }
#else
    for (const QString& b : busids) startTunnel(b, QString());
#endif
}

void UsbWorker::onHelperOutput()
{
    while (m_Helper && m_Helper->canReadLine()) {
        const QStringList f = QString::fromUtf8(m_Helper->readLine()).trimmed().split(' ');
        if (f.value(0) == QLatin1String("ready") && f.size() >= 3) {
            m_Pending.removeAll(f[1]);
            startTunnel(f[1], f[2]);
        }
        else if (f.value(0) == QLatin1String("error") && f.size() >= 2) {
            m_Pending.removeAll(f[1]);
            emit tunnelState(f[1], false, f.mid(2).join(' '));
        }
    }
}

void UsbWorker::startTunnel(const QString& busid, const QString& localPath)
{
    if (m_Tunnels.contains(busid)) return;
    auto* t = new UsbTunnel(m_Host, m_Port, m_Cert, QStringLiteral("usb/attach"), busid, localPath, this);
    m_Tunnels.insert(busid, t);
    connect(t, &UsbTunnel::established, this, [this, busid]() { emit tunnelState(busid, true, QString()); });
    connect(t, &UsbTunnel::finished, this, [this, t, busid](QString error) {
        m_Tunnels.remove(busid);
        t->deleteLater();
        emit tunnelState(busid, false, error);
    });
    t->start();
}

void UsbWorker::stopOne(QString busid)
{
    if (UsbTunnel* t = m_Tunnels.value(busid)) t->stop(); // the agent detaches it on the VM
    m_Pending.removeAll(busid);
    if (m_Helper) {
        // Linux: hand the device back to its local driver.
        m_Helper->write(("release " + busid + "\n").toUtf8());
    }
}

void UsbWorker::stopAll()
{
    const auto tunnels = m_Tunnels.values();
    for (UsbTunnel* t : tunnels) t->stop();  // finished() removes them
    if (m_Helper) {
        // EOF on stdin: the helper gives every device back to its driver and exits.
        m_Helper->closeWriteChannel();
        QProcess* p = m_Helper;
        QTimer::singleShot(10000, p, [p]() { if (p->state() != QProcess::NotRunning) p->kill(); });
    }
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
#if defined(Q_OS_WIN32) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
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
#elif defined(Q_OS_LINUX)
    // The helper exports devices through the kernel's usbip-host (no usbipd daemon).
    m_UsbipdPath = qEnvironmentVariable("CLOUDDECK_USB_HELPER");
    if (m_UsbipdPath.isEmpty()) m_UsbipdPath = QCoreApplication::applicationDirPath() + "/clouddeck-usb-helper";
    if (!QFileInfo::exists(m_UsbipdPath)) m_UsbipdPath.clear();
    m_Devices.clear();
    const QStringList fwd = forwardList();
    static const QRegularExpression devName("^\\d+-[\\d.]+$");  // "1-2", "3-1.4" (not interfaces "1-2:1.0")
    const QDir sys("/sys/bus/usb/devices");
    const QStringList entries = sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
    auto attr = [&](const QString& dev, const char* name) {
        QFile f(sys.filePath(dev + "/" + name));
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
    };
    for (const QString& busid : entries) {
        if (!devName.match(busid).hasMatch() || attr(busid, "bDeviceClass") == QLatin1String("09")) continue;  // skip hubs
        const QString vid = attr(busid, "idVendor"), pid = attr(busid, "idProduct");
        const QString serial = attr(busid, "serial");
        QString desc = (attr(busid, "manufacturer") + " " + attr(busid, "product")).trimmed();
        if (desc.isEmpty()) desc = tr("USB device %1:%2").arg(vid, pid);
        const QString instance = vid + ":" + pid + ":" + (serial.isEmpty() ? "@" + busid : serial);
        QVariantMap m;
        m["busid"] = busid;
        m["instanceId"] = instance;
        m["description"] = desc;
        m["shared"] = true;  // exported on demand by the helper during a stream
        m["forward"] = fwd.contains(instance);
        m["attached"] = m_Attached.value(busid, false);
        m["warning"] = deviceWarning(desc);
        m_Devices << m;
    }
    if (m_UsbipdPath.isEmpty())
        setStatus(tr("This build has no clouddeck-usb-helper, so USB forwarding is unavailable."));
    else
        setStatus(m_Devices.isEmpty() ? tr("No USB devices found.")
                                      : tr("Checked devices are forwarded while a stream runs; you'll be asked for your password once per stream."));
#else
    setStatus(tr("USB forwarding isn't available on this platform."));
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

    // During a stream (in-stream menu): attach or detach right away.
    if (m_Streaming) {
        for (const QVariant& v : m_Devices) {
            const QVariantMap d = v.toMap();
            if (d["instanceId"].toString() != instanceId) continue;
            const QString busid = d["busid"].toString();
            if (forward && d["shared"].toBool() && !m_Attached.value(busid)) {
                QMetaObject::invokeMethod(m_Worker, "startForwarding", Qt::QueuedConnection,
                                          Q_ARG(QString, m_StreamHost), Q_ARG(quint16, m_StreamPort),
                                          Q_ARG(QSslCertificate, m_StreamCert), Q_ARG(QStringList, QStringList {busid}));
            }
            else if (!forward) {
                QMetaObject::invokeMethod(m_Worker, "stopOne", Qt::QueuedConnection, Q_ARG(QString, busid));
            }
        }
    }
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
        m_Streaming = true;
        m_StreamHost = host;
        m_StreamPort = httpsPort;
        m_StreamCert = serverCert;
        refresh();
        QStringList busids;
        for (const QVariant& v : m_Devices) {
            const QVariantMap d = v.toMap();
            if (d["forward"].toBool() && d["shared"].toBool()) busids << d["busid"].toString();
        }
        if (busids.isEmpty()) return;  // nothing to forward: no helper, no password prompt
        QMetaObject::invokeMethod(m_Worker, "startForwarding", Qt::QueuedConnection,
                                  Q_ARG(QString, host), Q_ARG(quint16, httpsPort),
                                  Q_ARG(QSslCertificate, serverCert), Q_ARG(QStringList, busids));
    }, Qt::QueuedConnection);
}

void UsbForwarder::onConnectionStopping()
{
    QMetaObject::invokeMethod(this, [this]() { m_Streaming = false; }, Qt::QueuedConnection);
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
