#include "settings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QSslCertificate>
#include <QUrl>

namespace {

constexpr const char *kServerUrlKey = "server/url";
constexpr const char *kApiKeyKey = "server/apiKey";
constexpr const char *kFingerprintKey = "server/certFingerprint";
constexpr const char *kPemKey = "server/certPem";
constexpr const char *kGrayscaleKey = "reader/grayscale";
constexpr const char *kHideStatusBarKey = "reader/hideStatusBar";

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
  QFile file(settings_.fileName());
  if (file.exists()) {
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }
}

QString Settings::serverUrl() const {
  return settings_.value(kServerUrlKey).toString();
}

QString Settings::apiKey() const {
  return settings_.value(kApiKeyKey).toString();
}

QString Settings::certFingerprint() const {
  return settings_.value(kFingerprintKey).toString();
}

QString Settings::certPem() const {
  return settings_.value(kPemKey).toString();
}

bool Settings::grayscale() const {
  return settings_.value(kGrayscaleKey, true).toBool();
}

bool Settings::hideStatusBar() const {
  return settings_.value(kHideStatusBarKey, true).toBool();
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

void Settings::setServerUrl(const QString &value) {
  settings_.setValue(kServerUrlKey, normalizeUrl(value));
  persist();
  emit configurationChanged();
}

void Settings::setApiKey(const QString &value) {
  settings_.setValue(kApiKeyKey, value.trimmed());
  persist();
  emit configurationChanged();
}

void Settings::setCertFingerprint(const QString &value) {
  settings_.setValue(kFingerprintKey, normalizeFingerprint(value));
  persist();
  emit configurationChanged();
}

void Settings::setCertPem(const QString &value) {
  settings_.setValue(kPemKey, value.trimmed());
  persist();
  emit configurationChanged();
}

void Settings::setGrayscale(bool value) {
  settings_.setValue(kGrayscaleKey, value);
  persist();
  emit configurationChanged();
}

void Settings::setHideStatusBar(bool value) {
  settings_.setValue(kHideStatusBarKey, value);
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
  if (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")) {
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

bool Settings::saveConnection(const QString &url, const QString &key) {
  const QString normalized = normalizeUrl(url);
  if (normalized.isEmpty() || key.trimmed().isEmpty()) {
    return false;
  }
  settings_.setValue(kServerUrlKey, normalized);
  settings_.setValue(kApiKeyKey, key.trimmed());
  settings_.remove(kFingerprintKey);
  settings_.remove(kPemKey);
  persist();
  emit configurationChanged();
  return true;
}

bool Settings::trustCertificate(const QString &fingerprint, const QString &pem) {
  const QString normalized = normalizeFingerprint(fingerprint);
  if (normalized.size() != 64) {
    return false;
  }
  settings_.setValue(kFingerprintKey, normalized);
  settings_.setValue(kPemKey, pem.trimmed());
  persist();
  emit configurationChanged();
  return true;
}

void Settings::clearConnection() {
  settings_.remove(kServerUrlKey);
  settings_.remove(kApiKeyKey);
  settings_.remove(kFingerprintKey);
  settings_.remove(kPemKey);
  persist();
  emit configurationChanged();
}

void Settings::persist() {
  settings_.sync();
  QFile file(settings_.fileName());
  if (file.exists()) {
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }
}
