#include <QtTest>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "book_store.h"

namespace {

QJsonArray makePages(int count) {
  QJsonArray pages;
  for (int page = 1; page <= count; ++page) {
    QJsonObject entry;
    entry.insert(QStringLiteral("number"), page);
    entry.insert(QStringLiteral("mediaType"),
                 page % 2 == 0 ? QStringLiteral("image/png") : QStringLiteral("image/jpeg"));
    pages.append(entry);
  }
  return pages;
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

class BookStoreTest : public QObject {
  Q_OBJECT

 private slots:
  void createManifestStoresPagesSeriesAndColor();
  void storePageWritesFilesAndCompletesDownload();
  void mediaTypeExtensionMapping_data();
  void mediaTypeExtensionMapping();
  void loadManifestRejectsInvalidInputs();
  void progressStateClampsAndClears();
  void removeBookUpdatesIdsAndCacheStats();
  void legacyDirectoryFallback();
};

void BookStoreTest::createManifestStoresPagesSeriesAndColor() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  BookStore store(&settings);
  QSignalSpy changed(&store, &BookStore::storeChanged);

  QVERIFY(store.createManifest(QStringLiteral("book-1"), QStringLiteral("Book"),
                               makePages(2), QStringLiteral("series-1")));

  QCOMPARE(changed.count(), 1);
  BookManifest manifest;
  QVERIFY(store.loadManifest(QStringLiteral("book-1"), &manifest));
  QCOMPARE(manifest.id, QStringLiteral("book-1"));
  QCOMPARE(manifest.title, QStringLiteral("Book"));
  QCOMPARE(manifest.seriesId, QStringLiteral("series-1"));
  QCOMPARE(manifest.pageCount, 2);
  QVERIFY(manifest.updatedAt.isValid());
  QVERIFY(store.isColorCache(QStringLiteral("book-1")));
  QVERIFY(!store.hasPendingProgress(QStringLiteral("book-1")));
  QCOMPARE(manifest.fileNames.value(1), QStringLiteral("0001.jpg"));
  QCOMPARE(manifest.fileNames.value(2), QStringLiteral("0002.png"));
  QCOMPARE(manifest.mediaTypes.value(1), QStringLiteral("image/jpeg"));
  QCOMPARE(manifest.mediaTypes.value(2), QStringLiteral("image/png"));
}

void BookStoreTest::storePageWritesFilesAndCompletesDownload() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  BookStore store(&settings);
  QVERIFY(store.createManifest(QStringLiteral("book-1"), QStringLiteral("Book"), makePages(2)));

  QVERIFY(store.storePage(QStringLiteral("book-1"), 1, QStringLiteral("image/jpeg"),
                          QByteArrayLiteral("first")));
  QVERIFY(store.storePage(QStringLiteral("book-1"), 2, QStringLiteral("image/png"),
                          QByteArrayLiteral("second")));

  QCOMPARE(store.pageCount(QStringLiteral("book-1")), 2);
  QCOMPARE(store.downloadedCount(QStringLiteral("book-1")), 2);
  QVERIFY(store.isDownloaded(QStringLiteral("book-1")));

  const QString firstPath = store.pagePath(QStringLiteral("book-1"), 1);
  QVERIFY(!firstPath.isEmpty());
  QFile first(firstPath);
  QVERIFY(first.open(QIODevice::ReadOnly));
  QCOMPARE(first.readAll(), QByteArrayLiteral("first"));
  QVERIFY(store.pageSource(QStringLiteral("book-1"), 1).startsWith(QStringLiteral("file://")));
  QVERIFY(store.pageSource(QStringLiteral("book-1"), 3).isEmpty());
}

void BookStoreTest::mediaTypeExtensionMapping_data() {
  QTest::addColumn<QString>("mediaType");
  QTest::addColumn<QString>("expectedFile");

  QTest::newRow("png") << QStringLiteral("image/png") << QStringLiteral("0001.png");
  QTest::newRow("webp") << QStringLiteral("image/webp") << QStringLiteral("0001.webp");
  QTest::newRow("gif") << QStringLiteral("image/gif") << QStringLiteral("0001.gif");
  QTest::newRow("jpeg") << QStringLiteral("image/jpeg") << QStringLiteral("0001.jpg");
  QTest::newRow("unknown") << QStringLiteral("application/octet-stream")
                           << QStringLiteral("0001.jpg");
}

void BookStoreTest::mediaTypeExtensionMapping() {
  QFETCH(QString, mediaType);
  QFETCH(QString, expectedFile);

  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  BookStore store(&settings);

  QVERIFY(store.storePage(QStringLiteral("book-1"), 1, mediaType, QByteArrayLiteral("data")));

  BookManifest manifest;
  QVERIFY(store.loadManifest(QStringLiteral("book-1"), &manifest));
  QCOMPARE(manifest.fileNames.value(1), expectedFile);
  QCOMPARE(manifest.mediaTypes.value(1), mediaType);
}

void BookStoreTest::loadManifestRejectsInvalidInputs() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  BookStore store(&settings);
  const QString directory = store.bookDirectory(QStringLiteral("book-1"));
  const QString path = QDir(directory).filePath(QStringLiteral("manifest.json"));
  QVERIFY(QDir().mkpath(directory));
  BookManifest manifest;

  QVERIFY(writeFile(path, QByteArrayLiteral("{")));
  QVERIFY(!store.loadManifest(QStringLiteral("book-1"), &manifest));

  QJsonObject mismatched;
  mismatched.insert(QStringLiteral("id"), QStringLiteral("other-book"));
  mismatched.insert(QStringLiteral("pageCount"), 1);
  QVERIFY(writeFile(path, QJsonDocument(mismatched).toJson(QJsonDocument::Compact)));
  QVERIFY(!store.loadManifest(QStringLiteral("book-1"), &manifest));

  QJsonObject invalidPage;
  invalidPage.insert(QStringLiteral("file"), QStringLiteral("bad.txt"));
  invalidPage.insert(QStringLiteral("mediaType"), QStringLiteral("image/jpeg"));
  QJsonObject files;
  files.insert(QStringLiteral("1"), invalidPage);
  QJsonObject valid;
  valid.insert(QStringLiteral("id"), QStringLiteral("book-1"));
  valid.insert(QStringLiteral("pageCount"), 1);
  valid.insert(QStringLiteral("files"), files);
  QVERIFY(writeFile(path, QJsonDocument(valid).toJson(QJsonDocument::Compact)));
  QVERIFY(store.loadManifest(QStringLiteral("book-1"), &manifest));
  QVERIFY(manifest.fileNames.isEmpty());
}

void BookStoreTest::progressStateClampsAndClears() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  BookStore store(&settings);
  QVERIFY(store.createManifest(QStringLiteral("book-1"), QStringLiteral("Book"), makePages(2)));

  store.setProgress(QStringLiteral("book-1"), -4, true);
  QCOMPARE(store.localPage(QStringLiteral("book-1")), 0);
  QVERIFY(store.hasPendingProgress(QStringLiteral("book-1")));

  store.clearPending(QStringLiteral("book-1"));
  QVERIFY(!store.hasPendingProgress(QStringLiteral("book-1")));

  store.setProgress(QStringLiteral("book-1"), 7, true);
  QCOMPARE(store.localPage(QStringLiteral("book-1")), 7);
  QVERIFY(store.hasPendingProgress(QStringLiteral("book-1")));
}

void BookStoreTest::removeBookUpdatesIdsAndCacheStats() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  BookStore store(&settings);
  QVERIFY(store.createManifest(QStringLiteral("downloaded"), QStringLiteral("Full"), makePages(1)));
  QVERIFY(store.storePage(QStringLiteral("downloaded"), 1, QStringLiteral("image/jpeg"),
                          QByteArrayLiteral("data")));
  QVERIFY(store.createManifest(QStringLiteral("partial"), QStringLiteral("Partial"), makePages(2)));

  const QStringList initialIds = store.downloadedBookIds();
  QCOMPARE(initialIds.size(), 2);
  QVERIFY(initialIds.contains(QStringLiteral("downloaded")));
  QVERIFY(initialIds.contains(QStringLiteral("partial")));

  const QVariantMap initialStats = store.cacheStats();
  QCOMPARE(initialStats.value(QStringLiteral("books")).toInt(), 1);
  QVERIFY(initialStats.value(QStringLiteral("bytes")).toLongLong() > 0);

  QSignalSpy changed(&store, &BookStore::storeChanged);
  QVERIFY(store.removeBook(QStringLiteral("downloaded")));
  QCOMPARE(changed.count(), 1);

  const QStringList remainingIds = store.downloadedBookIds();
  QCOMPARE(remainingIds.size(), 1);
  QVERIFY(remainingIds.contains(QStringLiteral("partial")));
  QCOMPARE(store.cacheStats().value(QStringLiteral("books")).toInt(), 0);
  QVERIFY(!store.isDownloaded(QStringLiteral("downloaded")));
}

void BookStoreTest::legacyDirectoryFallback() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  Settings settings(dir.path());
  const QString bookId = QStringLiteral("legacy-book");
  const QString key = QString::fromLatin1(
      QCryptographicHash::hash(bookId.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
  const QString legacyDirectory =
      QDir(settings.bookCacheRoot()).filePath(QStringLiteral("books/%1").arg(key));
  QVERIFY(QDir().mkpath(legacyDirectory));

  QJsonObject page;
  page.insert(QStringLiteral("file"), QStringLiteral("0001.jpg"));
  page.insert(QStringLiteral("mediaType"), QStringLiteral("image/jpeg"));
  QJsonObject files;
  files.insert(QStringLiteral("1"), page);
  QJsonObject manifestJson;
  manifestJson.insert(QStringLiteral("id"), bookId);
  manifestJson.insert(QStringLiteral("title"), QStringLiteral("Legacy Book"));
  manifestJson.insert(QStringLiteral("pageCount"), 1);
  manifestJson.insert(QStringLiteral("files"), files);
  QVERIFY(writeFile(QDir(legacyDirectory).filePath(QStringLiteral("manifest.json")),
                    QJsonDocument(manifestJson).toJson(QJsonDocument::Compact)));

  BookStore store(&settings);
  QCOMPARE(store.bookDirectory(bookId), legacyDirectory);

  BookManifest manifest;
  QVERIFY(store.loadManifest(bookId, &manifest));
  QCOMPARE(manifest.title, QStringLiteral("Legacy Book"));
  QVERIFY(store.downloadedBookIds().contains(bookId));
}

QTEST_GUILESS_MAIN(BookStoreTest)
#include "test_book_store.moc"
