#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "api/ApiClient.hpp"
#include "daemon/DaemonController.hpp"

// Drives the daemon's first-run setup (bridge address, pairing) over its REST API.
// The daemon serves a reduced API while it has no bridge configured, which is how
// setup mode is told apart from normal operation.
class SetupController : public QObject
{
  Q_OBJECT

  // True while the daemon waits for setup and the wizard should be shown
  Q_PROPERTY(bool active READ active NOTIFY modeChanged)
  // True once the daemon serves its normal API
  Q_PROPERTY(bool ready READ ready NOTIFY modeChanged)
  Q_PROPERTY(QString step READ step NOTIFY stepChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(bool discovering READ discovering NOTIFY busyChanged)
  Q_PROPERTY(bool pairing READ pairing NOTIFY busyChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QStringList bridges READ bridges NOTIFY bridgesChanged)
  Q_PROPERTY(QString bridgeAddress READ bridgeAddress NOTIFY bridgesChanged)

public:
  enum class Mode
  {
    Unknown,
    Setup,
    Ready
  };

  explicit SetupController(DaemonController& controller, QObject* parent = nullptr);

  bool active() const;
  bool ready() const;
  Mode mode() const;
  QString step() const;
  bool busy() const;
  bool discovering() const;
  bool pairing() const;
  QString error() const;
  QStringList bridges() const;
  QString bridgeAddress() const;

  // Overrides the polling of the link button and the waiting on the daemon after setup finished
  void setRetryIntervalMs(int ms);
  void setFinishDelayMs(int ms);

  Q_INVOKABLE void discover();
  Q_INVOKABLE void submitBridge(const QString& address);
  Q_INVOKABLE void pair();
  Q_INVOKABLE void stopPairing();
  Q_INVOKABLE void submitCredentials(const QString& username, const QString& clientKey);
  Q_INVOKABLE void back();
  Q_INVOKABLE void retry();
  Q_INVOKABLE void cancel();

signals:
  void modeChanged();
  void stepChanged();
  void busyChanged();
  void errorChanged();
  void bridgesChanged();

private:
  void onDaemonState();
  void probe();
  void reset();
  void setMode(Mode mode);
  void setStep(const QString& step);
  void setError(const QString& error);
  void setBusy(bool discovering, bool pairing);
  void tryRegister();
  void finish();
  void waitForNormalMode();

  DaemonController& m_controller;
  ApiClient m_api;

  Mode m_mode{Mode::Unknown};
  QString m_step{"bridge"};
  QString m_error;
  QStringList m_bridges;
  QString m_bridgeAddress;
  bool m_discovering{false};
  bool m_pairing{false};
  bool m_validating{false};

  int m_pairIntervalMs{2000};
  int m_finishIntervalMs{500};
  int m_finishDelayMs{1500};
  int m_pairAttempts{0};
  int m_finishAttempts{0};

  // Invalidates callbacks of requests that were in flight when the wizard was reset
  int m_epoch{0};
  // Same for a pairing the user walked away from
  int m_pairEpoch{0};
};
