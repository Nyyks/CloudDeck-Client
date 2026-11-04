// Unit tests for the network-free CloudDeck API helpers.
// Fixtures in shared/fixtures/clouddeck are shared with the Android tests.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "clouddeck/clouddeckapi.h"

using namespace CloudDeckApi;

static QJsonObject loadFixture(const char* name)
{
    QFile f(QStringLiteral(FIXTURE_DIR "/") + QLatin1String(name));
    if (!f.open(QIODevice::ReadOnly)) {
        qFatal("missing fixture %s", name);
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

static QString makeJwt(const QJsonObject& claims)
{
    auto b64 = [](const QByteArray& b) {
        return QString::fromLatin1(b.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    };
    return b64(R"({"alg":"RS256","kid":"x"})") + "." + b64(QJsonDocument(claims).toJson(QJsonDocument::Compact)) + ".sig";
}

class TestCloudDeckApi : public QObject
{
    Q_OBJECT
private slots:
    void allowlist_data()
    {
        QTest::addColumn<QString>("method");
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("allowed");
        const QJsonArray cases = loadFixture("allowlist.json").value("cases").toArray();
        QVERIFY(cases.size() > 10);
        for (const QJsonValue& v : cases) {
            const QJsonObject c = v.toObject();
            const QString label = c.value("method").toString() + " " + c.value("path").toString();
            QTest::newRow(qPrintable(label)) << c.value("method").toString() << c.value("path").toString()
                                             << c.value("allowed").toBool();
        }
    }
    void allowlist()
    {
        QFETCH(QString, method);
        QFETCH(QString, path);
        QFETCH(bool, allowed);
        QCOMPARE(isRequestAllowed(method, path), allowed);
    }

    void parseRunningMachine()
    {
        const Machine m = parseMachine(loadFixture("machine_running.json"));
        QVERIFY(m.valid);
        QCOMPARE(m.id, QStringLiteral("m_43ca6bbd2b"));
        QCOMPARE(m.status, QStringLiteral("running"));
        QCOMPARE(m.publicIp, QStringLiteral("203.0.113.10"));
        QCOMPARE(m.region, QStringLiteral("eu-de-nbg-1"));
        QCOMPARE(m.os, QStringLiteral("clouddeck-26.04.1"));
        QCOMPARE(m.lastStartedMs, Q_INT64_C(1793799500000));
    }

    void parseOffMachine()
    {
        const Machine m = parseMachine(loadFixture("machine_off.json"));
        QVERIFY(m.valid);
        QCOMPARE(m.status, QStringLiteral("off"));
        QVERIFY(m.publicIp.isEmpty());
    }

    void parseInvalidMachine()
    {
        QVERIFY(!parseMachine(QJsonObject()).valid);
        QVERIFY(!parseMachine(QJsonObject{{"id", "x_1"}, {"status", "off"}}).valid);
        QVERIFY(!parseMachine(QJsonObject{{"id", "m_1"}}).valid);
    }

    void hostAddress()
    {
        QCOMPARE(hostAddressFor("m_43ca6bbd2b"), QStringLiteral("43ca6bbd2b.my.clouddeck.app"));
        QVERIFY(hostAddressFor("").isEmpty());
        QVERIFY(hostAddressFor("m_").isEmpty());
        QVERIFY(hostAddressFor("43ca6bbd2b").isEmpty());
    }

    void jwtClaims()
    {
        // '?' and '>' in values force '-' and '_' into the base64url encoding.
        QJsonObject claims{{"custom:account", "a_7d7306fe31"}, {"email", "user@example.com"}, {"x", "??>>??>>"}};
        const QJsonObject decoded = decodeJwtClaims(makeJwt(claims));
        QCOMPARE(decoded.value("custom:account").toString(), QStringLiteral("a_7d7306fe31"));
        QCOMPARE(decoded.value("email").toString(), QStringLiteral("user@example.com"));
        QCOMPARE(decoded.value("x").toString(), QStringLiteral("??>>??>>"));
    }

    void jwtMalformed()
    {
        QVERIFY(decodeJwtClaims("").isEmpty());
        QVERIFY(decodeJwtClaims("abc").isEmpty());
        QVERIFY(decodeJwtClaims("a..c").isEmpty());
        QVERIFY(decodeJwtClaims("a.!!!notbase64!!!.c").isEmpty());
        QVERIFY(decodeJwtClaims("a.b.c.d").isEmpty());
    }

    void transitional()
    {
        QVERIFY(isTransitional("starting"));
        QVERIFY(isTransitional("stopping"));
        QVERIFY(!isTransitional("running"));
        QVERIFY(!isTransitional("off"));
    }
};

QTEST_GUILESS_MAIN(TestCloudDeckApi)
#include "tst_clouddeckapi.moc"
