#include "tray/TrayApp.hpp"

#include <QApplication>
#include <QMessageBox>

#include "tray/IconFactory.hpp"


TrayApp::TrayApp(DaemonController& controller, Settings& settings, QObject* parent)
  : QObject(parent)
  , m_settings(settings)
  , m_controller(controller)
{
  buildMenu();

  m_tray.setContextMenu(&m_menu);
  connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
    if(reason == QSystemTrayIcon::Trigger){
      emit configuratorRequested();
    }
  });

  connect(&m_tray, &QSystemTrayIcon::messageClicked, this, &TrayApp::configuratorRequested);

  connect(&m_controller, &DaemonController::stateChanged, this, [this](DaemonController::State state){
    refresh();
    if(state == DaemonController::State::Error){
      m_tray.showMessage(tr("Huenicorn Companion"), m_controller.lastError(), QSystemTrayIcon::Warning, 8000);
    }
  });
  connect(&m_controller, &DaemonController::logLine, &m_log, &LogWindow::append);

  refresh();
}


void TrayApp::show()
{
  m_tray.show();

  m_controller.beginMonitoring();
  if(m_settings.autostartDaemon()){
    // Give the poll a moment to adopt an already running daemon first
    QTimer::singleShot(800, &m_controller, [this](){
      if(m_controller.state() == DaemonController::State::Stopped){
        m_controller.start();
      }
    });
  }
}


void TrayApp::notify(const QString& title, const QString& message)
{
  m_tray.showMessage(title, message, QSystemTrayIcon::Information, 10000);
}


void TrayApp::buildMenu()
{
  m_openConfiguratorAction = m_menu.addAction(QString(), this, &TrayApp::configuratorRequested);
  m_menu.addSeparator();

  m_startAction = m_menu.addAction(QString(), this, [this](){
    // The daemon config may have changed since the tray started
    m_controller.setPort(m_settings.port());
    m_controller.start();
  });
  m_stopAction = m_menu.addAction(QString(), &m_controller, &DaemonController::stop);
  m_restartAction = m_menu.addAction(QString(), &m_controller, &DaemonController::restart);
  m_menu.addSeparator();

  m_logAction = m_menu.addAction(QString(), this, [this](){
    m_log.show();
    m_log.raise();
    m_log.activateWindow();
  });
  m_quitAction = m_menu.addAction(QString(), this, &TrayApp::quit);

  retranslate();
}


void TrayApp::retranslate()
{
  m_startAction->setText(tr("Start"));
  m_stopAction->setText(tr("Stop"));
  m_restartAction->setText(tr("Restart"));
  m_openConfiguratorAction->setText(tr("Open Companion"));
  m_logAction->setText(tr("Show log"));
  m_quitAction->setText(tr("Quit"));
  refresh();
}


void TrayApp::refresh()
{
  using enum DaemonController::State;
  const auto state = m_controller.state();

  QString text;
  switch(state){
    case Stopped: text = tr("Stopped"); break;
    case Starting: text = tr("Starting..."); break;
    case Running:
      text = m_controller.version().isEmpty() ? tr("Syncing") : tr("Syncing (v%1)").arg(m_controller.version());
      break;
    case Stopping: text = tr("Stopping..."); break;
    case Error: text = tr("Error: %1").arg(m_controller.lastError()); break;
  }

  m_tray.setIcon(IconFactory::forState(state));
  m_tray.setToolTip(tr("Huenicorn Companion: %1").arg(text));

  const bool idle = (state == Stopped || state == Error);
  m_startAction->setEnabled(idle);
  m_stopAction->setEnabled(!idle && state != Stopping);
  m_restartAction->setEnabled(state == Running);
}


void TrayApp::setPreQuitStep(std::function<void(Continuation)> step)
{
  m_preQuitStep = std::move(step);
}


void TrayApp::quit()
{
  if(m_quitting){
    return;
  }
  m_quitting = true;

  if(m_preQuitStep){
    m_preQuitStep([this](){ finishQuit(); });
  }
  else{
    finishQuit();
  }
}


void TrayApp::finishQuit()
{
  using enum DaemonController::State;

  if(const bool running = m_controller.state() == Running || m_controller.state() == Starting;
     running && m_settings.stopDaemonOnQuit()){
    connect(&m_controller, &DaemonController::stateChanged, qApp, [](DaemonController::State state){
      if(state == Stopped || state == Error){
        QApplication::quit();
      }
    });
    m_controller.stop();
    return;
  }

  QApplication::quit();
}
