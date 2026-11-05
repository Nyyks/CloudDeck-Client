// CloudDeck-Client: asks the host agent to switch the CloudDeck's display to the
// stream resolution and holds that for the length of the stream
// (docs/adr/0007-multi-monitor.md). The agent restores the previous mode as soon as
// this tunnel closes, including when the client crashes or loses its connection.
//
// Runs on its own thread with blocking sockets, so Session can call acquire() from
// its connection thread right before /launch.
#pragma once

#include <QAtomicInteger>
#include <QMutex>
#include <QSemaphore>
#include <QSslCertificate>
#include <QString>
#include <QThread>

class DisplayLease : public QThread
{
    Q_OBJECT
public:
    DisplayLease(QString host, quint16 httpsPort, QSslCertificate serverCert, int width, int height, int fps);
    ~DisplayLease() override; // closes the tunnel; the host restores its display

    // Starts the request and waits for the host's answer. Returns an empty string on
    // success, otherwise a user-readable reason.
    QString acquire(int timeoutMs = 45000);
    QString mode() const; // e.g. "3840x1080@60.00" once acquired

protected:
    void run() override;

private:
    void answer(const QString& error, const QString& mode = QString());

    const QString m_Host;
    const quint16 m_Port;
    const QSslCertificate m_ServerCert;
    const int m_Width, m_Height, m_Fps;

    QSemaphore m_Answered;
    mutable QMutex m_Lock;
    QString m_Error;
    QString m_Mode;
    QAtomicInteger<bool> m_AnswerGiven {false};
};
