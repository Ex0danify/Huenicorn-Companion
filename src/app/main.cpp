#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include "configurator/ConfiguratorWindow.hpp"
#include "daemon/DaemonController.hpp"
#include "settings/Settings.hpp"
#include "tray/TrayApp.hpp"

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName("huenicorn-qt");
  app.setOrganizationName("huenicorn");
  app.setDesktopFileName("huenicorn-qt");
  app.setQuitOnLastWindowClosed(false);

  if(!QSystemTrayIcon::isSystemTrayAvailable()){
    QMessageBox::critical(nullptr, QObject::tr("Huenicorn"), QObject::tr("No system tray available on this desktop."));
    return 1;
  }

  Settings settings;
  DaemonController controller;
  controller.setDaemonPath(settings.resolvedDaemonPath());
  controller.setPort(settings.port());

  TrayApp tray(controller, settings);
  ConfiguratorWindow configurator(controller);
  QObject::connect(&tray, &TrayApp::configuratorRequested, &configurator, &ConfiguratorWindow::show);

  tray.setPreQuitStep([&configurator](TrayApp::Continuation proceed){
    configurator.model().flushThen(std::move(proceed));
  });

  tray.show();
  return app.exec();
}
