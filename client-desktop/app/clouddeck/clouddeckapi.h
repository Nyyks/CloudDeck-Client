// Pure (network-free) helpers for the CloudDeck portal API.
// Spec: docs/clouddeck-api.md. Unit-tested in client-desktop/tests/clouddeck.
#pragma once

#include <QJsonObject>
#include <QString>

namespace CloudDeckApi {

constexpr const char* kApiBase = "https://api.clouddeck.app";
constexpr const char* kCognitoEndpoint = "https://cognito-idp.eu-central-1.amazonaws.com/";
constexpr const char* kCognitoClientId = "2e7an7pt3vqdae0abgskfs38k8";
constexpr const char* kHostSuffix = ".my.clouddeck.app";

// Known machine states (docs/clouddeck-api.md, "Machine status values").
namespace Status {
constexpr const char* Off = "off";
constexpr const char* Starting = "starting";
constexpr const char* Running = "running";
constexpr const char* Stopping = "stopping";
}

struct Machine {
    QString id;          // "m_43ca6bbd2b"
    QString status;      // see Status
    QString publicIp;    // empty while off
    QString os;          // "clouddeck-26.04.1"
    QString region;      // "eu-de-nbg-1"
    qint64 lastStartedMs = 0;
    bool valid = false;
};

// Endpoint allowlist. The client can only reach these method/path pairs.
// Everything else, notably POST /machines/{id}/reset (factory wipe), billing
// and account mutation, is refused before any request is sent.
bool isRequestAllowed(const QString& method, const QString& path);

// Parses GET /machines/{id}. Never reads or keeps the "password" field.
Machine parseMachine(const QJsonObject& obj);

// "m_43ca6bbd2b" -> "43ca6bbd2b.my.clouddeck.app" (same rule as the portal).
QString hostAddressFor(const QString& machineId);

// Decodes a JWT payload without verifying it. We only read claims from tokens
// Cognito just handed us over TLS. Returns an empty object on malformed input.
QJsonObject decodeJwtClaims(const QString& jwt);

// True when the machine is in a transitional state worth polling quickly.
bool isTransitional(const QString& status);

}
