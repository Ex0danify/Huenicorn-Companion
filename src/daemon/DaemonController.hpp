#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include "api/ApiClient.hpp"

// Supervises the huenicorn daemon: owns the child process when we started it,
// and tracks an externally started daemon through its REST API.
class DaemonController : public QObject
{
  Q_OBJECT

public:
  enum class State
  {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error
  };
  Q_ENUM(State)

  explicit DaemonController(QObject* parent = nullptr);

  State state() const;
  QString lastError() const;
  QString version() const;
  bool ownsProcess() const;

  QString daemonPath() const;
  void setDaemonPath(const QString& path);

  int port() const;
  void setPort(int port);

  QUrl baseUrl() const;

  // Poll interval and shutdown grace period are tunable for tests
  void setPollIntervalMs(int ms);
  void setStopTimeoutMs(int ms);

  void beginMonitoring();

public slots:
  void start();
  void stop();
  void restart();

signals:
  void stateChanged(DaemonController::State state);
  void logLine(const QString& line);

private:
  void setState(State state);
  void fail(const QString& message);
  void poll();
  void onPollReply(bool reachable, const QString& version);
  void probeUntilGone();
  void onProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStopTimeout();
  void requestRemoteStop();
  void flushOutput(const QByteArray& data, QByteArray& buffer);

  State m_state{State::Stopped};
  QString m_lastError;
  QString m_version;
  QString m_daemonPath;
  int m_port{8215};
  int m_stopTimeoutMs{8000};
  int m_startTimeoutMs{60000};
  bool m_restartPending{false};
  int m_missedPolls{0};

  QProcess m_process;
  ApiClient m_api;
  QTimer m_pollTimer;
  QTimer m_stopTimer;
  QTimer m_startTimer;
  QString m_lastOutput;
  QByteArray m_outBuffer;
  QByteArray m_errBuffer;
};
