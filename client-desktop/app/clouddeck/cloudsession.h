// QML-facing controller for the CloudDeck integration ("CloudDeck" singleton).
// Owns the CloudProvider, polls machine status, and runs "Start and connect":
//   start VM -> wait running -> wait DNS -> wait Sunshine -> auto-pair -> stream.
#pragma once

#include "cloudprovider.h"

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
    bool clipboardSync() const;
    void setClipboardSync(bool v);

signals:
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
