#include <QtTest>

#include <QHash>
#include <QList>
#include <QSignalSpy>

#include "models.h"

namespace {

ListItem makeItem(const QString &id, const QString &title, const QString &subtitle,
                  const QString &meta, int page, const QString &badge) {
  ListItem item;
  item.id = id;
  item.seriesId = QStringLiteral("series-1");
  item.title = title;
  item.subtitle = subtitle;
  item.meta = meta;
  item.badge = badge;
  item.page = page;
  item.pageCount = 10;
  item.downloaded = false;
  return item;
}

}

class ListModelTest : public QObject {
  Q_OBJECT

 private slots:
  void roleNamesAndData();
  void setItemsClearAndCountChanged();
  void markDownloadedUpdatesBadgeAndSignals();
  void markDownloadedFalseWithoutPageClearsBadge();
  void markSyncPendingKeepsSingleSuffix();
  void containsFilterMatchesAllTextCaseFolded();
  void prefixFilterMatchesTitleOnly();
  void blankAndUnchangedFilterDoNotEmit();
  void itemAtAndIndexOfIdBounds();
};

void ListModelTest::roleNamesAndData() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Title"),
                        QStringLiteral("Subtitle"), QStringLiteral("Meta"), 3,
                        QStringLiteral("READ")));
  model.setItems(items);

  const QHash<int, QByteArray> names = model.roleNames();
  QCOMPARE(names.value(ListModel::IdRole), QByteArrayLiteral("itemId"));
  QCOMPARE(names.value(ListModel::SeriesIdRole), QByteArrayLiteral("seriesId"));
  QCOMPARE(names.value(ListModel::TitleRole), QByteArrayLiteral("title"));
  QCOMPARE(names.value(ListModel::SubtitleRole), QByteArrayLiteral("subtitle"));
  QCOMPARE(names.value(ListModel::MetaRole), QByteArrayLiteral("meta"));
  QCOMPARE(names.value(ListModel::BadgeRole), QByteArrayLiteral("badge"));
  QCOMPARE(names.value(ListModel::PageRole), QByteArrayLiteral("page"));
  QCOMPARE(names.value(ListModel::PageCountRole), QByteArrayLiteral("pageCount"));
  QCOMPARE(names.value(ListModel::DownloadedRole), QByteArrayLiteral("downloaded"));

  const QModelIndex index = model.index(0);
  QCOMPARE(model.data(index, ListModel::IdRole).toString(), QStringLiteral("book-1"));
  QCOMPARE(model.data(index, ListModel::SeriesIdRole).toString(), QStringLiteral("series-1"));
  QCOMPARE(model.data(index, ListModel::TitleRole).toString(), QStringLiteral("Title"));
  QCOMPARE(model.data(index, ListModel::SubtitleRole).toString(), QStringLiteral("Subtitle"));
  QCOMPARE(model.data(index, ListModel::MetaRole).toString(), QStringLiteral("Meta"));
  QCOMPARE(model.data(index, ListModel::BadgeRole).toString(), QStringLiteral("READ"));
  QCOMPARE(model.data(index, ListModel::PageRole).toInt(), 3);
  QCOMPARE(model.data(index, ListModel::PageCountRole).toInt(), 10);
  QCOMPARE(model.data(index, ListModel::DownloadedRole).toBool(), false);
  QVERIFY(!model.data(index, Qt::DisplayRole).isValid());
  QVERIFY(!model.data(QModelIndex(), ListModel::IdRole).isValid());
}

void ListModelTest::setItemsClearAndCountChanged() {
  ListModel model;
  QSignalSpy countChanged(&model, &ListModel::countChanged);

  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("One"), QString(),
                        QString(), 0, QString()));
  items.append(makeItem(QStringLiteral("book-2"), QStringLiteral("Two"), QString(),
                        QString(), 0, QString()));
  model.setItems(items);

  QCOMPARE(model.rowCount(), 2);
  QCOMPARE(countChanged.count(), 1);

  model.clear();

  QCOMPARE(model.rowCount(), 0);
  QCOMPARE(countChanged.count(), 2);
}

void ListModelTest::markDownloadedUpdatesBadgeAndSignals() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Book"), QString(),
                        QStringLiteral("3 of 10 pages"), 3, QStringLiteral("READ")));
  model.setItems(items);
  QSignalSpy changed(&model, &ListModel::dataChanged);

  model.markDownloaded(QStringLiteral("book-1"), true);

  QCOMPARE(changed.count(), 1);
  QVERIFY(model.data(model.index(0), ListModel::DownloadedRole).toBool());
  QCOMPARE(model.data(model.index(0), ListModel::BadgeRole).toString(),
           QStringLiteral("OFFLINE"));
  const QList<int> roles = changed.at(0).at(2).value<QList<int>>();
  QVERIFY(roles.contains(ListModel::DownloadedRole));
  QVERIFY(roles.contains(ListModel::BadgeRole));

  model.markDownloaded(QStringLiteral("book-1"), false);

  QCOMPARE(changed.count(), 2);
  QVERIFY(!model.data(model.index(0), ListModel::DownloadedRole).toBool());
  QCOMPARE(model.data(model.index(0), ListModel::BadgeRole).toString(), QStringLiteral("READ"));
}

void ListModelTest::markDownloadedFalseWithoutPageClearsBadge() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Book"), QString(),
                        QString(), 0, QStringLiteral("OFFLINE")));
  model.setItems(items);

  model.markDownloaded(QStringLiteral("book-1"), false);

  QCOMPARE(model.data(model.index(0), ListModel::BadgeRole).toString(), QString());
}

void ListModelTest::markSyncPendingKeepsSingleSuffix() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Book"), QString(),
                        QStringLiteral("10 pages"), 3, QStringLiteral("OFFLINE")));
  model.setItems(items);

  model.markSyncPending(QStringLiteral("book-1"), true);
  model.markSyncPending(QStringLiteral("book-1"), true);

  QCOMPARE(model.data(model.index(0), ListModel::BadgeRole).toString(), QStringLiteral("SYNC"));
  QCOMPARE(model.data(model.index(0), ListModel::MetaRole).toString(),
           QStringLiteral("10 pages · sync pending"));

  model.markSyncPending(QStringLiteral("book-1"), false);

  QCOMPARE(model.data(model.index(0), ListModel::BadgeRole).toString(),
           QStringLiteral("OFFLINE"));
  QCOMPARE(model.data(model.index(0), ListModel::MetaRole).toString(),
           QStringLiteral("10 pages"));
}

void ListModelTest::containsFilterMatchesAllTextCaseFolded() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Alpha"), QStringLiteral("Guide"),
                        QStringLiteral("Fantasy"), 0, QString()));
  items.append(makeItem(QStringLiteral("book-2"), QStringLiteral("Beta"), QStringLiteral("HANDBOOK"),
                        QStringLiteral("Mystery"), 0, QString()));
  items.append(makeItem(QStringLiteral("book-3"), QStringLiteral("Gamma"), QStringLiteral("Notes"),
                        QStringLiteral("SCIENCE"), 0, QString()));
  model.setItems(items);

  model.setFilter(QStringLiteral("hand"));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.itemAt(0).id, QStringLiteral("book-2"));

  model.setFilter(QStringLiteral("science"));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.itemAt(0).id, QStringLiteral("book-3"));

  model.setFilter(QStringLiteral(" ALPHA "));
  QCOMPARE(model.filter(), QStringLiteral("ALPHA"));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.itemAt(0).id, QStringLiteral("book-1"));
}

void ListModelTest::prefixFilterMatchesTitleOnly() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Bravo"), QStringLiteral("Other"),
                        QStringLiteral("Other"), 0, QString()));
  items.append(makeItem(QStringLiteral("book-2"), QStringLiteral("Alpha"), QStringLiteral("Bravo"),
                        QStringLiteral("Bravo"), 0, QString()));
  model.setItems(items);

  model.setFilter(QStringLiteral("br"), true);

  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.itemAt(0).id, QStringLiteral("book-1"));
}

void ListModelTest::blankAndUnchangedFilterDoNotEmit() {
  ListModel model;
  QSignalSpy filterChanged(&model, &ListModel::filterChanged);
  QSignalSpy countChanged(&model, &ListModel::countChanged);

  model.setFilter(QStringLiteral("  "));
  QCOMPARE(filterChanged.count(), 0);
  QCOMPARE(countChanged.count(), 0);

  model.setFilter(QStringLiteral("alpha"));
  QCOMPARE(filterChanged.count(), 1);
  QCOMPARE(countChanged.count(), 1);

  model.setFilter(QStringLiteral(" alpha "));
  QCOMPARE(filterChanged.count(), 1);
  QCOMPARE(countChanged.count(), 1);
}

void ListModelTest::itemAtAndIndexOfIdBounds() {
  ListModel model;
  QVector<ListItem> items;
  items.append(makeItem(QStringLiteral("book-1"), QStringLiteral("Book"), QString(),
                        QString(), 0, QString()));
  model.setItems(items);

  QCOMPARE(model.itemAt(0).id, QStringLiteral("book-1"));
  QVERIFY(model.itemAt(-1).id.isEmpty());
  QVERIFY(model.itemAt(1).id.isEmpty());
  QCOMPARE(model.indexOfId(QStringLiteral("book-1")), 0);
  QCOMPARE(model.indexOfId(QStringLiteral("missing")), -1);
}

QTEST_GUILESS_MAIN(ListModelTest)
#include "test_list_model.moc"
