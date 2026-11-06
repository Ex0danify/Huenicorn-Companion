#include "installer/Requirements.hpp"

#include <QFile>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{
  struct PackageNames
  {
    QStringList rpm;
    QStringList deb;
    QStringList pacman;
  };


  const QMap<QString, PackageNames>& packageTable()
  {
    static const QMap<QString, PackageNames> table{
      {"git", {{"git"}, {"git"}, {"git"}}},
      {"cmake", {{"cmake"}, {"cmake"}, {"cmake"}}},
      {"compiler", {{"gcc-c++"}, {"g++"}, {"gcc"}}},
      {"make", {{"make"}, {"make"}, {"make"}}},
      {"tar", {{"tar"}, {"tar"}, {"tar"}}},
      {"xz", {{"xz"}, {"xz-utils"}, {"xz"}}},
      {"pkgconf", {{"pkgconf-pkg-config"}, {"pkgconf"}, {"pkgconf"}}},
      {"curl", {{"libcurl-devel"}, {"libcurl4-openssl-dev"}, {"curl"}}},
      {"opencv", {{"opencv-devel"}, {"libopencv-dev"}, {"opencv"}}},
      {"mbedtls", {{"mbedtls-devel"}, {"libmbedtls-dev"}, {"mbedtls"}}},
      {"x11", {{"libX11-devel", "libXext-devel", "libXrandr-devel"},
               {"libx11-dev", "libxext-dev", "libxrandr-dev"},
               {"libx11", "libxext", "libxrandr"}}},
      {"wayland", {{"pipewire-devel", "glib2-devel"},
                   {"libpipewire-0.3-dev", "libglib2.0-dev"},
                   {"pipewire", "glib2"}}},
    };
    return table;
  }


  bool anyTool(const HostProbe& probe, const QStringList& names)
  {
    for(const auto& name : names){
      if(probe.hasTool(name)){
        return true;
      }
    }
    return false;
  }
}


SessionType Requirements::detectSession(const QProcessEnvironment& environment)
{
  const auto type = environment.value("XDG_SESSION_TYPE").toLower();
  if(type == "wayland"){
    return SessionType::Wayland;
  }
  if(type == "x11"){
    return SessionType::X11;
  }

  // XWayland sets DISPLAY too, so Wayland has to be looked at first
  if(!environment.value("WAYLAND_DISPLAY").isEmpty()){
    return SessionType::Wayland;
  }
  if(!environment.value("DISPLAY").isEmpty()){
    return SessionType::X11;
  }
  return SessionType::Unknown;
}


QStringList Requirements::systemPath()
{
  return {"/usr/local/bin", "/usr/bin", "/bin"};
}


QProcessEnvironment Requirements::cleanEnvironment()
{
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert("PATH", systemPath().join(':'));
  environment.insert("GIT_TERMINAL_PROMPT", "0");

  static const QStringList leaky{
    "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "PKG_CONFIG_PATH", "LD_LIBRARY_PATH",
    "LIBRARY_PATH", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "CONDA_PREFIX"};
  for(const auto& name : leaky){
    environment.remove(name);
  }
  return environment;
}


int Requirements::parseMbedtlsMajor(const QByteArray& header)
{
  static const QRegularExpression pattern(R"(#\s*define\s+MBEDTLS_VERSION_MAJOR\s+(\d+))");
  const auto match = pattern.match(QString::fromUtf8(header));
  return match.hasMatch() ? match.captured(1).toInt() : 0;
}


HostProbe Requirements::systemProbe()
{
  HostProbe probe;

  probe.hasTool = [](const QString& tool){
    return !QStandardPaths::findExecutable(tool, systemPath()).isEmpty();
  };

  probe.hasModules = [](const QStringList& modules){
    const auto program = QStandardPaths::findExecutable("pkg-config", systemPath());
    if(program.isEmpty()){
      return false;
    }

    QProcess process;
    process.setProcessEnvironment(cleanEnvironment());
    process.start(program, QStringList{"--exists"} + modules);
    return process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
  };

  probe.mbedtlsMajor = [](){
    // Version 3 keeps the number in build_info.h, version 2 in version.h
    for(const auto* root : {"/usr/include", "/usr/local/include"}){
      for(const auto* name : {"build_info.h", "version.h"}){
        QFile file(QString("%1/mbedtls/%2").arg(root, name));
        if(file.open(QIODevice::ReadOnly)){
          const auto major = parseMbedtlsMajor(file.readAll());
          if(major > 0){
            return major;
          }
        }
      }
    }
    return 0;
  };

  return probe;
}


QList<Requirement> Requirements::check(SessionType session, const HostProbe& probe)
{
  QList<Requirement> list;
  auto add = [&list](const QString& id, const QString& label, bool met){
    list.append({id, label, met});
  };

  add("git", QObject::tr("git"), probe.hasTool("git"));
  add("cmake", QObject::tr("cmake"), probe.hasTool("cmake"));
  add("compiler", QObject::tr("C++ compiler"), anyTool(probe, {"c++", "g++", "clang++"}));
  add("make", QObject::tr("make"), anyTool(probe, {"make", "ninja"}));
  add("tar", QObject::tr("tar"), probe.hasTool("tar"));
  add("xz", QObject::tr("xz"), probe.hasTool("xz"));
  add("pkgconf", QObject::tr("pkg-config"), anyTool(probe, {"pkg-config", "pkgconf"}));
  add("curl", QObject::tr("libcurl development files"), probe.hasModules({"libcurl"}));
  add("opencv", QObject::tr("OpenCV development files"), probe.hasModules({"opencv4"}));
  add("mbedtls", QObject::tr("Mbed TLS %1 or newer development files").arg(MinMbedtlsMajor),
      probe.mbedtlsMajor() >= MinMbedtlsMajor);

  if(session != SessionType::Wayland){
    add("x11", QObject::tr("X11 development files"), probe.hasModules({"x11", "xext", "xrandr"}));
  }
  if(session != SessionType::X11){
    add("wayland", QObject::tr("PipeWire and GLib development files"),
        probe.hasModules({"libpipewire-0.3", "glib-2.0", "gio-2.0", "gio-unix-2.0"}));
  }
  return list;
}


bool Requirements::allMet(const QList<Requirement>& requirements)
{
  for(const auto& requirement : requirements){
    if(!requirement.met){
      return false;
    }
  }
  return true;
}


QString Requirements::installHint(PackageKind kind, const QList<Requirement>& requirements)
{
  QStringList packages;
  for(const auto& requirement : requirements){
    if(requirement.met || !packageTable().contains(requirement.id)){
      continue;
    }

    const auto& names = packageTable()[requirement.id];
    const auto& wanted = kind == PackageKind::Rpm ? names.rpm : kind == PackageKind::Deb ? names.deb : names.pacman;
    for(const auto& name : wanted){
      if(!packages.contains(name)){
        packages.append(name);
      }
    }
  }

  if(packages.isEmpty() || kind == PackageKind::None){
    return {};
  }

  const QString list = packages.join(' ');
  switch(kind){
    case PackageKind::Rpm: return "sudo dnf install " + list;
    case PackageKind::Deb: return "sudo apt install " + list;
    case PackageKind::Pacman: return "sudo pacman -S --needed " + list;
    case PackageKind::None: break;
  }
  return {};
}
