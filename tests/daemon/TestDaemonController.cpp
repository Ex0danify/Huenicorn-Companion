#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "FakeDaemonApi.hpp"
#include "daemon/DaemonController.hpp"

namespace
{
  bool waitForState(const DaemonController& controller, DaemonController::State state, int ms = 5000)
  {
    QElapsedTimer timer;
    timer.start();
    while(controller.state() != state && timer.elapsed() < ms){
      QTest::qWait(20);
    }
    return controller.state() == state;
  }
}


class TestDaemonController : public QObject
{
  Q_OBJECT

private slots:
  void adoptsExternalDaemon() const
  {
    FakeDaemonApi api;
    DaemonController controller;
    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    controller.beginMonitoring();

    QVERIFY(waitForState(controller, DaemonController::State::Running));
    QCOMPARE(controller.version(), QString("9.9.9"));
    QVERIFY(!controller.ownsProcess());
  }

  void externalDaemonDisappearing() const
  {
    auto api = std::make_unique<FakeDaemonApi>();
    DaemonController controller;
    controller.setPort(api->port());
    controller.setPollIntervalMs(100);
    controller.beginMonitoring();
    QVERIFY(waitForState(controller, DaemonController::State::Running));

    api->close();
    QVERIFY(waitForState(controller, DaemonController::State::Stopped));
  }

  void stopSendsStopRequestToExternalDaemon() const
  {
    FakeDaemonApi api;
    DaemonController controller;
    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    controller.beginMonitoring();
    QVERIFY(waitForState(controller, DaemonController::State::Running));

    controller.stop();
    QCOMPARE(controller.state(), DaemonController::State::Stopping);
    QTRY_COMPARE_WITH_TIMEOUT(api.stopRequests, 1, 3000);

    api.close();
    QVERIFY(waitForState(controller, DaemonController::State::Stopped));
  }

  void startWithoutBinaryFails() const
  {
    DaemonController controller;
    controller.start();
    QCOMPARE(controller.state(), DaemonController::State::Error);
    QVERIFY(!controller.lastError().isEmpty());
  }

  void missingBinaryFailsWithoutSpawning() const
  {
    DaemonController controller;
    controller.setDaemonPath("/nonexistent/huenicorn");
    controller.start();
    QCOMPARE(controller.state(), DaemonController::State::Error);
    QVERIFY2(controller.lastError().contains("/nonexistent/huenicorn"), qPrintable(controller.lastError()));
    QVERIFY(!controller.ownsProcess());
  }

  void canStartNeedsAnExistingExecutable() const
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto plain = dir.filePath("plain");
    QFile file(plain);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    DaemonController controller;
    QVERIFY(!controller.canStart());
    controller.setDaemonPath("/nonexistent/huenicorn");
    QVERIFY(!controller.canStart());
    controller.setDaemonPath(plain);
    QVERIFY(!controller.canStart());
    file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
    QVERIFY(controller.canStart());
  }

  void binaryOnlyChangesWhileStopped() const
  {
    FakeDaemonApi api;
    DaemonController controller;
    QVERIFY(controller.canChangeBinary());

    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    controller.beginMonitoring();
    QVERIFY(waitForState(controller, DaemonController::State::Running));
    QVERIFY(!controller.canChangeBinary());

    controller.stop();
    QVERIFY(!controller.canChangeBinary());
    api.close();
    QVERIFY(waitForState(controller, DaemonController::State::Stopped));
    QVERIFY(controller.canChangeBinary());

    DaemonController failing;
    failing.start();
    QCOMPARE(failing.state(), DaemonController::State::Error);
    QVERIFY(failing.canChangeBinary());
  }

  void checkNowAdoptsExternalDaemonWithoutWaiting() const
  {
    FakeDaemonApi api;
    DaemonController controller;
    controller.setPort(api.port());
    controller.setPollIntervalMs(60000);
    controller.checkNow();
    QVERIFY(waitForState(controller, DaemonController::State::Running));
  }

  void ownedProcessLifecycle() const
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto script = dir.filePath("fake-daemon.sh");
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\necho hello-from-daemon\nexec sleep 60\n");
    file.close();
    file.setPermissions(file.permissions() | QFileDevice::ExeOwner);

    FakeDaemonApi api;
    DaemonController controller;
    controller.setDaemonPath(script);
    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    controller.setStopTimeoutMs(500);

    QStringList lines;
    connect(&controller, &DaemonController::logLine, this, [&lines](const QString& line){ lines << line; });

    controller.start();
    QCOMPARE(controller.state(), DaemonController::State::Starting);
    QVERIFY(waitForState(controller, DaemonController::State::Running));
    QVERIFY(controller.ownsProcess());
    QTRY_VERIFY_WITH_TIMEOUT(lines.contains("hello-from-daemon"), 2000);

    // The fake API ignores the stop request, so the SIGTERM fallback must end it.
    // It keeps answering polls, so the controller may re-adopt it right after Stopped.
    // Record the transitions instead of sampling the current state.
    QList<DaemonController::State> states;
    connect(&controller, &DaemonController::stateChanged, this, [&states](DaemonController::State state){ states << state; });
    controller.stop();
    QTRY_VERIFY_WITH_TIMEOUT(states.contains(DaemonController::State::Stopped), 5000);
    QVERIFY(!controller.ownsProcess());
  }

  void daemonGetsNoOpBrowserLauncher() const
  {
    QStandardPaths::setTestModeEnabled(true);
    const QString stub = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/nobrowser/xdg-open";
    QFile::remove(stub);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto script = dir.filePath("fake-daemon.sh");
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\necho \"opener=$(command -v xdg-open)\"\nxdg-open http://127.0.0.1:1\necho \"exit=$?\"\nexec sleep 60\n");
    file.close();
    file.setPermissions(file.permissions() | QFileDevice::ExeOwner);

    FakeDaemonApi api;
    DaemonController controller;
    controller.setDaemonPath(script);
    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    controller.setStopTimeoutMs(500);

    QStringList lines;
    connect(&controller, &DaemonController::logLine, this, [&lines](const QString& line){ lines << line; });

    controller.start();
    QTRY_VERIFY_WITH_TIMEOUT(lines.contains("exit=0"), 3000);
    QVERIFY(lines.contains("opener=" + stub));
    QVERIFY(QFileInfo(stub).isExecutable());

    controller.stop();
    QVERIFY(waitForState(controller, DaemonController::State::Stopped, 5000));
    QStandardPaths::setTestModeEnabled(false);
  }

  void unexpectedExitIsAnError() const
  {
    QTemporaryDir dir;
    const auto script = dir.filePath("crash.sh");
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\necho \"boom: missing library\" >&2\nexit 3\n");
    file.close();
    file.setPermissions(file.permissions() | QFileDevice::ExeOwner);

    DaemonController controller;
    controller.setDaemonPath(script);
    controller.setPort(1);
    controller.start();
    QVERIFY(waitForState(controller, DaemonController::State::Error));
    QVERIFY(controller.lastError().contains("3"));
    QVERIFY2(controller.lastError().contains("boom: missing library"), qPrintable(controller.lastError()));
  }
};

QTEST_MAIN(TestDaemonController)
#include "TestDaemonController.moc"
