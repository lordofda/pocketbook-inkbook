#include "controller.h"

#include "device.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <limits>

namespace {

qint64 readDateMs(const QJsonValue &value) {
  const QString text = value.toObject()
                           .value(QStringLiteral("readProgress"))
                           .toObject()
                           .value(QStringLiteral("readDate"))
                           .toString();
  return QDateTime::fromString(text, Qt::ISODate).toMSecsSinceEpoch();
}

double extractNumber(const QString &text) {
  int start = 0;
  while (start < text.size() && !text.at(start).isDigit()) {
    ++start;
  }
  if (start >= text.size()) {
    return std::numeric_limits<double>::max();
  }
  int end = start;
  while (end < text.size() &&
         (text.at(end).isDigit() || text.at(end) == QLatin1Char('.'))) {
    ++end;
  }
  bool ok = false;
  const double number = text.mid(start, end - start).toDouble(&ok);
  return ok ? number : std::numeric_limits<double>::max();
}

}

AppController::AppController(Settings *settings, KomgaApi *api, BookStore *store, QObject *parent)
    : QObject(parent),
      settings_(settings),
      api_(api),
      store_(store),
      downloadQueueIndex_(0),
      activeDownloadId_(0),
      syncCheckId_(0),
      syncWriteId_(0),
      progressRetryCount_(0),
      requestEpoch_(0),
      currentPage_(1),
      pageCount_(0),
      downloadedCount_(0),
      downloadTotal_(0),
      busy_(false),
      showTrustPrompt_(false),
      inProgressBooks_(false),
      offlineBook_(false),
      recentSeries_(false),
      onDeckBooks_(false),
      bookCompleted_(false),
      colorPage_(false) {
  connect(api_, &KomgaApi::librariesReceived, this,
          [this](int requestId, const QJsonArray &items) { onLibraries(items, requestId); });
  connect(api_, &KomgaApi::seriesReceived, this,
          [this](int requestId, const QJsonArray &items) { onSeries(items, requestId); });
  connect(api_, &KomgaApi::booksReceived, this,
          [this](int requestId, const QJsonArray &items) { onBooks(items, requestId); });
  connect(api_, &KomgaApi::newSeriesReceived, this,
          [this](int requestId, const QJsonArray &items) { onNewSeries(items, requestId); });
  connect(api_, &KomgaApi::onDeckReceived, this,
          [this](int requestId, const QJsonArray &items) { onOnDeck(items, requestId); });
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
  connect(&filterTimer_, &QTimer::timeout, this, &AppController::applyFilter);
  syncTimer_.setSingleShot(true);
  filterTimer_.setSingleShot(true);
  applyStatusBar();

  resetForAccount();
}

void AppController::insertPending(int requestId, const QString &name, const QString &id,
                                  int page, bool manual, const QString &title) {
  if (requestId <= 0) {
    return;
  }
  PendingAction action;
  action.name = name;
  action.id = id;
  action.title = title;
  action.page = page;
  action.manual = manual;
  action.epoch = requestEpoch_;
  pending_.insert(requestId, action);
}

bool AppController::isCurrent(const PendingAction &action) const {
  return !action.name.isEmpty() && action.epoch == requestEpoch_;
}

void AppController::clearReaderState() {
  const QString pendingBookId = currentBookId_;
  if (!pendingBookId.isEmpty() && !offlineBook_ && settings_->configured() &&
      store_->hasPendingProgress(pendingBookId)) {
    beginProgressSync(false);
  }
  ++requestEpoch_;
  syncTimer_.stop();
  if (syncCheckId_ > 0) {
    pending_.remove(syncCheckId_);
  }
  if (syncWriteId_ > 0) {
    pending_.remove(syncWriteId_);
  }
  syncCheckId_ = 0;
  syncWriteId_ = 0;
  progressRetryCount_ = 0;
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (it.value().name == QStringLiteral("pages") ||
        it.value().name == QStringLiteral("download") ||
        it.value().name == QStringLiteral("syncCheck") ||
        it.value().name == QStringLiteral("sync")) {
      it.value().epoch = requestEpoch_;
    }
  }
  currentBookId_.clear();
  currentBookTitle_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  bookCompleted_ = false;
  nextBookId_.clear();
  nextBookTitle_.clear();
  currentPage_ = 1;
  pageCount_ = 0;
  downloadTotal_ = 0;
  downloadedCount_ = 0;
  offlineBook_ = false;
  onDeckBooks_ = false;
}

void AppController::beginDownload(const QString &id, const QString &title) {
  if (id.isEmpty()) {
    return;
  }
  clearDownloads();
  downloadTargetId_ = id;
  setStatus(QStringLiteral("Fetching page list..."));
  const int requestId = api_->fetchPages(id);
  insertPending(requestId, QStringLiteral("pages"), id, 0, false, title);
  emit stateChanged();
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

ListModel *AppController::newSeriesModel() {
  return &newSeriesModel_;
}

ListModel *AppController::onDeckModel() {
  return &onDeckModel_;
}

QString AppController::filterText() const {
  return filterText_;
}

void AppController::setFilterText(const QString &text) {
  const QString normalized = text.trimmed();
  if (filterText_ == normalized && !filterPrefix_) {
    return;
  }
  filterText_ = normalized;
  filterPrefix_ = false;
  filterTimer_.start(180);
  emit stateChanged();
}

void AppController::setFilterPrefix(const QString &text) {
  const QString normalized = text.trimmed();
  if (filterText_ == normalized && filterPrefix_) {
    return;
  }
  filterText_ = normalized;
  filterPrefix_ = true;
  filterTimer_.start(0);
  emit stateChanged();
}

void AppController::applyFilter() {
  libraryModel_.setFilter(filterText_, filterPrefix_);
  seriesModel_.setFilter(filterText_, filterPrefix_);
  bookModel_.setFilter(filterText_, filterPrefix_);
  offlineModel_.setFilter(filterText_, filterPrefix_);
  newSeriesModel_.setFilter(filterText_, filterPrefix_);
  onDeckModel_.setFilter(filterText_, filterPrefix_);
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

QString AppController::pagePath() const {
  return pagePath_;
}

bool AppController::colorPage() const {
  return colorPage_;
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

QString AppController::currentSeriesId() const {
  return currentSeriesId_;
}

bool AppController::bookCompleted() const {
  return bookCompleted_;
}

QString AppController::nextBookId() const {
  return nextBookId_;
}

QString AppController::nextBookTitle() const {
  return nextBookTitle_;
}

void AppController::configure(const QString &url, const QString &key, const QString &name) {
  if (!settings_->saveConnection(url, key, name)) {
    setStatus(QStringLiteral("Enter a valid HTTPS URL and API key"));
    return;
  }
  ++requestEpoch_;
  pending_.clear();
  lastLibraries_ = QJsonArray();
  discoveredFingerprint_.clear();
  discoveredPem_.clear();
  showTrustPrompt_ = false;
  setStatus(QStringLiteral("Connecting to Komga..."));
  const int requestId = api_->testConnection();
  insertPending(requestId, QStringLiteral("test"));
  emit stateChanged();
}

void AppController::saveAccount(const QString &url, const QString &key, const QString &name) {
  if (!settings_->saveConnection(url, key, name)) {
    setStatus(QStringLiteral("Enter a valid HTTPS URL and API key"));
    return;
  }
  setStatus(QStringLiteral("Account saved"));
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
  insertPending(requestId, QStringLiteral("libraries"));
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
    insertPending(requestId, QStringLiteral("books"));
  } else if (view_ == QStringLiteral("recent")) {
    requestId = api_->fetchNewSeries();
    insertPending(requestId, QStringLiteral("newSeries"));
  } else if (view_ == QStringLiteral("onDeck")) {
    requestId = api_->fetchOnDeck();
    insertPending(requestId, QStringLiteral("onDeck"));
  } else if (view_ == QStringLiteral("series") && !currentLibraryId_.isEmpty()) {
    requestId = api_->fetchSeries(currentLibraryId_);
    insertPending(requestId, QStringLiteral("series"), currentLibraryId_);
  } else if (view_ == QStringLiteral("books") && !currentSeriesId_.isEmpty()) {
    requestId = api_->fetchBooks(currentSeriesId_);
    insertPending(requestId, QStringLiteral("books"), currentSeriesId_);
  } else {
    requestId = api_->fetchLibraries();
    insertPending(requestId, QStringLiteral("libraries"));
  }
  setStatus(QStringLiteral("Loading..."));
}

void AppController::openLibrary(int row) {
  clearReaderState();
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
  insertPending(requestId, QStringLiteral("series"), item.id);
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
  clearReaderState();
  const ListItem item = seriesModel_.itemAt(row);
  if (item.id.isEmpty()) {
    return;
  }
  currentSeriesId_ = item.id;
  inProgressBooks_ = false;
  recentSeries_ = false;
  onDeckBooks_ = false;
  offlineBook_ = false;
  selectedTitle_ = item.title;
  selectedMeta_ = item.subtitle;
  bookModel_.clear();
  setView(QStringLiteral("books"));
  const int requestId = api_->fetchBooks(item.id);
  insertPending(requestId, QStringLiteral("books"), item.id);
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
  openBookDetails(item.id, item.title, item.subtitle, item.seriesId, item.page, item.pageCount);
}

void AppController::openBookId(const QString &id) {
  const int row = bookModel_.indexOfId(id);
  if (row >= 0) {
    openBook(row);
  }
}

void AppController::openSeriesForBookId(const QString &seriesId) {
  clearReaderState();
  if (seriesId.isEmpty()) {
    setStatus(QStringLiteral("Series information unavailable"));
    return;
  }
  currentSeriesId_ = seriesId;
  currentLibraryId_.clear();
  currentBookId_.clear();
  nextBookId_.clear();
  nextBookTitle_.clear();
  bookCompleted_ = false;
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  inProgressBooks_ = false;
  recentSeries_ = false;
  onDeckBooks_ = false;
  offlineBook_ = false;
  selectedTitle_ = QStringLiteral("Series");
  selectedMeta_.clear();
  bookModel_.clear();
  setView(QStringLiteral("books"));
  const int requestId = api_->fetchBooks(seriesId);
  insertPending(requestId, QStringLiteral("books"), seriesId);
  setStatus(QStringLiteral("Loading series..."));
  emit stateChanged();
}

void AppController::openCurrentSeries() {
  openSeriesForBookId(currentSeriesId_);
}

void AppController::openNextBook() {
  if (nextBookId_.isEmpty()) {
    setStatus(QStringLiteral("No next book found"));
    return;
  }
  for (const QJsonValue &value : lastBooks_) {
    const QJsonObject object = value.toObject();
    if (object.value(QStringLiteral("id")).toString() != nextBookId_) {
      continue;
    }
    QString seriesId = object.value(QStringLiteral("seriesId")).toString();
    if (seriesId.isEmpty()) {
      seriesId = currentSeriesId_;
    }
    const QJsonObject media = object.value(QStringLiteral("media")).toObject();
    const int total = media.value(QStringLiteral("pagesCount")).toInt();
    const QJsonObject progress = object.value(QStringLiteral("readProgress")).toObject();
    int page = progress.value(QStringLiteral("page")).toInt();
    if (progress.value(QStringLiteral("completed")).toBool()) {
      page = total;
    }
    openBookDetails(nextBookId_, bookTitle(object),
                    object.value(QStringLiteral("seriesTitle")).toString(), seriesId, page, total);
    return;
  }
  const int row = bookModel_.indexOfId(nextBookId_);
  if (row >= 0) {
    openBook(row);
  } else {
    setStatus(QStringLiteral("Next book is not available in this list"));
  }
}

void AppController::openInProgress() {
  clearReaderState();
  bookModel_.clear();
  currentBookId_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  inProgressBooks_ = true;
  recentSeries_ = false;
  onDeckBooks_ = false;
  offlineBook_ = false;
  selectedTitle_ = QStringLiteral("In progress");
  selectedMeta_.clear();
  setView(QStringLiteral("inProgress"));
  const int requestId = api_->fetchInProgressBooks();
  insertPending(requestId, QStringLiteral("books"));
  setStatus(QStringLiteral("Loading in-progress books..."));
  emit stateChanged();
}

void AppController::goHome() {
  clearReaderState();
  currentBookId_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  inProgressBooks_ = false;
  recentSeries_ = false;
  onDeckBooks_ = false;
  offlineBook_ = false;
  selectedTitle_.clear();
  selectedMeta_.clear();
  setView(QStringLiteral("libraries"));
  const int requestId = api_->fetchLibraries();
  insertPending(requestId, QStringLiteral("libraries"));
  setStatus(QStringLiteral("Loading libraries..."));
  emit stateChanged();
}

void AppController::openRecent() {
  clearReaderState();
  currentBookId_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  inProgressBooks_ = false;
  recentSeries_ = true;
  onDeckBooks_ = false;
  offlineBook_ = false;
  selectedTitle_ = QStringLiteral("Recently added series");
  selectedMeta_.clear();
  setView(QStringLiteral("recent"));
  const int requestId = api_->fetchNewSeries();
  insertPending(requestId, QStringLiteral("newSeries"));
  setStatus(QStringLiteral("Loading recently added series..."));
  emit stateChanged();
}

void AppController::openOnDeck() {
  clearReaderState();
  currentBookId_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  inProgressBooks_ = false;
  recentSeries_ = false;
  onDeckBooks_ = true;
  offlineBook_ = false;
  bookModel_.clear();
  onDeckModel_.clear();
  selectedTitle_ = QStringLiteral("On deck");
  selectedMeta_.clear();
  setView(QStringLiteral("onDeck"));
  const int requestId = api_->fetchOnDeck();
  insertPending(requestId, QStringLiteral("onDeck"));
  setStatus(QStringLiteral("Loading on-deck books..."));
  emit stateChanged();
}

void AppController::openRecentSeriesId(const QString &id) {
  const int row = newSeriesModel_.indexOfId(id);
  if (row < 0) {
    setStatus(QStringLiteral("Series not found"));
    return;
  }
  recentSeries_ = true;
  const ListItem item = newSeriesModel_.itemAt(row);
  currentSeriesId_ = item.id;
  currentLibraryId_.clear();
  inProgressBooks_ = false;
  onDeckBooks_ = false;
  offlineBook_ = false;
  selectedTitle_ = item.title;
  selectedMeta_ = item.subtitle;
  bookModel_.clear();
  setView(QStringLiteral("books"));
  const int requestId = api_->fetchBooks(item.id);
  insertPending(requestId, QStringLiteral("books"), item.id);
  setStatus(QStringLiteral("Loading books..."));
  emit stateChanged();
}

void AppController::openOnDeckBookId(const QString &id) {
  const int row = onDeckModel_.indexOfId(id);
  if (row < 0) {
    setStatus(QStringLiteral("Book not found"));
    return;
  }
  const ListItem item = onDeckModel_.itemAt(row);
  openBookDetails(item.id, item.title, item.subtitle, item.seriesId, item.page, item.pageCount);
  onDeckBooks_ = true;
  emit stateChanged();
}

void AppController::openAccounts() {
  clearReaderState();
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
  setStatus(QStringLiteral("Using %1").arg(settings_->activeAccountName()));
  emit stateChanged();
}

void AppController::setDefaultAccount(const QString &id) {
  if (!settings_->setDefaultAccount(id)) {
    setStatus(QStringLiteral("Could not set startup account"));
    return;
  }
  setStatus(QStringLiteral("Startup account set to %1").arg(settings_->defaultAccountName()));
  emit stateChanged();
}

void AppController::editAccount(const QString &id) {
  if (!settings_->switchAccount(id)) {
    setStatus(QStringLiteral("Could not open account"));
    return;
  }
  resetForAccount(true);
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
  clearReaderState();
  currentBookId_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  selectedTitle_ = QStringLiteral("Offline books");
  selectedMeta_.clear();
  populateOfflineBooks();
  setView(QStringLiteral("offline"));
  emit stateChanged();
}

void AppController::openOfflineBookId(const QString &id) {
  clearReaderState();
  BookManifest manifest;
  if (id.isEmpty() || !store_->loadManifest(id, &manifest)) {
    setStatus(QStringLiteral("Offline copy not found"));
    return;
  }
  currentBookId_ = manifest.id;
  currentBookTitle_ = manifest.title;
  currentSeriesId_.clear();
  nextBookId_.clear();
  nextBookTitle_.clear();
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
    pagePath_.clear();
    colorPage_ = false;
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

void AppController::back() {
  if (view_ == QStringLiteral("reader")) {
    const bool wasOffline = offlineBook_;
    const bool wasOnDeck = onDeckBooks_;
    clearReaderState();
    if (wasOffline) {
      setView(QStringLiteral("offline"));
    } else if (wasOnDeck) {
      setView(QStringLiteral("onDeck"));
    } else {
      setView(QStringLiteral("books"));
    }
  } else if (view_ == QStringLiteral("books")) {
    if (inProgressBooks_) {
      setView(QStringLiteral("inProgress"));
    } else if (onDeckBooks_) {
      setView(QStringLiteral("onDeck"));
    } else if (recentSeries_) {
      setView(QStringLiteral("recent"));
    } else {
      setView(QStringLiteral("series"));
    }
  } else if (view_ == QStringLiteral("inProgress")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("offline")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("accounts")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("recent")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("onDeck")) {
    setView(QStringLiteral("libraries"));
  } else if (view_ == QStringLiteral("series")) {
    setView(QStringLiteral("libraries"));
  }
  emit stateChanged();
}

void AppController::downloadCurrentBook() {
  beginDownload(currentBookId_, currentBookTitle_);
}

void AppController::downloadBookId(const QString &id) {
  if (id.isEmpty()) {
    return;
  }
  ListItem item;
  bool found = false;
  int row = bookModel_.indexOfId(id);
  if (row >= 0) {
    item = bookModel_.itemAt(row);
    found = true;
  } else {
    row = onDeckModel_.indexOfId(id);
    if (row >= 0) {
      item = onDeckModel_.itemAt(row);
      found = true;
    }
  }
  if (!found) {
    for (const QJsonValue &value : lastBooks_) {
      const QJsonObject object = value.toObject();
      if (object.value(QStringLiteral("id")).toString() != id) {
        continue;
      }
      item.id = id;
      item.seriesId = object.value(QStringLiteral("seriesId")).toString();
      item.title = bookTitle(object);
      item.subtitle = object.value(QStringLiteral("seriesTitle")).toString();
      item.pageCount = object.value(QStringLiteral("media"))
                           .toObject()
                           .value(QStringLiteral("pagesCount"))
                           .toInt();
      found = true;
      break;
    }
  }
  if (!found) {
    setStatus(QStringLiteral("Book not found"));
    return;
  }
  if (store_->isDownloaded(id)) {
    setStatus(QStringLiteral("Already downloaded"));
    return;
  }
  beginDownload(id, item.title.isEmpty() ? id : item.title);
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
  const QString removedId = currentBookId_;
  const bool wasOffline = offlineBook_;
  clearReaderState();
  store_->removeBook(removedId);
  refreshBooksModel();
  populateOfflineBooks();
  setView(wasOffline ? QStringLiteral("offline") : QStringLiteral("books"));
  setStatus(QStringLiteral("Offline copy removed"));
  emit stateChanged();
}

void AppController::exit() {
  clearReaderState();
  QCoreApplication::quit();
}

void AppController::onLibraries(const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) ||
      (action.name != QStringLiteral("test") && action.name != QStringLiteral("libraries"))) {
    return;
  }
  if (action.name == QStringLiteral("test") && showTrustPrompt_) {
    lastLibraries_ = items;
    setStatus(QStringLiteral("Confirm the server certificate to continue"));
    return;
  }
  finishConnection();
  populateLibraries(items);
}

void AppController::onSeries(const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || action.name != QStringLiteral("series") ||
      action.id != currentLibraryId_ || view_ != QStringLiteral("series")) {
    return;
  }
  populateSeries(items);
}

void AppController::onBooks(const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || action.name != QStringLiteral("books")) {
    return;
  }
  if (!action.id.isEmpty()) {
    if (action.id != currentSeriesId_ || view_ != QStringLiteral("books")) {
      return;
    }
  } else if (view_ != QStringLiteral("inProgress")) {
    return;
  }
  populateBooks(items);
}

void AppController::onNewSeries(const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || action.name != QStringLiteral("newSeries") ||
      view_ != QStringLiteral("recent")) {
    return;
  }
  populateNewSeries(items);
}

void AppController::onOnDeck(const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || action.name != QStringLiteral("onDeck") ||
      view_ != QStringLiteral("onDeck")) {
    return;
  }
  populateOnDeck(items);
}

void AppController::onBook(const QJsonObject &book, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || action.name != QStringLiteral("syncCheck") ||
      action.id != currentBookId_ || view_ != QStringLiteral("reader")) {
    if (requestId == syncCheckId_) {
      syncCheckId_ = 0;
    }
    return;
  }
  if (requestId == syncCheckId_) {
    syncCheckId_ = 0;
  }

  const QString bookId = action.id;
  const QJsonObject progress = book.value(QStringLiteral("readProgress")).toObject();
  const QJsonObject media = book.value(QStringLiteral("media")).toObject();
  const int total = qMax(media.value(QStringLiteral("pagesCount")).toInt(),
                         store_->pageCount(bookId));
  int serverPage = progress.value(QStringLiteral("page")).toInt();
  if (progress.value(QStringLiteral("completed")).toBool()) {
    serverPage = total;
  }
  serverPage = qMax(0, serverPage);
  if (total > 0 && serverPage > total) {
    serverPage = total;
  }
  const int localPage = store_->localPage(bookId);

  if (serverPage > localPage) {
    store_->setProgress(bookId, serverPage, false);
    if (pageCount_ <= 0 && serverPage > 0) {
      pageCount_ = serverPage;
    }
    currentPage_ = total > 0 ? qBound(1, serverPage, total) : qMax(1, serverPage);
    pagePath_ = store_->pagePath(bookId, currentPage_);
    pageSource_ = store_->pageSource(bookId, currentPage_);
    colorPage_ = !pagePath_.isEmpty() && store_->isColorCache(bookId);
    bookCompleted_ = total > 0 && currentPage_ >= total;

    setStatus(QStringLiteral("Server is further ahead · moved to page %1").arg(currentPage_));
    emit stateChanged();
    return;
  }

  if (localPage > serverPage) {
    const bool completed = total > 0 && localPage >= total;
    const int updateId = api_->updateProgress(bookId, localPage, completed);
    insertPending(updateId, QStringLiteral("sync"), bookId, localPage, action.manual);
    syncWriteId_ = updateId;
    setStatus(QStringLiteral("Pushing local progress to server..."));
    emit stateChanged();
    return;
  }

  store_->clearPending(bookId);
  setStatus(action.manual ? QStringLiteral("Progress already in sync") : QStringLiteral("Progress synced"));
  emit stateChanged();
}

void AppController::onPages(const QString &bookId, const QJsonArray &items, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || action.name != QStringLiteral("pages") ||
      action.id != downloadTargetId_ || bookId != action.id) {
    return;
  }
  if (items.isEmpty()) {
    setStatus(QStringLiteral("No pages found for this book"));
    emit stateChanged();
    return;
  }
  store_->createManifest(bookId, action.title.isEmpty() ? action.id : action.title, items);
  downloadBookId_ = bookId;
  downloadQueue_ = pagesFromArray(items);
  downloadQueueIndex_ = 0;
  downloadTotal_ = downloadQueue_.size();
  downloadedCount_ = store_->downloadedCount(bookId);
  startNextDownload();
  emit stateChanged();
}

void AppController::onProgressUpdated(const QString &bookId, bool ok, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (requestId == syncWriteId_) {
    syncWriteId_ = 0;
  }
  if (!isCurrent(action) || action.name != QStringLiteral("sync") || action.id != bookId) {
    return;
  }
  const int localPage = store_->localPage(bookId);
  if (!ok) {
    setStatus(QStringLiteral("Progress sync failed"));
    progressRetryCount_ = 0;
    retryProgressSync();
    emit stateChanged();
    return;
  }
  progressRetryCount_ = 0;
  if (localPage > action.page) {
    scheduleProgressSync();
    if (bookId == currentBookId_) {
      setStatus(QStringLiteral("Syncing newer page..."));
    }
    emit stateChanged();
    return;
  }
  store_->clearPending(bookId);
  if (bookId == currentBookId_) {
    setStatus(QStringLiteral("Progress synced"));
  }
  emit stateChanged();
}

void AppController::onCertificate(const QString &fingerprint, const QString &pem, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) ||
      (action.name != QStringLiteral("test") && action.name != QStringLiteral("libraries"))) {
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
    if (action.name == QStringLiteral("test")) {
      setStatus(QStringLiteral("Certificate trusted · loading libraries"));
      const int requestId = api_->fetchLibraries();
      insertPending(requestId, QStringLiteral("libraries"));
    } else {
      setStatus(QStringLiteral("Certificate trusted"));
    }
  }
  emit stateChanged();
}

void AppController::onDownloadFinished(bool ok, const QString &message, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (!isCurrent(action) || requestId != activeDownloadId_ ||
      action.name != QStringLiteral("download")) {
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

  ++downloadQueueIndex_;
  ++downloadedCount_;
  if (view_ == QStringLiteral("reader") && currentBookId_ == downloadBookId_) {
    updatePage(currentPage_);
  }
  bookModel_.markDownloaded(downloadBookId_, true);
  onDeckModel_.markDownloaded(downloadBookId_, true);
  startNextDownload();
  emit stateChanged();
}

void AppController::onRequestFailed(const QString &message, int requestId) {
  const PendingAction action = pending_.take(requestId);
  if (requestId == syncCheckId_) {
    syncCheckId_ = 0;
  }
  if (requestId == syncWriteId_) {
    syncWriteId_ = 0;
  }
  if (!isCurrent(action)) {
    return;
  }
  if (action.name == QStringLiteral("syncCheck") || action.name == QStringLiteral("sync")) {
    progressRetryCount_ = 0;
    retryProgressSync();
  }
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
  if (view_ != QStringLiteral("reader") || offlineBook_ || currentBookId_.isEmpty() ||
      !store_->hasPendingProgress(currentBookId_)) {
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
  if (offlineBook_) {
    if (manual) {
      setStatus(QStringLiteral("Offline books cannot sync progress"));
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
  if ((syncCheckId_ > 0 && pending_.contains(syncCheckId_)) ||
      (syncWriteId_ > 0 && pending_.contains(syncWriteId_))) {
    if (manual) {
      setStatus(QStringLiteral("Sync already in progress"));
    }
    return;
  }

  const int requestId = api_->fetchBook(currentBookId_);
  syncCheckId_ = requestId;
  insertPending(requestId, QStringLiteral("syncCheck"), currentBookId_,
                store_->localPage(currentBookId_), manual);
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

void AppController::resetForAccount(bool stayInSetup) {
  syncTimer_.stop();
  filterTimer_.stop();
  filterPrefix_ = false;
  clearDownloads();
  ++requestEpoch_;
  pending_.clear();
  lastLibraries_ = QJsonArray();
  lastSeries_ = QJsonArray();
  lastBooks_ = QJsonArray();
  libraryModel_.clear();
  seriesModel_.clear();
  bookModel_.clear();
  offlineModel_.clear();
  newSeriesModel_.clear();
  onDeckModel_.clear();
  filterText_.clear();
  libraryModel_.setFilter(QString());
  seriesModel_.setFilter(QString());
  bookModel_.setFilter(QString());
  offlineModel_.setFilter(QString());
  newSeriesModel_.setFilter(QString());
  onDeckModel_.setFilter(QString());
  currentLibraryId_.clear();
  currentSeriesId_.clear();
  nextBookId_.clear();
  nextBookTitle_.clear();
  currentBookId_.clear();
  currentBookTitle_.clear();
  pageSource_.clear();
  pagePath_.clear();
  colorPage_ = false;
  bookCompleted_ = false;
  syncCheckId_ = 0;
  syncWriteId_ = 0;
  progressRetryCount_ = 0;
  currentPage_ = 1;
  pageCount_ = 0;
  downloadTotal_ = 0;
  downloadedCount_ = 0;
  discoveredFingerprint_.clear();
  discoveredPem_.clear();
  showTrustPrompt_ = false;
  inProgressBooks_ = false;
  offlineBook_ = false;
  recentSeries_ = false;
  onDeckBooks_ = false;
  selectedTitle_.clear();
  selectedMeta_.clear();

  if (stayInSetup) {
    setView(QStringLiteral("setup"));
    emit stateChanged();
    return;
  }

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
    item.seriesId = item.id;
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
  QVector<QJsonValue> values;
  values.reserve(items.size());
  for (const QJsonValue &value : items) {
    values.append(value);
  }
  if (inProgressBooks_) {
    std::stable_sort(values.begin(), values.end(),
                     [](const QJsonValue &left, const QJsonValue &right) {
                       return readDateMs(left) > readDateMs(right);
                     });
  } else {
    std::stable_sort(values.begin(), values.end(),
                     [this](const QJsonValue &left, const QJsonValue &right) {
                       const double leftNumber = bookSortNumber(left.toObject());
                       const double rightNumber = bookSortNumber(right.toObject());
                       if (leftNumber != rightNumber) {
                         return leftNumber < rightNumber;
                       }
                       return QString::localeAwareCompare(bookTitle(left.toObject()),
                                                          bookTitle(right.toObject())) < 0;
                     });
  }
  lastBooks_ = QJsonArray();
  for (const QJsonValue &value : values) {
    lastBooks_.append(value);
  }
  refreshBooksModel();
  setStatus(QStringLiteral("Books loaded"));
  emit stateChanged();
}

void AppController::populateNewSeries(const QJsonArray &items) {
  QVector<ListItem> rows;
  rows.reserve(items.size());
  for (const QJsonValue &value : items) {
    const QJsonObject object = value.toObject();
    ListItem item;
    item.id = object.value(QStringLiteral("id")).toString();
    item.seriesId = item.id;
    item.title = seriesTitle(object);
    item.subtitle = object.value(QStringLiteral("name")).toString();
    item.meta = seriesMeta(object);
    item.badge = object.value(QStringLiteral("booksUnreadCount")).toInt() > 0
                     ? QStringLiteral("UNREAD")
                     : QString();
    rows.append(item);
  }
  newSeriesModel_.setItems(rows);
  setStatus(QStringLiteral("Recently added series loaded"));
  emit stateChanged();
}

void AppController::populateOnDeck(const QJsonArray &items) {
  QVector<ListItem> rows;
  rows.reserve(items.size());
  for (const QJsonValue &value : items) {
    const QJsonObject object = value.toObject();
    const QString id = object.value(QStringLiteral("id")).toString();
    ListItem item;
    item.id = id;
    item.seriesId = object.value(QStringLiteral("seriesId")).toString();
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
    item.badge = item.downloaded ? QStringLiteral("OFFLINE")
                                 : (item.page > 0 ? QStringLiteral("READ") : QString());
    rows.append(item);
  }
  onDeckModel_.setItems(rows);
  setStatus(QStringLiteral("On-deck books loaded"));
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
    item.seriesId = object.value(QStringLiteral("seriesId")).toString();
    if (item.seriesId.isEmpty()) {
      item.seriesId = currentSeriesId_;
    }
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

void AppController::openBookDetails(const QString &id, const QString &title,
                                    const QString &seriesTitle, const QString &seriesId,
                                    int page, int bookPageCount) {
  if (id.isEmpty()) {
    return;
  }
  clearReaderState();
  currentBookId_ = id;
  currentBookTitle_ = title;
  currentSeriesId_ = seriesId;
  offlineBook_ = false;
  onDeckBooks_ = false;
  selectedTitle_ = title;
  selectedMeta_ = seriesTitle;
  pageCount_ = bookPageCount > 0 ? bookPageCount : store_->pageCount(id);
  downloadTotal_ = store_->pageCount(id);
  downloadedCount_ = store_->downloadedCount(id);
  const bool downloaded = store_->isDownloaded(id);
  const int storedPage = store_->localPage(id);
  updatePage(storedPage > 0 ? storedPage : (page > 0 ? page : 1));
  setView(QStringLiteral("reader"));
  setStatus(downloaded ? QStringLiteral("Ready to read")
                       : QStringLiteral("Download this book to read offline"));
  emit stateChanged();
}

void AppController::refreshNextBook() {
  nextBookId_.clear();
  nextBookTitle_.clear();
  if (currentBookId_.isEmpty() || lastBooks_.isEmpty()) {
    return;
  }

  int currentIndex = -1;
  QJsonObject current;
  for (int i = 0; i < lastBooks_.size(); ++i) {
    const QJsonObject object = lastBooks_.at(i).toObject();
    if (object.value(QStringLiteral("id")).toString() == currentBookId_) {
      currentIndex = i;
      current = object;
      break;
    }
  }
  if (currentIndex < 0) {
    return;
  }

  QString seriesId = current.value(QStringLiteral("seriesId")).toString();
  if (seriesId.isEmpty()) {
    seriesId = currentSeriesId_;
  }
  const double currentNumber = bookSortNumber(current);
  int bestIndex = -1;
  double bestNumber = std::numeric_limits<double>::max();
  int fallbackIndex = -1;
  for (int i = 0; i < lastBooks_.size(); ++i) {
    if (i == currentIndex) {
      continue;
    }
    const QJsonObject object = lastBooks_.at(i).toObject();
    const QString otherSeries = object.value(QStringLiteral("seriesId")).toString();
    if (!seriesId.isEmpty() && !otherSeries.isEmpty() && otherSeries != seriesId) {
      continue;
    }
    if (fallbackIndex < 0) {
      fallbackIndex = i;
    }
    const double number = bookSortNumber(object);
    if (number > currentNumber && number < bestNumber) {
      bestNumber = number;
      bestIndex = i;
    }
  }
  if (bestIndex < 0 && currentNumber >= std::numeric_limits<double>::max()) {
    bestIndex = fallbackIndex;
  }
  if (bestIndex < 0) {
    return;
  }

  const QJsonObject next = lastBooks_.at(bestIndex).toObject();
  nextBookId_ = next.value(QStringLiteral("id")).toString();
  nextBookTitle_ = bookTitle(next);
}

double AppController::bookSortNumber(const QJsonObject &object) const {
  const QJsonObject metadata = object.value(QStringLiteral("metadata")).toObject();
  if (metadata.contains(QStringLiteral("numberSort"))) {
    bool ok = false;
    const double number = metadata.value(QStringLiteral("numberSort")).toVariant().toDouble(&ok);
    if (ok) {
      return number;
    }
  }
  const double metadataNumber = extractNumber(
      metadata.value(QStringLiteral("number")).toString());
  if (metadataNumber < std::numeric_limits<double>::max()) {
    return metadataNumber;
  }
  bool ok = false;
  const double number = object.value(QStringLiteral("number")).toVariant().toDouble(&ok);
  return ok ? number : std::numeric_limits<double>::max();
}

void AppController::updatePage(int page) {
  if (pageCount_ > 0) {
    page = qBound(1, page, pageCount_);
  } else {
    page = 1;
  }
  currentPage_ = page;
  pagePath_ = store_->pagePath(currentBookId_, page);
  pageSource_ = store_->pageSource(currentBookId_, page);
  colorPage_ = !pagePath_.isEmpty() && store_->isColorCache(currentBookId_);
  bookCompleted_ = pageCount_ > 0 && page >= pageCount_;
  refreshNextBook();
  if (!pageSource_.isEmpty()) {
    store_->setProgress(currentBookId_, page, true);
    progressRetryCount_ = 0;
    scheduleProgressSync();
  }
  emit stateChanged();
}

void AppController::scheduleProgressSync() {
  syncTimer_.stop();
  syncTimer_.start(qMin(10000, 900 * (progressRetryCount_ + 1)));
}

void AppController::retryProgressSync() {
  if (view_ != QStringLiteral("reader") || currentBookId_.isEmpty() || offlineBook_ ||
      !store_->hasPendingProgress(currentBookId_)) {
    return;
  }
  if (progressRetryCount_ >= 5) {
    setStatus(QStringLiteral("Progress sync paused · retry when the page changes"));
    emit stateChanged();
    return;
  }
  ++progressRetryCount_;
  scheduleProgressSync();
}

void AppController::startNextDownload() {
  if (downloadQueueIndex_ >= downloadQueue_.size()) {
    activeDownloadId_ = 0;
    const int requested = downloadQueue_.size();
    const int stored = store_->downloadedCount(downloadBookId_);
    setStatus(stored >= requested
                  ? QStringLiteral("Book downloaded · %1 pages").arg(requested)
                  : QStringLiteral("Book download incomplete · %1 of %2 pages stored")
                        .arg(stored)
                        .arg(requested));
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
  if (activeDownloadId_ <= 0) {
    clearDownloads();
    setStatus(QStringLiteral("Download failed for page %1").arg(page));
    emit stateChanged();
    return;
  }
  insertPending(activeDownloadId_, QStringLiteral("download"), rawPath, page);
  setStatus(QStringLiteral("Downloading %1/%2 pages")
                .arg(downloadQueueIndex_ + 1)
                .arg(downloadQueue_.size()));
  emit stateChanged();
}

void AppController::clearDownloads() {
  if (activeDownloadId_ > 0) {
    api_->cancelRequest(activeDownloadId_);
    pending_.remove(activeDownloadId_);
  }
  activeDownloadId_ = 0;
  downloadQueue_.clear();
  downloadQueueIndex_ = 0;
  downloadTargetId_.clear();
  downloadBookId_.clear();
}

QVector<int> AppController::pagesFromArray(const QJsonArray &items) const {
  QVector<int> pages;
  pages.reserve(items.size());
  for (int i = 0; i < items.size(); ++i) {
    const int number = items.at(i).toObject().value(QStringLiteral("number")).toInt(i + 1);
    if (number > 0) {
      pages.append(number);
    }
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
