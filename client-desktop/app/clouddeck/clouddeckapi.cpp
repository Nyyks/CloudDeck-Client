#include "clouddeckapi.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace CloudDeckApi {

bool isRequestAllowed(const QString& method, const QString& path)
{
    // IDs are prefixed (a_/m_) followed by lowercase alphanumerics in practice;
    // accept [A-Za-z0-9] to be tolerant, but never '/', '.', or '%'.
    static const QRegularExpression getAccount(QStringLiteral("^/accounts/a_[A-Za-z0-9]+$"));
    static const QRegularExpression getMachine(QStringLiteral("^/machines/m_[A-Za-z0-9]+$"));
    static const QRegularExpression postMachine(QStringLiteral("^/machines/m_[A-Za-z0-9]+/(start|stop|clients)$"));

    if (method == QLatin1String("GET")) {
        return getAccount.match(path).hasMatch() || getMachine.match(path).hasMatch();
    }
    if (method == QLatin1String("POST")) {
        return postMachine.match(path).hasMatch();
    }
    return false;
}

Machine parseMachine(const QJsonObject& obj)
{
    Machine m;
    m.id = obj.value(QStringLiteral("id")).toString();
    m.status = obj.value(QStringLiteral("status")).toString();
    m.publicIp = obj.value(QStringLiteral("public_ip")).toString();
    m.os = obj.value(QStringLiteral("os")).toString();
    m.region = obj.value(QStringLiteral("region")).toString();
    m.lastStartedMs = static_cast<qint64>(obj.value(QStringLiteral("last_started")).toDouble(0));
    m.valid = m.id.startsWith(QLatin1String("m_")) && m.id.size() > 2 && !m.status.isEmpty();
    return m;
}

QString hostAddressFor(const QString& machineId)
{
    if (!machineId.startsWith(QLatin1String("m_")) || machineId.size() <= 2) {
        return QString();
    }
    return machineId.mid(2) + QLatin1String(kHostSuffix);
}

QJsonObject decodeJwtClaims(const QString& jwt)
{
    const QStringList parts = jwt.split(QLatin1Char('.'));
    if (parts.size() != 3 || parts[1].isEmpty()) {
        return QJsonObject();
    }
    const QByteArray payload = QByteArray::fromBase64(parts[1].toLatin1(),
                                                      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

bool isTransitional(const QString& status)
{
    return status == QLatin1String(Status::Starting) || status == QLatin1String(Status::Stopping) ||
           status == QLatin1String("pending") || status == QLatin1String("provisioning");
}

}
