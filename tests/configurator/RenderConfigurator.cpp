#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#include "FakeDaemonApi.hpp"
#include "configurator/ConfiguratorWindow.hpp"

// Usage: RenderConfigurator <output-dir>. Writes one PNG per tab.
int main(int argc, char* argv[])
{
  QGuiApplication app(argc, argv);
  if(argc < 2){
    return 2;
  }
  const QString outDir = argv[1];

  FakeDaemonApi api;
  DaemonController controller;
  controller.setPort(api.port());
  controller.setPollIntervalMs(100);
  ConfiguratorWindow configurator(controller);
  auto* window = configurator.window();
  window->show();
  controller.beginMonitoring();

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
