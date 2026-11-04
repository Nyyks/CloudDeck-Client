// Abstract cloud-PC provider. Everything portal-specific stays behind this
// interface, so a breaking portal change only touches one implementation.
// The Android client implements the same contract in Java (docs/clouddeck-api.md).
#pragma once

#include "clouddeckapi.h"

#include <QObject>
#include <functional>

class CloudProvider : public QObject
{
    Q_OBJECT
public:
    using Machine = CloudDeckApi::Machine;
    // error is empty on success
    using DoneCb = std::function<void(const QString& error)>;
    using MachineCb = std::function<void(const QString& error, const Machine& machine)>;

    enum class SignInResult { Ok, MfaRequired, Failed };
    using SignInCb = std::function<void(SignInResult result, const QString& error)>;

    explicit CloudProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~CloudProvider() override = default;

    virtual QString name() const = 0;
    virtual bool isSignedIn() const = 0;
    virtual QString accountEmail() const = 0;

    virtual void signIn(const QString& email, const QString& password, SignInCb cb) = 0;
    virtual void respondMfa(const QString& code, SignInCb cb) = 0;
    // Restores a session from the keychain (refresh token). Calls back with
    // SignInResult::Failed if there is nothing stored or it was revoked.
    virtual void restoreSession(SignInCb cb) = 0;
    virtual void signOut() = 0;

    // The account's (single) machine.
    virtual void getMachine(MachineCb cb) = 0;
    virtual void startMachine(const QString& sessionMode, DoneCb cb) = 0;
    virtual void stopMachine(DoneCb cb) = 0;
    virtual void submitPairingPin(const QString& pin, DoneCb cb) = 0;
    virtual QString hostAddress(const Machine& machine) const = 0;

signals:
    void sessionExpired();
};
