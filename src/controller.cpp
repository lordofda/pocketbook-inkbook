#include "controller.h"

#include "device.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

namespace {

qint64 readDateMs(const QJsonValue &value) {
  const QString text = value.toObject()
                           .value(QStringLiteral("readProgress"))
                           .toObject()
                           .value(QStringLiteral("readDate"))
                           .toString();
  return QDateTime::fromString(text, Qt::ISODate).toMSecsSinceEpoch();
}

}

AppController::AppController(Settings *settings, KomgaApi *api, BookStore *store, QObject *parent)
    : QObject(parent),
      settings_(settings),
      api_(api),
      store_(store),
      downloadQueueIndex_(0),
      activeDownloadId_(0),
      currentPage_(1),
      pageCount_(0),
      downloadedCount_(0),
      downloadTotal_(0),
      busy_(false),
      showTrustPrompt_(false),
      inProgressBooks_(false),
      offlineBook_(false) {
  connect(api_, &KomgaApi::librariesReceived, this,
          [this](int requestId, const QJsonArray &items) { onLibraries(items, requestId); });
  connect(api_, &KomgaApi::seriesReceived, this,
          [this](int requestId, const QJsonArray &items) { onSeries(items, requestId); });
  connect(api_, &KomgaApi::booksReceived, this,
          [this](int requestId, const QJsonArray &items) { onBooks(items, requestId); });
  connect(api_, &KomgaApi::bookReceived, this,
          [this](int requestId, const QJsonObject &book) { onBook(book, requestId); });
  connect(api_, &KomgaApi::pagesReceived, this,
          [this](int requestId, const QString &bookId, const QJsonArray &items) {
            onPages(bookId, items, requestId);
          });
  connect(api_, &KomgaApi::progressUpdated, this,
          [this](int requestId, const QString &bookId, bool ok) {
            onProgressUpdated(bookId, ok, requestId);
          });
  connect(api_, &KomgaApi::certificateDiscovered, this,
          [this](int requestId, const QString &fingerprint, const QString &pem) {
            onCertificate(fingerprint, pem, requestId);
          });
  connect(api_, &KomgaApi::downloadFinished, this,
          [this](int requestId, bool ok, const QString &message) {
            onDownloadFinished(ok, message, requestId);
          });
  connect(api_, &KomgaApi::requestFailed, this,
          [this](int requestId, const QString &message) { onRequestFailed(message, requestId); });
  connect(api_, &KomgaApi::busyChanged, this, &AppController::onBusyChanged);
  connect(api_, &KomgaApi::downloadProgress, this,
          [this](int requestId, qint64 received, qint64 total) {
            if (requestId == activeDownloadId_ && total > 0) {
              const int percent = static_cast<int>(received * 100 / total);
              setStatus(QStringLiteral("Downloading %1/%2 pages · %3%")
                            .arg(downloadQueueIndex_ + 1)
                            .arg(downloadQueue_.size())
                            .arg(percent));
            }
          });
  connect(settings_, &Settings::configurationChanged, this, &AppController::stateChanged);
  connect(store_, &BookStore::storeChanged, this, &AppController::stateChanged);
  connect(&syncTimer_, &QTimer::timeout, this, &AppController::syncPendingProgress);
  syncTimer_.setSingleShot(true);
  applyStatusBar();

  resetForAccount();
}

ListModel *AppController::libraryModel() {
  return &libraryModel_;
}

ListModel *AppController::seriesModel() {
  return &seriesModel_;
}

ListModel *AppController::bookModel() {
  return &bookModel_;
}

ListModel *AppController::offlineModel() {
  return &offlineModel_;
}

QString AppController::view() const {
  return view_;
}

QString AppController::status() const {
  return status_;
}

bool AppController::busy() const {
  return busy_;
}

bool AppController::showTrustPrompt() const {
  return showTrustPrompt_;
}

QString AppController::discoveredFingerprint() const {
  return discoveredFingerprint_;
}

QString AppController::discoveredPem() const {
  return discoveredPem_;
}

QString AppController::selectedTitle() const {
  return selectedTitle_;
}

QString AppController::selectedMeta() const {
  return selectedMeta_;
}

QString AppController::currentBookId() const {
  return currentBookId_;
}

QString AppController::pageSource() const {
  return pageSource_;
}

int AppController::currentPage() const {
  return currentPage_;
}

int AppController::pageCount() const {
  return pageCount_;
}

int AppController::downloadedCount() const {
  return downloadedCount_;
}

int AppController::downloadTotal() const {
  return downloadTotal_;
}

bool AppController::currentBookDownloaded() const {
  return store_->isDownloaded(currentBookId_);
}

bool AppController::grayscale() const {
  return settings_->grayscale();
}

void AppController::configure(const QString &url, const QString &key, const QString &name) {
  if (!settings_->saveConnection(url, key, name)) {
    setStatus(QStringLiteral("Enter a valid HTTPS URL and API key"));
    return;
  }
  pending_.clear();
  lastLibraries_ = QJsonArray();
  discoveredFingerprint_.clear();
  discoveredPem_.clear();
  showTrustPrompt_ = false;
  setStatus(QStringLiteral("Connecting to Komga..."));
  const int requestId = api_->testConnection();
  pending_.insert(requestId, {QStringLiteral("test"), QString(), 0});
  emit stateChanged();
}

void AppController::trustDiscoveredCertificate() {
  if (discoveredFingerprint_.isEmpty() ||
      !settings_->trustCertificate(discoveredFingerprint_, discoveredPem_)) {
    setStatus(QStringLiteral("Invalid certificate fingerprint"));
    return;
  }
  showTrustPrompt_ = false;
  if (!lastLibraries_.isEmpty()) {
    finishConnection();
    populateLibraries(lastLibraries_);
    lastLibraries_ = QJsonArray();
    return;
  }
  setStatus(QStringLiteral("Certificate trusted · loading libraries"));
  const int requestId = api_->fetchLibraries();
  pending_.insert(requestId, {QStringLiteral("libraries"), QString(), 0});
  emit stateChanged();
}

void AppController::cancelTrust() {
  showTrustPrompt_ = false;
  lastLibraries_ = QJsonArray();
  discoveredFingerprint_.clear();
  discoveredPem_.clear();
  setView(QStringLiteral("setup"));
  setStatus(QStringLiteral("Connection cancelled"));
}

void AppController::refresh() {
  if (!settings_->configured()) {
    setView(QStringLiteral("setup"));
    setStatus(QStringLiteral("Enter Komga server details"));
    return;
  }
  if (settings_->serverUrl().startsWith(QStringLiteral("https"), Qt::CaseInsensitive) &&
      !settings_->trusted()) {
    setView(QStringLiteral("setup"));
    setStatus(QStringLiteral("Trust the server certificate to continue"));
    return;
  }

  if (view_ == QStringLiteral("accounts")) {
    setStatus(QStringLiteral("Choose a Komga account"));
    return;
  }
  if (view_ == QStringLiteral("offline")) {
    populateOfflineBooks();
    setStatus(QStringLiteral("Offline books refreshed"));
    return;
  }

  int requestId = 0;
  if (view_ == QStringLiteral("inProgress")) {
    requestId = api_->fetchInProgressBooks();
    pending_.insert(requestId, {QStringLiteral("books"), QString(), 0});
  } else if (view_ == QStringLiteral("series") && !currentLibraryId_.isEmpty()) {
    requestId = api_->fetchSeries(currentLibraryId_);
    pending_.insert(requestId, {QStringLiteral("series"), currentLibraryId_, 0});
  } else if (view_ == QStringLiteral("books") && !currentSeriesId_.isEmpty()) {
    requestId = api_->fetchBooks(currentSeriesId_);
    pending_.insert(requestId, {QStringLiteral("books"), currentSeriesId_, 0});
  } else {
    requestId = api_->fetchLibraries();
    pending_.insert(requestId, {QStringLiteral("libraries"), QString(), 0});
  }
  setStatus(QStringLiteral("Loading..."));
}

void AppController::openLibrary(int row) {
  const ListItem item = libraryModel_.itemAt(row);
  if (item.id.isEmpty()) {
    return;
  }
  currentLibraryId_ = item.id;
  currentSeriesId_.clear();
  inProgressBooks_ = false;
  selectedTitle_ = item.title;
  selectedMeta_ = item.subtitle;
  seriesModel_.clear();
  setView(QStringLiteral("series"));
  const int requestId = api_->fetchSeries(item.id);
  pending_.insert(requestId, {QStringLiteral("series"), item.id, 0});
  setStatus(QStringLiteral("Loading series..."));
  emit stateChanged();
}

void AppController::openLibraryId(const QString &id) {
  const int row = libraryModel_.indexOfId(id);
  if (row >= 0) {
    openLibrary(row);
  }
}

void AppController::openSeries(int row) {
  const ListItem item = seriesModel_.itemAt(row);
  if (item.id.isEmpty()) {
    return;
  }
  currentSeriesId_ = item.id;
  inProgressBooks_ = false;
  selectedTitle_ = item.title;
  selectedMeta_ = item.subtitle;
  bookModel_.clear();
  setView(QStringLiteral("books"));
  const int requestId = api_->fetchBooks(item.id);
  pending_.insert(requestId, {QStringLiteral("books"), item.id, 0});
  setStatus(QStringLiteral("Loading books..."));
  emit stateChanged();
}

void AppController::openSeriesId(const QString &id) {
  const int row = seriesModel_.indexOfId(id);
  if (row >= 0) {
    openSeries(row);
  }
}

void AppController::openBook(int row) {
  const ListItem item = bookModel_.itemAt(row);
  if (item.id.isEmpty()) {
    return;
  }
  currentBookId_ = item.id;
  currentBookTitle_ = item.title;
  offlineBook_ = false;
  selectedTitle_ = item.title;
  selectedMeta_ = item.subtitle;
  pageCount_ = item.pageCount;
  downloadTotal_ = store_->pageCount(item.id);
  downloadedCount_ = store_->downloadedCount(item.id);
  const bool downloaded = store_->isDownloaded(item.id);
  const int storedPage = store_->localPage(item.id);
  const int serverPage = item.page > 0 ? item.page : 1;
  updatePage(downloaded && storedPage > 0 ? storedPage : serverPage);
  setView(QStringLiteral("reader"));
  setStatus(downloaded ? QStringLiteral("Ready to read")
                       : QStringLiteral("Download this book to read offline"));
  emit stateChanged();
}

void AppController::openBookId(const QString &id) {
  const int row = bookModel_.indexOfId(id);
  if (row >= 0) {
    openBook(row);
  }
}

void AppController::openInProgress() {
  bookModel_.clear();
  currentBookId_.clear();
  pageSource_.clear();
  inProgressBooks_ = true;
  selectedTitle_ = QStringLiteral("In progress");
  selectedMeta_.clear();
  setView(QStringLiteral("inProgress"));
  const int requestId = api_->fetchInProgressBooks();
  pending_.insert(requestId, {QStringLiteral("books"), QString(), 0});
  setStatus(QStringLiteral("Loading in-progress books..."));
  emit stateChanged();
}

void AppController::openAccounts() {
  selectedTitle_ = QStringLiteral("Accounts");
  selectedMeta_.clear();
  setView(QStringLiteral("accounts"));
  setStatus(QStringLiteral("Choose a Komga account"));
  emit stateChanged();
}

void AppController::newAccount() {
  if (settings_->createAccount(QStringLiteral("New account")).isEmpty()) {
    setStatus(QStringLiteral("Could not create account"));
    return;
  }
  resetForAccount();
  setStatus(QStringLiteral("Name and connect the new account"));
  emit stateChanged();
}

void AppController::switchAccount(const QString &id) {
  if (!settings_->switchAccount(id)) {
    setStatus(QStringLiteral("Could not switch account"));
    return;
  }
  resetForAccount();
  setStatus(QStringLiteral("Switched to %1").arg(settings_->activeAccountName()));
  emit stateChanged();
}

void AppController::editAccount(const QString &id) {
  if (!settings_->switchAccount(id)) {
    setStatus(QStringLiteral("Could not open account"));
    return;
  }
  resetForAccount();
  setStatus(QStringLiteral("Edit account details"));
  emit stateChanged();
}

void AppController::deleteAccount(const QString &id) {
  if (!settings_->removeAccount(id)) {
    setStatus(QStringLiteral("Cannot remove the last account"));
    return;
  }
  resetForAccount();
  setStatus(QStringLiteral("Account removed"));
  emit stateChanged();
}

void AppController::openOffline() {
  currentBookId_.clear();
  pageSource_.clear();
  selectedTitle_ = QStringLiteral("Offline books");
  selectedMeta_.clear();
  populateOfflineBooks();
  setView(QStringLiteral("offline"));
  emit stateChanged();
}

void AppController::openOfflineBookId(const QString &id) {
  BookManifest manifest;
  if (id.isEmpty() || !store_->loadManifest(id, &manifest)) {
    setStatus(QStringLiteral("Offline copy not found"));
    return;
  }
  currentBookId_ = manifest.id;
  currentBookTitle_ = manifest.title;
  offlineBook_ = true;
  selectedTitle_ = manifest.title;
  selectedMeta_ = QStringLiteral("Offline copy");
  pageCount_ = manifest.pageCount;
  downloadTotal_ = manifest.pageCount;
  downloadedCount_ = store_->downloadedCount(id);
  updatePage(manifest.lastPage > 0 ? manifest.lastPage : 1);
  setView(QStringLiteral("reader"));
  setStatus(QStringLiteral("Reading offline copy"));
  emit stateChanged();
}

void AppController::deleteOfflineBook(const QString &id) {
  if (id.isEmpty() || !store_->removeBook(id)) {
    setStatus(QStringLiteral("Could not remove offline copy"));
    return;
  }
  if (currentBookId_ == id) {
    currentBookId_.clear();
    pageSource_.clear();
    currentPage_ = 1;
    pageCount_ = 0;
  }
  refreshBooksModel();
  populateOfflineBooks();
  setStatus(QStringLiteral("Offline copy removed"));
  emit stateChanged();
}

void AppController::syncCurrentProgress() {
  if (currentBookId_.isEmpty()) {
    setStatus(QStringLiteral("Open a book before syncing progress"));
    return;
  }
  beginProgressSync(true);
}

void AppController::toggleStatusBar() {
  settings_->setHideStatusBar(!settings_->hideStatusBar());
  applyStatusBar();
  setStatus(settings_->hideStatusBar() ? QStringLiteral("System status bar hidden")
                                       : QStringLiteral("System status bar shown"));
  emit stateChanged();
}

void AppController::applyStatusBar() {
  setSystemStatusBarVisible(!settings_->hideStatusBar());
}

void AppController::noteRowTap(const QString &id, int row) {
  setStatus(QStringLiteral("Tap: id=%1 row=%2 view=%3")
                .arg(id, QString::number(row), view_));
}

void AppController::back() {
  if (view_ == QStringLiteral("reader")) {
    setView(offlineBook_ ? QStringLiteral("offline") : QStringLiteral("books"));
    pageSource_.clear();
  } else if (view_ == QStringLiteral("books")) {
    setView(inProgressBooks_ ? QStringLiteral("inProgress") : QStringLiteral("series"));
  } else if (view_ == QStringLiteral("inProgress")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("offline")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("accounts")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("series")) {
    setView(QStringLiteral("libraries"));
  }
  emit stateChanged();
}

void AppController::downloadCurrentBook() {
  if (currentBookId_.isEmpty()) {
    return;
  }
  clearDownloads();
  setStatus(QStringLiteral("Fetching page list..."));
  const int requestId = api_->fetchPages(currentBookId_);
  pending_.insert(requestId, {QStringLiteral("pages"), currentBookId_, 0});
  emit stateChanged();
}

void AppController::nextPage() {
  if (pageCount_ > 0) {
    updatePage(currentPage_ + 1);
  }
}

void AppController::previousPage() {
  if (pageCount_ > 0) {
    updatePage(currentPage_ - 1);
  }
}

void AppController::setPage(int page) {
  if (pageCount_ > 0) {
    updatePage(page);
  }
}

void AppController::deleteCurrentBook() {
  if (currentBookId_.isEmpty()) {
    return;
  }
  store_->removeBook(currentBookId_);
  pageSource_.clear();
  downloadedCount_ = 0;
  downloadTotal_ = 0;
  refreshBooksModel();
  populateOfflineBooks();
  setView(offlineBook_ ? QStringLiteral("offline") : QStringLiteral("books"));
  setStatus(QStringLiteral("Offline copy removed"));
  emit stateChanged();
}

void AppController::toggleGrayscale() {
  settings_->setGrayscale(!settings_->grayscale());
  setStatus(settings_->grayscale() ? QStringLiteral("Grayscale downloads enabled")
                                   : QStringLiteral("Color downloads enabled"));
  emit stateChanged();
}

void AppController::exit() {
  QCoreApplication::quit();
}

void AppController::onLibraries(const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (action.name == QStringLiteral("test")) {
    if (showTrustPrompt_) {
      lastLibraries_ = items;
      setStatus(QStringLiteral("Confirm the server certificate to continue"));
      return;
    }
  }
  finishConnection();
  populateLibraries(items);
}

void AppController::onSeries(const QJsonArray &items, int requestId) {
  pending_.remove(requestId);
  populateSeries(items);
}

void AppController::onBooks(const QJsonArray &items, int requestId) {
  pending_.remove(requestId);
  populateBooks(items);
}

void AppController::onBook(const QJsonObject &book, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (action.name != QStringLiteral("syncCheck") || action.id != currentBookId_) {
    return;
  }

  const QString bookId = action.id;
  const QJsonObject progress = book.value(QStringLiteral("readProgress")).toObject();
  const QJsonObject media = book.value(QStringLiteral("media")).toObject();
  const int total = media.value(QStringLiteral("pagesCount")).toInt(store_->pageCount(bookId));
  int serverPage = progress.value(QStringLiteral("page")).toInt();
  if (progress.value(QStringLiteral("completed")).toBool()) {
    serverPage = total;
  }
  const int localPage = action.page;

  if (serverPage > localPage) {
    store_->setProgress(bookId, serverPage, false);
    currentPage_ = total > 0 ? qBound(1, serverPage, total) : qMax(1, serverPage);
    pageSource_ = store_->pageSource(bookId, currentPage_);
    setStatus(QStringLiteral("Server is further ahead · moved to page %1").arg(currentPage_));
    emit stateChanged();
    return;
  }

  if (localPage > serverPage) {
    const int updateId = api_->updateProgress(bookId, localPage);
    pending_.insert(updateId, {QStringLiteral("sync"), bookId, localPage, action.manual});
    setStatus(QStringLiteral("Pushing local progress to server..."));
    emit stateChanged();
    return;
  }

  store_->clearPending(bookId);
  setStatus(QStringLiteral("Progress already in sync"));
  emit stateChanged();
}

void AppController::onPages(const QString &bookId, const QJsonArray &items, int requestId) {
  pending_.remove(requestId);
  if (bookId != currentBookId_ || items.isEmpty()) {
    setStatus(QStringLiteral("No pages found for this book"));
    return;
  }
  store_->createManifest(bookId, currentBookTitle_, items);
  downloadBookId_ = bookId;
  downloadQueue_ = pagesFromArray(items);
  downloadQueueIndex_ = 0;
  downloadTotal_ = downloadQueue_.size();
  downloadedCount_ = 0;
  startNextDownload();
  emit stateChanged();
}

void AppController::onProgressUpdated(const QString &bookId, bool ok, int requestId) {
  pending_.remove(requestId);
  if (!ok || bookId != currentBookId_) {
    return;
  }
  store_->clearPending(bookId);
  setStatus(QStringLiteral("Progress synced"));
}

void AppController::onCertificate(const QString &fingerprint, const QString &pem, int requestId) {
  const PendingAction action = pending_.value(requestId);
  if (action.name != QStringLiteral("test") && action.name != QStringLiteral("libraries")) {
    return;
  }
  discoveredFingerprint_ = fingerprint;
  discoveredPem_ = pem;
  if (!settings_->trusted() ||
      settings_->normalizeFingerprint(settings_->certFingerprint()) !=
          settings_->normalizeFingerprint(fingerprint)) {
    showTrustPrompt_ = true;
    setStatus(QStringLiteral("Verify the server certificate fingerprint"));
  } else {
    showTrustPrompt_ = false;
    setStatus(QStringLiteral("Certificate trusted"));
  }
  emit stateChanged();
}

void AppController::onDownloadFinished(bool ok, const QString &message, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (requestId != activeDownloadId_ || action.name != QStringLiteral("download")) {
    return;
  }
  if (!ok) {
    clearDownloads();
    setStatus(QStringLiteral("Download failed: %1").arg(message));
    emit stateChanged();
    return;
  }

  const QString rawPath = action.id;
  QFile raw(rawPath);
  QByteArray data;
  if (raw.open(QIODevice::ReadOnly)) {
    data = raw.readAll();
    raw.close();
  }
  raw.remove();
  const int page = action.page;
  BookManifest manifest;
  store_->loadManifest(downloadBookId_, &manifest);
  const QString mediaType =
      manifest.mediaTypes.value(page, QStringLiteral("image/jpeg"));
  if (data.isEmpty() || !store_->storePage(downloadBookId_, page, mediaType, data)) {
    clearDownloads();
    setStatus(QStringLiteral("Download failed: cannot store page %1").arg(page));
    emit stateChanged();
    return;
  }

  ++downloadedCount_;
  ++downloadQueueIndex_;
  if (currentBookId_ == downloadBookId_) {
    updatePage(currentPage_);
  }
  refreshBooksModel();
  startNextDownload();
  emit stateChanged();
}

void AppController::onRequestFailed(const QString &message, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (action.name == QStringLiteral("download")) {
    clearDownloads();
  }
  setStatus(QStringLiteral("%1: %2").arg(action.name.isEmpty() ? QStringLiteral("Request failed")
                                                                : action.name, message));
  if (action.name == QStringLiteral("libraries") && view_ == QStringLiteral("libraries") &&
      libraryModel_.rowCount() == 0) {
    populateOfflineBooks();
    setView(QStringLiteral("offline"));
    setStatus(QStringLiteral("Server unavailable · showing offline books"));
  }
  emit stateChanged();
}

void AppController::onBusyChanged(bool busy) {
  busy_ = busy;
  emit stateChanged();
}

void AppController::syncPendingProgress() {
  if (currentBookId_.isEmpty() || !store_->hasPendingProgress(currentBookId_)) {
    return;
  }
  beginProgressSync(false);
}

void AppController::beginProgressSync(bool manual) {
  if (currentBookId_.isEmpty()) {
    if (manual) {
      setStatus(QStringLiteral("Open a book before syncing progress"));
    }
    return;
  }
  if (!settings_->configured() ||
      (settings_->serverUrl().startsWith(QStringLiteral("https"), Qt::CaseInsensitive) &&
       !settings_->trusted())) {
    if (manual) {
      setStatus(QStringLiteral("Komga is not configured for sync"));
    }
    return;
  }

  const int page = store_->localPage(currentBookId_);
  const int requestId = api_->fetchBook(currentBookId_);
  pending_.insert(requestId, {QStringLiteral("syncCheck"), currentBookId_, page, manual});
  setStatus(QStringLiteral("Checking server progress..."));
  emit stateChanged();
}

void AppController::setView(const QString &view) {
  if (view_ == view) {
    return;
  }
  view_ = view;
  emit stateChanged();
}

void AppController::setStatus(const QString &status) {
  if (status_ == status) {
    return;
  }
  status_ = status;
  emit stateChanged();
}

void AppController::resetForAccount() {
  syncTimer_.stop();
  clearDownloads();
  pending_.clear();
  lastLibraries_ = QJsonArray();
  lastSeries_ = QJsonArray();
  lastBooks_ = QJsonArray();
  libraryModel_.clear();
  seriesModel_.clear();
  bookModel_.clear();
  offlineModel_.clear();
  currentLibraryId_.clear();
  currentSeriesId_.clear();
  currentBookId_.clear();
  currentBookTitle_.clear();
  pageSource_.clear();
  currentPage_ = 1;
  pageCount_ = 0;
  downloadTotal_ = 0;
  downloadedCount_ = 0;
  discoveredFingerprint_.clear();
  discoveredPem_.clear();
  showTrustPrompt_ = false;
  inProgressBooks_ = false;
  offlineBook_ = false;
  selectedTitle_.clear();
  selectedMeta_.clear();

  const bool needsTrust =
      settings_->serverUrl().startsWith(QStringLiteral("https"), Qt::CaseInsensitive) &&
      !settings_->trusted();
  if (settings_->configured() && !needsTrust) {
    setView(QStringLiteral("libraries"));
    setStatus(QStringLiteral("Connecting to Komga..."));
    QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
  } else {
    setView(QStringLiteral("setup"));
    setStatus(settings_->configured() ? QStringLiteral("Trust the server certificate to continue")
                                      : QStringLiteral("Enter Komga server details"));
  }
  emit stateChanged();
}

void AppController::finishConnection() {
  setView(QStringLiteral("libraries"));
  selectedTitle_.clear();
  selectedMeta_.clear();
  emit stateChanged();
}

void AppController::populateLibraries(const QJsonArray &items) {
  QVector<ListItem> rows;
  rows.reserve(items.size());
  for (const QJsonValue &value : items) {
    const QJsonObject object = value.toObject();
    ListItem item;
    item.id = object.value(QStringLiteral("id")).toString();
    item.title = object.value(QStringLiteral("name")).toString();
    item.subtitle = object.value(QStringLiteral("root")).toString();
    item.meta = QStringLiteral("Library");
    rows.append(item);
  }
  libraryModel_.setItems(rows);
  setStatus(QStringLiteral("Libraries loaded"));
  emit stateChanged();
}

void AppController::populateSeries(const QJsonArray &items) {
  lastSeries_ = items;
  QVector<ListItem> rows;
  rows.reserve(items.size());
  for (const QJsonValue &value : items) {
    const QJsonObject object = value.toObject();
    ListItem item;
    item.id = object.value(QStringLiteral("id")).toString();
    item.title = seriesTitle(object);
    item.subtitle = object.value(QStringLiteral("name")).toString();
    item.meta = seriesMeta(object);
    item.badge = object.value(QStringLiteral("booksUnreadCount")).toInt() > 0
                     ? QStringLiteral("UNREAD")
                     : QString();
    rows.append(item);
  }
  seriesModel_.setItems(rows);
  setStatus(QStringLiteral("Series loaded"));
  emit stateChanged();
}

void AppController::populateBooks(const QJsonArray &items) {
  lastBooks_ = items;
  if (inProgressBooks_) {
    QVector<QJsonValue> values;
    values.reserve(items.size());
    for (const QJsonValue &value : items) {
      values.append(value);
    }
    std::stable_sort(values.begin(), values.end(),
                     [](const QJsonValue &left, const QJsonValue &right) {
                       return readDateMs(left) > readDateMs(right);
                     });
    lastBooks_ = QJsonArray();
    for (const QJsonValue &value : values) {
      lastBooks_.append(value);
    }
  }
  refreshBooksModel();
  setStatus(QStringLiteral("Books loaded"));
  emit stateChanged();
}

void AppController::populateOfflineBooks() {
  QVector<ListItem> rows;
  const QStringList ids = store_->downloadedBookIds();
  rows.reserve(ids.size());
  for (const QString &id : ids) {
    BookManifest manifest;
    if (!store_->loadManifest(id, &manifest)) {
      continue;
    }
    ListItem item;
    item.id = id;
    item.title = manifest.title.isEmpty() ? id : manifest.title;
    item.subtitle = QStringLiteral("Page %1 of %2")
                        .arg(qMax(1, manifest.lastPage))
                        .arg(qMax(1, manifest.pageCount));
    item.meta = QStringLiteral("%1 of %2 pages cached%3")
                    .arg(store_->downloadedCount(id))
                    .arg(manifest.pageCount)
                    .arg(manifest.pending ? QStringLiteral(" · sync pending")
                                          : QString());
    item.badge = manifest.pending ? QStringLiteral("SYNC") : QStringLiteral("OFFLINE");
    item.downloaded = true;
    item.page = manifest.lastPage;
    item.pageCount = manifest.pageCount;
    rows.append(item);
  }
  std::sort(rows.begin(), rows.end(), [](const ListItem &left, const ListItem &right) {
    return QString::localeAwareCompare(left.title, right.title) < 0;
  });
  offlineModel_.setItems(rows);

  const QVariantMap stats = store_->cacheStats();
  const int megabytes =
      static_cast<int>(stats.value(QStringLiteral("bytes")).toLongLong() / (1024 * 1024));
  setStatus(QStringLiteral("%1 offline books · %2 MB cached").arg(rows.size()).arg(megabytes));
  emit stateChanged();
}

void AppController::refreshBooksModel() {
  QVector<ListItem> rows;
  rows.reserve(lastBooks_.size());
  for (const QJsonValue &value : lastBooks_) {
    const QJsonObject object = value.toObject();
    const QString id = object.value(QStringLiteral("id")).toString();
    ListItem item;
    item.id = id;
    item.title = bookTitle(object);
    item.subtitle = object.value(QStringLiteral("seriesTitle")).toString();
    item.meta = bookMeta(object);
    item.downloaded = store_->isDownloaded(id);
    item.pageCount = object.value(QStringLiteral("media"))
                         .toObject()
                         .value(QStringLiteral("pagesCount"))
                         .toInt();
    const QJsonObject progress = object.value(QStringLiteral("readProgress")).toObject();
    item.page = progress.value(QStringLiteral("page")).toInt();
    if (progress.value(QStringLiteral("completed")).toBool()) {
      item.page = item.pageCount;
    }
    if (item.downloaded) {
      item.badge = QStringLiteral("OFFLINE");
    } else if (item.page > 0) {
      item.badge = QStringLiteral("READ");
    }
    rows.append(item);
  }
  bookModel_.setItems(rows);
  emit stateChanged();
}

void AppController::updatePage(int page) {
  if (pageCount_ > 0) {
    page = qBound(1, page, pageCount_);
  } else {
    page = 1;
  }
  currentPage_ = page;
  pageSource_ = store_->pageSource(currentBookId_, page);
  if (!pageSource_.isEmpty()) {
    store_->setProgress(currentBookId_, page, true);
    scheduleProgressSync();
  }
  emit stateChanged();
}

void AppController::scheduleProgressSync() {
  syncTimer_.stop();
  syncTimer_.start(900);
}

void AppController::startNextDownload() {
  if (downloadQueueIndex_ >= downloadQueue_.size()) {
    activeDownloadId_ = 0;
    setStatus(QStringLiteral("Book downloaded · %1 pages").arg(downloadTotal_));
    refreshBooksModel();
    emit stateChanged();
    return;
  }

  const int page = downloadQueue_.at(downloadQueueIndex_);
  const QString finalPath = store_->pagePath(downloadBookId_, page);
  if (finalPath.isEmpty()) {
    ++downloadQueueIndex_;
    startNextDownload();
    return;
  }
  const QString rawPath = finalPath + QStringLiteral(".raw");
  activeDownloadId_ = api_->downloadPage(downloadBookId_, page, rawPath);
  if (activeDownloadId_ < 0) {
    clearDownloads();
    setStatus(QStringLiteral("Download failed for page %1").arg(page));
    emit stateChanged();
    return;
  }
  pending_.insert(activeDownloadId_, {QStringLiteral("download"), rawPath, page});
  setStatus(QStringLiteral("Downloading %1/%2 pages")
                .arg(downloadQueueIndex_ + 1)
                .arg(downloadQueue_.size()));
  emit stateChanged();
}

void AppController::clearDownloads() {
  pending_.remove(activeDownloadId_);
  activeDownloadId_ = 0;
  downloadQueue_.clear();
  downloadQueueIndex_ = 0;
  downloadBookId_.clear();
}

QVector<int> AppController::pagesFromArray(const QJsonArray &items) const {
  QVector<int> pages;
  pages.reserve(items.size());
  for (int i = 0; i < items.size(); ++i) {
    pages.append(items.at(i).toObject().value(QStringLiteral("number")).toInt(i + 1));
  }
  std::sort(pages.begin(), pages.end());
  return pages;
}

QString AppController::bookTitle(const QJsonObject &object) const {
  const QString title = object.value(QStringLiteral("metadata"))
                            .toObject()
                            .value(QStringLiteral("title"))
                            .toString();
  return title.isEmpty() ? object.value(QStringLiteral("name")).toString() : title;
}

QString AppController::seriesTitle(const QJsonObject &object) const {
  const QString title = object.value(QStringLiteral("metadata"))
                            .toObject()
                            .value(QStringLiteral("title"))
                            .toString();
  return title.isEmpty() ? object.value(QStringLiteral("name")).toString() : title;
}

QString AppController::bookMeta(const QJsonObject &object) const {
  const QJsonObject media = object.value(QStringLiteral("media")).toObject();
  const int pages = media.value(QStringLiteral("pagesCount")).toInt();
  const QString number = object.value(QStringLiteral("metadata"))
                             .toObject()
                             .value(QStringLiteral("number"))
                             .toString();
  if (!number.isEmpty()) {
    return QStringLiteral("#%1 · %2 pages").arg(number).arg(pages);
  }
  const int sortNumber = object.value(QStringLiteral("number")).toInt();
  return sortNumber > 0 ? QStringLiteral("#%1 · %2 pages").arg(sortNumber).arg(pages)
                        : QStringLiteral("%1 pages").arg(pages);
}

QString AppController::seriesMeta(const QJsonObject &object) const {
  const int books = object.value(QStringLiteral("booksCount")).toInt();
  const int unread = object.value(QStringLiteral("booksUnreadCount")).toInt();
  return QStringLiteral("%1 books · %2 unread").arg(books).arg(unread);
}
