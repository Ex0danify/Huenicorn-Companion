#include "settings/Autostart.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
  QString entryPath()
  {
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/autostart";
    return dir + "/huenicorn-companion.desktop";
  }
}


bool Autostart::isEnabled()
{
  return QFile::exists(entryPath());
}


bool Autostart::setEnabled(bool enabled)
{
  const auto path = entryPath();

  if(!enabled){
    return !QFile::exists(path) || QFile::remove(path);
  }

  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)){
    return false;
  }

  QTextStream out(&file);
  out << "[Desktop Entry]\n"
      << "Type=Application\n"
      << "Name=Huenicorn Companion\n"
      << "Exec=\"" << QCoreApplication::applicationFilePath() << "\"\n"
      << "Icon=huenicorn-companion\n"
      << "X-GNOME-Autostart-enabled=true\n";
  return true;
}
