#include "configurator/ConfiguratorModel.hpp"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonObject>
#include <QtMath>

namespace
{
  // Matches Imaging::UVCorner on the daemon side
  constexpr int CornerTopLeft = 0;
  constexpr int CornerBottomRight = 3;

  // Collapses slider drags into one save
  constexpr int SaveDelayMs = 500;

  QString colorForChannel(int channelId)
  {
    static const QStringList palette = {
      "#4cc9f0", "#f72585", "#b5e048", "#ffb703", "#9d4edd", "#ff6b6b", "#2ec4b6", "#f4a261"
    };
    return palette.at(channelId % palette.size());
  }
}


ConfiguratorModel::ConfiguratorModel(DaemonController& controller, QObject* parent)
  : QObject(parent)
  , m_controller(controller)
  , m_setup(controller)
{
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(SaveDelayMs);
  connect(&m_saveTimer, &QTimer::timeout, this, &ConfiguratorModel::save);

  connect(&m_controller, &DaemonController::stateChanged, this, &ConfiguratorModel::onDaemonState);
  connect(&m_controller, &DaemonController::daemonPathChanged, this, &ConfiguratorModel::daemonPathChanged);
  connect(&m_controller, &DaemonController::daemonPathChanged, this, &ConfiguratorModel::canStartChanged);
  connect(&m_setup, &SetupController::modeChanged, this, [this](){
    if(m_setup.ready() && running()){
      m_api.setBaseUrl(m_controller.baseUrl());
      refresh();
    }
  });
  onDaemonState();
}


QString ConfiguratorModel::daemonState() const
{
  using State = DaemonController::State;
  switch(m_controller.state()){
    case State::Stopped: return "stopped";
    case State::Starting: return "starting";
    case State::Running: return "running";
    case State::Stopping: return "stopping";
    case State::Error: return "error";
  }
  return "stopped";
}


QString ConfiguratorModel::statusText() const
{
  using State = DaemonController::State;
  switch(m_controller.state()){
    case State::Stopped: return tr("Not running");
    case State::Starting: return tr("Starting...");
    case State::Running: return tr("Connected");
    case State::Stopping: return tr("Stopping...");
    case State::Error: return m_controller.lastError();
  }
  return {};
}


bool ConfiguratorModel::running() const
{
  return m_controller.state() == DaemonController::State::Running;
}


bool ConfiguratorModel::busy() const
{
  const auto state = m_controller.state();
  return state == DaemonController::State::Starting || state == DaemonController::State::Stopping;
}


QString ConfiguratorModel::version() const { return m_controller.version(); }
bool ConfiguratorModel::loaded() const { return m_loaded; }
bool ConfiguratorModel::dirty() const { return m_dirty; }
bool ConfiguratorModel::saving() const { return m_saving || m_saveTimer.isActive(); }
QVariantList ConfiguratorModel::areas() const { return m_areas; }
QString ConfiguratorModel::currentAreaId() const { return m_currentAreaId; }
QVariantList ConfiguratorModel::channels() const { return m_channels; }
int ConfiguratorModel::displayWidth() const { return m_displayWidth; }
int ConfiguratorModel::displayHeight() const { return m_displayHeight; }
int ConfiguratorModel::subsampleWidth() const { return m_subsampleWidth; }
QVariantList ConfiguratorModel::subsampleCandidates() const { return m_subsampleCandidates; }
int ConfiguratorModel::refreshRate() const { return m_refreshRate; }
int ConfiguratorModel::maxRefreshRate() const { return m_maxRefreshRate; }
double ConfiguratorModel::smoothing() const { return m_smoothing; }
QVariantList ConfiguratorModel::interpolations() const { return m_interpolations; }
int ConfiguratorModel::interpolation() const { return m_interpolation; }
QString ConfiguratorModel::daemonPath() const { return m_controller.daemonPath(); }
bool ConfiguratorModel::canStart() const { return m_controller.canStart(); }
bool ConfiguratorModel::canChangeBinary() const { return m_controller.canChangeBinary(); }
int ConfiguratorModel::port() const { return m_controller.port(); }
SetupController* ConfiguratorModel::setup() { return &m_setup; }


QString ConfiguratorModel::currentAreaName() const
{
  for(const auto& area : m_areas){
    const auto map = area.toMap();
    if(map.value("id").toString() == m_currentAreaId){
      return map.value("name").toString();
    }
  }
  return {};
}


int ConfiguratorModel::activeChannelCount() const
{
  int count = 0;
  for(const auto& channel : m_channels){
    count += channel.toMap().value("active").toBool() ? 1 : 0;
  }
  return count;
}


void ConfiguratorModel::onDaemonState()
{
  emit daemonStateChanged();
  emit canStartChanged();

  // Loading waits for the setup probe, a daemon in setup mode lacks the normal API
  if(running()){
    m_api.setBaseUrl(m_controller.baseUrl());
    if(m_setup.ready()){
      refresh();
    }
  }
  else{
    clear();
  }
}


void ConfiguratorModel::clear()
{
  m_areas.clear();
  m_channels.clear();
  m_currentAreaId.clear();
  m_dirty = false;
  m_resave = false;
  m_saveTimer.stop();
  setLoaded(false);
  emit areasChanged();
  emit channelsChanged();
  emit dirtyChanged();
  settleFlushWaiters();
}


void ConfiguratorModel::setLoaded(bool loaded)
{
  if(m_loaded != loaded){
    m_loaded = loaded;
    emit loadedChanged();
  }
}


void ConfiguratorModel::markDirty()
{
  m_dirty = true;
  m_saveTimer.start();
  emit dirtyChanged();
}


void ConfiguratorModel::fail(const QString& what)
{
  emit requestFailed(what);
}


void ConfiguratorModel::refresh()
{
  if(!running()){
    return;
  }

  m_pending = 4;
  auto done = [this](){
    if(--m_pending == 0){
      m_dirty = false;
      m_saveTimer.stop();
      emit dirtyChanged();
      setLoaded(true);
    }
  };

  m_api.get("/api/entertainmentConfigurations", [this, done](bool ok, const QJsonDocument& doc){
    if(ok){ applyAreas(doc.object()); } else { fail(tr("Could not load entertainment areas")); }
    done();
  });
  m_api.get("/api/channels", [this, done](bool ok, const QJsonDocument& doc){
    if(ok){ applyChannels(doc.array()); } else { fail(tr("Could not load channels")); }
    done();
  });
  m_api.get("/api/displayInfo", [this, done](bool ok, const QJsonDocument& doc){
    if(ok){ applyDisplay(doc.object()); } else { fail(tr("Could not load display info")); }
    done();
  });
  m_api.get("/api/interpolationInfo", [this, done](bool ok, const QJsonDocument& doc){
    if(ok){ applyInterpolation(doc.object()); } else { fail(tr("Could not load interpolation info")); }
    done();
  });
}


void ConfiguratorModel::toggleDaemon()
{
  using State = DaemonController::State;
  const auto state = m_controller.state();
  if(state == State::Running || state == State::Starting){
    m_controller.stop();
  }
  else if(state == State::Stopped || state == State::Error){
    m_controller.start();
  }
}


void ConfiguratorModel::reconnect()
{
  // The binary may have been restored or removed since the last state change
  emit canStartChanged();
  m_controller.checkNow();
}


void ConfiguratorModel::chooseDaemon()
{
  emit chooseDaemonRequested();
}


void ConfiguratorModel::applyAreas(const QJsonObject& object)
{
  m_areas.clear();
  for(const auto& value : object.value("entertainmentConfigurations").toArray()){
    const auto area = value.toObject();
    m_areas.append(QVariantMap{
      {"id", area.value("entertainmentConfigurationId").toString()},
      {"name", area.value("name").toString()}
    });
  }
  m_currentAreaId = object.value("currentEntertainmentConfigurationId").toString();
  emit areasChanged();
}


void ConfiguratorModel::applyChannels(const QJsonArray& channels)
{
  m_channels.clear();
  for(const auto& value : channels){
    const auto channel = value.toObject();
    const int id = channel.value("channelId").toInt();
    const auto uvs = channel.value("uvs").toObject();
    const auto a = uvs.value("uvA").toObject();
    const auto b = uvs.value("uvB").toObject();

    QStringList names;
    for(const auto& device : channel.value("devices").toArray()){
      const auto name = device.toObject().value("name").toString();
      if(!name.isEmpty()){
        names << name;
      }
    }

    m_channels.append(QVariantMap{
      {"channelId", id},
      {"name", names.isEmpty() ? tr("Channel %1").arg(id) : names.join(", ")},
      {"active", channel.value("active").toBool()},
      {"x0", a.value("x").toDouble()},
      {"y0", a.value("y").toDouble()},
      {"x1", b.value("x").toDouble()},
      {"y1", b.value("y").toDouble()},
      {"gamma", channel.value("gammaFactor").toDouble()},
      {"color", colorForChannel(id)}
    });
  }
  emit channelsChanged();
}


void ConfiguratorModel::applyDisplay(const QJsonObject& object)
{
  m_displayWidth = object.value("x").toInt(1920);
  m_displayHeight = object.value("y").toInt(1080);
  m_subsampleWidth = object.value("subsampleWidth").toInt();
  m_refreshRate = object.value("selectedRefreshRate").toInt();
  m_maxRefreshRate = qMax(1, object.value("maxRefreshRate").toInt(m_refreshRate));
  m_smoothing = object.value("selectedTransitionSmoothing").toDouble();

  m_subsampleCandidates.clear();
  for(const auto& value : object.value("subsampleResolutionCandidates").toArray()){
    const auto candidate = value.toObject();
    const int x = candidate.value("x").toInt();
    const int y = candidate.value("y").toInt();
    const int percent = m_displayWidth > 0 ? qRound(100.0 * x / m_displayWidth) : 0;
    m_subsampleCandidates.append(QVariantMap{
      {"width", x},
      {"label", QString("%1×%2").arg(x).arg(y)},
      {"percent", percent}
    });
  }
  emit settingsChanged();
}


void ConfiguratorModel::applyInterpolation(const QJsonObject& object)
{
  m_interpolations.clear();
  for(const auto& value : object.value("available").toArray()){
    const auto entry = value.toObject();
    for(auto it = entry.begin(); it != entry.end(); ++it){
      m_interpolations.append(QVariantMap{{"name", it.key()}, {"value", it.value().toInt()}});
    }
  }
  m_interpolation = object.value("current").toInt();
  emit settingsChanged();
}


void ConfiguratorModel::patchChannel(int channelId, const QVariantMap& patch)
{
  for(auto& entry : m_channels){
    auto map = entry.toMap();
    if(map.value("channelId").toInt() == channelId){
      for(auto it = patch.begin(); it != patch.end(); ++it){
        map.insert(it.key(), it.value());
      }
      entry = map;
      break;
    }
  }
}


void ConfiguratorModel::selectArea(const QString& areaId)
{
  if(areaId == m_currentAreaId){
    return;
  }

  m_api.put("/api/setEntertainmentConfiguration", areaId, [this, areaId](bool ok, const QJsonDocument& doc){
    const auto object = doc.object();
    if(!ok || !object.value("succeeded").toBool()){
      fail(tr("Could not switch entertainment area"));
      return;
    }
    m_currentAreaId = areaId;
    emit areasChanged();
    applyChannels(object.value("channels").toArray());
    markDirty();
  });
}


void ConfiguratorModel::setChannelActive(int channelId, bool active)
{
  patchChannel(channelId, {{"active", active}});
  emit channelsChanged();

  m_api.post(QString("/api/setChannelActivity/%1").arg(channelId), QJsonObject{{"active", active}}, [this](bool ok, const QJsonDocument& doc){
    const auto object = doc.object();
    if(!ok || !object.value("succeeded").toBool()){
      fail(tr("Could not change channel"));
      refresh();
      return;
    }
    applyChannels(object.value("channels").toArray());
    markDirty();
  });
}


void ConfiguratorModel::setChannelRect(int channelId, double x0, double y0, double x1, double y1)
{
  x0 = qBound(0.0, x0, 1.0);
  y0 = qBound(0.0, y0, 1.0);
  x1 = qBound(0.0, x1, 1.0);
  y1 = qBound(0.0, y1, 1.0);
  patchChannel(channelId, {{"x0", x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}});
  emit channelsChanged();

  const auto path = QString("/api/setChannelUV/%1").arg(channelId);
  auto corner = [](int type, double x, double y){
    return QJsonObject{{"type", type}, {"x", x}, {"y", y}};
  };

  // The daemon pushes the opposite corner along, so the second call settles it
  m_api.put(path, corner(CornerTopLeft, x0, y0), [this, path, channelId, x1, y1, corner](bool ok, const QJsonDocument&){
    if(!ok){
      fail(tr("Could not move channel"));
      refresh();
      return;
    }

    m_api.put(path, corner(CornerBottomRight, x1, y1), [this, channelId](bool ok, const QJsonDocument& doc){
      if(!ok){
        fail(tr("Could not move channel"));
        refresh();
        return;
      }
      const auto object = doc.object();
      const auto a = object.value("uvA").toObject();
      const auto b = object.value("uvB").toObject();
      patchChannel(channelId, {
        {"x0", a.value("x").toDouble()}, {"y0", a.value("y").toDouble()},
        {"x1", b.value("x").toDouble()}, {"y1", b.value("y").toDouble()}
      });
      emit channelsChanged();
      markDirty();
    });
  });
}


void ConfiguratorModel::setChannelGamma(int channelId, double gamma)
{
  gamma = qBound(-1.0, gamma, 1.0);
  patchChannel(channelId, {{"gamma", gamma}});
  emit channelsChanged();

  m_api.put(QString("/api/setChannelGammaFactor/%1").arg(channelId), gamma, [this](bool ok, const QJsonDocument&){
    if(ok){
      markDirty();
    }
    else{
      fail(tr("Could not set gamma"));
    }
  });
}


void ConfiguratorModel::setSubsampleWidth(int width)
{
  m_api.put("/api/setSubsampleWidth", width, [this](bool ok, const QJsonDocument& doc){
    if(!ok){
      fail(tr("Could not set resolution"));
      return;
    }
    m_subsampleWidth = doc.object().value("subsampleWidth").toInt(m_subsampleWidth);
    emit settingsChanged();
    markDirty();
  });
}


void ConfiguratorModel::setRefreshRate(int rate)
{
  rate = qBound(1, rate, m_maxRefreshRate);
  m_refreshRate = rate;
  emit settingsChanged();

  m_api.put("/api/setRefreshRate", rate, [this](bool ok, const QJsonDocument& doc){
    if(!ok){
      fail(tr("Could not set refresh rate"));
      return;
    }
    m_refreshRate = doc.object().value("refreshRate").toInt(m_refreshRate);
    emit settingsChanged();
    markDirty();
  });
}


void ConfiguratorModel::setInterpolation(int interpolation)
{
  m_interpolation = interpolation;
  emit settingsChanged();

  m_api.put("/api/setInterpolation", interpolation, [this](bool ok, const QJsonDocument&){
    if(ok){
      markDirty();
    }
    else{
      fail(tr("Could not set interpolation"));
    }
  });
}


void ConfiguratorModel::setSmoothing(double percent)
{
  percent = qBound(0.0, percent, 95.0);
  m_smoothing = percent;
  emit settingsChanged();

  m_api.put("/api/setTransitionSmoothing", percent, [this](bool ok, const QJsonDocument&){
    if(ok){
      markDirty();
    }
    else{
      fail(tr("Could not set smoothing"));
    }
  });
}


void ConfiguratorModel::save()
{
  m_saveTimer.stop();
  if(m_saving){
    m_resave = true;
    return;
  }

  m_saving = true;
  m_api.post("/api/saveProfile", QJsonObject{}, [this](bool ok, const QJsonDocument&){
    m_saving = false;
    if(ok){
      if(m_resave){
        m_resave = false;
        save();
        return;
      }
      // An edit that landed during the request still has its own save queued
      m_dirty = m_saveTimer.isActive();
    }
    m_resave = false;
    emit dirtyChanged();
    emit saved(ok);
    // A failed write is final here, retrying forever would hold up the caller
    if(!ok){
      m_saveTimer.stop();
    }
    settleFlushWaiters();
  });
  emit dirtyChanged();
}


void ConfiguratorModel::settleFlushWaiters()
{
  if(m_saving || m_saveTimer.isActive()){
    return;
  }
  auto waiters = std::move(m_flushWaiters);
  m_flushWaiters.clear();
  for(const auto& done : waiters){
    done();
  }
}


void ConfiguratorModel::flushThen(std::function<void()> done)
{
  if(!running() || (!m_dirty && !m_saving && !m_saveTimer.isActive())){
    done();
    return;
  }
  m_flushWaiters.push_back(std::move(done));
  save();
}


void ConfiguratorModel::flush()
{
  if(m_saveTimer.isActive()){
    save();
  }
}


void ConfiguratorModel::openWebConfigurator()
{
  QDesktopServices::openUrl(m_controller.baseUrl());
}
