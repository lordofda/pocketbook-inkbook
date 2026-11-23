#pragma once

#include <QObject>
#include <QString>

struct ScreenSize {
  int width;
  int height;
  int panelHeight;
};

ScreenSize openInkViewScreen();
QString inkViewFontFamily();
void setSystemStatusBarVisible(bool visible);

class DeviceInfo : public QObject {
  Q_OBJECT

  Q_PROPERTY(int screenWidth READ screenWidth CONSTANT)
  Q_PROPERTY(int screenHeight READ screenHeight CONSTANT)
  Q_PROPERTY(int panelHeight READ panelHeight CONSTANT)
  Q_PROPERTY(QString model READ model CONSTANT)
  Q_PROPERTY(QString firmware READ firmware CONSTANT)
  Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)

 public:
  explicit DeviceInfo(ScreenSize screen, QObject *parent = nullptr);

  int screenWidth() const;
  int screenHeight() const;
  int panelHeight() const;
  QString model() const;
  QString firmware() const;
  QString fontFamily() const;

 private:
  ScreenSize screen_;
};
