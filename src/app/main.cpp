#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>

#include "configurator/ConfiguratorWindow.hpp"
#include "daemon/DaemonController.hpp"
#include "installer/DaemonInstaller.hpp"
#include "settings/Settings.hpp"
#include "tray/TrayApp.hpp"
#include "updater/UpdateController.hpp"

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName("huenicorn-qt");
  app.setApplicationVersion(HUENICORN_QT_VERSION);
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

  UpdateController updater(HUENICORN_QT_VERSION);
  updater.setAutoCheck(settings.checkUpdatesOnStart());
  updater.setSkippedVersion(settings.skippedUpdateVersion());
  QObject::connect(&updater, &UpdateController::autoCheckChanged, [&](){
    settings.setCheckUpdatesOnStart(updater.autoCheck());
  });
  QObject::connect(&updater, &UpdateController::skippedVersionChanged, [&](){
    settings.setSkippedUpdateVersion(updater.skippedVersion());
  });
  QObject::connect(&updater, &UpdateController::checkCompleted, [&](){
    settings.setLastUpdateCheck(QDateTime::currentDateTime());
  });

  DaemonInstaller installer(PinnedDaemon::current());
  QObject::connect(&installer, &DaemonInstaller::installed, [&](const QString& path){
    settings.setDaemonPath(path);
    controller.setDaemonPath(path);
    controller.start();
  });

  TrayApp tray(controller, settings);
  ConfiguratorWindow configurator(controller, updater, installer);
  QObject::connect(&tray, &TrayApp::configuratorRequested, &configurator, &ConfiguratorWindow::show);

  tray.setPreQuitStep([&configurator](TrayApp::Continuation proceed){
    configurator.model().flushThen(std::move(proceed));
  });

  QObject::connect(&updater, &UpdateController::backgroundUpdateFound, [&tray](const QString& version){
    tray.notify(QObject::tr("Huenicorn update available"),
                QObject::tr("Version %1 is available. Open the configurator and go to Settings to install it.").arg(version));
  });

  // The installed binary is replaced on disk, so ask the new one to start once this instance is gone
  QObject::connect(&updater, &UpdateController::restartRequested, [&tray](){
    auto executable = QStandardPaths::findExecutable("huenicorn-qt");
    if(executable.isEmpty()){
      executable = QCoreApplication::applicationFilePath().remove(" (deleted)");
    }
    QProcess::startDetached("/bin/sh", {"-c", "sleep 3; exec \"$0\"", executable});
    tray.quit();
  });

  tray.show();

  QTimer::singleShot(5000, &updater, [&](){
    if(updater.autoCheck() && UpdateController::checkDue(settings.lastUpdateCheck(), QDateTime::currentDateTime())){
      updater.checkInBackground();
    }
  });

  return app.exec();
}
