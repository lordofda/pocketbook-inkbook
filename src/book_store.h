#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include "settings.h"

struct BookManifest {
  QString id;
  QString title;
  int pageCount = 0;
  int lastPage = 0;
  bool pending = false;
  bool color = false;
  QDateTime updatedAt;
  QHash<int, QString> fileNames;
  QHash<int, QString> mediaTypes;
};

class BookStore : public QObject {
  Q_OBJECT

 public:
  explicit BookStore(Settings *settings, QObject *parent = nullptr);

  Q_INVOKABLE bool isDownloaded(const QString &bookId) const;
  Q_INVOKABLE int downloadedCount(const QString &bookId) const;
  Q_INVOKABLE int pageCount(const QString &bookId) const;
  Q_INVOKABLE int localPage(const QString &bookId) const;
  Q_INVOKABLE bool hasPendingProgress(const QString &bookId) const;
  Q_INVOKABLE QString pageSource(const QString &bookId, int page) const;
  Q_INVOKABLE bool isColorCache(const QString &bookId) const;
  Q_INVOKABLE bool removeBook(const QString &bookId);
  Q_INVOKABLE QStringList downloadedBookIds() const;
  Q_INVOKABLE QVariantMap cacheStats() const;

  QString bookDirectory(const QString &bookId) const;
  QString pagePath(const QString &bookId, int page) const;
  bool loadManifest(const QString &bookId, BookManifest *manifest) const;
  bool saveManifest(const BookManifest &manifest);
  bool createManifest(const QString &bookId, const QString &title, const QJsonArray &pages);
  bool storePage(const QString &bookId, int page, const QString &mediaType, const QByteArray &data);
  void setProgress(const QString &bookId, int page, bool pending);
  void clearPending(const QString &bookId);

 signals:
  void storeChanged();

 private:
  QString manifestPath(const QString &bookId) const;
  QString extensionFor(const QString &mediaType) const;

  Settings *settings_;
};
