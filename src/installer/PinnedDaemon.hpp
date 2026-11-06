#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// A dependency the daemon build would otherwise download without any hash check
struct DependencySource
{
  // Name the daemon's CMake passes to FetchContent, and the package it tries first
  QString fetchName;
  QString packageName;
  QString url;
  QString sha256;
};

struct DaemonPin
{
  QString repoUrl;
  QString tag;
  // Full commit the tag must resolve to, so a moved tag is caught
  QString commit;
  QList<DependencySource> dependencies;
  // Missing includes in the daemon sources that newer compilers no longer paper over
  QStringList compilerFlags;

  QString version() const;
};

namespace PinnedDaemon
{
  // Bump tag, commit and every dependency hash together
  DaemonPin current();
}
