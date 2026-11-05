#include <QtTest>

#include "FakeDaemonApi.hpp"
#include "configurator/ConfiguratorModel.hpp"

class TestConfiguratorModel : public QObject
{
  Q_OBJECT

private:
  struct Fixture
  {
    FakeDaemonApi api;
    DaemonController controller;
    std::unique_ptr<ConfiguratorModel> model;

    explicit Fixture(bool setupMode = false)
    {
      api.setupMode = setupMode;
      controller.setPort(api.port());
      controller.setPollIntervalMs(100);
      model = std::make_unique<ConfiguratorModel>(controller);
      controller.beginMonitoring();
    }

    QVariantMap channel(int id) const
    {
      for(const auto& c : model->channels()){
        if(c.toMap().value("channelId").toInt() == id){
          return c.toMap();
        }
      }
      return {};
    }
  };

private slots:
  void loadsStateOnceDaemonIsRunning()
  {
    Fixture f;
    QVERIFY(!f.model->loaded());
    QTRY_VERIFY(f.model->loaded());

    QCOMPARE(f.model->areas().size(), 2);
    QCOMPARE(f.model->currentAreaName(), QString("Gaming desk"));
    QCOMPARE(f.model->channels().size(), 2);
    QCOMPARE(f.model->activeChannelCount(), 1);
    QCOMPARE(f.model->displayWidth(), 2560);
    QCOMPARE(f.model->refreshRate(), 60);
    QCOMPARE(f.model->maxRefreshRate(), 144);
    QCOMPARE(f.model->smoothing(), 15.0);
    QCOMPARE(f.model->interpolations().size(), 3);
    QCOMPARE(f.model->interpolation(), 2);
    QVERIFY(!f.model->dirty());
    QCOMPARE(f.channel(0).value("name").toString(), QString("Ceiling Spot 1"));
  }

  void doesNotLoadWhileTheDaemonIsInSetup()
  {
    Fixture f(true);
    QSignalSpy failures(f.model.get(), &ConfiguratorModel::requestFailed);

    QTRY_VERIFY_WITH_TIMEOUT(f.model->setup()->active(), 5000);
    QTest::qWait(300);

    QVERIFY(!f.model->loaded());
    QCOMPARE(f.api.count("GET", "/api/channels"), 0);
    QVERIFY(failures.isEmpty());
  }

  void loadsAfterSetupFinished()
  {
    Fixture f(true);
    QTRY_VERIFY_WITH_TIMEOUT(f.model->setup()->active(), 5000);
    f.model->setup()->setRetryIntervalMs(10);

    f.model->setup()->submitBridge("192.168.1.20");
    QTRY_COMPARE(f.model->setup()->step(), QString("pair"));
    f.model->setup()->pair();

    QTRY_VERIFY_WITH_TIMEOUT(f.model->loaded(), 5000);
    QVERIFY(!f.model->setup()->active());
    QCOMPARE(f.model->channels().size(), 2);
  }

  void clearsWhenDaemonDisappears()
  {
    auto f = std::make_unique<Fixture>();
    QTRY_VERIFY(f->model->loaded());

    f->api.close();
    QTRY_VERIFY_WITH_TIMEOUT(!f->model->loaded(), 5000);
    QVERIFY(f->model->channels().isEmpty());
  }

  void channelActivityRoundTrips()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setChannelActive(1, true);
    QVERIFY(f.channel(1).value("active").toBool());
    QTRY_COMPARE(f.api.count("POST", "/api/setChannelActivity/1"), 1);
    QTRY_VERIFY(f.model->dirty());
    QCOMPARE(f.model->activeChannelCount(), 2);
    QCOMPARE(QJsonDocument::fromJson(f.api.lastBody("/api/setChannelActivity/1")).object().value("active").toBool(), true);
  }

  void channelRectSendsBothCornersAndAppliesDaemonResult()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setChannelRect(0, 0.1, 0.2, 0.4, 0.6);
    QTRY_COMPARE(f.api.count("PUT", "/api/setChannelUV/0"), 2);

    // Both corners go out concurrently, so arrival order is not defined
    QMap<int, QJsonObject> corners;
    for(int i = f.api.requests.size() - 2; i < f.api.requests.size(); i++){
      const auto body = QJsonDocument::fromJson(f.api.requests[i].body).object();
      corners.insert(body.value("type").toInt(), body);
    }
    QCOMPARE(corners.keys(), (QList<int>{0, 3}));
    QCOMPARE(corners.value(3).value("x").toDouble(), 0.4);

    QTRY_VERIFY(f.model->dirty());
    const auto c = f.channel(0);
    QCOMPARE(c.value("x0").toDouble(), 0.1);
    QCOMPARE(c.value("y1").toDouble(), 0.6);
  }

  void rectIsClampedToScreen()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setChannelRect(0, -0.5, 0.0, 2.0, 1.0);
    const auto c = f.channel(0);
    QCOMPARE(c.value("x0").toDouble(), 0.0);
    QCOMPARE(c.value("x1").toDouble(), 1.0);
  }

  void settingsAreSentAndClamped()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setRefreshRate(999);
    QCOMPARE(f.model->refreshRate(), 144);
    f.model->setSmoothing(200);
    QCOMPARE(f.model->smoothing(), 95.0);
    f.model->setChannelGamma(0, 5);
    QCOMPARE(f.channel(0).value("gamma").toDouble(), 1.0);

    QTRY_COMPARE(f.api.count("PUT", "/api/setRefreshRate"), 1);
    QTRY_COMPARE(f.api.count("PUT", "/api/setTransitionSmoothing"), 1);
    QTRY_COMPARE(f.api.count("PUT", "/api/setChannelGammaFactor/0"), 1);
    QCOMPARE(f.api.lastBody("/api/setRefreshRate"), QByteArray("144"));
    QCOMPARE(f.api.lastBody("/api/setTransitionSmoothing"), QByteArray("95"));
  }

  void selectingAreaSendsIdAsJsonString()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->selectArea("area-b");
    QTRY_COMPARE(f.model->currentAreaId(), QString("area-b"));
    QCOMPARE(f.model->currentAreaName(), QString("Living room"));
    QCOMPARE(f.api.lastBody("/api/setEntertainmentConfiguration"), QByteArray("\"area-b\""));
  }

  void changesAreSavedAutomatically()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    QSignalSpy saved(f.model.get(), &ConfiguratorModel::saved);
    f.model->setInterpolation(1);
    QTRY_VERIFY(f.model->dirty());

    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(saved.first().first().toBool());
    QTRY_VERIFY(!f.model->dirty());
    QVERIFY(!f.model->saving());
  }

  void rapidChangesCollapseIntoOneSave()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    for(int rate = 30; rate < 40; ++rate){
      f.model->setRefreshRate(rate);
    }
    QSignalSpy saved(f.model.get(), &ConfiguratorModel::saved);
    QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 3000);
    QCOMPARE(f.api.count("POST", "/api/saveProfile"), 1);
    QCOMPARE(f.model->refreshRate(), 39);
  }

  void editDuringSaveIsSavedAgain()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setInterpolation(1);
    f.model->save();
    f.model->setSmoothing(40);
    QTRY_VERIFY_WITH_TIMEOUT(!f.model->dirty() && !f.model->saving(), 3000);
    QVERIFY(f.api.count("POST", "/api/saveProfile") >= 2);
  }

  void failedSaveKeepsDirtyFlag()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setInterpolation(1);
    QTRY_VERIFY(f.model->dirty());
    f.api.failSaves = true;

    QSignalSpy saved(f.model.get(), &ConfiguratorModel::saved);
    f.model->save();
    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(!saved.first().first().toBool());
    QVERIFY(f.model->dirty());
  }

  void flushThenRunsImmediatelyWhenClean()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    bool called = false;
    f.model->flushThen([&called](){ called = true; });
    QVERIFY(called);
    QCOMPARE(f.api.count("POST", "/api/saveProfile"), 0);
  }

  void flushThenWaitsForPendingSave()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setInterpolation(1);
    QTRY_VERIFY(f.model->dirty());

    int savesWhenCalled = -1;
    f.model->flushThen([&](){ savesWhenCalled = f.api.count("POST", "/api/saveProfile"); });

    // Must not wait for the debounce timer to run out
    QTRY_VERIFY_WITH_TIMEOUT(savesWhenCalled >= 0, 400);
    QCOMPARE(savesWhenCalled, 1);
    QVERIFY(!f.model->dirty());
  }

  void flushThenWaitsForEditMadeDuringSave()
  {
    Fixture f;
    QTRY_VERIFY(f.model->loaded());

    f.model->setInterpolation(1);
    QTRY_VERIFY(f.model->dirty());
    f.model->save();
    f.model->setSmoothing(40);

    bool called = false;
    f.model->flushThen([&called](){ called = true; });
    QTRY_VERIFY_WITH_TIMEOUT(called, 3000);
    QVERIFY(!f.model->dirty());
    QVERIFY(!f.model->saving());
  }
};

QTEST_MAIN(TestConfiguratorModel)
#include "TestConfiguratorModel.moc"
