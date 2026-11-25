#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <inkview.h>

class QQuickWindow;

class Settings;

class NativeReader : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool active READ active NOTIFY activeChanged)

 public:
  explicit NativeReader(Settings *settings, QObject *parent = nullptr);
  ~NativeReader() override;

  bool active() const;
  void setWindow(QQuickWindow *window);

  Q_INVOKABLE void show(const QString &path);
  Q_INVOKABLE void close();

 signals:
  void activeChanged();
  void closed();
  void loadFailed();
  void previousRequested();
  void nextRequested();

 private:
  static int eventHandler(int type, int par1, int par2);
  void draw();
  void activate();

  Settings *settings_;
  QQuickWindow *window_ = nullptr;
  QString path_;
  iv_handler previousHandler_ = nullptr;
  QTimer drawTimer_;
  bool active_ = false;
  bool fullUpdateNeeded_ = false;

  static NativeReader *instance_;
};
