#include "updater/Platform.hpp"

#include <QFile>

namespace
{
  QString suffixFor(PackageKind kind)
  {
    switch(kind){
      case PackageKind::Rpm: return ".rpm";
      case PackageKind::Deb: return ".deb";
      case PackageKind::Pacman: return ".pkg.tar.zst";
      case PackageKind::None: break;
    }
    return {};
  }


  PackageKind kindForId(const QString& id)
  {
    if(id == "fedora" || id == "rhel" || id == "centos"){
      return PackageKind::Rpm;
    }
    if(id == "debian" || id == "ubuntu"){
      return PackageKind::Deb;
    }
    if(id == "arch"){
      return PackageKind::Pacman;
    }
    return PackageKind::None;
  }
}


SystemInfo Platform::parseOsRelease(const QByteArray& content)
{
  QString id;
  QStringList like;

  for(const auto& rawLine : QString::fromUtf8(content).split('\n')){
    const auto line = rawLine.trimmed();
    const auto equals = line.indexOf('=');
    if(line.startsWith('#') || equals < 0){
      continue;
    }

    const auto key = line.left(equals);
    auto value = line.mid(equals + 1);
    if(value.size() >= 2 && (value.startsWith('"') || value.startsWith('\'')) && value.endsWith(value[0])){
      value = value.mid(1, value.size() - 2);
    }

    if(key == "ID"){
      id = value.toLower();
    }
    else if(key == "ID_LIKE"){
      like = value.toLower().split(' ', Qt::SkipEmptyParts);
    }
  }

  SystemInfo info;
  if(!id.isEmpty()){
    info.ids.append(id);
  }
  info.ids.append(like);

  for(const auto& candidate : info.ids){
    info.kind = kindForId(candidate);
    if(info.kind != PackageKind::None){
      break;
    }
  }
  return info;
}


SystemInfo Platform::detect()
{
  QFile file("/etc/os-release");
  if(!file.open(QIODevice::ReadOnly)){
    return {};
  }
  return parseOsRelease(file.readAll());
}


std::optional<ReleaseAsset> Platform::pickAsset(const QList<ReleaseAsset>& assets, const SystemInfo& system)
{
  const auto suffix = suffixFor(system.kind);
  if(suffix.isEmpty()){
    return std::nullopt;
  }

  QList<ReleaseAsset> candidates;
  for(const auto& asset : assets){
    if(asset.name.startsWith("huenicorn-qt-") && asset.name.endsWith(suffix)){
      candidates.append(asset);
    }
  }

  // Assets are named after the distro they were built on, so prefer the closest relative
  for(const auto& id : system.ids){
    for(const auto& asset : candidates){
      if(asset.name.contains("-" + id)){
        return asset;
      }
    }
  }

  if(candidates.isEmpty()){
    return std::nullopt;
  }
  return candidates.first();
}


QStringList Platform::installCommand(PackageKind kind, const QString& file)
{
  switch(kind){
    case PackageKind::Rpm: return {"dnf", "install", "-y", file};
    // pkexec starts with a clean environment, so apt would otherwise try to ask questions
    case PackageKind::Deb: return {"env", "DEBIAN_FRONTEND=noninteractive", "apt-get", "install", "-y", file};
    case PackageKind::Pacman: return {"pacman", "-U", "--noconfirm", file};
    case PackageKind::None: break;
  }
  return {};
}
