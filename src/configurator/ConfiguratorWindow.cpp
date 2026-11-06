#include "configurator/ConfiguratorWindow.hpp"

#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>

ConfiguratorWindow::ConfiguratorWindow(DaemonController& controller, UpdateController& updater, DaemonInstaller& installer, QObject* parent)
  : QObject(parent)
  , m_model(controller)
{
  // Resources in a static library are dropped by the linker unless initialized by hand
  Q_INIT_RESOURCE(qml);
  m_engine.rootContext()->setContextProperty("backend", &m_model);
  m_engine.rootContext()->setContextProperty("updater", &updater);
  m_engine.rootContext()->setContextProperty("installer", &installer);
}


ConfiguratorModel& ConfiguratorWindow::model()
{
  return m_model;
}


QQuickWindow* ConfiguratorWindow::window()
{
  ensureLoaded();
  return m_window;
}


bool ConfiguratorWindow::ensureLoaded()
{
  if(m_window){
    return true;
  }

  connect(&m_engine, &QQmlApplicationEngine::warnings, this, [](const QList<QQmlError>& errors){
    for(const auto& error : errors){
      qWarning().noquote() << error.toString();
    }
  });
  m_engine.load(QUrl("qrc:/configurator/qml/Main.qml"));
  if(!m_engine.rootObjects().isEmpty()){
    m_window = qobject_cast<QQuickWindow*>(m_engine.rootObjects().first());
  }
  return m_window != nullptr;
}


void ConfiguratorWindow::show()
{
  if(!ensureLoaded()){
    return;
  }

  m_window->show();
  m_window->raise();
  m_window->requestActivate();
}
