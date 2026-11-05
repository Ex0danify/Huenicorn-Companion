#include <QtTest>

#include "FakeDaemonApi.hpp"
#include "setup/SetupController.hpp"

class TestSetupController : public QObject
{
  Q_OBJECT

private:
  struct Fixture
  {
    FakeDaemonApi api;
    DaemonController controller;
    std::unique_ptr<SetupController> setup;

    explicit Fixture(bool setupMode = true)
    {
      api.setupMode = setupMode;
      controller.setPort(api.port());
      controller.setPollIntervalMs(100);
      setup = std::make_unique<SetupController>(controller);
      setup->setRetryIntervalMs(10);
      setup->setFinishDelayMs(0);
      controller.beginMonitoring();
    }

    // Walks the wizard to the pairing step
    void toPairStep()
    {
      QTRY_VERIFY_WITH_TIMEOUT(setup->active(), 5000);
      setup->submitBridge("192.168.1.20");
      QTRY_COMPARE(setup->step(), QString("pair"));
    }
  };

private slots:
  void normalDaemonIsNotInSetup()
  {
    Fixture f(false);
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->ready(), 5000);
    QVERIFY(!f.setup->active());
    QCOMPARE(f.api.count("GET", "/api/autodetectBridge"), 0);
  }

  void detectsSetupModeAndLooksForBridges()
  {
    Fixture f;
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->active(), 5000);
    QVERIFY(!f.setup->ready());
    QTRY_COMPARE(f.setup->bridges(), QStringList{"192.168.1.20"});
    QCOMPARE(f.setup->bridgeAddress(), QString("192.168.1.20"));
    QCOMPARE(f.setup->step(), QString("bridge"));
  }

  void reportsWhenNoBridgeIsFound()
  {
    FakeDaemonApi api;
    api.setupMode = true;
    api.detectedBridges.clear();
    DaemonController controller;
    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    SetupController setup(controller);
    controller.beginMonitoring();

    QTRY_VERIFY_WITH_TIMEOUT(!setup.error().isEmpty(), 5000);
    QVERIFY(setup.bridges().isEmpty());
    QVERIFY(!setup.busy());
  }

  void emptyBridgeAddressNeverReachesTheDaemon()
  {
    Fixture f;
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->active(), 5000);

    // The daemon dereferences the end of an empty address
    f.setup->submitBridge("   ");
    QVERIFY(!f.setup->error().isEmpty());
    QCOMPARE(f.api.count("PUT", "/api/validateBridgeAddress"), 0);
  }

  void unknownBridgeStaysOnTheAddressStep()
  {
    Fixture f;
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->active(), 5000);

    f.setup->submitBridge("10.0.0.99");
    QTRY_VERIFY(!f.setup->error().isEmpty());
    QCOMPARE(f.setup->step(), QString("bridge"));
    QVERIFY(!f.setup->busy());
  }

  void knownBridgeMovesToPairing()
  {
    Fixture f;
    f.toPairStep();
    QCOMPARE(f.api.lastBody("/api/validateBridgeAddress"), QByteArray("{\"bridgeAddress\":\"192.168.1.20\"}"));
    QVERIFY(f.setup->error().isEmpty());
  }

  void keepsPairingUntilTheLinkButtonIsPressed()
  {
    Fixture f;
    f.api.linkButtonAfter = 3;
    f.api.normalModeAfter = 2;
    f.toPairStep();

    f.setup->pair();
    QVERIFY(f.setup->pairing());

    QTRY_VERIFY_WITH_TIMEOUT(f.setup->ready(), 5000);
    QCOMPARE(f.api.registerAttempts, 4);
    QVERIFY(f.api.setupFinished);
    QVERIFY(!f.setup->pairing());
    QVERIFY(f.setup->error().isEmpty());
  }

  void leavesTheDaemonAloneWhileItsSetupServerStops()
  {
    Fixture f;
    f.api.shutdownWindowMs = 250;
    f.setup->setFinishDelayMs(400);
    f.toPairStep();

    f.setup->submitCredentials("known-user", "00112233445566778899AABBCCDDEEFF");
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->ready(), 5000);
    QVERIFY(!f.api.requestedDuringShutdown);
  }

  void pausesDaemonPollingWhileItsSetupServerStops()
  {
    Fixture f;
    f.api.shutdownWindowMs = 250;
    f.setup->setFinishDelayMs(400);
    f.toPairStep();

    f.setup->submitCredentials("known-user", "00112233445566778899AABBCCDDEEFF");
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->ready(), 5000);
    QVERIFY(!f.api.versionPolledDuringShutdown);

    // Monitoring picks up again afterwards
    const int polls = f.api.count("GET", "/api/version");
    QTRY_VERIFY(f.api.count("GET", "/api/version") > polls);
  }

  void doesNotReportReadyBeforeTheDaemonSwitchedMode()
  {
    Fixture f;
    f.api.normalModeAfter = 1000;
    f.toPairStep();

    f.setup->pair();
    QTRY_COMPARE(f.setup->step(), QString("finishing"));
    QTest::qWait(150);
    QVERIFY(!f.setup->ready());
  }

  void giveUpOnErrorsOtherThanTheLinkButton()
  {
    Fixture f;
    f.api.linkButtonAfter = 1000;
    f.api.pairError = "unreachable bridge";
    f.toPairStep();

    f.setup->pair();
    QTRY_VERIFY(!f.setup->error().isEmpty());
    QVERIFY(!f.setup->pairing());
    QCOMPARE(f.api.registerAttempts, 1);
    QCOMPARE(f.setup->step(), QString("pair"));
  }

  void stoppingPairingEndsTheRetries()
  {
    Fixture f;
    f.api.linkButtonAfter = 100000;
    f.toPairStep();

    f.setup->pair();
    QTRY_VERIFY(f.api.registerAttempts >= 2);
    f.setup->stopPairing();
    QVERIFY(!f.setup->pairing());

    // One request may already be on the wire
    QTest::qWait(60);
    const int attempts = f.api.registerAttempts;
    QTest::qWait(150);
    QCOMPARE(f.api.registerAttempts, attempts);
  }

  void stoppingPairingKeepsBridgeSearchAlive()
  {
    Fixture f;
    f.api.linkButtonAfter = 100000;
    f.toPairStep();

    f.setup->pair();
    f.setup->discover();
    f.setup->stopPairing();
    QTRY_VERIFY(!f.setup->busy());
  }

  void goingBackLeavesThePairingStep()
  {
    Fixture f;
    f.toPairStep();
    f.setup->back();
    QCOMPARE(f.setup->step(), QString("bridge"));
  }

  void malformedClientKeyIsRejectedLocally()
  {
    Fixture f;
    f.toPairStep();

    f.setup->submitCredentials("known-user", "not-a-key");
    QVERIFY(!f.setup->error().isEmpty());
    f.setup->submitCredentials("", "00112233445566778899AABBCCDDEEFF");
    QVERIFY(!f.setup->error().isEmpty());
    QCOMPARE(f.api.count("PUT", "/api/validateCredentials"), 0);
  }

  void rejectedCredentialsStayOnThePairingStep()
  {
    Fixture f;
    f.toPairStep();

    f.setup->submitCredentials("someone-else", "00112233445566778899AABBCCDDEEFF");
    QTRY_VERIFY(!f.setup->error().isEmpty());
    QCOMPARE(f.setup->step(), QString("pair"));
    QVERIFY(!f.api.setupFinished);
  }

  void acceptedCredentialsFinishSetup()
  {
    Fixture f;
    f.toPairStep();

    f.setup->submitCredentials("known-user", "00112233445566778899aabbccddeeff");
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->ready(), 5000);
    QVERIFY(f.api.setupFinished);
    QCOMPARE(f.api.lastBody("/api/validateCredentials"),
             QByteArray("{\"clientkey\":\"00112233445566778899AABBCCDDEEFF\",\"username\":\"known-user\"}"));
  }

  void cancelAbortsTheDaemonSetup()
  {
    Fixture f;
    QTRY_VERIFY_WITH_TIMEOUT(f.setup->active(), 5000);

    f.setup->cancel();
    QTRY_COMPARE(f.api.count("POST", "/api/abort"), 1);
  }

  void resetsWhenTheDaemonGoesAway()
  {
    auto f = std::make_unique<Fixture>();
    f->toPairStep();

    f->api.close();
    QTRY_VERIFY_WITH_TIMEOUT(!f->setup->active(), 5000);
    QCOMPARE(f->setup->step(), QString("bridge"));
    QVERIFY(f->setup->bridges().isEmpty());
  }
};

QTEST_MAIN(TestSetupController)
#include "TestSetupController.moc"
