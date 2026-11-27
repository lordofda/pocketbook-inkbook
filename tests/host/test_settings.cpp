#include <QtTest>

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>

#include "book_store.h"
#include "settings.h"

namespace {

void addAccount(QSettings &ini, const QString &id, const QString &name) {
  ini.beginGroup(QStringLiteral("accounts/") + id);
  ini.setValue(QStringLiteral("name"), name);
  ini.setValue(QStringLiteral("url"), QStringLiteral("https://komga.example.com"));
  ini.setValue(QStringLiteral("apiKey"), QStringLiteral("test-key"));
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
  void seriesIdRoundTrip();
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

void SettingsTest::seriesIdRoundTrip() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  Settings settings(directory.path());
  BookStore store(&settings);
  const QString bookId = QStringLiteral("book-1");
  const QString seriesId = QStringLiteral("series-1");

  QJsonArray pages;
  QJsonObject page;
  page.insert(QStringLiteral("number"), 1);
  page.insert(QStringLiteral("mediaType"), QStringLiteral("image/jpeg"));
  pages.append(page);

  QVERIFY(store.createManifest(bookId, QStringLiteral("Book"), pages, seriesId));

  BookManifest manifest;
  QVERIFY(store.loadManifest(bookId, &manifest));
  QCOMPARE(manifest.seriesId, seriesId);

  store.setProgress(bookId, 2, true);
  QVERIFY(store.loadManifest(bookId, &manifest));
  QCOMPARE(manifest.seriesId, seriesId);
  QVERIFY(manifest.pending);
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

QTEST_MAIN(SettingsTest)
#include "test_settings.moc"
