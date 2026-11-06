#include "daemon/DaemonController.hpp"

#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>

namespace
{
  // A few missed polls in a row before a running daemon counts as gone
  constexpr int MaxMissedPolls = 2;
}


DaemonController::DaemonController(QObject* parent)
  : QObject(parent)
{
  m_pollTimer.setInterval(2000);
  connect(&m_pollTimer, &QTimer::timeout, this, &DaemonController::poll);

  m_stopTimer.setSingleShot(true);
  connect(&m_stopTimer, &QTimer::timeout, this, &DaemonController::onStopTimeout);

  // Startup can be slow while the daemon talks to the bridge
  m_startTimer.setSingleShot(true);
  connect(&m_startTimer, &QTimer::timeout, this, [this](){
    if(m_state == State::Starting){
      m_process.terminate();
      fail(tr("Daemon did not become ready in time"));
    }
  });

  connect(&m_process, &QProcess::finished, this, &DaemonController::onProcessFinished);
  connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error){
    if(error == QProcess::FailedToStart){
      fail(tr("Could not start %1").arg(m_daemonPath));
    }
  });
  connect(&m_process, &QProcess::readyReadStandardOutput, this, [this](){
    flushOutput(m_process.readAllStandardOutput(), m_outBuffer);
  });
  connect(&m_process, &QProcess::readyReadStandardError, this, [this](){
    flushOutput(m_process.readAllStandardError(), m_errBuffer);
  });
}


DaemonController::State DaemonController::state() const
{
  return m_state;
}


QString DaemonController::lastError() const
{
  return m_lastError;
}


QString DaemonController::version() const
{
  return m_version;
}


bool DaemonController::ownsProcess() const
{
  return m_process.state() != QProcess::NotRunning;
}


QString DaemonController::daemonPath() const
{
  return m_daemonPath;
}


void DaemonController::setDaemonPath(const QString& path)
{
  if(m_daemonPath != path){
    m_daemonPath = path;
    emit daemonPathChanged();
  }
}


int DaemonController::port() const
{
  return m_port;
}


void DaemonController::setPort(int port)
{
  m_port = port;
}


QUrl DaemonController::baseUrl() const
{
  return QUrl(QString("http://127.0.0.1:%1").arg(m_port));
}


void DaemonController::setPollIntervalMs(int ms)
{
  m_pollTimer.setInterval(ms);
}


void DaemonController::setStopTimeoutMs(int ms)
{
  m_stopTimeoutMs = ms;
}


void DaemonController::setPollingPaused(bool paused)
{
  m_pollPaused = paused;
}


void DaemonController::beginMonitoring()
{
  m_pollTimer.start();
  poll();
}


void DaemonController::start()
{
  if(m_state == State::Starting || m_state == State::Running || m_state == State::Stopping){
    return;
  }

  if(m_daemonPath.isEmpty()){
    fail(tr("Huenicorn daemon binary not found. Set its path in the settings."));
    return;
  }

  m_lastError.clear();
  m_lastOutput.clear();
  m_version.clear();
  m_missedPolls = 0;
  setState(State::Starting);

  // The daemon logs through a block-buffered stdout when piped, so force line buffering
  const auto stdbuf = QStandardPaths::findExecutable("stdbuf");
  if(stdbuf.isEmpty()){
    m_process.setProgram(m_daemonPath);
    m_process.setArguments({});
  }
  else{
    m_process.setProgram(stdbuf);
    m_process.setArguments({"-oL", "-eL", m_daemonPath});
  }
  m_process.start();
  m_startTimer.start(m_startTimeoutMs);

  if(!m_pollTimer.isActive()){
    m_pollTimer.start();
  }
}


void DaemonController::stop()
{
  if(m_state == State::Stopped || m_state == State::Stopping){
    return;
  }

  if(m_state == State::Error && !ownsProcess()){
    setState(State::Stopped);
    return;
  }

  setState(State::Stopping);
  m_startTimer.stop();
  m_stopTimer.start(m_stopTimeoutMs);
  requestRemoteStop();
}


void DaemonController::restart()
{
  if(m_state == State::Stopped || m_state == State::Error){
    start();
    return;
  }

  m_restartPending = true;
  stop();
}


void DaemonController::setState(State state)
{
  if(m_state == state){
    return;
  }

  m_state = state;
  emit stateChanged(state);
}


void DaemonController::fail(const QString& message)
{
  m_lastError = message;
  m_startTimer.stop();
  m_stopTimer.stop();
  m_restartPending = false;
  setState(State::Error);
}


void DaemonController::poll()
{
  if(m_pollPaused){
    return;
  }

  m_api.setBaseUrl(baseUrl());
  m_api.get("/api/version", [this](bool ok, const QJsonDocument& doc){
    onPollReply(ok, ok ? doc.object().value("version").toString() : QString());
  }, 1500);
}


void DaemonController::onPollReply(bool reachable, const QString& version)
{
  if(reachable){
    m_missedPolls = 0;
    m_version = version;
    m_startTimer.stop();

    if(m_state == State::Stopped || m_state == State::Starting || m_state == State::Error){
      // Also adopts a daemon someone else started
      m_lastError.clear();
      setState(State::Running);
    }
    return;
  }

  if(m_state == State::Running && !ownsProcess()){
    if(++m_missedPolls >= MaxMissedPolls){
      setState(State::Stopped);
    }
  }
}


void DaemonController::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
  m_stopTimer.stop();
  m_startTimer.stop();

  const bool wasStopping = (m_state == State::Stopping);
  const bool restart = m_restartPending;
  m_restartPending = false;

  if(wasStopping || m_state == State::Stopped){
    setState(State::Stopped);
    if(restart){
      start();
    }
    return;
  }

  // Exited by itself, e.g. missing bridge setup or crash
  if(status == QProcess::CrashExit || exitCode != 0){
    // Flush unterminated output, the reason (e.g. a loader error) is usually in the last line
    flushOutput("\n", m_errBuffer);
    flushOutput("\n", m_outBuffer);
    auto message = tr("Daemon exited unexpectedly (code %1)").arg(exitCode);
    if(!m_lastOutput.isEmpty()){
      message += ": " + m_lastOutput;
    }
    fail(message);
  }
  else{
    setState(State::Stopped);
  }
}


void DaemonController::onStopTimeout()
{
  if(ownsProcess()){
    m_process.terminate();
    // SIGTERM is handled by the daemon, kill is the last resort
    QTimer::singleShot(m_stopTimeoutMs, this, [this](){
      if(ownsProcess()){
        m_process.kill();
      }
    });
    return;
  }

  fail(tr("Daemon did not stop"));
}


void DaemonController::requestRemoteStop()
{
  m_api.setBaseUrl(baseUrl());
  m_api.post("/api/stop", QJsonObject{}, [this](bool ok, const QJsonDocument&){
    if(m_state != State::Stopping){
      return;
    }

    if(!ok && ownsProcess()){
      // API not reachable yet, fall back to a signal
      m_process.terminate();
    }
    else if(!ownsProcess()){
      // External daemon: its API going away is the only signal we get
      probeUntilGone();
    }
  }, 2000);
}


void DaemonController::probeUntilGone()
{
  QTimer::singleShot(300, this, [this](){
    if(m_state != State::Stopping){
      return;
    }

    m_api.get("/api/version", [this](bool alive, const QJsonDocument&){
      if(m_state != State::Stopping){
        return;
      }

      if(alive){
        probeUntilGone();
      }
      else{
        m_stopTimer.stop();
        setState(State::Stopped);
      }
    }, 800);
  });
}


void DaemonController::flushOutput(const QByteArray& data, QByteArray& buffer)
{
  buffer.append(data);
  qsizetype newline;
  while((newline = buffer.indexOf('\n')) >= 0){
    const auto line = QString::fromUtf8(buffer.left(newline)).trimmed();
    if(!line.isEmpty()){
      m_lastOutput = line;
    }
    emit logLine(line);
    buffer.remove(0, newline + 1);
  }
}
