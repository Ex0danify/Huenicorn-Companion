#include <QtTest>

#include <QQuickItem>
#include <QQuickWindow>

#include "FakeDaemonApi.hpp"
#include "FakeCommandRunner.hpp"
#include "configurator/ConfiguratorWindow.hpp"

// Catches QML syntax, import and binding errors that only show up at load time
class TestConfiguratorQml : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    // Settings would otherwise write into the real user config
    QVERIFY(m_config.isValid());
    qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
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
      controller.setDaemonPath("/usr/bin/true");
      controller.setPollIntervalMs(100);
      UpdateController updater("0.1.0");
      DaemonInstaller installer(PinnedDaemon::current());
      Settings settings;
      Localization localization(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization);

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

  void retranslatesLiveWithoutWarnings()
  {
    {
      FakeDaemonApi api;
      DaemonController controller;
      controller.setPort(api.port());
      controller.setDaemonPath("/usr/bin/true");
      controller.setPollIntervalMs(100);
      UpdateController updater("0.1.0");
      DaemonInstaller installer(PinnedDaemon::current());
      Settings settings;
      Localization localization(settings);
      localization.setLanguage("en");
      ConfiguratorWindow configurator(controller, updater, installer, localization);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      QVERIFY(visibleTexts(window->contentItem()).contains("Settings"));

      localization.setLanguage("de");
      QTest::qWait(60);
      const auto german = visibleTexts(window->contentItem());
      QVERIFY(german.contains("Einstellungen"));
      QVERIFY(!german.contains("Settings"));
      for(int tab = 0; tab < 4; tab++){
        window->setProperty("tab", tab);
        QTest::qWait(60);
        QVERIFY(!window->grabWindow().isNull());
      }

      localization.setLanguage("en");
      QTest::qWait(60);
      QVERIFY(visibleTexts(window->contentItem()).contains("Settings"));
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
      controller.setDaemonPath("/usr/bin/true");
      controller.setPollIntervalMs(100);
      UpdateController updater("0.1.0");
      DaemonInstaller installer(PinnedDaemon::current());
      Settings settings;
      Localization localization(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization);

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

  void rendersInstallPageInEveryPhase()
  {
    {
      DaemonController controller;
      UpdateController updater("0.1.0");

      DaemonPin pin = PinnedDaemon::current();
      pin.dependencies.clear();
      DaemonInstaller installer(pin);

      QTemporaryDir dir;
      installer.setInstallDir(dir.path() + "/bin");
      installer.setWorkRoot(dir.path() + "/work");
      installer.setSession(SessionType::Wayland);
      installer.setSystem({PackageKind::Rpm, {"fedora"}});

      auto runnerOwner = std::make_unique<FakeRunner>();
      auto* runner = runnerOwner.get();
      installer.setRunner(std::move(runnerOwner));
      bool failBuild = false;
      runner->handler = [&](const Command& command, QString& output){
        if(command.arguments.contains("rev-parse")){
          output = pin.commit + "\n";
        }
        if(command.program == "cmake" && command.arguments.value(0) == "--build"){
          if(failBuild){
            output = "error: broken\n";
            return 2;
          }
          QDir().mkpath(command.arguments.value(1));
          QFile binary(command.arguments.value(1) + "/huenicorn");
          if(binary.open(QIODevice::WriteOnly)){
            binary.write("daemon");
          }
        }
        return 0;
      };

      Settings settings;
      Localization localization(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization);
      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();

      auto render = [&](const QString& phase){
        QTRY_COMPARE(installer.phase(), phase);
        QTest::qWait(60);
        QVERIFY(!window->grabWindow().isNull());
      };

      HostProbe probe;
      probe.hasTool = [](const QString& tool){ return tool != "cmake"; };
      probe.hasModules = [](const QStringList&){ return true; };
      probe.mbedtlsMajor = [](){ return 3; };
      installer.setProbe(probe);
      installer.refresh();
      render("missing");

      probe.hasTool = [](const QString&){ return true; };
      installer.setProbe(probe);
      installer.refresh();
      render("ready");

      runner->hang = true;
      installer.build();
      render("building");
      installer.cancel();
      render("ready");

      runner->hang = false;
      failBuild = true;
      installer.build();
      render("failed");

      failBuild = false;
      installer.build();
      render("installed");
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

private:
  QTemporaryDir m_config;

  static QStringList visibleTexts(QQuickItem* item)
  {
    QStringList texts;
    if(item->isVisible() && item->property("text").isValid()){
      texts.append(item->property("text").toString());
    }
    for(auto* child : item->childItems()){
      texts.append(visibleTexts(child));
    }
    return texts;
  }

  static QStringList& warnings()
  {
    static QStringList list;
    return list;
  }
};

QTEST_MAIN(TestConfiguratorQml)
#include "TestConfiguratorQml.moc"
