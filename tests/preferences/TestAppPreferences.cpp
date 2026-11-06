#include <QtTest>

#include "preferences/AppPreferences.hpp"

class TestAppPreferences : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    // Settings would otherwise write into the real user config
    QVERIFY(m_config.isValid());
    qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
  }

  void persistsAndNotifiesOnChange()
  {
    Settings settings;
    AppPreferences preferences(settings);
    QSignalSpy autostart(&preferences, &AppPreferences::autostartDaemonChanged);
    QSignalSpy stopOnQuit(&preferences, &AppPreferences::stopDaemonOnQuitChanged);

    preferences.setAutostartDaemon(false);
    preferences.setStopDaemonOnQuit(false);
    QCOMPARE(autostart.count(), 1);
    QCOMPARE(stopOnQuit.count(), 1);

    // A second instance sees what the first one wrote, like the tray does
    Settings other;
    QVERIFY(!other.autostartDaemon());
    QVERIFY(!other.stopDaemonOnQuit());

    preferences.setAutostartDaemon(false);
    QCOMPARE(autostart.count(), 1);

    preferences.setAutostartDaemon(true);
    QVERIFY(other.autostartDaemon());
    QCOMPARE(autostart.count(), 2);
  }

private:
  QTemporaryDir m_config;
};

QTEST_MAIN(TestAppPreferences)
#include "TestAppPreferences.moc"
