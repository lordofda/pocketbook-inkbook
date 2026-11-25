#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QFile>
#include <QSslCertificate>
#include <QSslError>

#include "settings.h"

struct ApiRequest {
  int id = 0;
  QString purpose;
  QString bookId;
  QString destination;
  QString observedFingerprint;
  QString observedPem;
  QFile *file = nullptr;
};

class KomgaApi : public QObject {
  Q_OBJECT

 public:
  explicit KomgaApi(Settings *settings, QObject *parent = nullptr);

  Q_INVOKABLE int testConnection();
  Q_INVOKABLE int fetchLibraries();
  Q_INVOKABLE int fetchSeries(const QString &libraryId);
  Q_INVOKABLE int fetchBooks(const QString &seriesId);
  Q_INVOKABLE int fetchInProgressBooks();
  Q_INVOKABLE int fetchNewSeries();
  Q_INVOKABLE int fetchOnDeck();
  Q_INVOKABLE int fetchBook(const QString &bookId);
  Q_INVOKABLE int fetchPages(const QString &bookId);
  Q_INVOKABLE int updateProgress(const QString &bookId, int page, bool completed = false);
  Q_INVOKABLE int downloadPage(const QString &bookId, int page, const QString &destination);
  Q_INVOKABLE QString pageUrl(const QString &bookId, int page) const;
  Q_INVOKABLE void cancelRequest(int requestId);

 signals:
  void librariesReceived(int requestId, const QJsonArray &items);
  void seriesReceived(int requestId, const QJsonArray &items);
  void booksReceived(int requestId, const QJsonArray &items);
  void newSeriesReceived(int requestId, const QJsonArray &items);
  void onDeckReceived(int requestId, const QJsonArray &items);
  void bookReceived(int requestId, const QJsonObject &book);
  void pagesReceived(int requestId, const QString &bookId, const QJsonArray &items);
  void progressUpdated(int requestId, const QString &bookId, bool ok);
  void certificateDiscovered(int requestId, const QString &fingerprint, const QString &pem);
  void downloadProgress(int requestId, qint64 received, qint64 total);
  void downloadFinished(int requestId, bool ok, const QString &message);
  void requestFailed(int requestId, const QString &message);
  void busyChanged(bool busy);

 private slots:
  void onFinished();
  void onSslErrors(const QList<QSslError> &errors);
  void onReadyRead();
  void onDownloadProgress(qint64 received, qint64 total);

 private:
  QNetworkRequest makeRequest(const QString &path, bool json = false,
                              bool includeKey = true) const;
  int beginRequest(const QString &purpose, const QNetworkRequest &request,
                   const QByteArray &body = QByteArray(),
                   const QString &bookId = QString(), const QString &destination = QString());
  void cleanup(QNetworkReply *reply);
  QString endpoint(const QString &path) const;
  QString encodedSegment(const QString &value) const;
  QString certificateFingerprint(const QSslCertificate &certificate) const;
  bool fingerprintMatches(const QString &fingerprint) const;
  QString replyMessage(QNetworkReply *reply, const QByteArray &body) const;

  Settings *settings_;
  QNetworkAccessManager manager_;
  QHash<QNetworkReply *, ApiRequest> requests_;
  int nextId_;
};
