#include "book_store.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QUrl>
#include <QVector>

namespace {

constexpr int kColorStep = 17;

QString pageKey(int page) {
  return QString::number(page);
}

int boostChannel(int value, int luminance) {
  const int contrasted = qBound(0, qRound((value - 128) * 1.12 + 128), 255);
  return qBound(0, qRound(luminance + (contrasted - luminance) * 1.28), 255);
}

void applyColorBoost(QImage &image) {
  for (int y = 0; y < image.height(); ++y) {
    QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = line[x];
      const int red = qRed(pixel);
      const int green = qGreen(pixel);
      const int blue = qBlue(pixel);
      const int luminance = (77 * red + 150 * green + 29 * blue) / 256;
      line[x] = qRgba(boostChannel(red, luminance), boostChannel(green, luminance),
                      boostChannel(blue, luminance), qAlpha(pixel));
    }
  }
}

void applyColorDither(QImage &image) {
  const int width = image.width();
  const int height = image.height();
  QVector<double> current(width * 3, 0.0);
  QVector<double> next(width * 3, 0.0);
  for (int y = 0; y < height; ++y) {
    next.fill(0.0);
    QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const QRgb pixel = line[x];
      const int values[3] = {qRed(pixel), qGreen(pixel), qBlue(pixel)};
      int outputs[3] = {0, 0, 0};
      for (int channel = 0; channel < 3; ++channel) {
        const int index = x * 3 + channel;
        const int value = qBound(0, qRound(values[channel] + current[index]), 255);
        outputs[channel] =
            qBound(0, qRound(static_cast<double>(value) / kColorStep) * kColorStep, 255);
        const double error = value - outputs[channel];
        if (x + 1 < width) {
          current[index + 3] += error * 7.0 / 16.0;
        }
        if (y + 1 < height) {
          next[index] += error * 5.0 / 16.0;
          if (x > 0) {
            next[index - 3] += error * 3.0 / 16.0;
          }
          if (x + 1 < width) {
            next[index + 3] += error / 16.0;
          }
        }
      }
      line[x] = qRgba(outputs[0], outputs[1], outputs[2], qAlpha(pixel));
    }
    current.swap(next);
  }
}

}

BookStore::BookStore(Settings *settings, QObject *parent)
    : QObject(parent), settings_(settings) {}

QString BookStore::bookDirectory(const QString &bookId) const {
  if (bookId.isEmpty()) {
    return QString();
  }
  const QString key = QString::fromLatin1(
      QCryptographicHash::hash(bookId.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
  return QDir(settings_->bookCacheRoot()).filePath(QStringLiteral("books/%1").arg(key));
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
  manifest->id = object.value(QStringLiteral("id")).toString(bookId);
  manifest->title = object.value(QStringLiteral("title")).toString();
  manifest->pageCount = object.value(QStringLiteral("pageCount")).toInt();
  manifest->lastPage = object.value(QStringLiteral("lastPage")).toInt();
  manifest->pending = object.value(QStringLiteral("pending")).toBool();
  manifest->updatedAt = QDateTime::fromString(
      object.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);

  const QJsonObject files = object.value(QStringLiteral("files")).toObject();
  for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
    const QJsonObject page = it.value().toObject();
    const int number = it.key().toInt();
    manifest->fileNames.insert(number, page.value(QStringLiteral("file")).toString());
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
  object.insert(QStringLiteral("updatedAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
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
  manifest.pageCount = pages.size();
  manifest.updatedAt = QDateTime::currentDateTimeUtc();

  for (int i = 0; i < pages.size(); ++i) {
    const QJsonObject page = pages.at(i).toObject();
    const int number = page.value(QStringLiteral("number")).toInt(i + 1);
    manifest.mediaTypes.insert(number,
                               page.value(QStringLiteral("mediaType")).toString(QStringLiteral("image/jpeg")));
    if (!manifest.fileNames.contains(number)) {
      const QString extension = extensionFor(manifest.mediaTypes.value(number));
      manifest.fileNames.insert(number,
                                QStringLiteral("%1.%2").arg(number, 4, 10, QLatin1Char('0')).arg(extension));
    }
  }
  return saveManifest(manifest);
}

bool BookStore::storePage(const QString &bookId, int page, const QString &mediaType,
                          const QByteArray &data) {
  const QString directory = bookDirectory(bookId);
  if (directory.isEmpty() || !QDir().mkpath(directory)) {
    return false;
  }

  const QByteArray output = convertImage(data, mediaType);
  QString extension = extensionFor(mediaType);
  if (output != data && settings_->grayscale() && extension != QStringLiteral("png")) {
    extension = QStringLiteral("jpg");
  }
  if (output != data && !settings_->grayscale()) {
    extension = QStringLiteral("png");
  }
  const QString fileName =
      QStringLiteral("%1.%2").arg(page, 4, 10, QLatin1Char('0')).arg(extension);

  QSaveFile file(QDir(directory).filePath(fileName));
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(output);
  if (!file.commit()) {
    return false;
  }

  BookManifest manifest;
  if (!loadManifest(bookId, &manifest)) {
    manifest.id = bookId;
    manifest.pageCount = page;
  }
  manifest.fileNames.insert(page, fileName);
  manifest.mediaTypes.insert(page, mediaType);
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

QStringList BookStore::downloadedBookIds() const {
  QStringList result;
  const QString root = QDir(settings_->bookCacheRoot()).filePath(QStringLiteral("books"));
  QDirIterator directories(root, QDir::Dirs | QDir::NoDotAndDotDot);
  while (directories.hasNext()) {
    const QString directory = directories.next();
    QFile file(QDir(directory).filePath(QStringLiteral("manifest.json")));
    if (file.open(QIODevice::ReadOnly)) {
      const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
      const QString id = object.value(QStringLiteral("id")).toString();
      if (!id.isEmpty()) {
        result.append(id);
      }
    }
  }
  return result;
}

QVariantMap BookStore::cacheStats() const {
  qint64 bytes = 0;
  int books = 0;
  const QString root = QDir(settings_->bookCacheRoot()).filePath(QStringLiteral("books"));
  QDirIterator files(root, QDir::Files, QDirIterator::Subdirectories);
  while (files.hasNext()) {
    bytes += QFileInfo(files.next()).size();
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

QByteArray BookStore::convertImage(const QByteArray &data, const QString &mediaType) const {
  if (!settings_->grayscale() && !settings_->colorBoost()) {
    return data;
  }
  QImage image = QImage::fromData(data);
  if (image.isNull()) {
    return data;
  }
  QBuffer buffer;
  buffer.open(QIODevice::WriteOnly);
  if (settings_->grayscale()) {
    image = image.convertToFormat(QImage::Format_Grayscale8);
    const char *format = mediaType.toLower().contains(QStringLiteral("png")) ? "PNG" : "JPEG";
    if (!image.save(&buffer, format, 88)) {
      return data;
    }
  } else {
    image = image.convertToFormat(QImage::Format_ARGB32);
    applyColorBoost(image);
    applyColorDither(image);
    if (!image.save(&buffer, "PNG")) {
      return data;
    }
  }
  return buffer.data();
}
