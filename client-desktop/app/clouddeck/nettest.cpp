#include "nettest.h"

#include "agenttunnel.h"

#include <QElapsedTimer>
#include <QSslSocket>
#include <QtDebug>

#include <algorithm>
#include <vector>

NetTest::NetTest(QString host, quint16 httpsPort, QSslCertificate serverCert)
    : m_Host(std::move(host)), m_Port(httpsPort), m_ServerCert(std::move(serverCert))
{
    setObjectName("CloudDeck network test");
}

NetTest::~NetTest()
{
    requestInterruption();
    wait();
}

void NetTest::run()
{
    const auto interrupted = [this]() { return isInterruptionRequested(); };
    QSslSocket s;
    QByteArray buf;
    emit progress(tr("Connecting to your CloudDeck..."));
    const QString error = AgentTunnel::open(s, m_Host, m_Port, m_ServerCert, "net/test", QDeadlineTimer(20000), interrupted, buf);
    if (!error.isEmpty()) {
        emit finished(0, 0, error);
        return;
    }

    // Latency: the first ping warms the path up and is not counted.
    emit progress(tr("Measuring latency..."));
    std::vector<double> rtts;
    for (int i = 0; i < 6; i++) {
        QElapsedTimer t;
        t.start();
        s.write("ping\n");
        QByteArray line;
        if (!AgentTunnel::readLine(s, buf, line, QDeadlineTimer(5000), interrupted) || line != "pong") {
            emit finished(0, 0, tr("the CloudDeck did not answer the latency test (install the latest host update)"));
            return;
        }
        if (i > 0) rtts.push_back(t.nsecsElapsed() / 1e6);
    }
    std::sort(rtts.begin(), rtts.end());
    const double rtt = rtts[rtts.size() / 2];

    // Throughput: grow the transfer until one takes at least 1.5 s (or 64 MiB).
    qint64 size = 1 << 20;
    double mbps = 0;
    for (int round = 0; round < 6 && !isInterruptionRequested(); round++) {
        emit progress(tr("Measuring speed..."));
        QElapsedTimer t;
        t.start();
        s.write("down " + QByteArray::number(size) + "\n");
        qint64 got = qMin<qint64>(buf.size(), size);
        buf.clear();
        while (got < size) {
            if (isInterruptionRequested() || t.elapsed() > 30000 ||
                (!s.waitForReadyRead(1000) && s.state() != QAbstractSocket::ConnectedState)) {
                emit finished(rtt, 0, tr("the speed test did not finish"));
                return;
            }
            got += s.read(size - got).size(); // never read past this transfer
        }
        const double seconds = t.nsecsElapsed() / 1e9;
        mbps = size * 8 / seconds / 1e6;
        qInfo() << "CloudDeck network test:" << size << "bytes in" << seconds << "s =" << mbps << "Mbit/s";
        if (seconds >= 1.5 || size >= (64 << 20)) break;
        size = qMin<qint64>(size * 4, 64 << 20);
    }
    s.disconnectFromHost();
    emit finished(rtt, mbps, QString());
}
