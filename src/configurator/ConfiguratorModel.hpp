#pragma once

#include <functional>
#include <vector>

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "api/ApiClient.hpp"
#include "daemon/DaemonController.hpp"
#include "setup/SetupController.hpp"

// State and actions behind the QML configurator. Mirrors the daemon's settings
// and keeps them in sync through the REST API. Changes are persisted
// automatically shortly after the last edit.
class ConfiguratorModel : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QString daemonState READ daemonState NOTIFY daemonStateChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY daemonStateChanged)
  Q_PROPERTY(bool running READ running NOTIFY daemonStateChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY daemonStateChanged)
  Q_PROPERTY(QString version READ version NOTIFY daemonStateChanged)
  Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
  // True once real values arrived, they stay available after the daemon stops
  Q_PROPERTY(bool hasLoaded READ hasLoaded NOTIFY loadedChanged)
  Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
  Q_PROPERTY(bool saving READ saving NOTIFY dirtyChanged)

  Q_PROPERTY(QVariantList areas READ areas NOTIFY areasChanged)
  Q_PROPERTY(QString currentAreaId READ currentAreaId NOTIFY areasChanged)
  Q_PROPERTY(QString currentAreaName READ currentAreaName NOTIFY areasChanged)

  Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
  Q_PROPERTY(int activeChannelCount READ activeChannelCount NOTIFY channelsChanged)

  Q_PROPERTY(int displayWidth READ displayWidth NOTIFY settingsChanged)
  Q_PROPERTY(int displayHeight READ displayHeight NOTIFY settingsChanged)
  Q_PROPERTY(int subsampleWidth READ subsampleWidth NOTIFY settingsChanged)
  Q_PROPERTY(QVariantList subsampleCandidates READ subsampleCandidates NOTIFY settingsChanged)
  Q_PROPERTY(int refreshRate READ refreshRate NOTIFY settingsChanged)
  Q_PROPERTY(int maxRefreshRate READ maxRefreshRate NOTIFY settingsChanged)
  Q_PROPERTY(double smoothing READ smoothing NOTIFY settingsChanged)
  Q_PROPERTY(QVariantList interpolations READ interpolations NOTIFY settingsChanged)
  Q_PROPERTY(int interpolation READ interpolation NOTIFY settingsChanged)

  Q_PROPERTY(QString daemonPath READ daemonPath NOTIFY daemonPathChanged)
  Q_PROPERTY(bool canStart READ canStart NOTIFY canStartChanged)
  Q_PROPERTY(bool canChangeBinary READ canChangeBinary NOTIFY daemonStateChanged)
  Q_PROPERTY(int port READ port NOTIFY daemonStateChanged)

  Q_PROPERTY(SetupController* setup READ setup CONSTANT)

public:
  explicit ConfiguratorModel(DaemonController& controller, QObject* parent = nullptr);

  QString daemonState() const;
  QString statusText() const;
  bool running() const;
  bool busy() const;
  QString version() const;
  bool loaded() const;
  bool hasLoaded() const;
  bool dirty() const;
  bool saving() const;

  QVariantList areas() const;
  QString currentAreaId() const;
  QString currentAreaName() const;

  QVariantList channels() const;
  int activeChannelCount() const;

  int displayWidth() const;
  int displayHeight() const;
  int subsampleWidth() const;
  QVariantList subsampleCandidates() const;
  int refreshRate() const;
  int maxRefreshRate() const;
  double smoothing() const;
  QVariantList interpolations() const;
  int interpolation() const;

  QString daemonPath() const;
  bool canStart() const;
  bool canChangeBinary() const;
  int port() const;
  SetupController* setup();

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void toggleDaemon();
  Q_INVOKABLE void reconnect();
  Q_INVOKABLE void chooseDaemon();
  Q_INVOKABLE void selectArea(const QString& areaId);
  Q_INVOKABLE void setChannelActive(int channelId, bool active);
  Q_INVOKABLE void setChannelRect(int channelId, double x0, double y0, double x1, double y1);
  Q_INVOKABLE void setChannelGamma(int channelId, double gamma);
  Q_INVOKABLE void setAllChannelsGamma(double gamma);
  Q_INVOKABLE void setSubsampleWidth(int width);
  Q_INVOKABLE void setRefreshRate(int rate);
  Q_INVOKABLE void setInterpolation(int interpolation);
  Q_INVOKABLE void setSmoothing(double percent);
  Q_INVOKABLE void save();
  Q_INVOKABLE void flush();

  // Runs done once nothing is left to write. Runs right away if there is nothing to save,
  // and also if the write fails, so a dead daemon cannot block the caller.
  void flushThen(std::function<void()> done);
  Q_INVOKABLE void openWebConfigurator();

signals:
  void daemonStateChanged();
  void daemonPathChanged();
  void canStartChanged();
  void chooseDaemonRequested();
  void loadedChanged();
  void dirtyChanged();
  void areasChanged();
  void channelsChanged();
  void settingsChanged();
  void saved(bool ok);
  void requestFailed(const QString& message);

private:
  void onDaemonState();
  void clear();
  void applyChannels(const QJsonArray& channels);
  void applyAreas(const QJsonObject& object);
  void applyDisplay(const QJsonObject& object);
  void applyInterpolation(const QJsonObject& object);
  void patchChannel(int channelId, const QVariantMap& patch);
  void markDirty();
  void setLoaded(bool loaded);
  void settleFlushWaiters();
  void fail(const QString& what);

  DaemonController& m_controller;
  ApiClient m_api;
  SetupController m_setup;

  bool m_loaded{false};
  bool m_hasLoaded{false};
  bool m_dirty{false};
  int m_pending{0};
  bool m_saving{false};
  bool m_resave{false};
  QTimer m_saveTimer;
  std::vector<std::function<void()>> m_flushWaiters;

  QVariantList m_areas;
  QString m_currentAreaId;
  QVariantList m_channels;

  int m_displayWidth{1920};
  int m_displayHeight{1080};
  int m_subsampleWidth{0};
  QVariantList m_subsampleCandidates;
  int m_refreshRate{60};
  int m_maxRefreshRate{60};
  double m_smoothing{0};
  QVariantList m_interpolations;
  int m_interpolation{0};
};
