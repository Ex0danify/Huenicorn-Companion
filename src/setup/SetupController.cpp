#include "setup/SetupController.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>

namespace
{
  // Roughly 30 seconds to walk to the bridge and press the pairing button
  constexpr int MaxPairAttempts = 15;

  // The daemon may wait on a screen sharing prompt before it serves its API again
  constexpr int MaxFinishAttempts = 240;

  // Address lookups and pairing talk to the bridge over TLS, which can be slow
  constexpr int BridgeTimeoutMs = 15000;
  constexpr int DiscoveryTimeoutMs = 20000;
}


SetupController::SetupController(DaemonController& controller, QObject* parent)
  : QObject(parent)
  , m_controller(controller)
{
  connect(&m_controller, &DaemonController::stateChanged, this, &SetupController::onDaemonState);
  onDaemonState();
}


bool SetupController::active() const { return m_mode == Mode::Setup; }
bool SetupController::ready() const { return m_mode == Mode::Ready; }
SetupController::Mode SetupController::mode() const { return m_mode; }
QString SetupController::step() const { return m_step; }
bool SetupController::busy() const { return m_discovering || m_pairing || m_validating; }
bool SetupController::discovering() const { return m_discovering; }
bool SetupController::pairing() const { return m_pairing; }
QString SetupController::error() const { return m_error; }
QStringList SetupController::bridges() const { return m_bridges; }
QString SetupController::bridgeAddress() const { return m_bridgeAddress; }


void SetupController::setRetryIntervalMs(int ms)
{
  m_pairIntervalMs = ms;
  m_finishIntervalMs = ms;
  m_finishDelayMs = ms;
}


void SetupController::setFinishDelayMs(int ms)
{
  m_finishDelayMs = ms;
}


void SetupController::onDaemonState()
{
  if(m_controller.state() == DaemonController::State::Running){
    probe();
  }
  else{
    reset();
  }
}


void SetupController::probe()
{
  const int epoch = m_epoch;
  m_api.setBaseUrl(m_controller.baseUrl());

  // Only the setup server knows this route
  m_api.get("/api/configFilePath", [this, epoch](bool ok, const QJsonDocument& doc){
    if(epoch != m_epoch){
      return;
    }

    if(ok && doc.object().contains("configFilePath")){
      setMode(Mode::Setup);
      discover();
    }
    else{
      setMode(Mode::Ready);
    }
  });
}


void SetupController::reset()
{
  m_epoch++;
  m_controller.setPollingPaused(false);
  m_bridges.clear();
  m_bridgeAddress.clear();
  m_error.clear();
  m_step = "bridge";
  m_discovering = m_pairing = m_validating = false;
  emit bridgesChanged();
  emit errorChanged();
  emit stepChanged();
  emit busyChanged();
  setMode(Mode::Unknown);
}


void SetupController::setMode(Mode mode)
{
  if(m_mode != mode){
    m_mode = mode;
    emit modeChanged();
  }
}


void SetupController::setStep(const QString& step)
{
  if(m_step != step){
    m_step = step;
    emit stepChanged();
  }
}


void SetupController::setError(const QString& error)
{
  if(m_error != error){
    m_error = error;
    emit errorChanged();
  }
}


void SetupController::setBusy(bool discovering, bool pairing)
{
  m_discovering = discovering;
  m_pairing = pairing;
  emit busyChanged();
}


void SetupController::discover()
{
  if(m_discovering){
    return;
  }

  const int epoch = m_epoch;
  setError({});
  setBusy(true, m_pairing);

  m_api.get("/api/autodetectBridge", [this, epoch](bool ok, const QJsonDocument& doc){
    if(epoch != m_epoch){
      return;
    }
    setBusy(false, m_pairing);

    const auto object = doc.object();
    if(!ok || !object.value("succeeded").toBool()){
      setError(object.value("error").toString(tr("Could not look for a bridge. Enter its address manually.")));
      return;
    }

    QStringList found;
    for(const auto& entry : object.value("bridges").toArray()){
      const auto address = entry.toObject().value("internalipaddress").toString();
      if(!address.isEmpty()){
        found.append(address);
      }
    }

    m_bridges = found;
    if(!found.isEmpty()){
      m_bridgeAddress = found.first();
    }
    else{
      setError(tr("No bridge found on your network. Enter its address manually."));
    }
    emit bridgesChanged();
  }, DiscoveryTimeoutMs);
}


void SetupController::submitBridge(const QString& address)
{
  const auto trimmed = address.trimmed();
  if(trimmed.isEmpty()){
    setError(tr("Please enter the address of your bridge."));
    return;
  }
  if(m_validating){
    return;
  }

  const int epoch = m_epoch;
  setError({});
  m_validating = true;
  emit busyChanged();

  m_api.put("/api/validateBridgeAddress", QJsonObject{{"bridgeAddress", trimmed}}, [this, epoch, trimmed](bool ok, const QJsonDocument& doc){
    if(epoch != m_epoch){
      return;
    }
    m_validating = false;
    emit busyChanged();

    if(ok && doc.object().value("succeeded").toBool()){
      m_bridgeAddress = trimmed;
      emit bridgesChanged();
      setStep("pair");
    }
    else{
      setError(tr("Could not find a Hue bridge at this address."));
    }
  }, BridgeTimeoutMs);
}


void SetupController::pair()
{
  if(m_pairing){
    return;
  }

  m_pairAttempts = 0;
  setError({});
  setBusy(m_discovering, true);
  tryRegister();
}


void SetupController::stopPairing()
{
  if(m_pairing){
    m_pairEpoch++;
    setBusy(m_discovering, false);
  }
}


void SetupController::tryRegister()
{
  const int epoch = m_epoch;
  const int pairEpoch = m_pairEpoch;
  m_pairAttempts++;

  m_api.put("/api/registerNewUser", QJsonValue(QJsonValue::Null), [this, epoch, pairEpoch](bool ok, const QJsonDocument& doc){
    if(epoch != m_epoch || pairEpoch != m_pairEpoch){
      return;
    }

    const auto object = doc.object();
    if(ok && object.value("succeeded").toBool()){
      setBusy(m_discovering, false);
      finish();
      return;
    }

    const auto reason = object.value("error").toString();
    if(ok && reason.contains("link button", Qt::CaseInsensitive) && m_pairAttempts < MaxPairAttempts){
      QTimer::singleShot(m_pairIntervalMs, this, [this, epoch, pairEpoch](){
        if(epoch == m_epoch && pairEpoch == m_pairEpoch){
          tryRegister();
        }
      });
      return;
    }

    setBusy(m_discovering, false);
    if(!ok){
      setError(tr("The daemon did not answer. Try again."));
    }
    else if(reason.contains("link button", Qt::CaseInsensitive)){
      setError(tr("The pairing button was not pressed in time. Press it and try again."));
    }
    else if(reason.contains("unreachable", Qt::CaseInsensitive)){
      setError(tr("Could not reach the bridge."));
    }
    else{
      setError(reason.isEmpty() ? tr("Pairing failed.") : reason);
    }
  }, BridgeTimeoutMs);
}


void SetupController::submitCredentials(const QString& username, const QString& clientKey)
{
  const auto user = username.trimmed();
  const auto key = clientKey.trimmed().toUpper();

  if(user.isEmpty() || key.isEmpty()){
    setError(tr("Please enter both the username and the client key."));
    return;
  }
  if(!QRegularExpression("^[A-F0-9]{32}$").match(key).hasMatch()){
    setError(tr("The client key must be 32 hexadecimal characters."));
    return;
  }
  if(m_validating){
    return;
  }

  const int epoch = m_epoch;
  setError({});
  m_validating = true;
  emit busyChanged();

  m_api.put("/api/validateCredentials", QJsonObject{{"username", user}, {"clientkey", key}}, [this, epoch](bool ok, const QJsonDocument& doc){
    if(epoch != m_epoch){
      return;
    }
    m_validating = false;
    emit busyChanged();

    if(ok && doc.object().value("succeeded").toBool()){
      finish();
    }
    else{
      setError(tr("The bridge rejected these credentials."));
    }
  }, BridgeTimeoutMs);
}


void SetupController::finish()
{
  const int epoch = m_epoch;
  setError({});
  setStep("finishing");
  m_controller.setPollingPaused(true);

  // The setup server stops right after answering, so the reply itself carries no information.
  // A request that reaches it while it shuts down crashes the daemon, hence the grace period.
  m_api.post("/api/finishSetup", QJsonValue(QJsonValue::Null), [this, epoch](bool, const QJsonDocument&){
    if(epoch != m_epoch){
      return;
    }
    m_finishAttempts = 0;
    QTimer::singleShot(m_finishDelayMs, this, [this, epoch](){
      if(epoch == m_epoch){
        waitForNormalMode();
      }
    });
  });
}


void SetupController::waitForNormalMode()
{
  const int epoch = m_epoch;
  m_finishAttempts++;

  // Only the normal API knows this route
  m_api.get("/api/webUIStatus", [this, epoch](bool ok, const QJsonDocument&){
    if(epoch != m_epoch){
      return;
    }

    if(ok){
      m_controller.setPollingPaused(false);
      setStep("bridge");
      setMode(Mode::Ready);
      return;
    }

    if(m_finishAttempts >= MaxFinishAttempts){
      m_controller.setPollingPaused(false);
      setError(tr("The daemon did not come up after setup. Check the log from the tray menu."));
      return;
    }

    QTimer::singleShot(m_finishIntervalMs, this, [this, epoch](){
      if(epoch == m_epoch){
        waitForNormalMode();
      }
    });
  }, 1000);
}


void SetupController::back()
{
  if(m_step == "pair"){
    stopPairing();
    setError({});
    setStep("bridge");
  }
}


void SetupController::retry()
{
  m_epoch++;
  m_controller.setPollingPaused(false);
  m_discovering = m_pairing = m_validating = false;
  emit busyChanged();
  setError({});
  setStep("bridge");
  probe();
}


void SetupController::cancel()
{
  // The setup server has no stop route, aborting makes the daemon exit on its own
  m_api.post("/api/abort", QJsonValue(QJsonValue::Null), nullptr);
}
