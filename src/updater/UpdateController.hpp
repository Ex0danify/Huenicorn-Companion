#pragma once

#include <memory>
#include <optional>

#include <QDateTime>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTemporaryDir>
#include <QUrl>
#include <functional>

#include "updater/Platform.hpp"
#include "updater/Release.hpp"

class QProcess;

// Checks GitHub for a newer release, downloads the matching package and installs it
// through the system package manager with elevated rights.
class UpdateController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY phaseChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY phaseChanged)
  Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
  Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY releaseChanged)
  Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY releaseChanged)
  Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY releaseChanged)
  Q_PROPERTY(bool canInstall READ canInstall NOTIFY releaseChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY autoCheckChanged)

public:
  enum class Phase
  {
    Idle,
    Checking,
    UpToDate,
    Available,
    Downloading,
    Installing,
    Installed,
    Failed
  };

  static constexpr int CheckIntervalHours = 24;
  static inline const QUrl DefaultApiUrl{"https://api.github.com/repos/Ex0danify/Huenicorn-Companion/releases/latest"};

  explicit UpdateController(const QString& currentVersion, QObject* parent = nullptr);

  QString phase() const;
  Phase phaseValue() const;
  bool busy() const;
  QString errorText() const;
  QString currentVersion() const;
  QString latestVersion() const;
  QString releaseNotes() const;
  QUrl releaseUrl() const;
  bool canInstall() const;
  double progress() const;

  bool autoCheck() const;
  void setAutoCheck(bool enabled);

  // A version the user chose to ignore, it is not announced in the background any more
  QString skippedVersion() const;
  void setSkippedVersion(const QString& version);

  // Whether the periodic startup check should run
  static bool checkDue(const QDateTime& lastCheck, const QDateTime& now);

  void setApiUrl(const QUrl& url);
  void setSystem(const SystemInfo& system);
  void setElevationProgram(const QString& program);

  Q_INVOKABLE void check();
  Q_INVOKABLE void install();
  Q_INVOKABLE void skipVersion();
  Q_INVOKABLE void restart();

  // Same as check(), but a newer release is announced through backgroundUpdateFound
  void checkInBackground();

signals:
  void phaseChanged();
  void releaseChanged();
  void progressChanged();
  void autoCheckChanged();
  void skippedVersionChanged();
  void restartRequested();
  // A check reached GitHub and got an answer
  void checkCompleted();
  void backgroundUpdateFound(const QString& version);

private:
  using FetchDone = std::function<void(const QByteArray& body, const QString& error)>;

  void startCheck(bool background);
  void onRelease(const QByteArray& body, bool background);
  void fetch(const QUrl& url, FetchDone done);
  void download(const ReleaseAsset& asset, const QString& expectedSha256);
  void runInstall(const QString& file);
  void setPhase(Phase phase);
  void setProgress(double progress);
  void fail(const QString& message);
  QNetworkRequest makeRequest(const QUrl& url) const;

  QNetworkAccessManager m_network;
  QString m_currentVersion;
  QUrl m_apiUrl;
  SystemInfo m_system;
  QString m_elevation{"pkexec"};

  Phase m_phase{Phase::Idle};
  QString m_error;
  double m_progress{0};
  bool m_autoCheck{true};
  QString m_skippedVersion;

  std::optional<ReleaseInfo> m_release;
  std::optional<ReleaseAsset> m_asset;

  std::unique_ptr<QTemporaryDir> m_workDir;
  QFile m_file;
  QByteArray m_installOutput;
};
