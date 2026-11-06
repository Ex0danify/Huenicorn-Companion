#pragma once

#include <functional>

#include <QList>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include "updater/Platform.hpp"

enum class SessionType
{
  Unknown,
  X11,
  Wayland
};

// How the checks look at the machine, replaceable in tests
struct HostProbe
{
  std::function<bool(const QString& tool)> hasTool;
  // True only when every listed pkg-config module exists
  std::function<bool(const QStringList& modules)> hasModules;
  // Major version of the installed Mbed TLS headers, 0 if there are none
  std::function<int()> mbedtlsMajor;
};

struct Requirement
{
  QString id;
  QString label;
  bool met{false};
};

namespace Requirements
{
  // Daemon sources need Mbed TLS 3 or newer
  constexpr int MinMbedtlsMajor = 3;

  SessionType detectSession(const QProcessEnvironment& environment);

  // Plain system locations only, so a conda or similar prefix never leaks into the build
  QStringList systemPath();
  QProcessEnvironment cleanEnvironment();
  HostProbe systemProbe();

  int parseMbedtlsMajor(const QByteArray& header);

  QList<Requirement> check(SessionType session, const HostProbe& probe);
  bool allMet(const QList<Requirement>& requirements);

  // Install command for the missing items, empty when nothing is missing or the system is unknown
  QString installHint(PackageKind kind, const QList<Requirement>& requirements);
}
