#include <QtTest>

#include <algorithm>

#include <QQuickItem>
#include <QQuickView>
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

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

  void disablesScreenAndLightsWithoutDaemon()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();

      auto tabEnabled = [&](const char* name){
        auto* item = window->findChild<QQuickItem*>(name);
        return item != nullptr && item->isEnabled();
      };
      QVERIFY(window->findChild<QQuickItem*>("tabScreen") != nullptr);

      QVERIFY(!configurator.model().running());
      QVERIFY(!tabEnabled("tabScreen"));
      QVERIFY(!tabEnabled("tabLights"));
      QVERIFY(tabEnabled("settingsButton"));

      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);
      QVERIFY(tabEnabled("tabScreen"));
      QVERIFY(tabEnabled("tabLights"));

      window->setProperty("tab", 1);
      api.close();
      QTRY_VERIFY_WITH_TIMEOUT(!configurator.model().running(), 5000);
      QTRY_COMPARE_WITH_TIMEOUT(window->property("tab").toInt(), 0, 2000);
      QVERIFY(!tabEnabled("tabScreen"));
      QVERIFY(tabEnabled("settingsButton"));
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void cogTogglesSettingsAndRestoresPreviousTab()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();
      QVERIFY(QTest::qWaitForWindowExposed(window));
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      auto* cog = window->findChild<QQuickItem*>("settingsButton");
      QVERIFY(cog != nullptr);
      QVERIFY(window->findChild<QQuickItem*>("tabSettings") == nullptr);

      auto click = [&](){
        const QPointF p = cog->mapToScene(QPointF(cog->width() / 2, cog->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, p.toPoint());
      };

      window->setProperty("tab", 2);
      click();
      QCOMPARE(window->property("tab").toInt(), 3);
      click();
      QCOMPARE(window->property("tab").toInt(), 2);
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void gammaBubbleShowsWhileDragging()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);
      window->setProperty("tab", 2);

      // Channel updates rebuild the delegates, so items are looked up again each time
      auto slider = [window](){ return itemByName(window->contentItem(), "gammaSlider"); };
      auto bubbleOpacity = [&slider](){
        auto* bubble = slider() ? itemByName(slider(), "valueBubble") : nullptr;
        return bubble ? bubble->property("opacity").toDouble() : -1.0;
      };
      QTRY_VERIFY_WITH_TIMEOUT(slider() != nullptr, 3000);
      QTest::qWait(200);
      QCOMPARE(bubbleOpacity(), 0.0);

      const auto target = slider()->mapToScene(QPointF(slider()->width() * 0.75, slider()->height() / 2)).toPoint();
      QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, target);
      QTRY_COMPARE_WITH_TIMEOUT(bubbleOpacity(), 1.0, 2000);
      QVERIFY(slider()->property("popupText").toString().startsWith("Gamma +"));
      QCOMPARE(api.count("PUT", "/api/setChannelGammaFactor"), 0);

      QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, target);
      QTRY_COMPARE_WITH_TIMEOUT(api.count("PUT", "/api/setChannelGammaFactor/0"), 1, 3000);
      QTRY_COMPARE_WITH_TIMEOUT(bubbleOpacity(), 0.0, 2000);
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void savedLabelFadesOutAfterSaving()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      auto* status = window->findChild<QQuickItem*>("saveStatus");
      QVERIFY(status != nullptr);
      QVERIFY(!status->isVisible());

      // Nothing is shown while the save is pending
      configurator.model().setInterpolation(1);
      QTest::qWait(100);
      QVERIFY(!status->isVisible());

      QTRY_VERIFY_WITH_TIMEOUT(status->isVisible(), 3000);
      QCOMPARE(status->property("text").toString(), QString("Saved"));
      QCOMPARE(status->property("color").value<QColor>(), QColor("#79c800"));
      QTRY_VERIFY_WITH_TIMEOUT(!status->isVisible(), 6000);
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void failedSaveShowsRedLabelThatStays()
  {
    {
      FakeDaemonApi api;
      api.failSaves = true;
      DaemonController controller;
      controller.setPort(api.port());
      controller.setDaemonPath("/usr/bin/true");
      controller.setPollIntervalMs(100);
      UpdateController updater("0.1.0");
      DaemonInstaller installer(PinnedDaemon::current());
      Settings settings;
      Localization localization(settings);
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      auto* status = window->findChild<QQuickItem*>("saveStatus");
      QVERIFY(status != nullptr);

      configurator.model().setInterpolation(1);
      QTRY_VERIFY_WITH_TIMEOUT(status->isVisible(), 3000);
      QCOMPARE(status->property("text").toString(), QString("Could not save profile"));
      QCOMPARE(status->property("color").value<QColor>(), QColor("#ff6161"));

      QTest::qWait(3500);
      QVERIFY(status->isVisible());
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void missingBinaryShowsInstallPageAndChooser()
  {
    {
      FakeDaemonApi api;
      DaemonController controller;
      controller.setPort(api.port());
      controller.setDaemonPath("/nonexistent/huenicorn");
      UpdateController updater("0.1.0");
      DaemonInstaller installer(PinnedDaemon::current());
      // Unmet requirements make the page taller than the window, which depends on the host
      HostProbe probe;
      probe.hasTool = [](const QString&){ return true; };
      probe.hasModules = [](const QStringList&){ return true; };
      probe.mbedtlsMajor = [](){ return 3; };
      installer.setProbe(probe);
      installer.setSession(SessionType::Wayland);
      installer.setSystem({PackageKind::Rpm, {"fedora"}});
      Settings settings;
      Localization localization(settings);
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();
      QVERIFY(QTest::qWaitForWindowExposed(window));
      QTest::qWait(100);

      QVERIFY(!configurator.model().canStart());
      QVERIFY(visibleTexts(window->contentItem()).contains("Choose existing binary..."));
      QVERIFY(visibleTexts(window->contentItem()).contains("Configured binary not found: /nonexistent/huenicorn"));

      auto* chooser = window->findChild<QQuickItem*>("chooseBinaryButton");
      QVERIFY(chooser != nullptr);
      QTRY_VERIFY(chooser->isVisible() && chooser->width() > 0 && chooser->height() > 0);
      QSignalSpy spy(&configurator.model(), &ConfiguratorModel::chooseDaemonRequested);
      const QPointF p = chooser->mapToScene(QPointF(chooser->width() / 2, chooser->height() / 2));
      QVERIFY2(QRectF(0, 0, window->width(), window->height()).contains(p),
               qPrintable(QString("button at %1,%2 is outside the %3x%4 window").arg(p.x()).arg(p.y()).arg(window->width()).arg(window->height())));
      QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, p.toPoint());
      QCOMPARE(spy.count(), 1);

      // Picking a real binary swaps the install page for the sync page
      controller.setDaemonPath("/usr/bin/true");
      QTest::qWait(100);
      QVERIFY(configurator.model().canStart());
      QVERIFY(!visibleTexts(window->contentItem()).contains("Choose existing binary..."));
      QVERIFY(visibleTexts(window->contentItem()).contains("Start Sync"));
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void changeBinaryFollowsDaemonState()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->setProperty("tab", 3);
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      auto* button = window->findChild<QQuickItem*>("changeBinaryButton");
      QVERIFY(button != nullptr);
      QVERIFY(!button->isEnabled());

      api.close();
      QTRY_VERIFY_WITH_TIMEOUT(!configurator.model().running(), 5000);
      QTRY_VERIFY_WITH_TIMEOUT(button->isEnabled(), 2000);

      QSignalSpy spy(&configurator.model(), &ConfiguratorModel::chooseDaemonRequested);
      QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
      QCOMPARE(spy.count(), 1);
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void startupTogglesWritePreferences()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->setProperty("tab", 3);
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      auto* autostart = itemByName(window->contentItem(), "autostartDaemonToggle");
      auto* stopOnQuit = itemByName(window->contentItem(), "stopOnQuitToggle");
      QVERIFY(autostart != nullptr);
      QVERIFY(stopOnQuit != nullptr);
      QVERIFY(itemByName(window->contentItem(), "launchAtLoginToggle") != nullptr);

      QCOMPARE(autostart->property("checked").toBool(), preferences.autostartDaemon());
      const bool before = preferences.autostartDaemon();
      QVERIFY(QMetaObject::invokeMethod(autostart, "toggled", Q_ARG(bool, !before)));
      QCOMPARE(preferences.autostartDaemon(), !before);
      QTRY_COMPARE_WITH_TIMEOUT(autostart->property("checked").toBool(), !before, 1000);
      QCOMPARE(Settings().autostartDaemon(), !before);

      const bool quitBefore = preferences.stopDaemonOnQuit();
      QVERIFY(QMetaObject::invokeMethod(stopOnQuit, "toggled", Q_ARG(bool, !quitBefore)));
      QCOMPARE(preferences.stopDaemonOnQuit(), !quitBefore);
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void settingsKeepTheirLayoutWithoutDaemon()
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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->setProperty("tab", 3);
      window->show();
      QTest::qWait(100);

      auto hasChips = [](const QStringList& texts){
        return std::any_of(texts.begin(), texts.end(), [](const QString& t){ return t.contains(u'\u00d7'); });
      };
      auto root = [&](){ return window->contentItem(); };
      const QString hint = "Available while light sync is running.";

      // Never connected: headings and the hint, but only empty placeholders
      const auto offline = visibleTexts(root());
      QVERIFY(offline.contains("Capture resolution"));
      QVERIFY(offline.contains("Interpolation"));
      QVERIFY(offline.contains("Transition smoothing"));
      QVERIFY(offline.contains(hint));
      QVERIFY(!offline.contains("Light sync is not running"));
      QVERIFY(!hasChips(offline));
      QVERIFY(!configurator.model().hasLoaded());

      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);
      QTest::qWait(60);

      const auto online = visibleTexts(root());
      QVERIFY(!online.contains(hint));
      QVERIFY(hasChips(online));
      QVERIFY(itemByName(root(), "resolutionChips")->isEnabled());
      QVERIFY(itemByName(root(), "interpolationTiles")->isEnabled());
      QVERIFY(itemByName(root(), "smoothingSlider")->isEnabled());

      // Hovering a resolution chip lights it up while the daemon runs, and not afterwards
      auto hoverColor = [&](){
        auto* flow = itemByName(root(), "resolutionChips");
        QQuickItem* chip = nullptr;
        for(auto* child : flow->childItems()){
          if(child->property("modelData").isValid() && !child->property("selected").toBool()){
            chip = child;
            break;
          }
        }
        if(chip == nullptr){
          return QColor();
        }
        QTest::mouseMove(window, QPoint(2, 2));
        QTest::qWait(200);
        QTest::mouseMove(window, chip->mapToScene(QPointF(chip->width() / 2, chip->height() / 2)).toPoint());
        QTest::qWait(250);
        return chip->property("color").value<QColor>();
      };
      const QColor lit = hoverColor();
      QVERIFY(lit.isValid());
      QCOMPARE(lit, QColor("#353a43"));

      // After the daemon stops the last readings stay visible but can no longer be changed
      api.close();
      QTRY_VERIFY_WITH_TIMEOUT(!configurator.model().running(), 5000);
      QTest::qWait(60);
      const auto stopped = visibleTexts(root());
      QVERIFY(stopped.contains(hint));
      QVERIFY(hasChips(stopped));
      QVERIFY(configurator.model().hasLoaded());
      QVERIFY(!itemByName(root(), "resolutionChips")->isEnabled());
      QVERIFY(!itemByName(root(), "interpolationTiles")->isEnabled());
      QVERIFY(!itemByName(root(), "smoothingSlider")->isEnabled());
      QCOMPARE(hoverColor(), QColor("#262a31"));
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
      AppPreferences preferences(settings);
      localization.setLanguage("en");
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->show();
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      window->setProperty("tab", 3);
      QTest::qWait(60);
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

      window->setProperty("tab", 3);
      localization.setLanguage("en");
      QTest::qWait(60);
      QVERIFY(visibleTexts(window->contentItem()).contains("Settings"));
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void languageDropdownOpensAndPicks()
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
      AppPreferences preferences(settings);
      localization.setLanguage("en");
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

      auto* window = configurator.window();
      QVERIFY(window != nullptr);
      window->setProperty("tab", 3);
      window->show();
      QVERIFY(QTest::qWaitForWindowExposed(window));
      controller.beginMonitoring();
      QTRY_VERIFY_WITH_TIMEOUT(configurator.model().loaded(), 5000);

      auto* dropdown = window->findChild<QQuickItem*>("languageDropdown");
      QVERIFY(dropdown != nullptr);
      QCOMPARE(dropdown->property("open").toBool(), false);

      // A click on the header opens the list below it
      const auto center = dropdown->mapToScene(QPointF(dropdown->width() / 2, dropdown->implicitHeight() / 2)).toPoint();
      QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
      QTRY_COMPARE(dropdown->property("open").toBool(), true);
      QVERIFY(!window->grabWindow().isNull());

      // Entries are System, Deutsch and English; only System has no flag
      const auto model = dropdown->property("model").toList();
      QVERIFY(model.size() >= 3);
      QVERIFY(model.at(0).toMap().value("flag").toString().isEmpty());
      QVERIFY(!model.at(1).toMap().value("flag").toString().isEmpty());

      // Picking German switches the app and closes the list
      const auto list = dropdown->mapToScene(QPointF(dropdown->width() / 2, dropdown->implicitHeight() + 6 + 6 + 46 * 1 + 23)).toPoint();
      QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, list);
      QTRY_COMPARE(localization.active(), QString("de"));
      QCOMPARE(dropdown->property("open").toBool(), false);
      QCOMPARE(settings.language(), QString("de"));

      // Any click outside closes the list without changing anything, page margins and header included
      const QList<QPoint> outside{QPoint(2, center.y()), QPoint(window->width() - 3, center.y()), QPoint(window->width() / 2, 6)};
      for(const auto& point : outside){
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
        QTRY_COMPARE(dropdown->property("open").toBool(), true);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(dropdown->property("open").toBool(), false);
        QCOMPARE(localization.active(), QString("de"));
      }

      localization.setLanguage("en");
      window->hide();
    }

    QVERIFY2(warnings().isEmpty(), qPrintable(warnings().join("\n")));
  }

  void dropdownShowsFourRowsAndScrollsTheRest()
  {
    Q_INIT_RESOURCE(qml);
    QQuickView view(QUrl("qrc:/configurator/qml/components/Dropdown.qml"));
    QVERIFY(view.status() == QQuickView::Ready);
    view.show();
    auto* dropdown = view.rootObject();
    QVERIFY(dropdown != nullptr);

    auto entries = [](int count){
      QVariantList list;
      for(int i = 0; i < count; i++){
        list.append(QVariantMap{{"label", QString("Language %1").arg(i)}, {"value", i}});
      }
      return list;
    };

    dropdown->setProperty("model", entries(8));
    dropdown->setProperty("current", 7);
    QCOMPARE(dropdown->property("visibleRows").toInt(), 4);

    dropdown->setProperty("open", true);
    auto* flick = dropdown->findChild<QQuickItem*>("dropdownFlick");
    QVERIFY(flick != nullptr);
    const auto rowHeight = dropdown->property("rowHeight").toReal();
    QCOMPARE(flick->height(), 4 * rowHeight);
    QTRY_COMPARE(flick->property("contentHeight").toReal(), 8 * rowHeight);

    // The current entry is last, so it is scrolled into view when opening
    dropdown->setProperty("open", false);
    dropdown->setProperty("open", true);
    QCOMPARE(flick->property("contentY").toReal(), 4 * rowHeight);

    // A short list shows everything and has nothing to scroll
    dropdown->setProperty("open", false);
    dropdown->setProperty("model", entries(3));
    dropdown->setProperty("current", 0);
    QCOMPARE(dropdown->property("visibleRows").toInt(), 3);
    dropdown->setProperty("open", true);
    QCOMPARE(flick->height(), 3 * rowHeight);
    QTRY_COMPARE(flick->property("contentHeight").toReal(), 3 * rowHeight);
    QCOMPARE(flick->property("contentY").toReal(), 0.0);

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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);

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
      AppPreferences preferences(settings);
      ConfiguratorWindow configurator(controller, updater, installer, localization, preferences);
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

  // Repeater delegates are not QObject children of the page, so findChild misses them
  static QQuickItem* itemByName(QQuickItem* item, const QString& name)
  {
    if(item->objectName() == name){
      return item;
    }
    for(auto* child : item->childItems()){
      if(auto* found = itemByName(child, name)){
        return found;
      }
    }
    return nullptr;
  }

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
