#pragma once

#include <optional>

#include <QByteArray>
#include <QList>
#include <QStringList>

#include "updater/Release.hpp"

enum class PackageKind
{
  None,
  Rpm,
  Deb,
  Pacman
};

struct SystemInfo
{
  PackageKind kind{PackageKind::None};
  // os-release ID followed by ID_LIKE, closest relative first
  QStringList ids;
};

namespace Platform
{
  SystemInfo parseOsRelease(const QByteArray& content);
  SystemInfo detect();

  // The release package that fits this system, if there is one
  std::optional<ReleaseAsset> pickAsset(const QList<ReleaseAsset>& assets, const SystemInfo& system);

  // Program and arguments that install a local package file, before any privilege elevation
  QStringList installCommand(PackageKind kind, const QString& file);
}
