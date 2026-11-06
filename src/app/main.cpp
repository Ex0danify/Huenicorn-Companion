#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>

#include "configurator/ConfiguratorWindow.hpp"
#include "daemon/DaemonController.hpp"
#include "i18n/Localization.hpp"
#include "installer/DaemonInstaller.hpp"
#include "preferences/AppPreferences.hpp"
#include "settings/Settings.hpp"
#include "tray/TrayApp.hpp"
#include "updater/UpdateController.hpp"

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName("huenicorn-companion");
  app.setApplicationVersion(HUENICORN_COMPANION_VERSION);
  app.setOrganizationName("huenicorn");
  app.setDesktopFileName("huenicorn-companion");
  app.setQuitOnLastWindowClosed(false);

  Settings settings;
  Localization localization(settings);

  if(!QSystemTrayIcon::isSystemTrayAvailable()){
    QMessageBox::critical(nullptr, QCoreApplication::translate("App", "Huenicorn Companion"), QCoreApplication::translate("App", "No system tray available on this desktop."));
    return 1;
  }

  DaemonController controller;
  controller.setDaemonPath(settings.resolvedDaemonPath());
  controller.setPort(settings.port());

  UpdateController updater(HUENICORN_COMPANION_VERSION);
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
  QObject::connect(&localization, &Localization::changed, &tray, &TrayApp::retranslate);
  AppPreferences preferences(settings);
  ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);
  QObject::connect(&tray, &TrayApp::configuratorRequested, &configurator, &ConfiguratorWindow::show);
  QObject::connect(&configurator.model(), &ConfiguratorModel::chooseDaemonRequested, [&](){
    // A running daemon keeps its binary, the new path is for the next start
    if(!controller.canChangeBinary()){
      return;
    }
    const auto path = QFileDialog::getOpenFileName(nullptr, QCoreApplication::translate("App", "Select huenicorn binary"), controller.daemonPath());
    if(path.isEmpty() || !controller.canChangeBinary()){
      return;
    }
    settings.setDaemonPath(path);
    controller.setDaemonPath(path);
  });

  tray.setPreQuitStep([&configurator](TrayApp::Continuation proceed){
    configurator.model().flushThen(std::move(proceed));
  });

  QObject::connect(&updater, &UpdateController::backgroundUpdateFound, [&tray](const QString& version){
    tray.notify(QCoreApplication::translate("App", "Huenicorn Companion update available"),
                QCoreApplication::translate("App", "Version %1 of Huenicorn Companion is available. Open the configurator and go to Settings to install it.").arg(version));
  });

  // The installed binary is replaced on disk, so ask the new one to start once this instance is gone
  QObject::connect(&updater, &UpdateController::restartRequested, [&tray](){
    auto executable = QStandardPaths::findExecutable("huenicorn-companion");
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
