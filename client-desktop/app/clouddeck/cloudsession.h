// QML-facing controller for the CloudDeck integration ("CloudDeck" singleton).
// Owns the CloudProvider, polls machine status, and runs "Start and connect":
//   start VM -> wait running -> wait DNS -> wait Sunshine -> auto-pair -> stream.
#pragma once

#include "cloudprovider.h"
#include "netrecommend.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

class ComputerManager;
class NvComputer;
class QNetworkAccessManager;

class CloudSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(bool restoring READ restoring NOTIFY busyChanged)
    Q_PROPERTY(bool mfaRequired READ mfaRequired NOTIFY mfaRequiredChanged)
    Q_PROPERTY(QString email READ email NOTIFY signedInChanged)
    Q_PROPERTY(QString keychainBackend READ keychainBackend CONSTANT)
    Q_PROPERTY(QString machineStatus READ machineStatus NOTIFY machineChanged)
    Q_PROPERTY(QString machineRegion READ machineRegion NOTIFY machineChanged)
    Q_PROPERTY(QString hostAddress READ hostAddress NOTIFY machineChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY busyChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(QString appName READ appName WRITE setAppName NOTIFY settingsChanged)
    Q_PROPERTY(QString sessionMode READ sessionMode WRITE setSessionMode NOTIFY settingsChanged)
    Q_PROPERTY(bool autoStopOnSessionEnd READ autoStopOnSessionEnd WRITE setAutoStopOnSessionEnd NOTIFY settingsChanged)
    Q_PROPERTY(int autoStopIdleMinutes READ autoStopIdleMinutes WRITE setAutoStopIdleMinutes NOTIFY settingsChanged)
    Q_PROPERTY(bool clipboardSync READ clipboardSync WRITE setClipboardSync NOTIFY settingsChanged)
    Q_PROPERTY(bool autoReconnect READ autoReconnect WRITE setAutoReconnect NOTIFY settingsChanged)
    // On-screen controller (docs/on-screen-controller.md), same keys the stream overlay reads
    Q_PROPERTY(QString controllerStyle READ controllerStyle WRITE setControllerStyle NOTIFY settingsChanged)
    Q_PROPERTY(bool onScreenController READ onScreenController WRITE setOnScreenController NOTIFY settingsChanged)
    // Controllers start as a mouse (docs/controller-mouse.md); read by SdlInputHandler
    Q_PROPERTY(bool gamepadMouseAtStart READ gamepadMouseAtStart WRITE setGamepadMouseAtStart NOTIFY settingsChanged)
    // Network test (docs/adr/0011-network-test.md)
    Q_PROPERTY(bool netTestRunning READ netTestRunning NOTIFY netTestChanged)
    Q_PROPERTY(QString netTestStatus READ netTestStatus NOTIFY netTestChanged)
    Q_PROPERTY(QString netTestRecommendation READ netTestRecommendation NOTIFY netTestChanged)
    Q_PROPERTY(bool netTestHasRecommendation READ netTestHasRecommendation NOTIFY netTestChanged)

public:
    explicit CloudSession(QObject* parent = nullptr);

    Q_INVOKABLE void signIn(const QString& email, const QString& password);
    Q_INVOKABLE void submitMfaCode(const QString& code);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void refreshStatus();
    Q_INVOKABLE void startMachine();
    Q_INVOKABLE void stopMachine();
    Q_INVOKABLE void restartMachine();
    Q_INVOKABLE void startAndConnect(ComputerManager* computerManager);
    Q_INVOKABLE void cancelConnect();
    // Called by the UI when a stream that we launched has ended.
    Q_INVOKABLE void streamEnded();
    Q_INVOKABLE void clearError();
    // Network test: latency and CloudDeck -> PC speed, then recommended settings.
    Q_INVOKABLE void runNetworkTest(ComputerManager* computerManager);
    Q_INVOKABLE void applyRecommendation();
    Q_INVOKABLE void applyLowBandwidth();
    bool netTestRunning() const { return m_NetTest != nullptr; }
    QString netTestStatus() const { return m_NetTestStatus; }
    QString netTestRecommendation() const;
    bool netTestHasRecommendation() const { return m_HasRecommendation; }

    bool signedIn() const { return m_Provider->isSignedIn(); }
    bool restoring() const { return m_Restoring; }
    bool mfaRequired() const { return m_MfaRequired; }
    QString email() const { return m_Provider->accountEmail(); }
    QString keychainBackend() const;
    QString machineStatus() const { return m_Machine.status; }
    QString machineRegion() const { return m_Machine.region; }
    QString hostAddress() const { return m_Provider->hostAddress(m_Machine); }
    bool busy() const { return m_Busy || m_Restoring || connecting(); }
    bool connecting() const { return m_Stage != Stage::Idle; }
    QString progressText() const { return m_Progress; }
    QString errorText() const { return m_Error; }

    QString appName() const;
    void setAppName(const QString& v);
    QString sessionMode() const;
    void setSessionMode(const QString& v);
    bool autoStopOnSessionEnd() const;
    void setAutoStopOnSessionEnd(bool v);
    int autoStopIdleMinutes() const;
    void setAutoStopIdleMinutes(int v);
    // Quick Menu (any thread): whether the running stream was started from the CloudDeck
    // screen, and a one-shot request to stop the VM when that stream ends.
    static bool ownsActiveStream();
    static void requestStopAfterStream();

    bool clipboardSync() const;
    void setClipboardSync(bool v);
    bool autoReconnect() const { return autoReconnectEnabled(); }
    void setAutoReconnect(bool v);
    static bool autoReconnectEnabled(); // any thread; default on
    QString controllerStyle() const;
    void setControllerStyle(const QString& v);
    bool onScreenController() const;
    void setOnScreenController(bool v);
    bool gamepadMouseAtStart() const;
    void setGamepadMouseAtStart(bool v);

signals:
    void netTestChanged();
    void signedInChanged();
    void mfaRequiredChanged();
    void machineChanged();
    void busyChanged();
    void progressChanged();
    void errorChanged();
    void settingsChanged();
    // The VM is up and paired. The receiver pushes a stream segue for this launcher
    // (a CliStartStream::Launcher) and calls launcher.execute(ComputerManager).
    void streamReady(QObject* launcher, QString appName);

private:
    enum class Stage { Idle, StartingVm, WaitingRunning, WaitingDns, WaitingSunshine, Pairing };

    void setBusy(bool b);
    void setProgress(const QString& p);
    void setError(const QString& e);
    void setStage(Stage s);
    void failConnect(const QString& e);
    void onMachine(const CloudProvider::Machine& m);
    void schedulePoll();
    void stepConnect();
    void probeDns();
    void probeSunshine();
    void beginPairing();
    void waitForOnlineComputer();
    void releasePolling();
    void onComputerFound(NvComputer* computer);
    void submitPinWithRetry(const QString& pin, int attempt);
    void launchStream();
    void armIdleTimer();
    void applySettings(const NetRecommend::Settings& s);

    class NetTest* m_NetTest = nullptr;
    QString m_NetTestStatus;
    NetRecommend::Settings m_Recommendation;
    bool m_HasRecommendation = false;
    CloudProvider* m_Provider;
    QNetworkAccessManager* m_ProbeNam;
    CloudProvider::Machine m_Machine;
    bool m_Busy = false;
    bool m_Restoring = false;
    bool m_MfaRequired = false;
    QString m_Progress;
    QString m_Error;

    Stage m_Stage = Stage::Idle;
    QElapsedTimer m_StageClock;
    // The address the connect flow uses: the DNS name, or the public IP once DNS has lagged too long.
    QString m_ConnectHost;
    QString connectHost() const { return m_ConnectHost.isEmpty() ? hostAddress() : m_ConnectHost; }
    QPointer<ComputerManager> m_ComputerManager;
    QString m_HostUuid;          // Sunshine uniqueid from serverinfo (fresh DNS)
    bool m_ManagerPolling = false;
    QString m_PendingPin;
    bool m_PinAccepted = false;
    bool m_RestartPending = false;
    bool m_StreamActive = false;

    QTimer m_PollTimer;
    QTimer m_StepTimer;
    QTimer m_IdleTimer;
};
