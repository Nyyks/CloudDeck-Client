// OS keychain wrapper for small secrets (the CloudDeck refresh token).
//   Windows: Credential Manager (generic credential, per user, not roaming)
//   Linux:   freedesktop Secret Service over D-Bus (GNOME Keyring, KWallet >= 5.97)
//   Other / unavailable: process memory only, so the user signs in again next launch.
// There is deliberately no plaintext-file fallback.
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

class SecretStore
{
public:
    static SecretStore& get();

    // Returns false if the secret could not be stored persistently. It is then
    // kept in memory for this process only.
    bool write(const QString& key, const QByteArray& secret);
    QByteArray read(const QString& key);
    void remove(const QString& key);

    // Human-readable backend name for the UI/logs ("Windows Credential Manager", ...).
    QString backendName() const;

private:
    SecretStore() = default;
    bool backendWrite(const QString& key, const QByteArray& secret);
    bool backendRead(const QString& key, QByteArray& secret);
    void backendRemove(const QString& key);

    QHash<QString, QByteArray> m_MemoryOnly;
};
