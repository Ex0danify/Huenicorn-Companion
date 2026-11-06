#pragma once

#include <functional>
#include <memory>

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>

#include "installer/CommandRunner.hpp"
#include "installer/PinnedDaemon.hpp"
#include "installer/Requirements.hpp"

class QNetworkReply;

// Builds the pinned daemon from source into the user's own bin directory.
// Dependencies are never installed here, missing ones are only reported.
class DaemonInstaller : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY phaseChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY phaseChanged)
  Q_PROPERTY(QString step READ step NOTIFY progressChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QVariantList requirements READ requirements NOTIFY requirementsChanged)
  Q_PROPERTY(QString installHint READ installHint NOTIFY requirementsChanged)
  Q_PROPERTY(QString session READ session NOTIFY requirementsChanged)
  Q_PROPERTY(QString version READ version CONSTANT)
  Q_PROPERTY(QString repoUrl READ repoUrl CONSTANT)
  Q_PROPERTY(QString commit READ commit CONSTANT)
  Q_PROPERTY(QString installPath READ installPath CONSTANT)

public:
  enum class Phase
  {
    Idle,
    MissingRequirements,
    Ready,
    Building,
    Installed,
    Failed
  };

  using FetchDone = std::function<void(const QByteArray& body, const QString& error)>;
  using Fetcher = std::function<void(const QUrl& url, FetchDone done)>;

  explicit DaemonInstaller(const DaemonPin& pin, QObject* parent = nullptr);

  QString phase() const;
  Phase phaseValue() const;
  bool busy() const;
  QString errorText() const;
  QString step() const;
  double progress() const;
  QVariantList requirements() const;
  QString installHint() const;
  QString session() const;
  QString version() const;
  QString repoUrl() const;
  QString commit() const;
  QString installPath() const;

  void setRunner(std::unique_ptr<CommandRunner> runner);
  void setFetcher(Fetcher fetcher);
  void setProbe(const HostProbe& probe);
  void setSession(SessionType session);
  void setSystem(const SystemInfo& system);
  void setInstallDir(const QString& dir);
  void setWorkRoot(const QString& dir);

  // Looks at the machine again, e.g. after the user installed what was missing
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void build();
  Q_INVOKABLE void cancel();

signals:
  void phaseChanged();
  void progressChanged();
  void requirementsChanged();
  void logLine(const QString& line);
  void installed(const QString& path);

private:
  struct Step
  {
    QString title;
    std::function<void()> run;
  };

  void planSteps();
  void runNextStep();
  void advance();
  void runCommand(const Command& command, std::function<void(const QString& tail)> onSuccess);
  void onCommandFinished(int exitCode, const QString& tail);
  void downloadDependency(const DependencySource& dependency);
  void installBinary();
  void finishCancel();
  void fail(const QString& message, const QString& tail = {});
  void setPhase(Phase phase);
  void fetchWithNetwork(const QUrl& url, FetchDone done);

  QString sourceDir() const;
  QString depsDir() const;
  QString buildDir() const;

  DaemonPin m_pin;
  std::unique_ptr<CommandRunner> m_runner;
  Fetcher m_fetcher;
  HostProbe m_probe;
  SessionType m_session;
  SystemInfo m_system;
  QString m_installDir;
  QString m_workRoot;

  Phase m_phase{Phase::Idle};
  QString m_error;
  QString m_stepTitle;
  double m_progress{0};
  QList<Requirement> m_requirements;
  QString m_hint;

  QList<Step> m_steps;
  int m_index{0};
  int m_epoch{0};
  bool m_cancelled{false};
  bool m_commandRunning{false};
  std::function<void(int, const QString&)> m_onFinished;
  std::unique_ptr<QTemporaryDir> m_workDir;

  QNetworkAccessManager m_network;
  QPointer<QNetworkReply> m_reply;
};
