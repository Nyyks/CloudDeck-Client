// CloudDeck-Client: network test against the CloudDeck (Phase 5,
// docs/adr/0011-network-test.md). Measures round-trip latency and CloudDeck -> PC
// throughput through the host agent's net/test operation, on a worker thread.
#pragma once

#include <QSslCertificate>
#include <QThread>

class NetTest : public QThread
{
    Q_OBJECT
public:
    NetTest(QString host, quint16 httpsPort, QSslCertificate serverCert);
    ~NetTest() override;

signals:
    void progress(QString text);
    // On success error is empty; rttMs is the median of a few pings, downMbps the
    // throughput of the largest transfer.
    void finished(double rttMs, double downMbps, QString error);

protected:
    void run() override;

private:
    const QString m_Host;
    const quint16 m_Port;
    const QSslCertificate m_ServerCert;
};
