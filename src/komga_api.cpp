#include "komga_api.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSslSocket>
#include <QUrl>

namespace {

constexpr const char *kLibrariesPath = "/api/v1/libraries";

bool isArrayResponse(const QString &purpose) {
  return purpose == QStringLiteral("test") || purpose == QStringLiteral("libraries") ||
         purpose == QStringLiteral("series") || purpose == QStringLiteral("books") ||
          purpose == QStringLiteral("newSeries") || purpose == QStringLiteral("onDeck") ||
          purpose == QStringLiteral("pages");
}

bool isPinningCompatible(QSslError::SslError error) {
  switch (error) {
    case QSslError::SelfSignedCertificate:
    case QSslError::SelfSignedCertificateInChain:
    case QSslError::UnableToGetLocalIssuerCertificate:
    case QSslError::HostNameMismatch:
      return true;
    default:
      return false;
  }
}

bool isLoopbackHost(const QUrl &url) {
  const QString host = url.host().toLower();
  return host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") ||
         host == QStringLiteral("::1");
}

int effectivePort(const QUrl &url) {
  if (url.port() != -1) {
    return url.port();
  }
  return url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 ? 443 : 80;
}

}

KomgaApi::KomgaApi(Settings *settings, QObject *parent)
    : QObject(parent), settings_(settings), nextId_(1) {}

QNetworkRequest KomgaApi::makeRequest(const QString &path, bool json,
                                      bool includeKey) const {
  QNetworkRequest request(QUrl(endpoint(path), QUrl::TolerantMode));
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("komga-reader/1.0"));
  if (includeKey) {
    request.setRawHeader("X-API-Key", settings_->apiKey().toUtf8());
  }
  request.setRawHeader("Accept", "application/json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::SameOriginRedirectPolicy);
  if (json) {
    request.setRawHeader("Content-Type", "application/json");
  }
  if (request.url().scheme() == QStringLiteral("https")) {
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    request.setSslConfiguration(ssl);
  }
  return request;
}

QString KomgaApi::endpoint(const QString &path) const {
  QString base = settings_->serverUrl();
  while (base.endsWith(QLatin1Char('/'))) {
    base.chop(1);
  }
  return base + path;
}

QString KomgaApi::encodedSegment(const QString &value) const {
  return QString::fromUtf8(QUrl::toPercentEncoding(value, QByteArrayLiteral("-._~")));
}

int KomgaApi::beginRequest(const QString &purpose, const QNetworkRequest &request,
                           const QByteArray &body, const QString &bookId,
                           const QString &destination) {
  const int id = nextId_++;
  auto failNow = [this, purpose](int requestId, const QString &message) {
    QMetaObject::invokeMethod(
        this,
        [this, purpose, requestId, message]() {
          if (purpose == QStringLiteral("download")) {
            emit downloadFinished(requestId, false, message);
          } else {
            emit requestFailed(requestId, message);
          }
        },
        Qt::QueuedConnection);
  };

  const QString scheme = request.url().scheme();
  if (scheme != QStringLiteral("https") &&
      !(scheme == QStringLiteral("http") && isLoopbackHost(request.url()))) {
    failNow(id, QStringLiteral("Server URL must use HTTPS"));
    return id;
  }

  ApiRequest pending;
  pending.id = id;
  pending.purpose = purpose;
  pending.bookId = bookId;
  pending.destination = destination;

  if (!destination.isEmpty()) {
    const QFileInfo info(destination);
    if (!QDir().mkpath(info.absolutePath())) {
      failNow(id, QStringLiteral("Cannot create download directory"));
      return id;
    }
    pending.file = new QFile(destination + QStringLiteral(".part"));
    if (!pending.file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      const QString message = pending.file->errorString();
      delete pending.file;
      pending.file = nullptr;
      failNow(id, message);
      return id;
    }
  }

  QNetworkReply *reply = nullptr;
  if (purpose == QStringLiteral("series") || purpose == QStringLiteral("books")) {
    reply = manager_.post(request, body);
  } else if (purpose == QStringLiteral("progress")) {
    reply = manager_.sendCustomRequest(request, "PATCH", body);
  } else {
    reply = manager_.get(request);
  }

  requests_.insert(reply, pending);
  connect(reply, &QNetworkReply::finished, this, &KomgaApi::onFinished);
  connect(reply, &QNetworkReply::sslErrors, this, &KomgaApi::onSslErrors);
  if (pending.file != nullptr) {
    connect(reply, &QNetworkReply::readyRead, this, &KomgaApi::onReadyRead);
    connect(reply, &QNetworkReply::downloadProgress, this, &KomgaApi::onDownloadProgress);
  }
  emit busyChanged(true);
  return id;
}

int KomgaApi::testConnection() {
  return beginRequest(QStringLiteral("test"),
                      makeRequest(QStringLiteral("/"), false, false));
}

int KomgaApi::fetchLibraries() {
  return beginRequest(QStringLiteral("libraries"), makeRequest(QString::fromLatin1(kLibrariesPath)));
}

int KomgaApi::fetchSeries(const QString &libraryId) {
  QJsonObject equality;
  equality.insert(QStringLiteral("operator"), QStringLiteral("is"));
  equality.insert(QStringLiteral("value"), libraryId);
  QJsonObject condition;
  condition.insert(QStringLiteral("libraryId"), equality);
  QJsonObject body;
  body.insert(QStringLiteral("condition"), condition);
  return beginRequest(QStringLiteral("series"),
                      makeRequest(QStringLiteral("/api/v1/series/list?unpaged=true"), true),
                      QJsonDocument(body).toJson(QJsonDocument::Compact));
}

int KomgaApi::fetchBooks(const QString &seriesId) {
  QJsonObject equality;
  equality.insert(QStringLiteral("operator"), QStringLiteral("is"));
  equality.insert(QStringLiteral("value"), seriesId);
  QJsonObject condition;
  condition.insert(QStringLiteral("seriesId"), equality);
  QJsonObject body;
  body.insert(QStringLiteral("condition"), condition);
  return beginRequest(
      QStringLiteral("books"),
      makeRequest(QStringLiteral("/api/v1/books/list?unpaged=true&sort=metadata.numberSort%2Casc"),
                  true),
      QJsonDocument(body).toJson(QJsonDocument::Compact));
}

int KomgaApi::fetchNewSeries() {
  return beginRequest(QStringLiteral("newSeries"),
                      makeRequest(QStringLiteral("/api/v1/series/new?unpaged=true")));
}

int KomgaApi::fetchOnDeck() {
  return beginRequest(QStringLiteral("onDeck"),
                      makeRequest(QStringLiteral("/api/v1/books/ondeck")));
}

int KomgaApi::fetchBook(const QString &bookId) {
  const QString path = QStringLiteral("/api/v1/books/%1").arg(encodedSegment(bookId));
  return beginRequest(QStringLiteral("book"), makeRequest(path), QByteArray(), bookId);
}

int KomgaApi::fetchPages(const QString &bookId) {
  const QString path = QStringLiteral("/api/v1/books/%1/pages").arg(encodedSegment(bookId));
  return beginRequest(QStringLiteral("pages"), makeRequest(path), QByteArray(), bookId);
}

int KomgaApi::fetchInProgressBooks() {
  QJsonObject equality;
  equality.insert(QStringLiteral("operator"), QStringLiteral("is"));
  equality.insert(QStringLiteral("value"), QStringLiteral("IN_PROGRESS"));
  QJsonObject condition;
  condition.insert(QStringLiteral("readStatus"), equality);
  QJsonObject body;
  body.insert(QStringLiteral("condition"), condition);
  return beginRequest(QStringLiteral("books"),
                      makeRequest(QStringLiteral("/api/v1/books/list?unpaged=true"), true),
                      QJsonDocument(body).toJson(QJsonDocument::Compact));
}

int KomgaApi::updateProgress(const QString &bookId, int page, bool completed) {
  QJsonObject body;
  body.insert(QStringLiteral("page"), page);
  if (completed) {
    body.insert(QStringLiteral("completed"), true);
  }
  const QString path =
      QStringLiteral("/api/v1/books/%1/read-progress").arg(encodedSegment(bookId));
  return beginRequest(QStringLiteral("progress"), makeRequest(path, true),
                      QJsonDocument(body).toJson(QJsonDocument::Compact), bookId);
}

int KomgaApi::downloadPage(const QString &bookId, int page, const QString &destination) {
  if (page < 1 || destination.isEmpty()) {
    return -1;
  }
  const QString path = QStringLiteral("/api/v1/books/%1/pages/%2")
                           .arg(encodedSegment(bookId))
                           .arg(page);
  return beginRequest(QStringLiteral("download"), makeRequest(path), QByteArray(), bookId,
                      destination);
}

QString KomgaApi::pageUrl(const QString &bookId, int page) const {
  if (page < 1) {
    return QString();
  }
  return endpoint(QStringLiteral("/api/v1/books/%1/pages/%2")
                      .arg(encodedSegment(bookId))
                      .arg(page));
}

void KomgaApi::cancelRequest(int requestId) {
  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    if (it.value().id != requestId) {
      continue;
    }
    QNetworkReply *reply = it.key();
    const ApiRequest pending = it.value();
    requests_.erase(it);
    if (pending.file != nullptr) {
      pending.file->flush();
      pending.file->close();
      pending.file->remove();
      delete pending.file;
    }
    reply->abort();
    reply->deleteLater();
    emit busyChanged(!requests_.isEmpty());
    return;
  }
}

void KomgaApi::onSslErrors(const QList<QSslError> &errors) {
  auto *reply = qobject_cast<QNetworkReply *>(sender());
  if (reply == nullptr || !requests_.contains(reply)) {
    return;
  }
  for (const QSslError &error : errors) {
    if (!isPinningCompatible(error.error())) {
      return;
    }
  }

  const QSslCertificate certificate = reply->sslConfiguration().peerCertificate();
  const QString fingerprint = certificateFingerprint(certificate);
  if (fingerprint.isEmpty()) {
    return;
  }

  ApiRequest &pending = requests_[reply];
  pending.observedFingerprint = fingerprint;
  pending.observedPem = QString::fromLatin1(certificate.toPem());

  if (pending.purpose == QStringLiteral("test") || fingerprintMatches(fingerprint)) {
    reply->ignoreSslErrors(errors);
  }
}

void KomgaApi::onReadyRead() {
  auto *reply = qobject_cast<QNetworkReply *>(sender());
  if (reply == nullptr || !requests_.contains(reply)) {
    return;
  }
  QFile *file = requests_.value(reply).file;
  if (file != nullptr) {
    file->write(reply->readAll());
  }
}

void KomgaApi::onDownloadProgress(qint64 received, qint64 total) {
  auto *reply = qobject_cast<QNetworkReply *>(sender());
  if (reply == nullptr || !requests_.contains(reply)) {
    return;
  }
  emit downloadProgress(requests_.value(reply).id, received, total);
}

void KomgaApi::onFinished() {
  auto *reply = qobject_cast<QNetworkReply *>(sender());
  if (reply == nullptr) {
    return;
  }
  if (!requests_.contains(reply)) {
    reply->deleteLater();
    return;
  }

  const ApiRequest pending = requests_.take(reply);
  if (pending.file != nullptr) {
    pending.file->flush();
    pending.file->close();
  }

  auto discardFile = [&pending]() {
    if (pending.file != nullptr) {
      pending.file->remove();
      delete pending.file;
    }
  };

  auto fail = [&](const QString &message) {
    discardFile();
    if (pending.purpose == QStringLiteral("download")) {
      emit downloadFinished(pending.id, false, message);
    } else {
      emit requestFailed(pending.id, message);
    }
    cleanup(reply);
  };

  if (reply->error() != QNetworkReply::NoError) {
    fail(reply->errorString());
    return;
  }

  const QUrl configured(settings_->serverUrl(), QUrl::TolerantMode);
  if (reply->url().scheme() != configured.scheme() ||
      reply->url().host().compare(configured.host(), Qt::CaseInsensitive) != 0 ||
      effectivePort(reply->url()) != effectivePort(configured)) {
    fail(QStringLiteral("Server redirected to a different host"));
    return;
  }

  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QByteArray body = reply->readAll();

  if (reply->url().scheme() == QStringLiteral("https")) {
    const QSslCertificate certificate = reply->sslConfiguration().peerCertificate();
    QString fingerprint = certificate.isNull() ? pending.observedFingerprint
                                               : certificateFingerprint(certificate);
    QString pem = certificate.isNull() ? pending.observedPem
                                       : QString::fromLatin1(certificate.toPem());
    if (fingerprint.isEmpty()) {
      fail(QStringLiteral("Server TLS certificate is unavailable"));
      return;
    }
    if (pending.purpose == QStringLiteral("test")) {
      emit certificateDiscovered(pending.id, fingerprint, pem);
      cleanup(reply);
      return;
    }
    if (!fingerprintMatches(fingerprint)) {
      fail(QStringLiteral("Server TLS certificate does not match the trusted fingerprint"));
      return;
    }
  }

  if (status < 200 || status >= 300) {
    fail(replyMessage(reply, body));
    return;
  }

  if (pending.purpose == QStringLiteral("download")) {
    if (QFile::exists(pending.destination)) {
      QFile::remove(pending.destination);
    }
    if (pending.file == nullptr || !pending.file->rename(pending.destination)) {
      fail(QStringLiteral("Cannot store downloaded page"));
      return;
    }
    delete pending.file;
    emit downloadFinished(pending.id, true, QString());
    cleanup(reply);
    return;
  }

  if (pending.purpose == QStringLiteral("progress")) {
    emit progressUpdated(pending.id, pending.bookId, true);
    cleanup(reply);
    return;
  }

  if (pending.purpose == QStringLiteral("book")) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
      fail(QStringLiteral("Invalid book response"));
      return;
    }
    emit bookReceived(pending.id, document.object());
    cleanup(reply);
    return;
  }

  if (!isArrayResponse(pending.purpose)) {
    fail(QStringLiteral("Unknown API request"));
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    fail(QStringLiteral("Invalid server response: %1").arg(parseError.errorString()));
    return;
  }

  QJsonArray items;
  if (document.isArray()) {
    items = document.array();
  } else if (document.isObject()) {
    items = document.object().value(QStringLiteral("content")).toArray();
  } else {
    fail(QStringLiteral("Unexpected server response"));
    return;
  }

  if (pending.purpose == QStringLiteral("test") || pending.purpose == QStringLiteral("libraries")) {
    emit librariesReceived(pending.id, items);
  } else if (pending.purpose == QStringLiteral("series")) {
    emit seriesReceived(pending.id, items);
  } else if (pending.purpose == QStringLiteral("books")) {
    emit booksReceived(pending.id, items);
  } else if (pending.purpose == QStringLiteral("newSeries")) {
    emit newSeriesReceived(pending.id, items);
  } else if (pending.purpose == QStringLiteral("onDeck")) {
    emit onDeckReceived(pending.id, items);
  } else if (pending.purpose == QStringLiteral("pages")) {
    emit pagesReceived(pending.id, pending.bookId, items);
  }
  cleanup(reply);
}

QString KomgaApi::certificateFingerprint(const QSslCertificate &certificate) const {
  if (certificate.isNull()) {
    return QString();
  }
  return settings_->normalizeFingerprint(
      QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex()));
}

bool KomgaApi::fingerprintMatches(const QString &fingerprint) const {
  const QString trusted = settings_->normalizeFingerprint(settings_->certFingerprint());
  return !trusted.isEmpty() && settings_->normalizeFingerprint(fingerprint) == trusted;
}

QString KomgaApi::replyMessage(QNetworkReply *reply, const QByteArray &body) const {
  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QString message = QStringLiteral("HTTP %1").arg(status);
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
  if (document.isObject()) {
    const QJsonObject object = document.object();
    const QString detail = object.value(QStringLiteral("message")).toString(
        object.value(QStringLiteral("error")).toString());
    if (!detail.isEmpty()) {
      return message + QStringLiteral(": ") + detail;
    }
  }
  const QString text = QString::fromUtf8(body).trimmed();
  if (!text.isEmpty()) {
    message += QStringLiteral(": ") + text.left(240);
  }
  return message;
}

void KomgaApi::cleanup(QNetworkReply *reply) {
  if (reply != nullptr) {
    reply->deleteLater();
  }
  emit busyChanged(!requests_.isEmpty());
}
