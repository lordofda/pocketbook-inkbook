#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariantList>

class Settings : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY configurationChanged)
  Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY configurationChanged)
  Q_PROPERTY(QString certFingerprint READ certFingerprint WRITE setCertFingerprint NOTIFY configurationChanged)
  Q_PROPERTY(QString certPem READ certPem WRITE setCertPem NOTIFY configurationChanged)
  Q_PROPERTY(bool hideStatusBar READ hideStatusBar WRITE setHideStatusBar NOTIFY configurationChanged)
  Q_PROPERTY(bool configured READ configured NOTIFY configurationChanged)
  Q_PROPERTY(bool trusted READ trusted NOTIFY configurationChanged)
  Q_PROPERTY(QString cacheRoot READ cacheRoot CONSTANT)
  Q_PROPERTY(QString settingsPath READ settingsPath CONSTANT)
  Q_PROPERTY(QVariantList accounts READ accounts NOTIFY configurationChanged)
  Q_PROPERTY(QString activeAccountId READ activeAccountId NOTIFY configurationChanged)
  Q_PROPERTY(QString activeAccountName READ activeAccountName WRITE setActiveAccountName NOTIFY configurationChanged)
  Q_PROPERTY(QString defaultAccountId READ defaultAccountId NOTIFY configurationChanged)
  Q_PROPERTY(QString defaultAccountName READ defaultAccountName NOTIFY configurationChanged)

 public:
  explicit Settings(QObject *parent = nullptr);

  QString serverUrl() const;
  QString apiKey() const;
  QString certFingerprint() const;
  QString certPem() const;
  bool hideStatusBar() const;
  bool configured() const;
  bool trusted() const;
  QString cacheRoot() const;
  QString settingsPath() const;
  QVariantList accounts() const;
  QString activeAccountId() const;
  QString activeAccountName() const;
  QString defaultAccountId() const;
  QString defaultAccountName() const;
  QString bookCacheRoot() const;

  void setServerUrl(const QString &value);
  void setApiKey(const QString &value);
  void setCertFingerprint(const QString &value);
  void setCertPem(const QString &value);
  void setHideStatusBar(bool value);
  void setActiveAccountName(const QString &value);

  Q_INVOKABLE QString normalizeUrl(const QString &value) const;
  Q_INVOKABLE QString fingerprintFromPem(const QString &pem) const;
  Q_INVOKABLE QString normalizeFingerprint(const QString &value) const;
  Q_INVOKABLE bool saveConnection(const QString &url, const QString &key,
                                   const QString &name = QString());
  Q_INVOKABLE bool trustCertificate(const QString &fingerprint, const QString &pem);
  Q_INVOKABLE void clearConnection();
  Q_INVOKABLE QString createAccount(const QString &name);
  Q_INVOKABLE bool switchAccount(const QString &id);
  Q_INVOKABLE bool setDefaultAccount(const QString &id);
  Q_INVOKABLE bool removeAccount(const QString &id);

 signals:
  void configurationChanged();

 private:
  QString accountGroupPath(const QString &id) const;
  QString accountKey(const QString &key, const QString &id = QString()) const;
  QString accountValue(const QString &key, const QString &id = QString()) const;
  void setAccountValue(const QString &key, const QString &value,
                       const QString &id = QString());
  void ensureAccounts();
  void migrateLegacySettings();
  void migrateLegacyCache();
  QStringList accountIds() const;
  QString firstAccountId() const;
  void persist();

  mutable QSettings settings_;
  QString cacheRoot_;
};
