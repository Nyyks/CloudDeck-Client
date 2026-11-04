#pragma once

#include "cloudprovider.h"

#include <QDateTime>
#include <QJsonDocument>

class QNetworkAccessManager;
class QNetworkReply;

class CloudDeckProvider : public CloudProvider
{
    Q_OBJECT
public:
    explicit CloudDeckProvider(QObject* parent = nullptr);

    QString name() const override { return QStringLiteral("CloudDeck"); }
    bool isSignedIn() const override { return !m_RefreshToken.isEmpty(); }
    QString accountEmail() const override { return m_Email; }

    void signIn(const QString& email, const QString& password, SignInCb cb) override;
    void respondMfa(const QString& code, SignInCb cb) override;
    void restoreSession(SignInCb cb) override;
    void signOut() override;

    void getMachine(MachineCb cb) override;
    void startMachine(const QString& sessionMode, DoneCb cb) override;
    void stopMachine(DoneCb cb) override;
    void submitPairingPin(const QString& pin, DoneCb cb) override;
    QString hostAddress(const Machine& machine) const override;

private:
    using JsonCb = std::function<void(const QString& error, int httpStatus, const QJsonDocument& doc)>;

    void cognito(const QString& target, const QJsonObject& body, JsonCb cb);
    bool applyAuthResult(const QJsonObject& authResult, bool persistRefresh);
    void ensureAccessToken(DoneCb cb);
    void refresh(DoneCb cb);
    void api(const QString& method, const QString& path, const QJsonObject& body, JsonCb cb, bool retried = false);
    void withMachineId(std::function<void(const QString& error, const QString& machineId)> cb);

    QNetworkAccessManager* m_Nam;
    QString m_Email;
    QString m_AccessToken;
    QString m_IdToken;
    QString m_RefreshToken;
    QDateTime m_AccessExpiry;
    QString m_AccountId;
    QString m_MachineId;
    // MFA challenge state
    QString m_ChallengeName;
    QString m_ChallengeSession;
    QString m_ChallengeUser;
};
