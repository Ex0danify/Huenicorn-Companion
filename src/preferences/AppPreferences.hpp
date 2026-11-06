#pragma once

#include <QObject>

#include "settings/Settings.hpp"

// Behaviour switches the user changes from the app, backed by the settings and the login autostart entry
class AppPreferences : public QObject
{
  Q_OBJECT

  Q_PROPERTY(bool autostartDaemon READ autostartDaemon WRITE setAutostartDaemon NOTIFY autostartDaemonChanged)
  Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY launchAtLoginChanged)
  Q_PROPERTY(bool stopDaemonOnQuit READ stopDaemonOnQuit WRITE setStopDaemonOnQuit NOTIFY stopDaemonOnQuitChanged)

public:
  explicit AppPreferences(Settings& settings, QObject* parent = nullptr);

  bool autostartDaemon() const;
  void setAutostartDaemon(bool enabled);

  bool launchAtLogin() const;
  void setLaunchAtLogin(bool enabled);

  bool stopDaemonOnQuit() const;
  void setStopDaemonOnQuit(bool enabled);

signals:
  void autostartDaemonChanged();
  void launchAtLoginChanged();
  void stopDaemonOnQuitChanged();
  void launchAtLoginFailed();

private:
  Settings& m_settings;
};
