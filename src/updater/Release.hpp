#pragma once

#include <optional>

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

struct ReleaseAsset
{
  QString name;
  QUrl url;
};

// The parts of a GitHub release the updater cares about
struct ReleaseInfo
{
  QString tag;
  QString notes;
  QUrl pageUrl;
  QList<ReleaseAsset> assets;

  // Fails without a tag. Assets with an untrusted download URL are dropped.
  static std::optional<ReleaseInfo> fromJson(const QJsonObject& object);

  std::optional<ReleaseAsset> findAsset(const QString& name) const;
};

namespace Release
{
  // Accepts "0.3.0" and "v0.3.0". Anything else, prereleases included, is never newer.
  bool isNewer(const QString& current, const QString& candidate);

  // HTTPS only, plain HTTP just for loopback
  bool isTrustedUrl(const QUrl& url);

  // Looks a file up in `sha256sum` output
  std::optional<QString> checksumFor(const QByteArray& sums, const QString& fileName);
}
