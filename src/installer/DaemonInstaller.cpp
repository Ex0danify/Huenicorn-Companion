#include "installer/DaemonInstaller.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include "updater/Release.hpp"

namespace
{
  constexpr int ErrorTailLines = 12;
  constexpr int MaxParallelJobs = 4;
  constexpr int DownloadTimeoutMs = 120000;

  QString lastLines(const QString& text, int count)
  {
    auto lines = text.split('\n', Qt::SkipEmptyParts);
    if(lines.size() > count){
      lines = lines.mid(lines.size() - count);
    }
    return lines.join('\n');
  }
}


DaemonInstaller::DaemonInstaller(const DaemonPin& pin, QObject* parent)
  : QObject(parent)
  , m_pin(pin)
  , m_probe(Requirements::systemProbe())
  , m_session(Requirements::detectSession(QProcessEnvironment::systemEnvironment()))
  , m_system(Platform::detect())
  , m_installDir(QDir::homePath() + "/.local/bin")
  , m_workRoot(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/daemon-build")
{
  setRunner(std::make_unique<ProcessRunner>());
  m_fetcher = [this](const QUrl& url, FetchDone done){
    fetchWithNetwork(url, std::move(done));
  };
}


QString DaemonInstaller::phase() const
{
  switch(m_phase){
    case Phase::Idle: return "idle";
    case Phase::MissingRequirements: return "missing";
    case Phase::Ready: return "ready";
    case Phase::Building: return "building";
    case Phase::Installed: return "installed";
    case Phase::Failed: return "failed";
  }
  return {};
}


DaemonInstaller::Phase DaemonInstaller::phaseValue() const { return m_phase; }
bool DaemonInstaller::busy() const { return m_phase == Phase::Building; }
QString DaemonInstaller::errorText() const { return m_error; }
QString DaemonInstaller::step() const { return m_stepTitle; }
double DaemonInstaller::progress() const { return m_progress; }
QString DaemonInstaller::installHint() const { return m_hint; }
QString DaemonInstaller::version() const { return m_pin.version(); }
QString DaemonInstaller::repoUrl() const { return m_pin.repoUrl; }
QString DaemonInstaller::commit() const { return m_pin.commit; }
QString DaemonInstaller::installPath() const { return m_installDir + "/huenicorn"; }
QString DaemonInstaller::sourceDir() const { return m_workDir->path() + "/src"; }
QString DaemonInstaller::depsDir() const { return m_workDir->path() + "/deps"; }
QString DaemonInstaller::buildDir() const { return m_workDir->path() + "/build"; }


QString DaemonInstaller::session() const
{
  switch(m_session){
    case SessionType::X11: return "x11";
    case SessionType::Wayland: return "wayland";
    case SessionType::Unknown: break;
  }
  return "unknown";
}


QVariantList DaemonInstaller::requirements() const
{
  QVariantList list;
  for(const auto& requirement : m_requirements){
    list.append(QVariantMap{{"id", requirement.id}, {"label", requirement.label}, {"met", requirement.met}});
  }
  return list;
}


void DaemonInstaller::setRunner(std::unique_ptr<CommandRunner> runner)
{
  m_runner = std::move(runner);
  connect(m_runner.get(), &CommandRunner::outputLine, this, &DaemonInstaller::logLine);
  connect(m_runner.get(), &CommandRunner::finished, this, &DaemonInstaller::onCommandFinished);
}


void DaemonInstaller::setFetcher(Fetcher fetcher) { m_fetcher = std::move(fetcher); }
void DaemonInstaller::setProbe(const HostProbe& probe) { m_probe = probe; }
void DaemonInstaller::setSession(SessionType session) { m_session = session; }
void DaemonInstaller::setSystem(const SystemInfo& system) { m_system = system; }
void DaemonInstaller::setInstallDir(const QString& dir) { m_installDir = dir; }
void DaemonInstaller::setWorkRoot(const QString& dir) { m_workRoot = dir; }


void DaemonInstaller::refresh()
{
  if(m_phase == Phase::Building){
    return;
  }

  m_requirements = Requirements::check(m_session, m_probe);
  m_hint = Requirements::installHint(m_system.kind, m_requirements);
  emit requirementsChanged();
  setPhase(Requirements::allMet(m_requirements) ? Phase::Ready : Phase::MissingRequirements);
}


void DaemonInstaller::build()
{
  if(m_phase == Phase::Building){
    return;
  }

  refresh();
  if(m_phase != Phase::Ready){
    return;
  }

  QDir().mkpath(m_workRoot);
  m_workDir = std::make_unique<QTemporaryDir>(m_workRoot + "/build-XXXXXX");
  if(!m_workDir->isValid()){
    m_workDir.reset();
    fail(tr("Could not create a working directory in %1").arg(m_workRoot));
    return;
  }

  m_cancelled = false;
  m_epoch++;
  m_index = 0;
  m_progress = 0;
  planSteps();
  setPhase(Phase::Building);
  runNextStep();
}


void DaemonInstaller::cancel()
{
  if(m_phase != Phase::Building || m_cancelled){
    return;
  }

  m_cancelled = true;
  m_epoch++;
  if(m_reply){
    m_reply->abort();
  }

  // The working directory may only go once the child process has really ended
  if(m_commandRunning){
    m_runner->cancel();
  }
  else{
    finishCancel();
  }
}


void DaemonInstaller::finishCancel()
{
  m_onFinished = nullptr;
  m_workDir.reset();
  m_progress = 0;
  m_stepTitle.clear();
  emit progressChanged();
  setPhase(Phase::Idle);
  refresh();
}


void DaemonInstaller::planSteps()
{
  m_steps.clear();

  m_steps.append({tr("Fetching Huenicorn %1").arg(m_pin.version()), [this](){
    runCommand({"git", {"clone", "--depth", "1", "--branch", m_pin.tag, m_pin.repoUrl, sourceDir()}, m_workDir->path()},
               [this](const QString&){ advance(); });
  }});

  m_steps.append({tr("Verifying source"), [this](){
    runCommand({"git", {"-C", sourceDir(), "rev-parse", "HEAD"}, m_workDir->path()}, [this](const QString& tail){
      for(const auto& line : tail.split('\n')){
        if(line.trimmed().toLower() == m_pin.commit.toLower()){
          advance();
          return;
        }
      }
      fail(tr("The downloaded source is not the expected commit, refusing to build"));
    });
  }});

  for(const auto& dependency : m_pin.dependencies){
    m_steps.append({tr("Downloading %1").arg(dependency.fetchName), [this, dependency](){
      downloadDependency(dependency);
    }});

    m_steps.append({tr("Unpacking %1").arg(dependency.fetchName), [this, dependency](){
      const auto target = depsDir() + "/" + dependency.fetchName;
      QDir().mkpath(target);
      runCommand({"tar", {"-xf", target + ".archive", "-C", target, "--strip-components=1"}, m_workDir->path()},
                 [this](const QString&){ advance(); });
    }});
  }

  m_steps.append({tr("Configuring"), [this](){
    QStringList arguments{"-S", sourceDir(), "-B", buildDir(), "-DCMAKE_BUILD_TYPE=Release",
                          "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"};
    // Only the verified copies may be used, never whatever is installed on the machine
    for(const auto& dependency : m_pin.dependencies){
      arguments << "-DCMAKE_DISABLE_FIND_PACKAGE_" + dependency.packageName + "=ON"
                << "-DFETCHCONTENT_SOURCE_DIR_" + dependency.fetchName.toUpper() + "=" + depsDir() + "/" + dependency.fetchName;
    }
    arguments << "-DCMAKE_CXX_FLAGS=" + m_pin.compilerFlags.join(' ');
    runCommand({"cmake", arguments, m_workDir->path()}, [this](const QString&){ advance(); });
  }});

  m_steps.append({tr("Compiling, this takes a few minutes"), [this](){
    const auto jobs = QString::number(qBound(1, QThread::idealThreadCount(), MaxParallelJobs));
    runCommand({"cmake", {"--build", buildDir(), "--parallel", jobs}, m_workDir->path()},
               [this](const QString&){ advance(); });
  }});

  m_steps.append({tr("Installing"), [this](){ installBinary(); }});
}


void DaemonInstaller::runNextStep()
{
  if(m_index >= m_steps.size()){
    return;
  }

  const auto& step = m_steps[m_index];
  m_stepTitle = step.title;
  emit progressChanged();
  emit logLine(">> " + step.title);
  step.run();
}


void DaemonInstaller::advance()
{
  m_index++;
  m_progress = static_cast<double>(m_index) / m_steps.size();
  emit progressChanged();
  runNextStep();
}


void DaemonInstaller::runCommand(const Command& command, std::function<void(const QString&)> onSuccess)
{
  m_commandRunning = true;
  m_onFinished = [this, onSuccess = std::move(onSuccess)](int exitCode, const QString& tail){
    if(exitCode != 0){
      fail(tr("%1 failed").arg(m_stepTitle), tail);
      return;
    }
    onSuccess(tail);
  };
  m_runner->run(command);
}


void DaemonInstaller::onCommandFinished(int exitCode, const QString& tail)
{
  m_commandRunning = false;

  if(m_cancelled){
    finishCancel();
    return;
  }

  auto handler = std::move(m_onFinished);
  m_onFinished = nullptr;
  if(handler && m_phase == Phase::Building){
    handler(exitCode, tail);
  }
}


void DaemonInstaller::downloadDependency(const DependencySource& dependency)
{
  const QUrl url(dependency.url);
  if(!Release::isTrustedUrl(url)){
    fail(tr("Refusing to download %1 over an insecure connection").arg(dependency.fetchName));
    return;
  }

  const int epoch = m_epoch;
  m_fetcher(url, [this, dependency, epoch](const QByteArray& body, const QString& error){
    if(epoch != m_epoch || m_phase != Phase::Building){
      return;
    }

    if(!error.isEmpty()){
      fail(tr("Could not download %1").arg(dependency.fetchName), error);
      return;
    }

    const auto actual = QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
    if(actual.toLower() != dependency.sha256.toLatin1().toLower()){
      fail(tr("The download of %1 does not match its expected checksum, refusing to build").arg(dependency.fetchName));
      return;
    }

    QDir().mkpath(depsDir());
    QFile file(depsDir() + "/" + dependency.fetchName + ".archive");
    if(!file.open(QIODevice::WriteOnly) || file.write(body) != body.size()){
      fail(tr("Could not store %1").arg(dependency.fetchName), file.errorString());
      return;
    }
    file.close();
    advance();
  });
}


void DaemonInstaller::installBinary()
{
  QFile source(buildDir() + "/huenicorn");
  if(!source.open(QIODevice::ReadOnly)){
    fail(tr("The build did not produce a daemon binary"));
    return;
  }

  QDir().mkpath(m_installDir);

  // An atomic replace keeps a working install intact if anything goes wrong
  QSaveFile target(installPath());
  if(!target.open(QIODevice::WriteOnly) || target.write(source.readAll()) < 0 || !target.commit()){
    fail(tr("Could not write %1").arg(installPath()), target.errorString());
    return;
  }

  QFile::setPermissions(installPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                         | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                         | QFileDevice::ReadOther | QFileDevice::ExeOther);

  m_workDir.reset();
  m_progress = 1;
  m_stepTitle.clear();
  emit progressChanged();
  setPhase(Phase::Installed);
  emit installed(installPath());
}


void DaemonInstaller::fail(const QString& message, const QString& tail)
{
  m_onFinished = nullptr;
  m_workDir.reset();
  m_stepTitle.clear();

  m_error = message;
  const auto detail = lastLines(tail, ErrorTailLines);
  if(!detail.isEmpty()){
    m_error += "\n" + detail;
  }
  emit progressChanged();
  setPhase(Phase::Failed);
}


void DaemonInstaller::setPhase(Phase phase)
{
  if(phase != Phase::Failed){
    m_error.clear();
  }
  m_phase = phase;
  emit phaseChanged();
}


void DaemonInstaller::fetchWithNetwork(const QUrl& url, FetchDone done)
{
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(DownloadTimeoutMs);

  auto* reply = m_network.get(request);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)](){
    reply->deleteLater();
    if(reply->error() != QNetworkReply::NoError){
      done({}, reply->errorString());
      return;
    }
    done(reply->readAll(), {});
  });
}
