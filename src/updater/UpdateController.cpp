#include "updater/UpdateController.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{
  constexpr int TransferTimeoutMs = 15000;
  constexpr int OutputTailBytes = 600;

  // pkexec reports a dismissed or refused authentication dialog with these
  constexpr int PkexecNotAuthorized = 126;
  constexpr int PkexecDismissed = 127;
}


UpdateController::UpdateController(const QString& currentVersion, QObject* parent)
  : QObject(parent)
  , m_currentVersion(currentVersion)
  , m_apiUrl(DefaultApiUrl)
  , m_system(Platform::detect())
{
}


QString UpdateController::phase() const
{
  switch(m_phase){
    case Phase::Idle: return "idle";
    case Phase::Checking: return "checking";
    case Phase::UpToDate: return "upToDate";
    case Phase::Available: return "available";
    case Phase::Downloading: return "downloading";
    case Phase::Installing: return "installing";
    case Phase::Installed: return "installed";
    case Phase::Failed: return "failed";
  }
  return {};
}


UpdateController::Phase UpdateController::phaseValue() const
{
  return m_phase;
}


bool UpdateController::busy() const
{
  return m_phase == Phase::Checking || m_phase == Phase::Downloading || m_phase == Phase::Installing;
}


QString UpdateController::errorText() const
{
  return m_error;
}


QString UpdateController::currentVersion() const
{
  return m_currentVersion;
}


QString UpdateController::latestVersion() const
{
  return m_release ? m_release->tag.trimmed().remove(QRegularExpression("^v")) : QString();
}


QString UpdateController::releaseNotes() const
{
  return m_release ? m_release->notes : QString();
}


QUrl UpdateController::releaseUrl() const
{
  return m_release ? m_release->pageUrl : QUrl();
}


bool UpdateController::canInstall() const
{
  return m_asset && m_release->findAsset("SHA256SUMS") && !QStandardPaths::findExecutable(m_elevation).isEmpty();
}


double UpdateController::progress() const
{
  return m_progress;
}


bool UpdateController::autoCheck() const
{
  return m_autoCheck;
}


void UpdateController::setAutoCheck(bool enabled)
{
  if(m_autoCheck != enabled){
    m_autoCheck = enabled;
    emit autoCheckChanged();
  }
}


QString UpdateController::skippedVersion() const
{
  return m_skippedVersion;
}


void UpdateController::setSkippedVersion(const QString& version)
{
  if(m_skippedVersion != version){
    m_skippedVersion = version;
    emit skippedVersionChanged();
  }
}


bool UpdateController::checkDue(const QDateTime& lastCheck, const QDateTime& now)
{
  // A clock that went backwards must not silence the check for good
  return !lastCheck.isValid() || now < lastCheck || lastCheck.secsTo(now) >= CheckIntervalHours * 3600;
}


void UpdateController::setApiUrl(const QUrl& url)
{
  m_apiUrl = url;
}


void UpdateController::setSystem(const SystemInfo& system)
{
  m_system = system;
}


void UpdateController::setElevationProgram(const QString& program)
{
  m_elevation = program;
}


void UpdateController::check()
{
  startCheck(false);
}


void UpdateController::checkInBackground()
{
  startCheck(true);
}


void UpdateController::startCheck(bool background)
{
  if(busy()){
    return;
  }

  m_error.clear();
  setPhase(Phase::Checking);
  fetch(m_apiUrl, [this, background](const QByteArray& body, const QString& error){
    if(!error.isEmpty()){
      fail(error);
      return;
    }
    onRelease(body, background);
  });
}


void UpdateController::onRelease(const QByteArray& body, bool background)
{
  const auto release = ReleaseInfo::fromJson(QJsonDocument::fromJson(body).object());
  if(!release){
    fail(tr("GitHub sent an unexpected answer"));
    return;
  }

  m_release = release;
  m_asset = Platform::pickAsset(release->assets, m_system);
  emit releaseChanged();
  emit checkCompleted();

  if(!Release::isNewer(m_currentVersion, release->tag)){
    setPhase(Phase::UpToDate);
    return;
  }

  setPhase(Phase::Available);
  if(background && latestVersion() != m_skippedVersion){
    emit backgroundUpdateFound(latestVersion());
  }
}


QNetworkRequest UpdateController::makeRequest(const QUrl& url) const
{
  QNetworkRequest request(url);
  request.setTransferTimeout(TransferTimeoutMs);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setHeader(QNetworkRequest::UserAgentHeader, "huenicorn-qt/" + m_currentVersion);
  request.setRawHeader("Accept", "application/vnd.github+json");
  return request;
}


void UpdateController::fetch(const QUrl& url, FetchDone done)
{
  if(!Release::isTrustedUrl(url)){
    done({}, tr("Refusing to download from an insecure address"));
    return;
  }

  auto* reply = m_network.get(makeRequest(url));
  connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)](){
    reply->deleteLater();
    if(reply->error() == QNetworkReply::NoError){
      done(reply->readAll(), {});
      return;
    }

    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(status == 403 || status == 429){
      done({}, tr("GitHub rate limit reached, try again later"));
      return;
    }
    done({}, reply->errorString());
  });
}


void UpdateController::install()
{
  if(m_phase != Phase::Available || !canInstall()){
    return;
  }

  m_workDir = std::make_unique<QTemporaryDir>();
  if(!m_workDir->isValid()){
    fail(tr("Could not create a temporary directory"));
    return;
  }

  m_error.clear();
  setProgress(0);
  setPhase(Phase::Downloading);

  const auto asset = *m_asset;
  fetch(m_release->findAsset("SHA256SUMS")->url, [this, asset](const QByteArray& body, const QString& error){
    if(!error.isEmpty()){
      fail(error);
      return;
    }

    const auto expected = Release::checksumFor(body, asset.name);
    if(!expected){
      fail(tr("The release has no checksum for %1").arg(asset.name));
      return;
    }
    download(asset, *expected);
  });
}


void UpdateController::download(const ReleaseAsset& asset, const QString& expectedSha256)
{
  // Never trust a file name that comes from the network as a path
  const auto path = m_workDir->filePath(QFileInfo(asset.name).fileName());
  m_file.setFileName(path);
  if(!m_file.open(QIODevice::WriteOnly)){
    fail(tr("Could not write %1").arg(path));
    return;
  }

  if(!Release::isTrustedUrl(asset.url)){
    fail(tr("Refusing to download from an insecure address"));
    return;
  }

  auto* reply = m_network.get(makeRequest(asset.url));
  connect(reply, &QNetworkReply::readyRead, this, [this, reply](){
    m_file.write(reply->readAll());
  });
  connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total){
    if(total > 0){
      setProgress(double(received) / double(total));
    }
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, path, expectedSha256](){
    reply->deleteLater();
    m_file.write(reply->readAll());
    m_file.close();

    if(reply->error() != QNetworkReply::NoError){
      fail(reply->errorString());
      return;
    }

    QFile downloaded(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if(!downloaded.open(QIODevice::ReadOnly) || !hash.addData(&downloaded)
       || QString::fromLatin1(hash.result().toHex()) != expectedSha256){
      fail(tr("The download does not match its checksum"));
      return;
    }

    setProgress(1);
    runInstall(path);
  });
}


void UpdateController::runInstall(const QString& file)
{
  const auto command = Platform::installCommand(m_system.kind, file);
  if(command.isEmpty()){
    fail(tr("This system is not supported"));
    return;
  }

  setPhase(Phase::Installing);
  m_installOutput.clear();

  auto* process = new QProcess(this);
  process->setProcessChannelMode(QProcess::MergedChannels);
  connect(process, &QProcess::readyRead, this, [this, process](){
    m_installOutput = (m_installOutput + process->readAll()).right(OutputTailBytes);
  });
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error){
    if(error == QProcess::FailedToStart){
      process->deleteLater();
      fail(tr("Could not start %1").arg(m_elevation));
    }
  });
  connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status){
    process->deleteLater();
    if(status == QProcess::NormalExit && code == 0){
      m_workDir.reset();
      setPhase(Phase::Installed);
      return;
    }

    if(code == PkexecNotAuthorized || code == PkexecDismissed){
      fail(tr("Authentication was cancelled or refused"));
      return;
    }
    fail(tr("Installation failed: %1").arg(QString::fromUtf8(m_installOutput).trimmed()));
  });

  process->start(m_elevation, command);
}


void UpdateController::skipVersion()
{
  if(m_release){
    setSkippedVersion(latestVersion());
  }
}


void UpdateController::restart()
{
  if(m_phase == Phase::Installed){
    emit restartRequested();
  }
}


void UpdateController::setPhase(Phase phase)
{
  m_phase = phase;
  emit phaseChanged();
}


void UpdateController::setProgress(double progress)
{
  m_progress = progress;
  emit progressChanged();
}


void UpdateController::fail(const QString& message)
{
  m_error = message;
  m_file.close();
  m_workDir.reset();
  setPhase(Phase::Failed);
}
