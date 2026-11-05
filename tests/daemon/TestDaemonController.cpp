#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "FakeDaemonApi.hpp"
#include "daemon/DaemonController.hpp"

namespace
{
  bool waitForState(DaemonController& controller, DaemonController::State state, int ms = 5000)
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
  void adoptsExternalDaemon()
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

  void externalDaemonDisappearing()
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

  void stopSendsStopRequestToExternalDaemon()
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

  void startWithoutBinaryFails()
  {
    DaemonController controller;
    controller.start();
    QCOMPARE(controller.state(), DaemonController::State::Error);
    QVERIFY(!controller.lastError().isEmpty());
  }

  void missingBinaryReportsError()
  {
    DaemonController controller;
    controller.setDaemonPath("/nonexistent/huenicorn");
    controller.start();
    QVERIFY(waitForState(controller, DaemonController::State::Error));
  }

  void ownedProcessLifecycle()
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
    connect(&controller, &DaemonController::logLine, this, [&](const QString& line){ lines << line; });

    controller.start();
    QCOMPARE(controller.state(), DaemonController::State::Starting);
    QVERIFY(waitForState(controller, DaemonController::State::Running));
    QVERIFY(controller.ownsProcess());
    QTRY_VERIFY_WITH_TIMEOUT(lines.contains("hello-from-daemon"), 2000);

    // The fake API ignores the stop request, so the SIGTERM fallback must end it.
    // It keeps answering polls, so the controller may re-adopt it right after Stopped.
    // Record the transitions instead of sampling the current state.
    QList<DaemonController::State> states;
    connect(&controller, &DaemonController::stateChanged, this, [&](DaemonController::State state){ states << state; });
    controller.stop();
    QTRY_VERIFY_WITH_TIMEOUT(states.contains(DaemonController::State::Stopped), 5000);
    QVERIFY(!controller.ownsProcess());
  }

  void unexpectedExitIsAnError()
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
