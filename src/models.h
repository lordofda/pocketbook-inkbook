#pragma once

#include <QAbstractListModel>
#include <QVector>

struct ListItem {
  QString id;
  QString seriesId;
  QString title;
  QString subtitle;
  QString meta;
  QString badge;
  int page = 0;
  int pageCount = 0;
  bool downloaded = false;
};

struct BookInfo {
  QString id;
  QString seriesId;
  QString title;
  QString seriesTitle;
  int pageCount = 0;
  int serverPage = 0;
};

class ListModel : public QAbstractListModel {
  Q_OBJECT

  Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

 public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    SubtitleRole,
    MetaRole,
    BadgeRole,
    PageRole,
    PageCountRole,
    DownloadedRole,
    SeriesIdRole
  };

  explicit ListModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setItems(const QVector<ListItem> &items);
  void clear();
  QString filter() const;
  void setFilter(const QString &filter);
  ListItem itemAt(int row) const;
  int indexOfId(const QString &id) const;

 signals:
  void countChanged();
  void filterChanged();

 private:
  void rebuildVisible();

  QVector<ListItem> items_;
  QVector<int> visible_;
  QString filter_;
};
