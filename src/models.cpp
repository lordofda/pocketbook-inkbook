#include "models.h"

ListModel::ListModel(QObject *parent) : QAbstractListModel(parent) {}

int ListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : items_.size();
}

QVariant ListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
    return QVariant();
  }
  const ListItem &item = items_.at(index.row());
  switch (role) {
    case IdRole:
      return item.id;
    case TitleRole:
      return item.title;
    case SubtitleRole:
      return item.subtitle;
    case MetaRole:
      return item.meta;
    case BadgeRole:
      return item.badge;
    case PageRole:
      return item.page;
    case PageCountRole:
      return item.pageCount;
    case DownloadedRole:
      return item.downloaded;
    default:
      return QVariant();
  }
}

QHash<int, QByteArray> ListModel::roleNames() const {
  return {{IdRole, "itemId"},
          {TitleRole, "title"},
          {SubtitleRole, "subtitle"},
          {MetaRole, "meta"},
          {BadgeRole, "badge"},
          {PageRole, "page"},
          {PageCountRole, "pageCount"},
          {DownloadedRole, "downloaded"}};
}

void ListModel::setItems(const QVector<ListItem> &items) {
  beginResetModel();
  items_ = items;
  endResetModel();
  emit countChanged();
}

void ListModel::clear() {
  setItems({});
}

ListItem ListModel::itemAt(int row) const {
  if (row < 0 || row >= items_.size()) {
    return {};
  }
  return items_.at(row);
}

int ListModel::indexOfId(const QString &id) const {
  for (int i = 0; i < items_.size(); ++i) {
    if (items_.at(i).id == id) {
      return i;
    }
  }
  return -1;
}
