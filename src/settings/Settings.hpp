#pragma once

#include <QString>

// Thin QSettings wrapper so the rest of the app never touches keys directly.
class Settings
{
public:
  QString daemonPath() const;
  void setDaemonPath(const QString& path);

  // Explicit override if set, otherwise whatever the daemon is configured with
  int port() const;
  void setPort(int port);

  bool autostartDaemon() const;
  void setAutostartDaemon(bool enabled);

  bool stopDaemonOnQuit() const;
  void setStopDaemonOnQuit(bool enabled);

  // Configured path, or the first "huenicorn" found next to us or on PATH
  QString resolvedDaemonPath() const;

  // restServerPort from the daemon's own config, or the default
  static int daemonConfigPort();

  static constexpr int DefaultPort = 8215;
};
