#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#include "FakeDaemonApi.hpp"
#include "configurator/ConfiguratorWindow.hpp"

// Usage: RenderConfigurator <output-dir> [setup]. Writes one PNG per tab, or per setup step.
int main(int argc, char* argv[])
{
  QGuiApplication app(argc, argv);
  if(argc < 2){
    return 2;
  }
  const QString outDir = argv[1];

  const bool setupMode = argc > 2 && QString(argv[2]) == "setup";

  FakeDaemonApi api;
  api.setupMode = setupMode;
  api.linkButtonAfter = 100000;
  api.normalModeAfter = 100000;
  api.detectedBridges = {"192.168.1.20", "192.168.1.31"};
  DaemonController controller;
  controller.setPort(api.port());
  controller.setDaemonPath("/usr/bin/true");
  controller.setPollIntervalMs(100);
  UpdateController updater("0.1.0");
  DaemonInstaller installer(PinnedDaemon::current());
  Settings settings;
  Localization localization(settings);
  AppPreferences preferences(settings);
  ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);
  auto* window = configurator.window();
  window->show();
  controller.beginMonitoring();

  if(setupMode){
    auto* setup = configurator.model().setup();
    int stage = 0;
    QTimer stepTimer;
    stepTimer.setInterval(700);
    QObject::connect(&stepTimer, &QTimer::timeout, [&](){
      if(!setup->active() || setup->busy()){
        return;
      }
      static const QStringList stages = {"setup-bridge", "setup-pair", "setup-finishing"};
      window->grabWindow().save(QString("%1/%2.png").arg(outDir, stages[stage]));
      if(++stage >= stages.size()){
        app.quit();
      }
      else if(stage == 1){
        setup->submitBridge("192.168.1.20");
      }
      else{
        setup->setRetryIntervalMs(50);
        api.linkButtonAfter = 0;
        setup->pair();
      }
    });
    stepTimer.start();
    return app.exec();
  }

  const QStringList names = {"sync", "screen", "lights", "settings"};
  int tab = -1;

  QTimer timer;
  timer.setInterval(500);
  QObject::connect(&timer, &QTimer::timeout, [&](){
    if(!configurator.model().loaded()){
      return;
    }
    if(tab >= 0){
      window->grabWindow().save(QString("%1/%2.png").arg(outDir, names[tab]));
    }
    if(++tab >= names.size()){
      app.quit();
      return;
    }
    window->setProperty("tab", tab);
    if(tab == 1){
      // Show a selected channel with its handles
      QMetaObject::invokeMethod(window, "selectChannel", Q_ARG(QVariant, 0));
    }
  });
  timer.start();

  return app.exec();
}
