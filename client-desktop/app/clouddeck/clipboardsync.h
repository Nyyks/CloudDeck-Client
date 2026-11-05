// CloudDeck-Client: two-way text clipboard sync with the CloudDeck during a stream
// (docs/adr/0008-clipboard-sync.md). The tunnel runs on this worker thread. The
// stream's SDL loop owns the local clipboard: it calls offer() when the local
// clipboard changes, and calls takeIncoming() when sdlEventType() arrives.
#pragma once

#include <QMutex>
#include <QSslCertificate>
#include <QString>
#include <QThread>

#include <SDL.h>

class ClipboardSync : public QThread
{
    Q_OBJECT
public:
    static constexpr int kMaxBytes = 100 * 1024; // same limit as the host agent

    static bool enabled();            // setting, default on
    static void setEnabled(bool on);
    // SDL event posted when text from the CloudDeck is waiting. Call once on the
    // SDL thread before starting a sync, so it's registered there.
    static Uint32 sdlEventType();

    ClipboardSync(QString host, quint16 httpsPort, QSslCertificate serverCert);
    ~ClipboardSync() override; // closes the tunnel

    void offer(const QString& text);      // local clipboard changed (any thread)
    bool takeIncoming(QString& text);     // text from the CloudDeck, if any

protected:
    void run() override;

private:
    const QString m_Host;
    const quint16 m_Port;
    const QSslCertificate m_ServerCert;

    QMutex m_Lock;
    QString m_Outgoing;
    bool m_HasOutgoing = false;
    QString m_Incoming;
    bool m_HasIncoming = false;
};
