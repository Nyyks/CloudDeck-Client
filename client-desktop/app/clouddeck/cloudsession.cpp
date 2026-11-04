#include "cloudsession.h"
#include "clouddeckprovider.h"
#include "secretstore.h"

#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "cli/startstream.h"
#include "settings/streamingpreferences.h"

#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QXmlStreamReader>

using namespace CloudDeckApi;

// Timeouts per stage (observed on 2026-11-04: start ~80 s, stop ~75 s).
static const int kRunningTimeoutMs   = 5 * 60 * 1000;
static const int kDnsTimeoutMs       = 2 * 60 * 1000;
static const int kSunshineTimeoutMs  = 2 * 60 * 1000;
static const int kPairTimeoutMs      = 90 * 1000;
static const int kFastPollMs = 5000;
static const int kSlowPollMs = 30000;
static const int kSunshineHttpPort = 47989;

CloudSession::CloudSession(QObject* parent)
    : QObject(parent), m_Provider(new CloudDeckProvider(this)), m_ProbeNam(new QNetworkAccessManager(this))
{
    m_ProbeNam->setTransferTimeout(5000);

    m_PollTimer.setSingleShot(true);
    connect(&m_PollTimer, &QTimer::timeout, this, &CloudSession::refreshStatus);
    m_StepTimer.setSingleShot(true);
    connect(&m_StepTimer, &QTimer::timeout, this, &CloudSession::stepConnect);
    m_IdleTimer.setSingleShot(true);
    connect(&m_IdleTimer, &QTimer::timeout, this, [this]() {
        if (!m_StreamActive && m_Machine.status == QLatin1String(Status::Running)) {
            qInfo() << "CloudDeck: idle timeout reached, stopping machine";
            stopMachine();
        }
    });

    connect(m_Provider, &CloudProvider::sessionExpired, this, [this]() {
        setError(tr("Your CloudDeck session expired. Please sign in again."));
        emit signedInChanged();
    });

    m_Restoring = true;
    m_Provider->restoreSession([this](CloudProvider::SignInResult r, const QString& err) {
        m_Restoring = false;
        emit busyChanged();
        if (r == CloudProvider::SignInResult::Ok) {
            emit signedInChanged();
            refreshStatus();
        }
        else if (!err.isEmpty()) {
            qWarning() << "CloudDeck: could not restore session:" << err;
        }
    });
}

QString CloudSession::keychainBackend() const { return SecretStore::get().backendName(); }

// ------------------------------------------------------------------ settings

QString CloudSession::appName() const { return QSettings().value("clouddeck/appName", "CloudDeck").toString(); }
void CloudSession::setAppName(const QString& v) { QSettings().setValue("clouddeck/appName", v); emit settingsChanged(); }
QString CloudSession::sessionMode() const { return QSettings().value("clouddeck/sessionMode", "").toString(); }
void CloudSession::setSessionMode(const QString& v) { QSettings().setValue("clouddeck/sessionMode", v); emit settingsChanged(); }
bool CloudSession::autoStopOnSessionEnd() const { return QSettings().value("clouddeck/autoStopOnSessionEnd", false).toBool(); }
void CloudSession::setAutoStopOnSessionEnd(bool v) { QSettings().setValue("clouddeck/autoStopOnSessionEnd", v); emit settingsChanged(); }
int CloudSession::autoStopIdleMinutes() const { return QSettings().value("clouddeck/autoStopIdleMinutes", 0).toInt(); }
void CloudSession::setAutoStopIdleMinutes(int v) { QSettings().setValue("clouddeck/autoStopIdleMinutes", qMax(0, v)); emit settingsChanged(); }

// ------------------------------------------------------------------ state helpers

void CloudSession::setBusy(bool b) { if (m_Busy != b) { m_Busy = b; emit busyChanged(); } }
void CloudSession::setProgress(const QString& p) { if (m_Progress != p) { m_Progress = p; emit progressChanged(); } }
void CloudSession::setError(const QString& e) { if (m_Error != e) { m_Error = e; emit errorChanged(); } }
void CloudSession::clearError() { setError(QString()); }

void CloudSession::setStage(Stage s)
{
    m_Stage = s;
    m_StageClock.restart();
    emit busyChanged();
}

void CloudSession::failConnect(const QString& e)
{
    qWarning() << "CloudDeck: start-and-connect failed:" << e;
    m_StepTimer.stop();
    releasePolling();
    setStage(Stage::Idle);
    setProgress(QString());
    setError(e);
}

// ------------------------------------------------------------------ account

void CloudSession::signIn(const QString& email, const QString& password)
{
    clearError();
    setBusy(true);
    m_Provider->signIn(email.trimmed(), password, [this](CloudProvider::SignInResult r, const QString& err) {
        setBusy(false);
        m_MfaRequired = (r == CloudProvider::SignInResult::MfaRequired);
        emit mfaRequiredChanged();
        if (r == CloudProvider::SignInResult::Failed) {
            setError(err.isEmpty() ? tr("Sign-in failed") : err);
            return;
        }
        if (r == CloudProvider::SignInResult::Ok) {
            emit signedInChanged();
            refreshStatus();
        }
    });
}

void CloudSession::submitMfaCode(const QString& code)
{
    clearError();
    setBusy(true);
    m_Provider->respondMfa(code.trimmed(), [this](CloudProvider::SignInResult r, const QString& err) {
        setBusy(false);
        m_MfaRequired = false;
        emit mfaRequiredChanged();
        if (r != CloudProvider::SignInResult::Ok) {
            setError(err.isEmpty() ? tr("Verification failed") : err);
            return;
        }
        emit signedInChanged();
        refreshStatus();
    });
}

void CloudSession::signOut()
{
    cancelConnect();
    m_PollTimer.stop();
    m_IdleTimer.stop();
    m_Provider->signOut();
    m_Machine = CloudProvider::Machine();
    emit machineChanged();
    emit signedInChanged();
}

// ------------------------------------------------------------------ machine status

void CloudSession::refreshStatus()
{
    if (!m_Provider->isSignedIn()) {
        return;
    }
    m_Provider->getMachine([this](const QString& err, const CloudProvider::Machine& m) {
        if (!err.isEmpty()) {
            qWarning() << "CloudDeck: status refresh failed:" << err;
            if (!connecting()) {
                setError(err);
            }
            schedulePoll();
            return;
        }
        onMachine(m);
    });
}

void CloudSession::onMachine(const CloudProvider::Machine& m)
{
    const bool changed = m.status != m_Machine.status || m.publicIp != m_Machine.publicIp;
    m_Machine = m;
    if (changed) {
        emit machineChanged();
    }

    if (m_RestartPending && m.status == QLatin1String(Status::Off)) {
        m_RestartPending = false;
        startMachine();
    }
    schedulePoll();
}

void CloudSession::schedulePoll()
{
    if (!m_Provider->isSignedIn()) {
        return;
    }
    const bool fast = isTransitional(m_Machine.status) || m_RestartPending || connecting();
    m_PollTimer.start(fast ? kFastPollMs : kSlowPollMs);
}

void CloudSession::startMachine()
{
    clearError();
    setBusy(true);
    m_Provider->startMachine(sessionMode(), [this](const QString& err) {
        setBusy(false);
        if (!err.isEmpty()) {
            setError(tr("Could not start your CloudDeck: %1").arg(err));
            if (connecting()) failConnect(m_Error);
            return;
        }
        m_Machine.status = QString::fromLatin1(Status::Starting);
        emit machineChanged();
        m_PollTimer.start(3000);
    });
}

void CloudSession::stopMachine()
{
    clearError();
    setBusy(true);
    m_IdleTimer.stop();
    m_Provider->stopMachine([this](const QString& err) {
        setBusy(false);
        if (!err.isEmpty()) {
            m_RestartPending = false;
            setError(tr("Could not stop your CloudDeck: %1").arg(err));
            return;
        }
        m_Machine.status = QString::fromLatin1(Status::Stopping);
        emit machineChanged();
        m_PollTimer.start(3000);
    });
}

void CloudSession::restartMachine()
{
    // The portal has no restart endpoint: stop, wait for "off", then start.
    m_RestartPending = true;
    stopMachine();
}

// ------------------------------------------------------------------ start and connect

void CloudSession::startAndConnect(ComputerManager* computerManager)
{
    if (connecting() || !computerManager) {
        return;
    }
    clearError();
    m_IdleTimer.stop();
    m_ComputerManager = computerManager;
    m_PendingPin.clear();
    m_PinAccepted = false;
    setStage(Stage::StartingVm);
    stepConnect();
}

void CloudSession::cancelConnect()
{
    if (!connecting()) {
        return;
    }
    m_StepTimer.stop();
    releasePolling();
    setStage(Stage::Idle);
    setProgress(QString());
}

void CloudSession::stepConnect()
{
    switch (m_Stage) {
    case Stage::Idle:
    case Stage::Pairing:
        return;

    case Stage::StartingVm: {
        const QString s = m_Machine.status;
        if (s == QLatin1String(Status::Running)) {
            setStage(Stage::WaitingDns);
            stepConnect();
        }
        else if (s == QLatin1String(Status::Off)) {
            setProgress(tr("Starting your CloudDeck..."));
            startMachine();
            setStage(Stage::WaitingRunning);
            m_StepTimer.start(kFastPollMs);
        }
        else {
            // starting / stopping / unknown: refresh and decide again
            setProgress(s == QLatin1String(Status::Stopping) ? tr("Waiting for your CloudDeck to finish stopping...")
                                                              : tr("Checking your CloudDeck..."));
            refreshStatus();
            if (m_StageClock.elapsed() > kRunningTimeoutMs) {
                failConnect(tr("Your CloudDeck did not become ready (status: %1).").arg(s));
                return;
            }
            m_StepTimer.start(kFastPollMs);
        }
        return;
    }

    case Stage::WaitingRunning: {
        const QString s = m_Machine.status;
        if (s == QLatin1String(Status::Running)) {
            setStage(Stage::WaitingDns);
            stepConnect();
            return;
        }
        if (s == QLatin1String("error") || s == QLatin1String("failed")) {
            failConnect(tr("CloudDeck reported an error while starting. Try again in a few minutes."));
            return;
        }
        if (m_StageClock.elapsed() > kRunningTimeoutMs) {
            failConnect(tr("Timed out waiting for your CloudDeck to start."));
            return;
        }
        setProgress(tr("Starting your CloudDeck... (%1 s)").arg(m_StageClock.elapsed() / 1000));
        m_StepTimer.start(kFastPollMs);  // m_PollTimer keeps m_Machine fresh
        return;
    }

    case Stage::WaitingDns:
        setProgress(tr("Waiting for network address..."));
        probeDns();
        return;

    case Stage::WaitingSunshine:
        setProgress(tr("Waiting for the streaming host..."));
        probeSunshine();
        return;
    }
}

void CloudSession::probeDns()
{
    // While the VM is off, <id>.my.clouddeck.app points at a parking address.
    // Proceed once it resolves to the machine's current public IP.
    const QString host = hostAddress();
    const QString expected = m_Machine.publicIp;
    if (host.isEmpty()) {
        failConnect(tr("Could not determine the host address."));
        return;
    }
    if (expected.isEmpty()) {
        // public_ip not populated yet; refresh and retry
        refreshStatus();
        m_StepTimer.start(2000);
        return;
    }
    QHostInfo::lookupHost(host, this, [this, expected](const QHostInfo& info) {
        if (m_Stage != Stage::WaitingDns) return;
        for (const QHostAddress& a : info.addresses()) {
            if (QHostAddress(expected).isEqual(a, QHostAddress::ConvertV4MappedToIPv4)) {
                setStage(Stage::WaitingSunshine);
                stepConnect();
                return;
            }
        }
        if (m_StageClock.elapsed() > kDnsTimeoutMs) {
            failConnect(tr("%1 still does not point to your CloudDeck. Your DNS cache may be stale; try again in a minute.")
                            .arg(hostAddress()));
            return;
        }
        m_StepTimer.start(3000);
    });
}

void CloudSession::probeSunshine()
{
    QNetworkRequest req(QUrl(QStringLiteral("http://%1:%2/serverinfo").arg(hostAddress()).arg(kSunshineHttpPort)));
    QNetworkReply* reply = m_ProbeNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_Stage != Stage::WaitingSunshine) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            // Identify the host by its unique ID, fetched over fresh DNS. Names and IPs
            // can't be trusted: CloudDeck recycles public IPs between customers' VMs.
            QXmlStreamReader xml(reply->readAll());
            m_HostUuid.clear();
            while (!xml.atEnd()) {
                if (xml.readNextStartElement() && xml.name() == QLatin1String("uniqueid")) {
                    m_HostUuid = xml.readElementText().trimmed();
                    break;
                }
            }
            if (m_HostUuid.isEmpty()) {
                failConnect(tr("The streaming host answered without an ID; is it running Sunshine?"));
                return;
            }
            beginPairing();
            return;
        }
        if (m_StageClock.elapsed() > kSunshineTimeoutMs) {
            failConnect(tr("Your CloudDeck is running, but its streaming host is not reachable. "
                           "If you started it from another network, restart it from this device."));
            return;
        }
        m_StepTimer.start(3000);
    });
}

void CloudSession::beginPairing()
{
    setStage(Stage::Pairing);
    setProgress(tr("Connecting to your CloudDeck..."));
    if (!m_ComputerManager) {
        failConnect(tr("Internal error: no computer manager"));
        return;
    }
    // Always (re-)add by the current hostname: for a known host this refreshes its
    // stored addresses (the VM's IP changes between starts); for a new one it adds it.
    // Then wait for the add to complete and look the host up by its unique ID. We don't
    // wait for computerStateChanged: it only fires on a change, which never comes for a
    // host that is already known and online.
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_ComputerManager, &ComputerManager::computerAddCompleted, this,
                    [this, conn](QVariant success, QVariant) {
        disconnect(*conn);
        if (m_Stage != Stage::Pairing) return;
        if (!success.toBool()) {
            failConnect(tr("Could not connect to %1.").arg(hostAddress()));
            return;
        }
        waitForOnlineComputer();
    });
    if (!m_ManagerPolling) {
        m_ComputerManager->startPolling();
        m_ManagerPolling = true;
    }
    m_ComputerManager->addNewHostManually(hostAddress());
}

void CloudSession::releasePolling()
{
    if (m_ManagerPolling && m_ComputerManager) {
        m_ComputerManager->stopPollingAsync();
    }
    m_ManagerPolling = false;
}

void CloudSession::waitForOnlineComputer()
{
    if (m_Stage != Stage::Pairing || !m_ComputerManager) return;
    for (NvComputer* c : m_ComputerManager->getComputers()) {
        if (c->uuid.compare(m_HostUuid, Qt::CaseInsensitive) == 0 && c->state == NvComputer::CS_ONLINE) {
            releasePolling();
            onComputerFound(c);
            return;
        }
    }
    if (m_StageClock.elapsed() > kPairTimeoutMs) {
        failConnect(tr("Timed out connecting to %1.").arg(hostAddress()));
        return;
    }
    QTimer::singleShot(500, this, &CloudSession::waitForOnlineComputer);
}

void CloudSession::onComputerFound(NvComputer* computer)
{
    if (m_Stage != Stage::Pairing) return;

    if (computer->pairState == NvComputer::PS_PAIRED) {
        launchStream();
        return;
    }

    // Auto-pair: generate the PIN ourselves, start the Moonlight pairing handshake
    // (Sunshine holds it open until a PIN arrives), and submit the PIN through the
    // portal, exactly as a user would type it there.
    setProgress(tr("Pairing with your CloudDeck..."));
    m_PendingPin = m_ComputerManager->generatePinString();
    m_PinAccepted = false;

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_ComputerManager, &ComputerManager::pairingCompleted, this,
                    [this, conn](NvComputer*, QString error) {
        disconnect(*conn);
        if (m_Stage != Stage::Pairing) return;
        if (!error.isEmpty()) {
            failConnect(tr("Pairing failed: %1").arg(error));
            return;
        }
        launchStream();
    });
    m_ComputerManager->pairHost(computer, m_PendingPin);

    // Give the pairing request a moment to reach Sunshine before the PIN does.
    QTimer::singleShot(1500, this, [this, pin = m_PendingPin]() { submitPinWithRetry(pin, 1); });
}

void CloudSession::submitPinWithRetry(const QString& pin, int attempt)
{
    if (m_Stage != Stage::Pairing || pin != m_PendingPin || m_PinAccepted) return;
    m_Provider->submitPairingPin(pin, [this, pin, attempt](const QString& err) {
        if (m_Stage != Stage::Pairing || pin != m_PendingPin) return;
        if (err.isEmpty()) {
            m_PinAccepted = true;
            return;
        }
        qWarning() << "CloudDeck: PIN submission attempt" << attempt << "failed:" << err;
        if (attempt >= 5) {
            failConnect(tr("Could not submit the pairing PIN through CloudDeck: %1").arg(err));
            return;
        }
        QTimer::singleShot(2000, this, [this, pin, attempt]() { submitPinWithRetry(pin, attempt + 1); });
    });
}

void CloudSession::launchStream()
{
    setStage(Stage::Idle);
    setProgress(QString());
    m_StreamActive = true;
    // Seek by unique ID, not hostname: the stored host may only know the VM by IP.
    auto* launcher = new CliStartStream::Launcher(m_HostUuid, appName(), StreamingPreferences::get(), this);
    emit streamReady(launcher, appName());
}

void CloudSession::streamEnded()
{
    m_StreamActive = false;
    if (autoStopOnSessionEnd() && m_Machine.status == QLatin1String(Status::Running)) {
        qInfo() << "CloudDeck: session ended, auto-stopping machine";
        stopMachine();
        return;
    }
    armIdleTimer();
}

void CloudSession::armIdleTimer()
{
    const int minutes = autoStopIdleMinutes();
    if (minutes > 0) {
        m_IdleTimer.start(minutes * 60 * 1000);
    }
}
