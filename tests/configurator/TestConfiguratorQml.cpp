#include <QtTest>

#include <QQuickWindow>

#include "FakeDaemonApi.hpp"
#include "configurator/ConfiguratorWindow.hpp"

// Catches QML syntax, import and binding errors that only show up at load time
class TestConfiguratorQml : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message){
      if(type == QtWarningMsg || type == QtCriticalMsg){
        warnings().append(message);
      }
    });
  }

  void cleanupTestCase()
  {
    qInstallMessageHandler(nullptr);
  }

  void init()
  {
    warnings().clear();
  }

  void loadsAndRendersWithoutWarnings()
  {
    {
      FakeDaemonApi api;
      DaemonController controller;
      controller.setPort(api.port());
      controller.setPollIntervalMs(100);
      UpdateController updater("0.1.0");
      ConfiguratorWindow configurator(controller, updater);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();

      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      for(int tab = 0; tab < 4; tab++){
        window->setProperty("tab", tab);
        QTest::qWait(60);
        QVERIFY(!window->grabWindow().isNull());
      }
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void rendersEverySetupStepWithoutWarnings()
  {
    {
      FakeDaemonApi api;
      api.setupMode = true;
      api.linkButtonAfter = 100000;
      DaemonController controller;
      controller.setPort(api.port());
      controller.setPollIntervalMs(100);
      UpdateController updater("0.1.0");
      ConfiguratorWindow configurator(controller, updater);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();

      auto* setup = configurator.model().setup();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(setup->active(), 5000);
      QTRY_VERIFY(!setup->busy());
      QTest::qWait(60);
      QVERIFY(!window->grabWindow().isNull());

      // A failed lookup puts the error text on screen
      setup->submitBridge("10.0.0.99");
      QTRY_VERIFY(!setup->error().isEmpty());
      QTest::qWait(60);
      QVERIFY(!window->grabWindow().isNull());

      setup->submitBridge("192.168.1.20");
      QTRY_COMPARE(setup->step(), QString("pair"));
      setup->pair();
      QTest::qWait(120);
      QVERIFY(!window->grabWindow().isNull());
      setup->stopPairing();

      setup->setRetryIntervalMs(10);
      api.linkButtonAfter = 0;
      api.normalModeAfter = 100000;
      setup->pair();
      QTRY_COMPARE(setup->step(), QString("finishing"));
      QTest::qWait(60);
      QVERIFY(!window->grabWindow().isNull());
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

private:
  static QStringList& warnings()
  {
    static QStringList list;
    return list;
  }
};

QTEST_MAIN(TestConfiguratorQml)
#include "TestConfiguratorQml.moc"
