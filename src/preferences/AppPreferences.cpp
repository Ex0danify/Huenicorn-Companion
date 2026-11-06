#include "preferences/AppPreferences.hpp"

#include "settings/Autostart.hpp"


AppPreferences::AppPreferences(Settings& settings, QObject* parent)
  : QObject(parent)
  , m_settings(settings)
{
}


bool AppPreferences::autostartDaemon() const
{
  return m_settings.autostartDaemon();
}


void AppPreferences::setAutostartDaemon(bool enabled)
{
  if(enabled != autostartDaemon()){
    m_settings.setAutostartDaemon(enabled);
    emit autostartDaemonChanged();
  }
}


bool AppPreferences::launchAtLogin() const
{
  return Autostart::isEnabled();
}


void AppPreferences::setLaunchAtLogin(bool enabled)
{
  if(enabled == launchAtLogin()){
    return;
  }

  if(!Autostart::setEnabled(enabled)){
    emit launchAtLoginFailed();
  }
  emit launchAtLoginChanged();
}


bool AppPreferences::stopDaemonOnQuit() const
{
  return m_settings.stopDaemonOnQuit();
}


void AppPreferences::setStopDaemonOnQuit(bool enabled)
{
  if(enabled != stopDaemonOnQuit()){
    m_settings.setStopDaemonOnQuit(enabled);
    emit stopDaemonOnQuitChanged();
  }
}
