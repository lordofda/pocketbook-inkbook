#include <QTemporaryDir>
#include <QtTest>

#include "book_store.h"
#include "settings.h"

class BookStoreTest : public QObject {
  Q_OBJECT

 private slots:
  void seriesIdRoundTrip();
};

void BookStoreTest::seriesIdRoundTrip() {
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

QTEST_MAIN(BookStoreTest)
#include "test_settings.moc"
