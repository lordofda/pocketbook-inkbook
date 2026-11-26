#include "settings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QJsonValue>
#include <QSslCertificate>
#include <QUuid>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>

namespace {

constexpr const char *kHideStatusBarKey = "reader/hideStatusBar";
constexpr const char *kActiveAccountKey = "general/activeAccount";
constexpr const char *kStartupAccountKey = "general/startupAccount";
constexpr const char *kAccountsRoot = "accounts";

QString defaultCacheRoot() {
  const QString deviceRoot = QStringLiteral("/mnt/ext1");
  if (QFile::exists(deviceRoot)) {
    return deviceRoot + QStringLiteral("/.komga-reader");
  }
  return QDir::homePath() + QStringLiteral("/.komga-reader");
}

}

Settings::Settings(QObject *parent)
    : QObject(parent),
      settings_(defaultCacheRoot() + QStringLiteral("/settings.ini"), QSettings::IniFormat),
      cacheRoot_(defaultCacheRoot()) {
  QDir().mkpath(cacheRoot_);
  QFile settingsFile(settings_.fileName());
  if (!settingsFile.exists() && settingsFile.open(QIODevice::WriteOnly)) {
    settingsFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    settingsFile.close();
  }
  initializeAccounts();
  QFile file(settings_.fileName());
  if (file.exists()) {
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }
}

QString Settings::accountGroupPath(const QString &id) const {
  return QStringLiteral("%1/%2").arg(QString::fromLatin1(kAccountsRoot), id);
}

QString Settings::accountKey(const QString &key, const QString &id) const {
  const QString account = id.isEmpty() ? activeAccountId() : id;
  return QStringLiteral("%1/%2").arg(accountGroupPath(account), key);
}

QString Settings::accountValue(const QString &key, const QString &id) const {
  return settings_.value(accountKey(key, id)).toString();
}

void Settings::setAccountValue(const QString &key, const QString &value,
                               const QString &id) {
  settings_.setValue(accountKey(key, id), value);
}

void Settings::initializeAccounts() {
  if (accountIds().isEmpty()) {
    const QString id = QStringLiteral("default");
    settings_.setValue(accountKey(QStringLiteral("name"), id), QStringLiteral("Default"));
    settings_.setValue(QString::fromLatin1(kStartupAccountKey), id);
    settings_.setValue(QString::fromLatin1(kActiveAccountKey), id);
  }
  const QString startup = startupAccountId();
  if (!startup.isEmpty()) {
    settings_.setValue(QString::fromLatin1(kActiveAccountKey), startup);
  }
  persist();
}

QStringList Settings::accountIds() const {
  settings_.beginGroup(QString::fromLatin1(kAccountsRoot));
  QStringList ids = settings_.childGroups();
  settings_.endGroup();
  ids.sort();
  return ids;
}

QString Settings::firstAccountId() const {
  const QStringList ids = accountIds();
  return ids.isEmpty() ? QString() : ids.first();
}

QString Settings::serverUrl() const {
  return accountValue(QStringLiteral("url"));
}

QString Settings::apiKey() const {
  return accountValue(QStringLiteral("apiKey"));
}

QString Settings::certFingerprint() const {
  return accountValue(QStringLiteral("fingerprint"));
}

QString Settings::certPem() const {
  return accountValue(QStringLiteral("pem"));
}

bool Settings::hideStatusBar() const {
  return settings_.value(QString::fromLatin1(kHideStatusBarKey), true).toBool();
}

bool Settings::configured() const {
  return !serverUrl().isEmpty() && !apiKey().isEmpty();
}

bool Settings::trusted() const {
  return !certFingerprint().isEmpty();
}

QString Settings::cacheRoot() const {
  return cacheRoot_;
}

QString Settings::settingsPath() const {
  return settings_.fileName();
}

QVariantList Settings::accounts() const {
  QVariantList result;
  const QString active = activeAccountId();
  QStringList ids = accountIds();
  std::sort(ids.begin(), ids.end(), [this, active](const QString &left, const QString &right) {
    if (left == active || right == active) {
      return left == active && right != active;
    }
    const QString leftName = accountValue(QStringLiteral("name"), left);
    const QString rightName = accountValue(QStringLiteral("name"), right);
    return QString::localeAwareCompare(leftName, rightName) < 0;
  });

  for (const QString &id : ids) {
    QVariantMap row;
    row.insert(QStringLiteral("id"), id);
    row.insert(QStringLiteral("name"),
               accountValue(QStringLiteral("name"), id).isEmpty()
                   ? id
                   : accountValue(QStringLiteral("name"), id));
    row.insert(QStringLiteral("serverUrl"), accountValue(QStringLiteral("url"), id));
    row.insert(QStringLiteral("configured"),
               !accountValue(QStringLiteral("url"), id).isEmpty() &&
                   !accountValue(QStringLiteral("apiKey"), id).isEmpty());
    row.insert(QStringLiteral("trusted"),
               !accountValue(QStringLiteral("fingerprint"), id).isEmpty());
    row.insert(QStringLiteral("active"), id == active);
    row.insert(QStringLiteral("isStartup"), id == startupAccountId());
    result.append(row);
  }
  return result;
}

QString Settings::activeAccountId() const {
  const QString id = settings_.value(QString::fromLatin1(kActiveAccountKey)).toString();
  return id.isEmpty() ? firstAccountId() : id;
}

QString Settings::activeAccountName() const {
  const QString name = accountValue(QStringLiteral("name"));
  return name.isEmpty() ? activeAccountId() : name;
}

QString Settings::startupAccountId() const {
  const QString id = settings_.value(QString::fromLatin1(kStartupAccountKey)).toString();
  return accountIds().contains(id) ? id : QString();
}

QString Settings::startupAccountName() const {
  const QString id = startupAccountId();
  if (id.isEmpty()) {
    return QStringLiteral("None");
  }
  const QString name = accountValue(QStringLiteral("name"), id);
  return name.isEmpty() ? id : name;
}

QString Settings::bookCacheRoot() const {
  return QDir(cacheRoot_).filePath(
      QStringLiteral("accounts/%1/books").arg(activeAccountId()));
}

void Settings::setServerUrl(const QString &value) {
  setAccountValue(QStringLiteral("url"), normalizeUrl(value));
  persist();
  emit configurationChanged();
}

void Settings::setApiKey(const QString &value) {
  setAccountValue(QStringLiteral("apiKey"), value.trimmed());
  persist();
  emit configurationChanged();
}

void Settings::setCertFingerprint(const QString &value) {
  setAccountValue(QStringLiteral("fingerprint"), normalizeFingerprint(value));
  persist();
  emit configurationChanged();
}

void Settings::setCertPem(const QString &value) {
  setAccountValue(QStringLiteral("pem"), value.trimmed());
  persist();
  emit configurationChanged();
}

void Settings::setHideStatusBar(bool value) {
  settings_.setValue(QString::fromLatin1(kHideStatusBarKey), value);
  persist();
  emit configurationChanged();
}

void Settings::setActiveAccountName(const QString &value) {
  setAccountValue(QStringLiteral("name"), value.trimmed());
  persist();
  emit configurationChanged();
}

QString Settings::normalizeUrl(const QString &value) const {
  QString text = value.trimmed();
  if (text.isEmpty()) {
    return QString();
  }
  if (!text.contains(QStringLiteral("://"))) {
    text.prepend(QStringLiteral("https://"));
  }
  QUrl url(text, QUrl::TolerantMode);
  if (!url.isValid() || url.host().isEmpty()) {
    return QString();
  }
  if (url.scheme() == QStringLiteral("http")) {
    const QString host = url.host().toLower();
    if (host != QStringLiteral("localhost") && host != QStringLiteral("127.0.0.1") &&
        host != QStringLiteral("::1")) {
      return QString();
    }
  } else if (url.scheme() != QStringLiteral("https")) {
    return QString();
  }
  url.setQuery(QString());
  url.setFragment(QString());
  return url.toString(QUrl::StripTrailingSlash | QUrl::RemoveQuery | QUrl::RemoveFragment);
}

QString Settings::fingerprintFromPem(const QString &pem) const {
  const QList<QSslCertificate> certificates =
      QSslCertificate::fromData(pem.toUtf8(), QSsl::Pem);
  if (certificates.isEmpty()) {
    return QString();
  }
  return normalizeFingerprint(
      QString::fromLatin1(certificates.first().digest(QCryptographicHash::Sha256).toHex()));
}

QString Settings::normalizeFingerprint(const QString &value) const {
  QString result;
  for (const QChar &character : value) {
    if (character.isLetterOrNumber()) {
      result.append(character.toLower());
    }
  }
  return result;
}

bool Settings::saveConnection(const QString &url, const QString &key, const QString &name) {
  const QString normalized = normalizeUrl(url);
  const QString trimmedKey = key.trimmed();
  if (normalized.isEmpty() || trimmedKey.isEmpty()) {
    return false;
  }
  const QString previousUrl = accountValue(QStringLiteral("url"));
  const QString previousKey = accountValue(QStringLiteral("apiKey"));
  QString label = name.trimmed();
  if (label.isEmpty()) {
    const QUrl parsed(normalized);
    label = parsed.host().isEmpty() ? QStringLiteral("Komga account") : parsed.host();
  }
  setAccountValue(QStringLiteral("name"), label);
  setAccountValue(QStringLiteral("url"), normalized);
  setAccountValue(QStringLiteral("apiKey"), trimmedKey);
  if (normalized != previousUrl || trimmedKey != previousKey.trimmed()) {
    settings_.remove(accountKey(QStringLiteral("fingerprint")));
    settings_.remove(accountKey(QStringLiteral("pem")));
    if (!previousUrl.isEmpty() && !previousKey.isEmpty()) {
      QDir(bookCacheRoot()).removeRecursively();
    }
  }
  persist();
  emit configurationChanged();
  return true;
}

bool Settings::trustCertificate(const QString &fingerprint, const QString &pem) {
  const QString normalized = normalizeFingerprint(fingerprint);
  const QString pemFingerprint = fingerprintFromPem(pem);
  if (normalized.size() != 64 || pemFingerprint.isEmpty() || pemFingerprint != normalized) {
    return false;
  }
  setAccountValue(QStringLiteral("fingerprint"), normalized);
  setAccountValue(QStringLiteral("pem"), pem.trimmed());
  persist();
  emit configurationChanged();
  return true;
}

void Settings::clearConnection() {
  settings_.remove(accountKey(QStringLiteral("url")));
  settings_.remove(accountKey(QStringLiteral("apiKey")));
  settings_.remove(accountKey(QStringLiteral("fingerprint")));
  settings_.remove(accountKey(QStringLiteral("pem")));
  QDir(bookCacheRoot()).removeRecursively();
  persist();
  emit configurationChanged();
}

QString Settings::createAccount(const QString &name) {
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
  const QString label = name.trimmed().isEmpty() ? QStringLiteral("New account") : name.trimmed();
  setAccountValue(QStringLiteral("name"), label, id);
  settings_.setValue(QString::fromLatin1(kActiveAccountKey), id);
  persist();
  emit configurationChanged();
  return id;
}

bool Settings::switchAccount(const QString &id) {
  if (id.isEmpty() || !accountIds().contains(id)) {
    return false;
  }
  settings_.setValue(QString::fromLatin1(kActiveAccountKey), id);
  persist();
  emit configurationChanged();
  return true;
}

bool Settings::setStartupAccount(const QString &id) {
  if (id.isEmpty() || !accountIds().contains(id)) {
    return false;
  }
  settings_.setValue(QString::fromLatin1(kStartupAccountKey), id);
  persist();
  emit configurationChanged();
  return true;
}

bool Settings::clearStartupAccount() {
  settings_.remove(QString::fromLatin1(kStartupAccountKey));
  persist();
  emit configurationChanged();
  return true;
}

bool Settings::removeAccount(const QString &id) {
  const QStringList ids = accountIds();
  if (id.isEmpty() || !ids.contains(id) || ids.size() <= 1) {
    return false;
  }
  const QString active = activeAccountId();
  const QString startup = startupAccountId();
  settings_.remove(accountGroupPath(id));
  if (startup == id) {
    settings_.remove(QString::fromLatin1(kStartupAccountKey));
  }
  if (active == id) {
    const QStringList remaining = accountIds();
    const QString replacement = startup != id && remaining.contains(startup)
                                    ? startup
                                    : (remaining.isEmpty() ? QString() : remaining.first());
    settings_.setValue(QString::fromLatin1(kActiveAccountKey), replacement);
  }
  persist();
  emit configurationChanged();
  return true;
}

void Settings::persist() {
  settings_.sync();
  QFile file(settings_.fileName());
  if (file.exists()) {
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }
}
