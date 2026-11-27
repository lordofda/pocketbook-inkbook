#include "models.h"

ListModel::ListModel(QObject *parent) : QAbstractListModel(parent) {}

int ListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : visible_.size();
}

QVariant ListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= visible_.size()) {
    return QVariant();
  }
  const ListItem &item = items_.at(visible_.at(index.row()));
  switch (role) {
    case IdRole:
      return item.id;
    case SeriesIdRole:
      return item.seriesId;
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
          {SeriesIdRole, "seriesId"},
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
  rebuildVisible();
  endResetModel();
  emit countChanged();
}

void ListModel::markDownloaded(const QString &id, bool downloaded) {
  bool changed = false;
  for (ListItem &item : items_) {
    if (item.id != id) {
      continue;
    }
    item.downloaded = downloaded;
    if (downloaded) {
      item.badge = QStringLiteral("OFFLINE");
    } else if (item.badge == QStringLiteral("OFFLINE")) {
      item.badge = item.page > 0 ? QStringLiteral("READ") : QString();
    }
    changed = true;
  }
  if (!changed) {
    return;
  }
  for (int row = 0; row < visible_.size(); ++row) {
    const int itemIndex = visible_.at(row);
    if (items_.at(itemIndex).id != id) {
      continue;
    }
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {DownloadedRole, BadgeRole});
  }
}

void ListModel::markSyncPending(const QString &id, bool pending) {
  const QString suffix = QStringLiteral(" · sync pending");
  bool changed = false;
  for (ListItem &item : items_) {
    if (item.id != id) {
      continue;
    }
    item.badge = pending ? QStringLiteral("SYNC") : QStringLiteral("OFFLINE");
    if (pending && !item.meta.endsWith(suffix)) {
      item.meta.append(suffix);
    } else if (!pending && item.meta.endsWith(suffix)) {
      item.meta.chop(suffix.size());
    }
    changed = true;
  }
  if (!changed) {
    return;
  }
  for (int row = 0; row < visible_.size(); ++row) {
    const int itemIndex = visible_.at(row);
    if (items_.at(itemIndex).id != id) {
      continue;
    }
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {MetaRole, BadgeRole});
  }
}

void ListModel::clear() {
  setItems({});
}

QString ListModel::filter() const {
  return filter_;
}

void ListModel::setFilter(const QString &filter) {
  setFilter(filter, false);
}

void ListModel::setFilter(const QString &filter, bool prefix) {
  const QString normalized = filter.trimmed();
  if (filter_ == normalized && prefixFilter_ == prefix) {
    return;
  }
  beginResetModel();
  filter_ = normalized;
  prefixFilter_ = prefix;
  rebuildVisible();
  endResetModel();
  emit filterChanged();
  emit countChanged();
}

void ListModel::rebuildVisible() {
  visible_.clear();
  visible_.reserve(items_.size());
  const QString needle = filter_.toCaseFolded();
  for (int i = 0; i < items_.size(); ++i) {
    const ListItem &item = items_.at(i);
    const bool matches = needle.isEmpty() ||
                         (prefixFilter_ ? item.title.toCaseFolded().startsWith(needle)
                                        : item.title.toCaseFolded().contains(needle) ||
                                              item.subtitle.toCaseFolded().contains(needle) ||
                                              item.meta.toCaseFolded().contains(needle));
    if (matches) {
      visible_.append(i);
    }
  }
}

ListItem ListModel::itemAt(int row) const {
  if (row < 0 || row >= visible_.size()) {
    return {};
  }
  return items_.at(visible_.at(row));
}

int ListModel::indexOfId(const QString &id) const {
  for (int i = 0; i < visible_.size(); ++i) {
    if (items_.at(visible_.at(i)).id == id) {
      return i;
    }
  }
  return -1;
}
