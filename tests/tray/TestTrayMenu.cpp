#include <QtTest>

#include "FakeDaemonApi.hpp"
#include "tray/TrayApp.hpp"

class TestTrayMenu : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    // Settings would otherwise read the real user config
    QVERIFY(m_config.isValid());
    qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
  }

  void menuLayout()
  {
    DaemonController controller;
    Settings settings;
    TrayApp tray(controller, settings);

    QStringList layout;
    for(auto* action : tray.menu().actions()){
      layout.append(action->isSeparator() ? "-" : action->text());
    }
    QCOMPARE(layout, QStringList({"Open Companion", "-", "Start", "Stop", "Restart", "-", "Show log", "Quit"}));
  }

  void controlsFollowDaemonState()
  {
    FakeDaemonApi api;
    DaemonController controller;
    controller.setPort(api.port());
    controller.setPollIntervalMs(100);
    Settings settings;
    TrayApp tray(controller, settings);

    // Without a binary the start attempt ends in an error
    controller.start();
    QCOMPARE(controller.state(), DaemonController::State::Error);
    QVERIFY(action(tray, "Start")->isEnabled());
    QVERIFY(!action(tray, "Stop")->isEnabled());
    QVERIFY(!action(tray, "Restart")->isEnabled());

    controller.beginMonitoring();
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), DaemonController::State::Running, 5000);
    QVERIFY(!action(tray, "Start")->isEnabled());
    QVERIFY(action(tray, "Stop")->isEnabled());
    QVERIFY(action(tray, "Restart")->isEnabled());

    controller.stop();
    QCOMPARE(controller.state(), DaemonController::State::Stopping);
    QVERIFY(!action(tray, "Stop")->isEnabled());
    QVERIFY(!action(tray, "Restart")->isEnabled());
  }

private:
  static QAction* action(TrayApp& tray, const QString& text)
  {
    for(auto* action : tray.menu().actions()){
      if(action->text() == text){
        return action;
      }
    }
    return nullptr;
  }

  QTemporaryDir m_config;
};

QTEST_MAIN(TestTrayMenu)
#include "TestTrayMenu.moc"
