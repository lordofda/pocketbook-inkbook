#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

#include "book_store.h"
#include "controller.h"
#include "device.h"
#include "komga_api.h"
#include "settings.h"

namespace {

void selectPlatformPlugin() {
  if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
    qputenv("QT_PLUGIN_PATH", QByteArray("/ebrmain/plugins"));
  }
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArray("pocketbook2"));
  }
}

}

int main(int argc, char *argv[]) {
  selectPlatformPlugin();
  QCoreApplication::setSetuidAllowed(true);

  const ScreenSize screen = openInkViewScreen();
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

  QGuiApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("Komga Reader"));

  const QString fontFamily = inkViewFontFamily();
  if (!fontFamily.isEmpty()) {
    QGuiApplication::setFont(QFont(fontFamily));
  }

  DeviceInfo deviceInfo(screen);
  Settings settings;
  KomgaApi api(&settings);
  BookStore store(&settings);
  AppController controller(&settings, &api, &store);

  QQmlApplicationEngine engine;
  engine.addImportPath(QStringLiteral("/ebrmain/qml"));
  engine.rootContext()->setContextProperty(QStringLiteral("deviceInfo"), &deviceInfo);
  engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &settings);
  engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) {
    return 1;
  }
  return app.exec();
}
