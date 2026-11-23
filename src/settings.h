#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY configurationChanged)
  Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY configurationChanged)
  Q_PROPERTY(QString certFingerprint READ certFingerprint WRITE setCertFingerprint NOTIFY configurationChanged)
  Q_PROPERTY(QString certPem READ certPem WRITE setCertPem NOTIFY configurationChanged)
  Q_PROPERTY(bool grayscale READ grayscale WRITE setGrayscale NOTIFY configurationChanged)
  Q_PROPERTY(bool configured READ configured NOTIFY configurationChanged)
  Q_PROPERTY(bool trusted READ trusted NOTIFY configurationChanged)
  Q_PROPERTY(QString cacheRoot READ cacheRoot CONSTANT)
  Q_PROPERTY(QString settingsPath READ settingsPath CONSTANT)

 public:
  explicit Settings(QObject *parent = nullptr);

  QString serverUrl() const;
  QString apiKey() const;
  QString certFingerprint() const;
  QString certPem() const;
  bool grayscale() const;
  bool configured() const;
  bool trusted() const;
  QString cacheRoot() const;
  QString settingsPath() const;

  void setServerUrl(const QString &value);
  void setApiKey(const QString &value);
  void setCertFingerprint(const QString &value);
  void setCertPem(const QString &value);
  void setGrayscale(bool value);

  Q_INVOKABLE QString normalizeUrl(const QString &value) const;
  Q_INVOKABLE QString fingerprintFromPem(const QString &pem) const;
  Q_INVOKABLE QString normalizeFingerprint(const QString &value) const;
  Q_INVOKABLE bool saveConnection(const QString &url, const QString &key);
  Q_INVOKABLE bool trustCertificate(const QString &fingerprint, const QString &pem);
  Q_INVOKABLE void clearConnection();

 signals:
  void configurationChanged();

 private:
  void persist();

  QSettings settings_;
  QString cacheRoot_;
};
