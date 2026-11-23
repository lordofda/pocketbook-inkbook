#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "book_store.h"
#include "komga_api.h"
#include "models.h"
#include "settings.h"

struct PendingAction {
  QString name;
  QString id;
  int page = 0;
  bool manual = false;
};

class AppController : public QObject {
  Q_OBJECT

  Q_PROPERTY(ListModel *libraryModel READ libraryModel CONSTANT)
  Q_PROPERTY(ListModel *seriesModel READ seriesModel CONSTANT)
  Q_PROPERTY(ListModel *bookModel READ bookModel CONSTANT)
  Q_PROPERTY(ListModel *offlineModel READ offlineModel CONSTANT)
  Q_PROPERTY(QString view READ view NOTIFY stateChanged)
  Q_PROPERTY(QString status READ status NOTIFY stateChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(bool showTrustPrompt READ showTrustPrompt NOTIFY stateChanged)
  Q_PROPERTY(QString discoveredFingerprint READ discoveredFingerprint NOTIFY stateChanged)
  Q_PROPERTY(QString discoveredPem READ discoveredPem NOTIFY stateChanged)
  Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY stateChanged)
  Q_PROPERTY(QString selectedMeta READ selectedMeta NOTIFY stateChanged)
  Q_PROPERTY(QString currentBookId READ currentBookId NOTIFY stateChanged)
  Q_PROPERTY(QString pageSource READ pageSource NOTIFY stateChanged)
  Q_PROPERTY(int currentPage READ currentPage NOTIFY stateChanged)
  Q_PROPERTY(int pageCount READ pageCount NOTIFY stateChanged)
  Q_PROPERTY(int downloadedCount READ downloadedCount NOTIFY stateChanged)
  Q_PROPERTY(int downloadTotal READ downloadTotal NOTIFY stateChanged)
  Q_PROPERTY(bool currentBookDownloaded READ currentBookDownloaded NOTIFY stateChanged)
  Q_PROPERTY(bool grayscale READ grayscale NOTIFY stateChanged)

 public:
  explicit AppController(Settings *settings, KomgaApi *api, BookStore *store,
                         QObject *parent = nullptr);

  ListModel *libraryModel();
  ListModel *seriesModel();
  ListModel *bookModel();
  ListModel *offlineModel();
  QString view() const;
  QString status() const;
  bool busy() const;
  bool showTrustPrompt() const;
  QString discoveredFingerprint() const;
  QString discoveredPem() const;
  QString selectedTitle() const;
  QString selectedMeta() const;
  QString currentBookId() const;
  QString pageSource() const;
  int currentPage() const;
  int pageCount() const;
  int downloadedCount() const;
  int downloadTotal() const;
  bool currentBookDownloaded() const;
  bool grayscale() const;

  Q_INVOKABLE void configure(const QString &url, const QString &key, const QString &name);
  Q_INVOKABLE void trustDiscoveredCertificate();
  Q_INVOKABLE void cancelTrust();
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void openLibrary(int row);
  Q_INVOKABLE void openLibraryId(const QString &id);
  Q_INVOKABLE void openSeries(int row);
  Q_INVOKABLE void openSeriesId(const QString &id);
  Q_INVOKABLE void openBook(int row);
  Q_INVOKABLE void openBookId(const QString &id);
  Q_INVOKABLE void openInProgress();
  Q_INVOKABLE void openAccounts();
  Q_INVOKABLE void newAccount();
  Q_INVOKABLE void switchAccount(const QString &id);
  Q_INVOKABLE void editAccount(const QString &id);
  Q_INVOKABLE void deleteAccount(const QString &id);
  Q_INVOKABLE void openOffline();
  Q_INVOKABLE void openOfflineBookId(const QString &id);
  Q_INVOKABLE void deleteOfflineBook(const QString &id);
  Q_INVOKABLE void syncCurrentProgress();
  Q_INVOKABLE void toggleStatusBar();
  void applyStatusBar();
  Q_INVOKABLE void noteRowTap(const QString &id, int row);
  Q_INVOKABLE void back();
  Q_INVOKABLE void downloadCurrentBook();
  Q_INVOKABLE void nextPage();
  Q_INVOKABLE void previousPage();
  Q_INVOKABLE void setPage(int page);
  Q_INVOKABLE void deleteCurrentBook();
  Q_INVOKABLE void toggleGrayscale();
  Q_INVOKABLE void exit();

 signals:
  void stateChanged();

 private slots:
  void onLibraries(const QJsonArray &items, int requestId);
  void onSeries(const QJsonArray &items, int requestId);
  void onBooks(const QJsonArray &items, int requestId);
  void onBook(const QJsonObject &book, int requestId);
  void onPages(const QString &bookId, const QJsonArray &items, int requestId);
  void onProgressUpdated(const QString &bookId, bool ok, int requestId);
  void onCertificate(const QString &fingerprint, const QString &pem, int requestId);
  void onDownloadFinished(bool ok, const QString &message, int requestId);
  void onRequestFailed(const QString &message, int requestId);
  void onBusyChanged(bool busy);
  void syncPendingProgress();

 private:
  void setView(const QString &view);
  void setStatus(const QString &status);
  void resetForAccount();
  void finishConnection();
  void populateLibraries(const QJsonArray &items);
  void populateSeries(const QJsonArray &items);
  void populateBooks(const QJsonArray &items);
  void populateOfflineBooks();
  void refreshBooksModel();
  void beginProgressSync(bool manual);
  void updatePage(int page);
  void scheduleProgressSync();
  void startNextDownload();
  void clearDownloads();
  QVector<int> pagesFromArray(const QJsonArray &items) const;
  QString bookTitle(const QJsonObject &object) const;
  QString seriesTitle(const QJsonObject &object) const;
  QString bookMeta(const QJsonObject &object) const;
  QString seriesMeta(const QJsonObject &object) const;

  Settings *settings_;
  KomgaApi *api_;
  BookStore *store_;
  ListModel libraryModel_;
  ListModel seriesModel_;
  ListModel bookModel_;
  ListModel offlineModel_;
  QHash<int, PendingAction> pending_;
  QJsonArray lastLibraries_;
  QJsonArray lastSeries_;
  QJsonArray lastBooks_;
  QTimer syncTimer_;
  QString view_;
  QString status_;
  QString discoveredFingerprint_;
  QString discoveredPem_;
  QString selectedTitle_;
  QString selectedMeta_;
  QString currentLibraryId_;
  QString currentSeriesId_;
  QString currentBookId_;
  QString currentBookTitle_;
  QString pageSource_;
  QString downloadBookId_;
  QVector<int> downloadQueue_;
  int downloadQueueIndex_;
  int activeDownloadId_;
  int currentPage_;
  int pageCount_;
  int downloadedCount_;
  int downloadTotal_;
  bool busy_;
  bool showTrustPrompt_;
  bool inProgressBooks_;
  bool offlineBook_;
};
