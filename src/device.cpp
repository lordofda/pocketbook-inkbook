#include "device.h"

#include <inkview.h>

namespace {

int positiveOr(int value, int fallback) {
  return value > 0 ? value : fallback;
}

}

ScreenSize openInkViewScreen() {
  InitInkview(TASK_MAKEACTIVE);
  return {positiveOr(ScreenWidth(), 1072), positiveOr(ScreenHeight(), 1440),
          positiveOr(PanelHeight(), 48)};
}

QString inkViewFontFamily() {
  const char *family = iv_get_default_font(FONT_FAMILY);
  return family != nullptr ? QString::fromUtf8(family) : QString();
}

void setSystemStatusBarVisible(bool visible) {
  SetCurrentApplicationAttribute(APPLICATION_READER, 1);
  SetPanelSeparatorEnabled(visible ? 1 : 0);
  SetShowPanelReader(visible ? 1 : 0);
  SetPanelType(visible ? PANEL_ENABLED : PANEL_DISABLED);
}

DeviceInfo::DeviceInfo(ScreenSize screen, QObject *parent)
    : QObject(parent), screen_(screen) {}

int DeviceInfo::screenWidth() const {
  return screen_.width;
}

int DeviceInfo::screenHeight() const {
  return screen_.height;
}

int DeviceInfo::panelHeight() const {
  return screen_.panelHeight;
}

QString DeviceInfo::model() const {
  return QString::fromUtf8(GetDeviceModel());
}

QString DeviceInfo::firmware() const {
  return QString::fromUtf8(GetSoftwareVersion());
}

QString DeviceInfo::fontFamily() const {
  return inkViewFontFamily();
}
