#include "native_reader.h"

#include "device.h"
#include "settings.h"

#include <QByteArray>
#include <QFile>
#include <QQuickWindow>
#include <QUrl>

#include <algorithm>
#include <cstdlib>

namespace {

int centerOffset(int total, int extent) {
  return std::max(0, (total - extent) / 2);
}

}

NativeReader *NativeReader::instance_ = nullptr;

NativeReader::NativeReader(Settings *settings, QObject *parent)
    : QObject(parent), settings_(settings) {
  instance_ = this;
  drawTimer_.setSingleShot(true);
  drawTimer_.setInterval(80);
  connect(&drawTimer_, &QTimer::timeout, this, &NativeReader::draw);
}

NativeReader::~NativeReader() {
  if (active_) {
    close();
  }
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

bool NativeReader::active() const {
  return active_;
}

void NativeReader::setWindow(QQuickWindow *window) {
  window_ = window;
}

void NativeReader::activate() {
  active_ = true;
  fullUpdateNeeded_ = true;
  emit activeChanged();
  previousHandler_ = SetEventHandler(&NativeReader::eventHandler);
  setSystemStatusBarVisible(false);
  if (window_ != nullptr) {
    window_->hide();
  }
}

void NativeReader::show(const QString &path) {
  QString localPath = path;
  if (localPath.startsWith(QStringLiteral("file://"))) {
    const QUrl url(localPath);
    if (url.isLocalFile()) {
      localPath = url.toLocalFile();
    }
  }
  if (localPath.isEmpty() || !QFile::exists(localPath)) {
    close();
    return;
  }
  if (!active_) {
    activate();
  }
  if (path_ == localPath) {
    return;
  }
  path_ = localPath;
  drawTimer_.start();
}

void NativeReader::close() {
  drawTimer_.stop();
  if (!active_) {
    return;
  }
  active_ = false;
  if (previousHandler_ != nullptr) {
    SetEventHandler(previousHandler_);
    previousHandler_ = nullptr;
  }
  path_.clear();
  if (window_ != nullptr) {
    window_->show();
  }
  setSystemStatusBarVisible(!settings_->hideStatusBar());
  FullUpdateHQ();
  emit activeChanged();
  emit closed();
}

int NativeReader::eventHandler(int type, int par1, int par2) {
  NativeReader *self = instance_;
  if (self == nullptr) {
    return 0;
  }
  if (!self->active_) {
    return self->previousHandler_ != nullptr ? self->previousHandler_(type, par1, par2) : 0;
  }

  const bool keyEvent = ISKEYEVENT(type);
  const bool pointerEvent = ISPOINTEREVENT(type) ||
      (type >= EVT_TOUCHUP && type <= EVT_TOUCHMOVE);
  switch (type) {
    case EVT_POINTERUP:
    case EVT_TOUCHUP: {
      const int width = std::max(ScreenWidth(), 1);
      if (par1 < width / 3) {
        emit self->previousRequested();
      } else if (par1 > width * 2 / 3) {
        emit self->nextRequested();
      } else {
        self->close();
      }
      return 1;
    }
    case EVT_KEYPRESS:
    case EVT_KEYPRESS_EXT:
      if (par1 == IV_KEY_PREV || par1 == IV_KEY_PREV2) {
        emit self->previousRequested();
        return 1;
      }
      if (par1 == IV_KEY_NEXT || par1 == IV_KEY_NEXT2) {
        emit self->nextRequested();
        return 1;
      }
      if (par1 == IV_KEY_BACK || par1 == IV_KEY_OK || par1 == IV_KEY_MENU) {
        self->close();
        return 1;
      }
      return 0;
    case EVT_EXIT:
      self->close();
      break;
    default:
      break;
  }
  if (!keyEvent && !pointerEvent && self->previousHandler_ != nullptr) {
    return self->previousHandler_(type, par1, par2);
  }
  return 0;
}

void NativeReader::draw() {
  if (!active_ || path_.isEmpty()) {
    return;
  }

  const int width = std::max(ScreenWidth(), 1);
  const int height = std::max(ScreenHeight(), 1);
  const QByteArray fileName = QFile::encodeName(path_);
  ibitmap *source = LoadImageToFormat(fileName.constData(), kFmtRGB24);
  if (source == nullptr) {
    emit loadFailed();
    close();
    return;
  }

  ibitmap *scaled = BitmapStretchProportionally(source, width, height);
  if (scaled == nullptr) {
    scaled = CopyBitmap(source);
  }
  if (scaled == nullptr) {
    std::free(source);
    emit loadFailed();
    close();
    return;
  }

  adjustBitmapDefault(scaled);
  FillArea(0, 0, width, height, WHITE);
  DrawBitmap(centerOffset(width, scaled->width), centerOffset(height, scaled->height), scaled);

  const bool hq = HQUpdateSupported() != 0;
  if (fullUpdateNeeded_) {
    if (hq) {
      FullUpdateHQ();
    } else {
      FullUpdate();
    }
    fullUpdateNeeded_ = false;
  } else if (hq) {
    PartialUpdateHQ(0, 0, width, height);
  } else {
    PartialUpdate(0, 0, width, height);
  }

  if (scaled != source) {
    std::free(source);
  }
  std::free(scaled);
}
