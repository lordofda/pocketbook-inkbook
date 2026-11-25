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

constexpr int kTopBarHeight = 72;
constexpr int kButtonBarHeight = 92;
constexpr int kButtonCount = 7;

int centerOffset(int total, int extent) {
  return std::max(0, (total - extent) / 2);
}

const char *buttonLabel(int index, bool chromeVisible) {
  switch (index) {
    case 3:
      return "Sync";
    case 4:
      return chromeVisible ? "Bar:on" : "Bar:off";
    case 5:
      return "Exit";
    default:
      return "Back";
  }
}

void drawNavigationIcon(int index, int x, int y, int width, int height) {
  DrawSymbol(x + centerOffset(width, 28), y + centerOffset(height, 28),
             index == 0 ? ARROW_LEFT : ARROW_RIGHT);
}

void drawDownloadIcon(int x, int y, int width, int height) {
  const int shaftWidth = std::max(6, width / 8);
  const int shaftHeight = height * 2 / 5;
  FillArea(x + centerOffset(width, shaftWidth), y + height / 6, shaftWidth, shaftHeight, BLACK);
  const int headHeight = std::max(3, height / 3);
  const int rows = headHeight;
  const int halfBase = width * 3 / 10;
  const int headTop = y + height / 6 + shaftHeight;
  for (int row = 0; row < rows; ++row) {
    const int half = halfBase * (rows - 1 - row) / std::max(1, rows - 1);
    FillArea(x + centerOffset(width, half * 2 + 1), headTop + row * headHeight / rows,
             half * 2 + 1, headHeight / rows + 1, BLACK);
  }
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

void NativeReader::show(const QString &path, int page, int pageCount, const QString &title,
                        bool completed) {
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
  if (path_ == localPath && page_ == page && pageCount_ == pageCount && title_ == title &&
      completed_ == completed) {
    return;
  }
  path_ = localPath;
  page_ = page;
  pageCount_ = pageCount;
  title_ = title;
  completed_ = completed;
  drawTimer_.start();
}

void NativeReader::close() {
  drawTimer_.stop();
  if (!active_) {
    return;
  }
  active_ = false;
  chromeVisible_ = false;
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
      const int height = std::max(ScreenHeight(), 1);
      if (self->chromeVisible_ && par2 >= height - kButtonBarHeight) {
        const int index = qBound(0, par1 * kButtonCount / width, kButtonCount - 1);
        switch (index) {
          case 0:
            emit self->previousRequested();
            break;
          case 1:
            emit self->nextRequested();
            break;
          case 2:
            emit self->downloadRequested();
            break;
          case 3:
            emit self->syncRequested();
            break;
          case 4:
            self->chromeVisible_ = !self->chromeVisible_;
            self->drawTimer_.start();
            break;
          case 5:
            emit self->exitRequested();
            break;
          default:
            emit self->backRequested();
            break;
        }
        return 1;
      }
      if (self->chromeVisible_ && par2 <= kTopBarHeight) {
        self->chromeVisible_ = false;
        self->drawTimer_.start();
        return 1;
      }
      if (par1 < width / 3) {
        emit self->previousRequested();
      } else if (par1 > width * 2 / 3) {
        emit self->nextRequested();
      } else {
        self->chromeVisible_ = !self->chromeVisible_;
        self->drawTimer_.start();
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
      if (par1 == IV_KEY_BACK) {
        emit self->backRequested();
        return 1;
      }
      if (par1 == IV_KEY_OK || par1 == IV_KEY_MENU) {
        self->chromeVisible_ = !self->chromeVisible_;
        self->drawTimer_.start();
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
  drawChrome();

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

void NativeReader::drawChrome() {
  if (!chromeVisible_) {
    return;
  }

  const int width = std::max(ScreenWidth(), 1);
  const int height = std::max(ScreenHeight(), 1);
  ifont *font = OpenFont(DEFAULTFONT, 22, 0);
  if (font != nullptr) {
    SetFont(font, BLACK);
  }

  FillArea(0, 0, width, kTopBarHeight, WHITE);
  DrawRect(0, 0, width, kTopBarHeight, BLACK);
  QString header = title_.isEmpty()
      ? QStringLiteral("Komga Reader · %1/%2").arg(page_).arg(pageCount_)
      : QStringLiteral("%1 · %2/%3").arg(title_).arg(page_).arg(pageCount_);
  if (completed_) {
    header.append(QStringLiteral(" · Finished"));
  }
  const QByteArray headerBytes = header.toUtf8();
  DrawTextRect(16, 8, width - 32, kTopBarHeight - 16, headerBytes.constData(),
               ALIGN_LEFT | VALIGN_MIDDLE);

  const int barTop = height - kButtonBarHeight;
  FillArea(0, barTop, width, kButtonBarHeight, WHITE);
  DrawRect(0, barTop, width, kButtonBarHeight, BLACK);
  for (int index = 0; index < kButtonCount; ++index) {
    const int left = index * width / kButtonCount;
    const int right = (index + 1) * width / kButtonCount;
    DrawRect(left + 4, barTop + 6, right - left - 8, kButtonBarHeight - 12, BLACK);
    if (index <= 1) {
      drawNavigationIcon(index, left + 4, barTop + 6, right - left - 8,
                         kButtonBarHeight - 12);
    } else if (index == 2) {
      drawDownloadIcon(left + 4, barTop + 6, right - left - 8, kButtonBarHeight - 12);
    } else {
      const QByteArray label(buttonLabel(index, chromeVisible_));
      DrawTextRect(left + 8, barTop + 10, right - left - 16, kButtonBarHeight - 20,
                   label.constData(), ALIGN_CENTER | VALIGN_MIDDLE);
    }
  }

  if (font != nullptr) {
    CloseFont(font);
  }
}
