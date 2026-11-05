// CloudDeck-Client: blocking helpers for host-agent tunnels held open on a worker
// thread (display lease, clipboard sync). TLS to Sunshine's HTTPS port with the
// pairing identity, then "Upgrade: clouddeck-agent/1" (docs/adr/0004, 0006).
#pragma once

#include <QByteArray>
#include <QDeadlineTimer>
#include <QSslCertificate>
#include <QSslSocket>
#include <QString>

#include <functional>

namespace AgentTunnel {

// Connects `s` (created on the calling thread), pins the paired host certificate,
// and upgrades GET /clouddeck/v1/<target>. Returns an empty string once the host
// answered 101, with any bytes after the headers in `rest`; otherwise a
// user-readable reason. `interrupted` is polled while waiting.
QString open(QSslSocket& s, const QString& host, quint16 port, const QSslCertificate& serverCert,
             const QByteArray& target, QDeadlineTimer deadline, const std::function<bool()>& interrupted,
             QByteArray& rest);

// Reads up to and including the next '\n' into `line` (without it), using `buf` for
// bytes already received. False on timeout, interruption or a closed socket.
bool readLine(QSslSocket& s, QByteArray& buf, QByteArray& line, QDeadlineTimer deadline,
              const std::function<bool()>& interrupted);

}
