#include "settings/Settings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

namespace
{
  QSettings store()
  {
    return QSettings(QSettings::IniFormat, QSettings::UserScope, "huenicorn", "huenicorn-companion");
  }
}


QString Settings::daemonPath() const
{
  return store().value("daemonPath").toString();
}


void Settings::setDaemonPath(const QString& path)
{
  store().setValue("daemonPath", path);
}


int Settings::port() const
{
  const auto stored = store().value("port");
  if(stored.isValid()){
    return stored.toInt();
  }

  return daemonConfigPort();
}


int Settings::daemonConfigPort()
{
  // The daemon keeps its config under ~/.config/huenicorn regardless of XDG
  QFile file(QDir::homePath() + "/.config/huenicorn/config.json");
  if(file.open(QIODevice::ReadOnly)){
    const auto port = QJsonDocument::fromJson(file.readAll()).object().value("restServerPort").toInt();
    if(port > 0){
      return port;
    }
  }

  return DefaultPort;
}


void Settings::setPort(int port)
{
  store().setValue("port", port);
}


bool Settings::autostartDaemon() const
{
  return store().value("autostartDaemon", true).toBool();
}


void Settings::setAutostartDaemon(bool enabled)
{
  store().setValue("autostartDaemon", enabled);
}


bool Settings::stopDaemonOnQuit() const
{
  return store().value("stopDaemonOnQuit", true).toBool();
}


void Settings::setStopDaemonOnQuit(bool enabled)
{
  store().setValue("stopDaemonOnQuit", enabled);
}


bool Settings::checkUpdatesOnStart() const
{
  return store().value("checkUpdatesOnStart", true).toBool();
}


void Settings::setCheckUpdatesOnStart(bool enabled)
{
  store().setValue("checkUpdatesOnStart", enabled);
}


QDateTime Settings::lastUpdateCheck() const
{
  return store().value("lastUpdateCheck").toDateTime();
}


void Settings::setLastUpdateCheck(const QDateTime& when)
{
  store().setValue("lastUpdateCheck", when);
}


QString Settings::skippedUpdateVersion() const
{
  return store().value("skippedUpdateVersion").toString();
}


void Settings::setSkippedUpdateVersion(const QString& version)
{
  store().setValue("skippedUpdateVersion", version);
}


QString Settings::language() const
{
  return store().value("language", "auto").toString();
}


void Settings::setLanguage(const QString& language)
{
  store().setValue("language", language);
}


QString Settings::resolvedDaemonPath() const
{
  const auto configured = daemonPath();
  if(!configured.isEmpty()){
    return configured;
  }

  const auto sibling = QCoreApplication::applicationDirPath() + "/huenicorn";
  if(QFileInfo(sibling).isExecutable()){
    return sibling;
  }

  return QStandardPaths::findExecutable("huenicorn");
}
