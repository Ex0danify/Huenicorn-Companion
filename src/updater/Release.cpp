#include "updater/Release.hpp"

#include <QHostAddress>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVersionNumber>

namespace
{
  QVersionNumber parseVersion(QString text)
  {
    text = text.trimmed();
    if(text.startsWith('v')){
      text.remove(0, 1);
    }

    static const QRegularExpression plain("^\\d+(\\.\\d+)*$");
    return plain.match(text).hasMatch() ? QVersionNumber::fromString(text) : QVersionNumber();
  }
}


std::optional<ReleaseInfo> ReleaseInfo::fromJson(const QJsonObject& object)
{
  ReleaseInfo info;
  info.tag = object.value("tag_name").toString();
  if(info.tag.isEmpty()){
    return std::nullopt;
  }

  info.notes = object.value("body").toString();
  info.pageUrl = QUrl(object.value("html_url").toString());

  for(const auto& value : object.value("assets").toArray()){
    const auto asset = value.toObject();
    const QUrl url(asset.value("browser_download_url").toString());
    const auto name = asset.value("name").toString();
    if(!name.isEmpty() && Release::isTrustedUrl(url)){
      info.assets.append({name, url});
    }
  }

  return info;
}


std::optional<ReleaseAsset> ReleaseInfo::findAsset(const QString& name) const
{
  for(const auto& asset : assets){
    if(asset.name == name){
      return asset;
    }
  }
  return std::nullopt;
}


bool Release::isNewer(const QString& current, const QString& candidate)
{
  const auto a = parseVersion(current);
  const auto b = parseVersion(candidate);
  return !a.isNull() && !b.isNull() && QVersionNumber::compare(a, b) < 0;
}


bool Release::isTrustedUrl(const QUrl& url)
{
  if(!url.isValid() || url.host().isEmpty()){
    return false;
  }
  if(url.scheme() == "https"){
    return true;
  }
  return url.scheme() == "http" && (url.host() == "localhost" || QHostAddress(url.host()).isLoopback());
}


std::optional<QString> Release::checksumFor(const QByteArray& sums, const QString& fileName)
{
  static const QRegularExpression separator("\\s+");

  for(const auto& line : QString::fromUtf8(sums).split('\n')){
    const auto parts = line.trimmed().split(separator);
    if(parts.size() != 2){
      continue;
    }

    // Binary mode marks the name with a leading asterisk
    auto name = parts[1];
    if(name.startsWith('*')){
      name.remove(0, 1);
    }
    if(name == fileName && parts[0].size() == 64){
      return parts[0].toLower();
    }
  }
  return std::nullopt;
}
