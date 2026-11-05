#pragma once

#include <functional>

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include "daemon/DaemonController.hpp"
#include "tray/LogWindow.hpp"
#include "settings/Settings.hpp"

class TrayApp : public QObject
{
  Q_OBJECT

public:
  TrayApp(DaemonController& controller, Settings& settings, QObject* parent = nullptr);

  void show();

  // Runs before quitting and must call the continuation when it is safe to go on.
  // Lets the composition root persist unsaved work without the tray knowing about it.
  using Continuation = std::function<void()>;
  void setPreQuitStep(std::function<void(Continuation)> step);

signals:
  // The composition root decides what "configurator" means, the tray does not know
  void configuratorRequested();

private:
  void buildMenu();
  void refresh();
  void openWebConfigurator();
  void chooseDaemon();
  void quit();
  void finishQuit();

  Settings& m_settings;
  DaemonController& m_controller;
  LogWindow m_log;
  std::function<void(Continuation)> m_preQuitStep;
  bool m_quitting{false};
  QSystemTrayIcon m_tray;
  QMenu m_menu;

  QAction* m_statusAction{};
  QAction* m_startAction{};
  QAction* m_stopAction{};
  QAction* m_restartAction{};
  QAction* m_webConfigAction{};
  QAction* m_autostartLoginAction{};
  QAction* m_autostartDaemonAction{};
  QAction* m_stopOnQuitAction{};
};
