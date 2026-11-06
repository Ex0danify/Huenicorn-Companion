#include "tray/TrayApp.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>

#include "settings/Autostart.hpp"
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
      m_tray.showMessage(tr("Huenicorn"), m_controller.lastError(), QSystemTrayIcon::Warning, 8000);
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
  m_statusAction = m_menu.addAction(QString());
  m_statusAction->setEnabled(false);
  m_menu.addSeparator();

  m_startAction = m_menu.addAction(tr("Start"), this, [this](){
    // The daemon config may have changed since the tray started
    m_controller.setPort(m_settings.port());
    m_controller.start();
  });
  m_stopAction = m_menu.addAction(tr("Stop"), &m_controller, &DaemonController::stop);
  m_restartAction = m_menu.addAction(tr("Restart"), &m_controller, &DaemonController::restart);
  m_menu.addSeparator();

  m_menu.addAction(tr("Open Configurator"), this, &TrayApp::configuratorRequested);
  m_webConfigAction = m_menu.addAction(tr("Open Web-Configurator"), this, &TrayApp::openWebConfigurator);
  m_menu.addAction(tr("Show log"), this, [this](){
    m_log.show();
    m_log.raise();
    m_log.activateWindow();
  });
  m_menu.addSeparator();

  auto* settings = m_menu.addMenu(tr("Settings"));
  m_autostartDaemonAction = settings->addAction(tr("Start light sync on app start"));
  m_autostartDaemonAction->setCheckable(true);
  m_autostartDaemonAction->setChecked(m_settings.autostartDaemon());
  connect(m_autostartDaemonAction, &QAction::toggled, this, [this](bool on){
    m_settings.setAutostartDaemon(on);
  });

  m_autostartLoginAction = settings->addAction(tr("Launch at login"));
  m_autostartLoginAction->setCheckable(true);
  m_autostartLoginAction->setChecked(Autostart::isEnabled());
  connect(m_autostartLoginAction, &QAction::toggled, this, [this](bool on){
    if(!Autostart::setEnabled(on)){
      QSignalBlocker blocker(m_autostartLoginAction);
      m_autostartLoginAction->setChecked(!on);
      m_tray.showMessage(tr("Huenicorn"), tr("Could not update the autostart entry"), QSystemTrayIcon::Warning);
    }
  });

  m_stopOnQuitAction = settings->addAction(tr("Stop daemon when quitting"));
  m_stopOnQuitAction->setCheckable(true);
  m_stopOnQuitAction->setChecked(m_settings.stopDaemonOnQuit());
  connect(m_stopOnQuitAction, &QAction::toggled, this, [this](bool on){
    m_settings.setStopDaemonOnQuit(on);
  });

  settings->addSeparator();
  settings->addAction(tr("Choose daemon binary..."), this, &TrayApp::chooseDaemon);

  m_menu.addSeparator();
  m_menu.addAction(tr("Quit"), this, &TrayApp::quit);
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

  // The error is already in the toast, tooltip and log, and a long one stretches the menu
  m_statusAction->setVisible(state != Error);
  m_statusAction->setText(text);
  m_tray.setIcon(IconFactory::forState(state));
  m_tray.setToolTip(tr("Huenicorn: %1").arg(text));

  const bool idle = (state == Stopped || state == Error);
  const bool busy = (state == Starting || state == Stopping);
  m_startAction->setEnabled(idle);
  m_stopAction->setEnabled(!idle && state != Stopping);
  m_restartAction->setEnabled(!busy);
  m_webConfigAction->setEnabled(state == Running);
}


void TrayApp::openWebConfigurator() const
{
  if(m_controller.state() == DaemonController::State::Running){
    QDesktopServices::openUrl(m_controller.baseUrl());
  }
}


void TrayApp::chooseDaemon()
{
  const auto path = QFileDialog::getOpenFileName(nullptr, tr("Select huenicorn binary"), m_controller.daemonPath());
  if(path.isEmpty()){
    return;
  }

  m_settings.setDaemonPath(path);
  m_controller.setDaemonPath(path);
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
