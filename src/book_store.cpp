#include "book_store.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

namespace {

QString pageKey(int page) {
  return QString::number(page);
}

QString directoryKey(const QString &bookId) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bookId.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
}

bool validPageFileName(const QString &name) {
  static const QRegularExpression pattern(QStringLiteral("^[0-9]{4,}\\.(jpg|jpeg|png|webp|gif)$"),
                                          QRegularExpression::CaseInsensitiveOption);
  return pattern.match(name).hasMatch();
}

}

BookStore::BookStore(Settings *settings, QObject *parent)
    : QObject(parent), settings_(settings) {}

QString BookStore::bookDirectory(const QString &bookId) const {
  if (bookId.isEmpty()) {
    return QString();
  }
  const QString root = settings_->bookCacheRoot();
  const QString canonical = QDir(root).filePath(directoryKey(bookId));
  if (QFileInfo::exists(canonical)) {
    return canonical;
  }
  const QString legacy = QDir(root).filePath(QStringLiteral("books/%1").arg(directoryKey(bookId)));
  if (QFileInfo::exists(legacy)) {
    return legacy;
  }
  return canonical;
}

QString BookStore::manifestPath(const QString &bookId) const {
  const QString directory = bookDirectory(bookId);
  return directory.isEmpty() ? QString() : directory + QStringLiteral("/manifest.json");
}

QString BookStore::pagePath(const QString &bookId, int page) const {
  BookManifest manifest;
  if (!loadManifest(bookId, &manifest)) {
    return QString();
  }
  const QString fileName = manifest.fileNames.value(page);
  if (fileName.isEmpty()) {
    return QString();
  }
  return QDir(bookDirectory(bookId)).filePath(fileName);
}

bool BookStore::loadManifest(const QString &bookId, BookManifest *manifest) const {
  if (manifest == nullptr) {
    return false;
  }
  QFile file(manifestPath(bookId));
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    return false;
  }
  const QJsonObject object = document.object();
  const QString storedId = object.value(QStringLiteral("id")).toString();
  if (!storedId.isEmpty() && storedId != bookId) {
    return false;
  }
  manifest->id = bookId;
  manifest->title = object.value(QStringLiteral("title")).toString();
  manifest->pageCount = object.value(QStringLiteral("pageCount")).toInt();
  manifest->lastPage = object.value(QStringLiteral("lastPage")).toInt();
  manifest->pending = object.value(QStringLiteral("pending")).toBool();
  manifest->color = object.value(QStringLiteral("color")).toBool(false);
  manifest->updatedAt = QDateTime::fromString(
      object.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);

  const QJsonObject files = object.value(QStringLiteral("files")).toObject();
  for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
    const QJsonObject page = it.value().toObject();
    const int number = it.key().toInt();
    const QString fileName = page.value(QStringLiteral("file")).toString();
    if (number < 1 || !validPageFileName(fileName)) {
      continue;
    }
    manifest->fileNames.insert(number, fileName);
    manifest->mediaTypes.insert(number, page.value(QStringLiteral("mediaType")).toString());
  }
  return true;
}

bool BookStore::saveManifest(const BookManifest &manifest) {
  const QString directory = bookDirectory(manifest.id);
  if (directory.isEmpty() || !QDir().mkpath(directory)) {
    return false;
  }

  QJsonObject files;
  const QList<int> pages = manifest.fileNames.keys();
  for (int page : pages) {
    QJsonObject entry;
    entry.insert(QStringLiteral("file"), manifest.fileNames.value(page));
    entry.insert(QStringLiteral("mediaType"), manifest.mediaTypes.value(page));
    files.insert(pageKey(page), entry);
  }

  QJsonObject object;
  object.insert(QStringLiteral("id"), manifest.id);
  object.insert(QStringLiteral("title"), manifest.title);
  object.insert(QStringLiteral("pageCount"), manifest.pageCount);
  object.insert(QStringLiteral("lastPage"), manifest.lastPage);
  object.insert(QStringLiteral("pending"), manifest.pending);
  object.insert(QStringLiteral("color"), manifest.color);
  object.insert(QStringLiteral("updatedAt"),
                (manifest.updatedAt.isValid() ? manifest.updatedAt
                                              : QDateTime::currentDateTimeUtc())
                    .toString(Qt::ISODate));
  object.insert(QStringLiteral("files"), files);

  QSaveFile file(manifestPath(manifest.id));
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
  if (!file.commit()) {
    return false;
  }
  emit storeChanged();
  return true;
}

bool BookStore::createManifest(const QString &bookId, const QString &title,
                               const QJsonArray &pages) {
  BookManifest manifest;
  loadManifest(bookId, &manifest);
  manifest.id = bookId;
  manifest.title = title;
  manifest.pageCount = 0;
  manifest.color = true;
  manifest.updatedAt = QDateTime::currentDateTimeUtc();

  QSet<int> currentPages;
  for (int i = 0; i < pages.size(); ++i) {
    const QJsonObject page = pages.at(i).toObject();
    const int number = page.value(QStringLiteral("number")).toInt(i + 1);
    if (number < 1) {
      continue;
    }
    currentPages.insert(number);
    manifest.mediaTypes.insert(number,
                               page.value(QStringLiteral("mediaType")).toString(QStringLiteral("image/jpeg")));
    if (!manifest.fileNames.contains(number)) {
      const QString extension = extensionFor(manifest.mediaTypes.value(number));
      manifest.fileNames.insert(number,
                                QStringLiteral("%1.%2").arg(number, 4, 10, QLatin1Char('0')).arg(extension));
    }
  }
  for (int page : manifest.fileNames.keys()) {
    if (!currentPages.contains(page)) {
      manifest.fileNames.remove(page);
      manifest.mediaTypes.remove(page);
    }
  }
  manifest.pageCount = currentPages.size();
  return saveManifest(manifest);
}

bool BookStore::storePage(const QString &bookId, int page, const QString &mediaType,
                          const QByteArray &data) {
  const QString directory = bookDirectory(bookId);
  if (directory.isEmpty() || !QDir().mkpath(directory)) {
    return false;
  }

  const QString extension = extensionFor(mediaType);
  const QString fileName =
      QStringLiteral("%1.%2").arg(page, 4, 10, QLatin1Char('0')).arg(extension);

  QSaveFile file(QDir(directory).filePath(fileName));
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(data);
  if (!file.commit()) {
    return false;
  }

  BookManifest manifest;
  if (!loadManifest(bookId, &manifest) || manifest.id != bookId) {
    manifest = BookManifest();
    manifest.id = bookId;
    manifest.pageCount = page;
  }
  manifest.fileNames.insert(page, fileName);
  manifest.mediaTypes.insert(page, mediaType);
  manifest.color = true;
  return saveManifest(manifest);
}

bool BookStore::isDownloaded(const QString &bookId) const {
  BookManifest manifest;
  return loadManifest(bookId, &manifest) && manifest.pageCount > 0 &&
         downloadedCount(bookId) >= manifest.pageCount;
}

int BookStore::downloadedCount(const QString &bookId) const {
  BookManifest manifest;
  if (!loadManifest(bookId, &manifest)) {
    return 0;
  }
  int count = 0;
  const QList<int> pages = manifest.fileNames.keys();
  for (int page : pages) {
    if (QFile::exists(QDir(bookDirectory(bookId)).filePath(manifest.fileNames.value(page)))) {
      ++count;
    }
  }
  return count;
}

int BookStore::pageCount(const QString &bookId) const {
  BookManifest manifest;
  return loadManifest(bookId, &manifest) ? manifest.pageCount : 0;
}

int BookStore::localPage(const QString &bookId) const {
  BookManifest manifest;
  return loadManifest(bookId, &manifest) ? manifest.lastPage : 0;
}

bool BookStore::hasPendingProgress(const QString &bookId) const {
  BookManifest manifest;
  return loadManifest(bookId, &manifest) && manifest.pending;
}

QString BookStore::pageSource(const QString &bookId, int page) const {
  const QString path = pagePath(bookId, page);
  if (path.isEmpty() || !QFile::exists(path)) {
    return QString();
  }
  return QUrl::fromLocalFile(path).toString();
}

bool BookStore::isColorCache(const QString &bookId) const {
  BookManifest manifest;
  return loadManifest(bookId, &manifest) && manifest.color;
}

bool BookStore::removeBook(const QString &bookId) {
  const QString directory = bookDirectory(bookId);
  if (directory.isEmpty()) {
    return false;
  }
  const bool removed = QDir(directory).removeRecursively();
  if (removed) {
    emit storeChanged();
  }
  return removed;
}

QStringList BookStore::cacheRoots() const {
  const QString root = settings_->bookCacheRoot();
  const QString legacy = QDir(root).filePath(QStringLiteral("books"));
  return QFileInfo::exists(legacy) ? QStringList{legacy, root} : QStringList{root};
}

QStringList BookStore::downloadedBookIds() const {
  QStringList result;
  for (const QString &root : cacheRoots()) {
    QDirIterator directories(root, QDir::Dirs | QDir::NoDotAndDotDot);
    while (directories.hasNext()) {
      const QString directory = directories.next();
      QFile file(QDir(directory).filePath(QStringLiteral("manifest.json")));
      if (!file.open(QIODevice::ReadOnly)) {
        continue;
      }
      const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
      const QString id = object.value(QStringLiteral("id")).toString();
      if (!id.isEmpty() && directoryKey(id) == QFileInfo(directory).fileName() &&
          !result.contains(id)) {
        result.append(id);
      }
    }
  }
  return result;
}

QVariantMap BookStore::cacheStats() const {
  qint64 bytes = 0;
  int books = 0;
  for (const QString &root : cacheRoots()) {
    QDirIterator files(root, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
      bytes += QFileInfo(files.next()).size();
    }
  }
  for (const QString &id : downloadedBookIds()) {
    if (isDownloaded(id)) {
      ++books;
    }
  }
  return {{QStringLiteral("bytes"), bytes}, {QStringLiteral("books"), books}};
}

void BookStore::setProgress(const QString &bookId, int page, bool pending) {
  BookManifest manifest;
  if (!loadManifest(bookId, &manifest)) {
    return;
  }
  manifest.lastPage = qMax(0, page);
  manifest.pending = pending;
  manifest.updatedAt = QDateTime::currentDateTimeUtc();
  saveManifest(manifest);
}

void BookStore::clearPending(const QString &bookId) {
  BookManifest manifest;
  if (loadManifest(bookId, &manifest) && manifest.pending) {
    manifest.pending = false;
    saveManifest(manifest);
  }
}

QString BookStore::extensionFor(const QString &mediaType) const {
  const QString type = mediaType.toLower();
  if (type.contains(QStringLiteral("png"))) {
    return QStringLiteral("png");
  }
  if (type.contains(QStringLiteral("webp"))) {
    return QStringLiteral("webp");
  }
  if (type.contains(QStringLiteral("gif"))) {
    return QStringLiteral("gif");
  }
  return QStringLiteral("jpg");
}
