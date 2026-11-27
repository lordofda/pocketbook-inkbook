#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "settings.h"

namespace {

constexpr auto kCertificatePem = R"(-----BEGIN CERTIFICATE-----
MIIDGzCCAgOgAwIBAgIUZQfFTjTtQ+0WsqSQiG1SIVnX+9AwDQYJKoZIhvcNAQEL
BQAwHTEbMBkGA1UEAwwSa29tZ2EtdGVzdC5pbnZhbGlkMB4XDTI2MTEyNzEwMjA0
OVoXDTM2MTEyNDEwMjA0OVowHTEbMBkGA1UEAwwSa29tZ2EtdGVzdC5pbnZhbGlk
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvaT5QJmiVF5i33Hgrx8o
O7vNDrMMF40wUnoka1W6xpf8jVrRufyr3Bs4cGwIED0nGM4PpJ6vXvbwC3E/BRLp
2dAXCN/cpJgq9Qq65IhFhP1fjak5Y0mHqCRhjzdcJ+JfvAalHUmvzmDZzBxdyOU8
nljpolgJ55xnhd4/tEVBAwRMm8CwSmzj4ynemnCR+52imDkN2qsVntxst2tjKhTG
ycs4RJIgC1bGdyBPdUNbPF4SMJBAxCRk+G2afRX6s0NnoqXAd/O2w3borPuSVYbF
wWYY9Y/D54asMNOzymKFVFVLwqeOscBGR505iO9+/l8WvPKAX5ZbVlDIIOmuVQ3N
AwIDAQABo1MwUTAdBgNVHQ4EFgQUoU7wb9xuevVI2WZwVbuA4+C9gogwHwYDVR0j
BBgwFoAUoU7wb9xuevVI2WZwVbuA4+C9gogwDwYDVR0TAQH/BAUwAwEB/zANBgkq
hkiG9w0BAQsFAAOCAQEAk4cCDMNkc/Lm1myiR55hR7pVcYa59okSyb7JuMA0lbCA
siwjnuwfcSFGpvccdeRDirGwrxt8LeveNoz2b6nP6wfh1GDTOzycAmH5E3/HzrOW
N8cc4MqxaIJLNYfAPN1xNg5SzCvpPTOuIRf/viaqVHHpcxkw7PZO3f7YWBkqMHJp
q0jByGu/l1hVTre+MMUzIYzb2VpobZzpC6qL1myA5GlMmMS2Pv9WDSXhVzbszsYR
U8AgE6PWWYxjFLnXAlWNkMTNutoXMYXSvgUUR5JXa7xA89EI9aosfQ/hqwWOuioX
n1p9mxjZBlivs4fvWboJl1ImkJmlG+hI0Pi8rTEPgA==
-----END CERTIFICATE-----)";

void addAccount(QSettings &ini, const QString &id, const QString &name,
                const QString &url = QStringLiteral("https://komga.example.com"),
                const QString &key = QStringLiteral("test-key")) {
  ini.beginGroup(QStringLiteral("accounts/") + id);
  ini.setValue(QStringLiteral("name"), name);
  ini.setValue(QStringLiteral("url"), url);
  ini.setValue(QStringLiteral("apiKey"), key);
  ini.endGroup();
}

void seedAccounts(const QString &root, const QString &startup, const QString &active) {
  QSettings ini(QDir(root).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
  addAccount(ini, QStringLiteral("alpha"), QStringLiteral("Alpha"));
  addAccount(ini, QStringLiteral("beta"), QStringLiteral("Beta"));
  ini.setValue(QStringLiteral("general/startupAccount"), startup);
  ini.setValue(QStringLiteral("general/activeAccount"), active);
  ini.sync();
}

bool writeFile(const QString &path, const QByteArray &data) {
  const QFileInfo info(path);
  if (!QDir().mkpath(info.absolutePath())) {
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(data) == data.size();
}

}

class SettingsTest : public QObject {
  Q_OBJECT

 private slots:
  void freshInstallCreatesDefaultAccount();
  void startupOverridesActiveAtLaunch();
  void clearStartupPreservesActive();
  void staleStartupIsIgnoredAndNotRepaired();
  void deletingStartupClearsStartup();
  void deletingActiveUsesStartup();
  void lastAccountCannotBeRemoved();
  void accountsOrderActiveFirstAndFallbackToId();
  void invalidAccountOperationsFail();
  void connectionRequiresUrlAndKey();
  void saveConnectionNormalizesAndUsesHostName();
  void saveConnectionChangeClearsPinAndCache();
  void clearConnectionRemovesCredentialsAndCache();
  void configuredAndTrustedTrackConnectionState();
  void statusBarPreferencePersists();
  void cachePathsUseActiveAccount();
  void trustCertificateAcceptsMatchingPem();
  void trustCertificateRejectsInvalidInput();
  void fingerprintFromPemRejectsInvalidData();
  void normalizeUrl_data();
  void normalizeUrl();
  void normalizeFingerprint_data();
  void normalizeFingerprint();
};

void SettingsTest::freshInstallCreatesDefaultAccount() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());

  QCOMPARE(settings.accounts().size(), static_cast<qsizetype>(1));
  QCOMPARE(settings.activeAccountId(), QStringLiteral("default"));
  QCOMPARE(settings.startupAccountId(), QStringLiteral("default"));
  QCOMPARE(settings.startupAccountName(), QStringLiteral("Default"));
}

void SettingsTest::startupOverridesActiveAtLaunch() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  seedAccounts(dir.path(), QStringLiteral("beta"), QStringLiteral("alpha"));

  Settings settings(dir.path());

  QCOMPARE(settings.startupAccountId(), QStringLiteral("beta"));
  QCOMPARE(settings.activeAccountId(), QStringLiteral("beta"));
}

void SettingsTest::clearStartupPreservesActive() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  seedAccounts(dir.path(), QStringLiteral("beta"), QStringLiteral("alpha"));

  Settings settings(dir.path());
  QVERIFY(settings.switchAccount(QStringLiteral("alpha")));
  QVERIFY(settings.clearStartupAccount());

  QCOMPARE(settings.startupAccountId(), QString());
  QCOMPARE(settings.activeAccountId(), QStringLiteral("alpha"));

  Settings reopened(dir.path());
  QCOMPARE(reopened.startupAccountId(), QString());
  QCOMPARE(reopened.activeAccountId(), QStringLiteral("alpha"));
}

void SettingsTest::staleStartupIsIgnoredAndNotRepaired() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  seedAccounts(dir.path(), QStringLiteral("missing"), QStringLiteral("beta"));

  Settings settings(dir.path());

  QVERIFY(settings.startupAccountId().isEmpty());
  QCOMPARE(settings.activeAccountId(), QStringLiteral("beta"));

  QSettings ini(QDir(dir.path()).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
  QCOMPARE(ini.value(QStringLiteral("general/startupAccount")).toString(),
           QStringLiteral("missing"));
}

void SettingsTest::deletingStartupClearsStartup() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  seedAccounts(dir.path(), QStringLiteral("alpha"), QStringLiteral("beta"));

  Settings settings(dir.path());
  QVERIFY(settings.removeAccount(QStringLiteral("alpha")));

  QCOMPARE(settings.startupAccountId(), QString());
  QCOMPARE(settings.activeAccountId(), QStringLiteral("beta"));
  QCOMPARE(settings.accounts().size(), static_cast<qsizetype>(1));
}

void SettingsTest::deletingActiveUsesStartup() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  seedAccounts(dir.path(), QStringLiteral("alpha"), QStringLiteral("beta"));

  Settings settings(dir.path());
  QVERIFY(settings.switchAccount(QStringLiteral("beta")));
  QVERIFY(settings.removeAccount(QStringLiteral("beta")));

  QCOMPARE(settings.startupAccountId(), QStringLiteral("alpha"));
  QCOMPARE(settings.activeAccountId(), QStringLiteral("alpha"));
}

void SettingsTest::lastAccountCannotBeRemoved() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());

  QVERIFY(!settings.removeAccount(QStringLiteral("default")));
  QCOMPARE(settings.accounts().size(), static_cast<qsizetype>(1));
}

void SettingsTest::accountsOrderActiveFirstAndFallbackToId() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QSettings ini(QDir(dir.path()).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
  addAccount(ini, QStringLiteral("active-id"), QStringLiteral("Zed"));
  addAccount(ini, QStringLiteral("other-id"), QString());
  ini.setValue(QStringLiteral("general/startupAccount"), QStringLiteral("active-id"));
  ini.setValue(QStringLiteral("general/activeAccount"), QStringLiteral("other-id"));
  ini.sync();

  Settings settings(dir.path());
  const QVariantList accounts = settings.accounts();
  QCOMPARE(accounts.size(), static_cast<qsizetype>(2));

  const QVariantMap active = accounts.at(0).toMap();
  const QVariantMap other = accounts.at(1).toMap();
  QCOMPARE(active.value(QStringLiteral("id")).toString(), QStringLiteral("active-id"));
  QVERIFY(active.value(QStringLiteral("active")).toBool());
  QVERIFY(active.value(QStringLiteral("isStartup")).toBool());
  QCOMPARE(other.value(QStringLiteral("id")).toString(), QStringLiteral("other-id"));
  QCOMPARE(other.value(QStringLiteral("name")).toString(), QStringLiteral("other-id"));
  QVERIFY(other.value(QStringLiteral("configured")).toBool());
}

void SettingsTest::invalidAccountOperationsFail() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());

  QVERIFY(!settings.switchAccount(QStringLiteral("missing")));
  QVERIFY(!settings.setStartupAccount(QStringLiteral("missing")));
  QVERIFY(!settings.removeAccount(QStringLiteral("missing")));
  QCOMPARE(settings.activeAccountId(), QStringLiteral("default"));
  QCOMPARE(settings.accounts().size(), static_cast<qsizetype>(1));
}

void SettingsTest::connectionRequiresUrlAndKey() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());

  QVERIFY(!settings.saveConnection(QStringLiteral("http://example.com"),
                                   QStringLiteral("test-key")));
  QVERIFY(!settings.saveConnection(QStringLiteral("https://komga.example.com"), QStringLiteral(" ")));
  QVERIFY(!settings.configured());
}

void SettingsTest::saveConnectionNormalizesAndUsesHostName() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  QSignalSpy changed(&settings, &Settings::configurationChanged);

  QVERIFY(settings.saveConnection(QStringLiteral(" komga.example.com/path?x=1#frag "),
                                  QStringLiteral(" key "), QStringLiteral(" ")));

  QCOMPARE(settings.serverUrl(), QStringLiteral("https://komga.example.com/path"));
  QCOMPARE(settings.apiKey(), QStringLiteral("key"));
  QCOMPARE(settings.activeAccountName(), QStringLiteral("komga.example.com"));
  QVERIFY(settings.configured());
  QCOMPARE(changed.count(), 1);
}

void SettingsTest::saveConnectionChangeClearsPinAndCache() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  QVERIFY(settings.saveConnection(QStringLiteral("https://old.example.com"),
                                  QStringLiteral("test-key")));
  settings.setCertFingerprint(QStringLiteral("AA:BB"));
  settings.setCertPem(QStringLiteral("old-pem"));
  const QString cacheFile = QDir(settings.bookCacheRoot()).filePath(QStringLiteral("cached.bin"));
  QVERIFY(writeFile(cacheFile, QByteArrayLiteral("cached")));

  QVERIFY(settings.saveConnection(QStringLiteral("https://new.example.com"),
                                  QStringLiteral("test-key")));

  QCOMPARE(settings.serverUrl(), QStringLiteral("https://new.example.com"));
  QVERIFY(settings.certFingerprint().isEmpty());
  QVERIFY(settings.certPem().isEmpty());
  QVERIFY(!QFile::exists(cacheFile));
}

void SettingsTest::clearConnectionRemovesCredentialsAndCache() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  QVERIFY(settings.saveConnection(QStringLiteral("https://komga.example.com"),
                                  QStringLiteral("test-key")));
  settings.setCertFingerprint(QStringLiteral("aabb"));
  const QString cacheDirectory = settings.bookCacheRoot();
  const QString cacheFile = QDir(cacheDirectory).filePath(QStringLiteral("cached.bin"));
  QVERIFY(writeFile(cacheFile, QByteArrayLiteral("cached")));

  settings.clearConnection();

  QVERIFY(settings.serverUrl().isEmpty());
  QVERIFY(settings.apiKey().isEmpty());
  QVERIFY(settings.certFingerprint().isEmpty());
  QVERIFY(!settings.configured());
  QVERIFY(!settings.trusted());
  QVERIFY(!QFile::exists(cacheDirectory));
}

void SettingsTest::configuredAndTrustedTrackConnectionState() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  QVERIFY(!settings.configured());
  QVERIFY(!settings.trusted());

  settings.setServerUrl(QStringLiteral("https://komga.example.com"));
  QVERIFY(!settings.configured());
  settings.setApiKey(QStringLiteral("test-key"));
  QVERIFY(settings.configured());
  settings.setCertFingerprint(QStringLiteral("aabb"));
  QVERIFY(settings.trusted());

  settings.setApiKey(QString());
  QVERIFY(!settings.configured());
  QVERIFY(settings.trusted());
}

void SettingsTest::statusBarPreferencePersists() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  QVERIFY(settings.hideStatusBar());
  settings.setHideStatusBar(false);

  Settings reopened(dir.path());
  QVERIFY(!reopened.hideStatusBar());
}

void SettingsTest::cachePathsUseActiveAccount() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());

  QCOMPARE(settings.cacheRoot(), dir.path());
  QCOMPARE(QFileInfo(settings.settingsPath()).absolutePath(), QDir(dir.path()).absolutePath());
  QCOMPARE(settings.bookCacheRoot(),
           QDir(dir.path()).filePath(QStringLiteral("accounts/default/books")));
}

void SettingsTest::trustCertificateAcceptsMatchingPem() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  const QString expected = QStringLiteral(
      "df2594724829b2db744ffed8f8be2170110f88f4615c7668626023f1a1209575");
  const QString pem = QString::fromUtf8(kCertificatePem);

  QCOMPARE(settings.fingerprintFromPem(pem), expected);
  QVERIFY(settings.trustCertificate(
      QStringLiteral("DF:25:94:72:48:29:B2:DB:74:4F:FE:D8:F8:BE:21:70:11:0F:88:F4:61:5C:76:68:62:60:23:F1:A1:20:95:75"),
      pem));
  QVERIFY(settings.trusted());
  QCOMPARE(settings.certFingerprint(), expected);
  QCOMPARE(settings.certPem(), pem.trimmed());
}

void SettingsTest::trustCertificateRejectsInvalidInput() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  const QString pem = QString::fromUtf8(kCertificatePem);

  QVERIFY(!settings.trustCertificate(QStringLiteral("aabb"), pem));
  QVERIFY(!settings.trustCertificate(
      QStringLiteral("df2594724829b2db744ffed8f8be2170110f88f4615c7668626023f1a1209575"),
      QStringLiteral("not a certificate")));
  QVERIFY(!settings.trusted());
}

void SettingsTest::fingerprintFromPemRejectsInvalidData() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());

  QVERIFY(settings.fingerprintFromPem(QString()).isEmpty());
  QVERIFY(settings.fingerprintFromPem(QStringLiteral("not a certificate")).isEmpty());
}

void SettingsTest::normalizeUrl_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<QString>("expected");

  QTest::newRow("empty") << QString() << QString();
  QTest::newRow("missing-scheme") << QStringLiteral("komga.example.com")
                                  << QStringLiteral("https://komga.example.com");
  QTest::newRow("https-query") << QStringLiteral("https://komga.example.com/path/?x=1#frag")
                               << QStringLiteral("https://komga.example.com/path");
  QTest::newRow("https-root-slash") << QStringLiteral("https://komga.example.com/")
                                    << QStringLiteral("https://komga.example.com/");
  QTest::newRow("http-localhost") << QStringLiteral("http://localhost:8080")
                                  << QStringLiteral("http://localhost:8080");
  QTest::newRow("http-loopback") << QStringLiteral("http://127.0.0.1")
                                 << QStringLiteral("http://127.0.0.1");
  QTest::newRow("http-remote") << QStringLiteral("http://example.com") << QString();
  QTest::newRow("other-scheme") << QStringLiteral("ftp://example.com") << QString();
  QTest::newRow("no-host") << QStringLiteral("https://") << QString();
}

void SettingsTest::normalizeUrl() {
  QFETCH(QString, input);
  QFETCH(QString, expected);

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  Settings settings(dir.path());

  QCOMPARE(settings.normalizeUrl(input), expected);
}

void SettingsTest::normalizeFingerprint_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<QString>("expected");

  QTest::newRow("separators") << QStringLiteral("AA:BB-cc 11") << QStringLiteral("aabbcc11");
  QTest::newRow("non-hex-letters") << QStringLiteral("zz!!") << QStringLiteral("zz");
  QTest::newRow("separators-only") << QStringLiteral("---") << QString();
}

void SettingsTest::normalizeFingerprint() {
  QFETCH(QString, input);
  QFETCH(QString, expected);

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  Settings settings(dir.path());

  QCOMPARE(settings.normalizeFingerprint(input), expected);
}

QTEST_GUILESS_MAIN(SettingsTest)
#include "test_settings.moc"
