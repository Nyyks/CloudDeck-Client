#include "secretstore.h"
#include "branding.h"

#include <QDebug>

[[maybe_unused]] static QString targetName(const QString& key)
{
    return QStringLiteral(CDC_APP_ID "/") + key;
}

SecretStore& SecretStore::get()
{
    static SecretStore s;
    return s;
}

bool SecretStore::write(const QString& key, const QByteArray& secret)
{
    if (backendWrite(key, secret)) {
        m_MemoryOnly.remove(key);
        fileRemove(key); // left over from a session without a keychain
        m_UsingFile = false;
        return true;
    }
    if (fileWrite(key, secret)) {
        qInfo() << "SecretStore: no keychain available; keeping" << key << "in a private file";
        m_MemoryOnly.remove(key);
        m_UsingFile = true;
        return true;
    }
    qWarning() << "SecretStore: no persistent storage available; keeping" << key << "in memory only";
    m_MemoryOnly.insert(key, secret);
    return false;
}

QByteArray SecretStore::read(const QString& key)
{
    if (m_MemoryOnly.contains(key)) {
        return m_MemoryOnly.value(key);
    }
    QByteArray secret;
    if (backendRead(key, secret)) {
        return secret;
    }
    if (fileRead(key, secret)) {
        m_UsingFile = true;
        return secret;
    }
    return QByteArray();
}

void SecretStore::remove(const QString& key)
{
    m_MemoryOnly.remove(key);
    backendRemove(key);
    fileRemove(key);
}

// ---------------------------------------------------------------------------
// Private-file fallback (Linux)

#ifdef Q_OS_LINUX
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <sys/stat.h>

static QString secretsDir()
{
    // ~/.config/<org>/secrets, next to Moonlight's own config (which holds the pairing key)
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/" CDC_ORG_NAME "/secrets");
}

static QString secretFile(const QString& key)
{
    QString safe = key;
    for (QChar& c : safe) if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('-')) c = QLatin1Char('_');
    return secretsDir() + QLatin1Char('/') + safe;
}

bool SecretStore::fileWrite(const QString& key, const QByteArray& secret)
{
    const QString dir = secretsDir();
    if (!QDir().mkpath(dir)) return false;
    if (::chmod(QFile::encodeName(dir).constData(), 0700) != 0) return false;
    const mode_t old = ::umask(0077); // the temp file is created 0600 from the start
    QSaveFile f(secretFile(key));
    bool ok = f.open(QIODevice::WriteOnly) && f.write(secret) == secret.size() && f.commit();
    ::umask(old);
    if (ok) ok = ::chmod(QFile::encodeName(secretFile(key)).constData(), 0600) == 0;
    return ok;
}

bool SecretStore::fileRead(const QString& key, QByteArray& secret)
{
    QFile f(secretFile(key));
    if (!f.open(QIODevice::ReadOnly)) return false;
    secret = f.readAll();
    return !secret.isEmpty();
}

void SecretStore::fileRemove(const QString& key)
{
    QFile::remove(secretFile(key));
}
#else
bool SecretStore::fileWrite(const QString&, const QByteArray&) { return false; }
bool SecretStore::fileRead(const QString&, QByteArray&) { return false; }
void SecretStore::fileRemove(const QString&) {}
#endif

// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN32)
#include <windows.h>
#include <wincred.h>

QString SecretStore::backendName() const { return QStringLiteral("Windows Credential Manager"); }

bool SecretStore::backendWrite(const QString& key, const QByteArray& secret)
{
    const std::wstring target = targetName(key).toStdWString();
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.constData()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE; // per user, does not roam
    static const std::wstring user = L"CloudDeck Client";
    cred.UserName = const_cast<LPWSTR>(user.c_str());
    if (!CredWriteW(&cred, 0)) {
        qWarning() << "CredWriteW failed:" << GetLastError();
        return false;
    }
    return true;
}

bool SecretStore::backendRead(const QString& key, QByteArray& secret)
{
    const std::wstring target = targetName(key).toStdWString();
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        return false;
    }
    secret = QByteArray(reinterpret_cast<const char*>(cred->CredentialBlob), static_cast<int>(cred->CredentialBlobSize));
    CredFree(cred);
    return true;
}

void SecretStore::backendRemove(const QString& key)
{
    const std::wstring target = targetName(key).toStdWString();
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

// ---------------------------------------------------------------------------
#elif defined(Q_OS_LINUX)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEventLoop>
#include <QTimer>

#include "secretstore_prompt.h"

// org.freedesktop.Secret.Secret = (oayays)
struct FdoSecret {
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};
Q_DECLARE_METATYPE(FdoSecret)
typedef QMap<QDBusObjectPath, FdoSecret> FdoSecretMap;
Q_DECLARE_METATYPE(FdoSecretMap)

QDBusArgument& operator<<(QDBusArgument& a, const FdoSecret& s)
{
    a.beginStructure();
    a << s.session << s.parameters << s.value << s.contentType;
    a.endStructure();
    return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, FdoSecret& s)
{
    a.beginStructure();
    a >> s.session >> s.parameters >> s.value >> s.contentType;
    a.endStructure();
    return a;
}

namespace {
const QString kService = QStringLiteral("org.freedesktop.secrets");
const QString kServicePath = QStringLiteral("/org/freedesktop/secrets");
const QString kServiceIface = QStringLiteral("org.freedesktop.Secret.Service");

class SecretServiceClient
{
public:
    SecretServiceClient() : m_Bus(QDBusConnection::sessionBus())
    {
        static bool registered = false;
        if (!registered) {
            qDBusRegisterMetaType<FdoSecret>();
            qDBusRegisterMetaType<FdoSecretMap>();
            registered = true;
        }
    }

    bool open()
    {
        if (!m_Bus.isConnected()) return false;
        QDBusInterface svc(kService, kServicePath, kServiceIface, m_Bus);
        if (!svc.isValid()) return false;
        // "plain" transfer: the secret crosses the user's own session bus unencrypted.
        // That's acceptable for a per-user bus; libsecret offers dh-ietf1024 on top.
        QDBusMessage r = svc.call(QStringLiteral("OpenSession"), QStringLiteral("plain"),
                                  QVariant::fromValue(QDBusVariant(QString())));
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().size() != 2) return false;
        m_Session = r.arguments().at(1).value<QDBusObjectPath>();
        return !m_Session.path().isEmpty();
    }

    QVariantMap attributes(const QString& key) const
    {
        QMap<QString, QString> attrs;
        attrs.insert(QStringLiteral("application"), QStringLiteral(CDC_APP_ID));
        attrs.insert(QStringLiteral("key"), key);
        QVariantMap m;
        for (auto it = attrs.cbegin(); it != attrs.cend(); ++it) m.insert(it.key(), it.value());
        return m;
    }

    // Waits on a Prompt object if the service needs user interaction (e.g. unlock).
    bool runPrompt(const QDBusObjectPath& prompt)
    {
        if (prompt.path() == QLatin1String("/") || prompt.path().isEmpty()) return true;
        QEventLoop loop;
        bool dismissed = true;
        PromptWatcher w(&loop, &dismissed);
        m_Bus.connect(kService, prompt.path(), QStringLiteral("org.freedesktop.Secret.Prompt"),
                      QStringLiteral("Completed"), &w, SLOT(completed(bool,QDBusVariant)));
        QDBusInterface p(kService, prompt.path(), QStringLiteral("org.freedesktop.Secret.Prompt"), m_Bus);
        p.call(QStringLiteral("Prompt"), QString());
        QTimer::singleShot(120000, &loop, &QEventLoop::quit);
        loop.exec();
        return !dismissed;
    }

    bool unlock(const QList<QDBusObjectPath>& objects)
    {
        QDBusInterface svc(kService, kServicePath, kServiceIface, m_Bus);
        QDBusMessage r = svc.call(QStringLiteral("Unlock"), QVariant::fromValue(objects));
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().size() != 2) return false;
        return runPrompt(r.arguments().at(1).value<QDBusObjectPath>());
    }

    QDBusObjectPath defaultCollection()
    {
        QDBusInterface svc(kService, kServicePath, kServiceIface, m_Bus);
        QDBusReply<QDBusObjectPath> r = svc.call(QStringLiteral("ReadAlias"), QStringLiteral("default"));
        return r.isValid() ? r.value() : QDBusObjectPath();
    }

    QList<QDBusObjectPath> search(const QString& key, bool unlockLocked)
    {
        QDBusInterface svc(kService, kServicePath, kServiceIface, m_Bus);
        QDBusMessage r = svc.call(QStringLiteral("SearchItems"), attributes(key));
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().size() != 2) return {};
        QList<QDBusObjectPath> unlocked = qdbus_cast<QList<QDBusObjectPath>>(r.arguments().at(0));
        QList<QDBusObjectPath> locked = qdbus_cast<QList<QDBusObjectPath>>(r.arguments().at(1));
        if (unlockLocked && !locked.isEmpty() && unlock(locked)) unlocked += locked;
        return unlocked;
    }

    bool write(const QString& key, const QByteArray& value)
    {
        QDBusObjectPath coll = defaultCollection();
        if (coll.path().isEmpty() || coll.path() == QLatin1String("/")) return false;
        if (!unlock({coll})) return false;
        QVariantMap props;
        props.insert(QStringLiteral("org.freedesktop.Secret.Item.Label"), QStringLiteral("CloudDeck Client (%1)").arg(key));
        props.insert(QStringLiteral("org.freedesktop.Secret.Item.Attributes"), attributes(key));
        FdoSecret s{m_Session, QByteArray(), value, QStringLiteral("text/plain")};
        QDBusInterface c(kService, coll.path(), QStringLiteral("org.freedesktop.Secret.Collection"), m_Bus);
        QDBusMessage r = c.call(QStringLiteral("CreateItem"), props, QVariant::fromValue(s), true);
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().size() != 2) {
            qWarning() << "Secret Service CreateItem failed:" << r.errorMessage();
            return false;
        }
        return runPrompt(r.arguments().at(1).value<QDBusObjectPath>());
    }

    bool read(const QString& key, QByteArray& out)
    {
        QList<QDBusObjectPath> items = search(key, true);
        if (items.isEmpty()) return false;
        QDBusInterface svc(kService, kServicePath, kServiceIface, m_Bus);
        QDBusMessage r = svc.call(QStringLiteral("GetSecrets"), QVariant::fromValue(items), QVariant::fromValue(m_Session));
        if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return false;
        FdoSecretMap map = qdbus_cast<FdoSecretMap>(r.arguments().at(0));
        if (map.isEmpty()) return false;
        out = map.first().value;
        return true;
    }

    void remove(const QString& key)
    {
        for (const QDBusObjectPath& item : search(key, true)) {
            QDBusInterface i(kService, item.path(), QStringLiteral("org.freedesktop.Secret.Item"), m_Bus);
            QDBusMessage r = i.call(QStringLiteral("Delete"));
            if (r.type() == QDBusMessage::ReplyMessage && r.arguments().size() == 1) {
                runPrompt(r.arguments().at(0).value<QDBusObjectPath>());
            }
        }
    }

private:
    QDBusConnection m_Bus;
    QDBusObjectPath m_Session;
};
}

QString SecretStore::backendName() const
{
    return m_UsingFile ? QStringLiteral("a private file (no keychain found)") : QStringLiteral("Secret Service (D-Bus)");
}

bool SecretStore::backendWrite(const QString& key, const QByteArray& secret)
{
    SecretServiceClient c;
    return c.open() && c.write(key, secret);
}

bool SecretStore::backendRead(const QString& key, QByteArray& secret)
{
    SecretServiceClient c;
    return c.open() && c.read(key, secret);
}

void SecretStore::backendRemove(const QString& key)
{
    SecretServiceClient c;
    if (c.open()) c.remove(key);
}

// ---------------------------------------------------------------------------
#else
QString SecretStore::backendName() const { return QStringLiteral("memory only"); }
bool SecretStore::backendWrite(const QString&, const QByteArray&) { return false; }
bool SecretStore::backendRead(const QString&, QByteArray&) { return false; }
void SecretStore::backendRemove(const QString&) {}
#endif
