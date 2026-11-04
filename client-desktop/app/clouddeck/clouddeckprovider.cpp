#include "clouddeckprovider.h"
#include "secretstore.h"

#include <QDebug>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

using namespace CloudDeckApi;

static const char* kRefreshKey = "clouddeck.refresh_token";
static const char* kEmailSetting = "clouddeck/email";
static const int kTimeoutMs = 20000;

CloudDeckProvider::CloudDeckProvider(QObject* parent)
    : CloudProvider(parent), m_Nam(new QNetworkAccessManager(this))
{
    m_Nam->setStrictTransportSecurityEnabled(true);
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_Nam->setTransferTimeout(kTimeoutMs);
    m_Email = QSettings().value(kEmailSetting).toString();
}

// ---------------------------------------------------------------- transport

static QString replyError(QNetworkReply* reply, const QJsonDocument& doc)
{
    // Cognito errors: {"__type":"NotAuthorizedException","message":"Incorrect username or password."}
    const QJsonObject o = doc.object();
    if (o.contains(QStringLiteral("message"))) {
        return o.value(QStringLiteral("message")).toString();
    }
    if (reply->error() != QNetworkReply::NoError) {
        return reply->errorString();
    }
    return QString();
}

void CloudDeckProvider::cognito(const QString& target, const QJsonObject& body, JsonCb cb)
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kCognitoEndpoint))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-amz-json-1.1"));
    req.setRawHeader("X-Amz-Target", ("AWSCognitoIdentityProviderService." + target).toLatin1());
    QNetworkReply* reply = m_Nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString err = (status >= 200 && status < 300) ? QString() : replyError(reply, doc);
        cb(err.isEmpty() && status == 0 ? reply->errorString() : err, status, doc);
    });
}

void CloudDeckProvider::api(const QString& method, const QString& path, const QJsonObject& body, JsonCb cb, bool retried)
{
    if (!isRequestAllowed(method, path)) {
        // Programming error: never send anything outside the allowlist.
        qCritical() << "CloudDeck: blocked request outside allowlist:" << method << path;
        cb(QStringLiteral("Request not allowed"), 0, QJsonDocument());
        return;
    }
    ensureAccessToken([this, method, path, body, cb, retried](const QString& err) {
        if (!err.isEmpty()) {
            cb(err, 0, QJsonDocument());
            return;
        }
        QNetworkRequest req{QUrl(QString::fromLatin1(kApiBase) + path)};
        req.setRawHeader("Authorization", "Bearer " + m_AccessToken.toLatin1());
        QNetworkReply* reply;
        if (method == QLatin1String("GET")) {
            reply = m_Nam->get(req);
        }
        else {
            req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            reply = m_Nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        }
        connect(reply, &QNetworkReply::finished, this, [this, reply, method, path, body, cb, retried]() {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (status == 401 && !retried) {
                // Access token rejected early (revoked/clock skew): refresh once and retry.
                m_AccessToken.clear();
                api(method, path, body, cb, true);
                return;
            }
            if (status >= 200 && status < 300) {
                cb(QString(), status, doc);
            }
            else {
                QString e = replyError(reply, doc);
                cb(e.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : e, status, doc);
            }
        });
    });
}

// ---------------------------------------------------------------- auth

bool CloudDeckProvider::applyAuthResult(const QJsonObject& ar, bool persistRefresh)
{
    m_AccessToken = ar.value(QStringLiteral("AccessToken")).toString();
    m_IdToken = ar.value(QStringLiteral("IdToken")).toString();
    const int expiresIn = ar.value(QStringLiteral("ExpiresIn")).toInt(3600);
    m_AccessExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    if (ar.contains(QStringLiteral("RefreshToken"))) {
        m_RefreshToken = ar.value(QStringLiteral("RefreshToken")).toString();
        if (persistRefresh) {
            SecretStore::get().write(QString::fromLatin1(kRefreshKey), m_RefreshToken.toUtf8());
        }
    }
    const QJsonObject claims = decodeJwtClaims(m_IdToken);
    m_AccountId = claims.value(QStringLiteral("custom:account")).toString();
    const QString email = claims.value(QStringLiteral("email")).toString();
    if (!email.isEmpty()) {
        m_Email = email;
        QSettings().setValue(kEmailSetting, email);
    }
    return !m_AccessToken.isEmpty() && !m_AccountId.isEmpty();
}

void CloudDeckProvider::signIn(const QString& email, const QString& password, SignInCb cb)
{
    QJsonObject params{{QStringLiteral("USERNAME"), email}, {QStringLiteral("PASSWORD"), password}};
    QJsonObject body{{QStringLiteral("AuthFlow"), QStringLiteral("USER_PASSWORD_AUTH")},
                     {QStringLiteral("ClientId"), QString::fromLatin1(kCognitoClientId)},
                     {QStringLiteral("AuthParameters"), params}};
    cognito(QStringLiteral("InitiateAuth"), body, [this, email, cb](const QString& err, int, const QJsonDocument& doc) {
        if (!err.isEmpty()) {
            cb(SignInResult::Failed, err);
            return;
        }
        const QJsonObject o = doc.object();
        if (o.contains(QStringLiteral("ChallengeName"))) {
            m_ChallengeName = o.value(QStringLiteral("ChallengeName")).toString();
            m_ChallengeSession = o.value(QStringLiteral("Session")).toString();
            m_ChallengeUser = email;
            if (m_ChallengeName == QLatin1String("SOFTWARE_TOKEN_MFA") || m_ChallengeName == QLatin1String("SMS_MFA")) {
                cb(SignInResult::MfaRequired, QString());
            }
            else {
                cb(SignInResult::Failed, tr("Unsupported sign-in challenge: %1").arg(m_ChallengeName));
            }
            return;
        }
        if (!applyAuthResult(o.value(QStringLiteral("AuthenticationResult")).toObject(), true)) {
            cb(SignInResult::Failed, tr("Unexpected sign-in response"));
            return;
        }
        cb(SignInResult::Ok, QString());
    });
}

void CloudDeckProvider::respondMfa(const QString& code, SignInCb cb)
{
    const QString codeKey = m_ChallengeName == QLatin1String("SMS_MFA") ? QStringLiteral("SMS_MFA_CODE")
                                                                         : QStringLiteral("SOFTWARE_TOKEN_MFA_CODE");
    QJsonObject responses{{QStringLiteral("USERNAME"), m_ChallengeUser}, {codeKey, code}};
    QJsonObject body{{QStringLiteral("ChallengeName"), m_ChallengeName},
                     {QStringLiteral("ClientId"), QString::fromLatin1(kCognitoClientId)},
                     {QStringLiteral("Session"), m_ChallengeSession},
                     {QStringLiteral("ChallengeResponses"), responses}};
    cognito(QStringLiteral("RespondToAuthChallenge"), body, [this, cb](const QString& err, int, const QJsonDocument& doc) {
        m_ChallengeSession.clear();
        if (!err.isEmpty()) {
            cb(SignInResult::Failed, err);
            return;
        }
        if (!applyAuthResult(doc.object().value(QStringLiteral("AuthenticationResult")).toObject(), true)) {
            cb(SignInResult::Failed, tr("Unexpected sign-in response"));
            return;
        }
        cb(SignInResult::Ok, QString());
    });
}

void CloudDeckProvider::refresh(DoneCb cb)
{
    if (m_RefreshToken.isEmpty()) {
        cb(tr("Not signed in"));
        return;
    }
    QJsonObject body{{QStringLiteral("AuthFlow"), QStringLiteral("REFRESH_TOKEN_AUTH")},
                     {QStringLiteral("ClientId"), QString::fromLatin1(kCognitoClientId)},
                     {QStringLiteral("AuthParameters"), QJsonObject{{QStringLiteral("REFRESH_TOKEN"), m_RefreshToken}}}};
    cognito(QStringLiteral("InitiateAuth"), body, [this, cb](const QString& err, int status, const QJsonDocument& doc) {
        if (!err.isEmpty()) {
            if (status == 400) {
                // NotAuthorizedException: refresh token expired or revoked.
                signOut();
                emit sessionExpired();
            }
            cb(err);
            return;
        }
        if (!applyAuthResult(doc.object().value(QStringLiteral("AuthenticationResult")).toObject(), true)) {
            cb(tr("Unexpected token refresh response"));
            return;
        }
        cb(QString());
    });
}

void CloudDeckProvider::ensureAccessToken(DoneCb cb)
{
    if (!m_AccessToken.isEmpty() && QDateTime::currentDateTimeUtc().secsTo(m_AccessExpiry) > 60) {
        cb(QString());
        return;
    }
    refresh(cb);
}

void CloudDeckProvider::restoreSession(SignInCb cb)
{
    const QByteArray stored = SecretStore::get().read(QString::fromLatin1(kRefreshKey));
    if (stored.isEmpty()) {
        cb(SignInResult::Failed, QString());
        return;
    }
    m_RefreshToken = QString::fromUtf8(stored);
    refresh([cb](const QString& err) {
        cb(err.isEmpty() ? SignInResult::Ok : SignInResult::Failed, err);
    });
}

void CloudDeckProvider::signOut()
{
    m_AccessToken.clear();
    m_IdToken.clear();
    m_RefreshToken.clear();
    m_AccountId.clear();
    m_MachineId.clear();
    SecretStore::get().remove(QString::fromLatin1(kRefreshKey));
}

// ---------------------------------------------------------------- machine

void CloudDeckProvider::withMachineId(std::function<void(const QString&, const QString&)> cb)
{
    if (!m_MachineId.isEmpty()) {
        cb(QString(), m_MachineId);
        return;
    }
    ensureAccessToken([this, cb](const QString& err) {
        if (!err.isEmpty()) {
            cb(err, QString());
            return;
        }
        api(QStringLiteral("GET"), QStringLiteral("/accounts/") + m_AccountId, {},
            [this, cb](const QString& err, int, const QJsonDocument& doc) {
                if (!err.isEmpty()) {
                    cb(err, QString());
                    return;
                }
                m_MachineId = doc.object().value(QStringLiteral("machine_id")).toString();
                if (m_MachineId.isEmpty()) {
                    cb(tr("This account has no CloudDeck machine"), QString());
                    return;
                }
                cb(QString(), m_MachineId);
            });
    });
}

void CloudDeckProvider::getMachine(MachineCb cb)
{
    withMachineId([this, cb](const QString& err, const QString& id) {
        if (!err.isEmpty()) {
            cb(err, Machine());
            return;
        }
        api(QStringLiteral("GET"), QStringLiteral("/machines/") + id, {},
            [cb](const QString& err, int, const QJsonDocument& doc) {
                if (!err.isEmpty()) {
                    cb(err, Machine());
                    return;
                }
                Machine m = parseMachine(doc.object());
                cb(m.valid ? QString() : tr("Unexpected machine response"), m);
            });
    });
}

void CloudDeckProvider::startMachine(const QString& sessionMode, DoneCb cb)
{
    withMachineId([this, sessionMode, cb](const QString& err, const QString& id) {
        if (!err.isEmpty()) {
            cb(err);
            return;
        }
        QJsonObject body;
        if (sessionMode == QLatin1String("desktop") || sessionMode == QLatin1String("gaming")) {
            body.insert(QStringLiteral("session"), sessionMode);
        }
        api(QStringLiteral("POST"), QStringLiteral("/machines/%1/start").arg(id), body,
            [cb](const QString& err, int, const QJsonDocument&) { cb(err); });
    });
}

void CloudDeckProvider::stopMachine(DoneCb cb)
{
    withMachineId([this, cb](const QString& err, const QString& id) {
        if (!err.isEmpty()) {
            cb(err);
            return;
        }
        api(QStringLiteral("POST"), QStringLiteral("/machines/%1/stop").arg(id), {},
            [cb](const QString& err, int, const QJsonDocument&) { cb(err); });
    });
}

void CloudDeckProvider::submitPairingPin(const QString& pin, DoneCb cb)
{
    withMachineId([this, pin, cb](const QString& err, const QString& id) {
        if (!err.isEmpty()) {
            cb(err);
            return;
        }
        api(QStringLiteral("POST"), QStringLiteral("/machines/%1/clients").arg(id), QJsonObject{{QStringLiteral("pin"), pin}},
            [cb](const QString& err, int, const QJsonDocument&) { cb(err); });
    });
}

QString CloudDeckProvider::hostAddress(const Machine& machine) const
{
    return hostAddressFor(machine.id);
}
