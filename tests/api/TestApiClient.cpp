#include <QtTest>

#include "FakeDaemonApi.hpp"
#include "api/ApiClient.hpp"

class TestApiClient : public QObject
{
  Q_OBJECT

private slots:
  void encodesBareScalars()
  {
    QCOMPARE(ApiClient::encodeBody(42), QByteArray("42"));
    QCOMPARE(ApiClient::encodeBody(0.5), QByteArray("0.5"));
    QCOMPARE(ApiClient::encodeBody(QString("area-a")), QByteArray("\"area-a\""));
    QCOMPARE(ApiClient::encodeBody(QJsonObject{{"active", true}}), QByteArray("{\"active\":true}"));
  }

  void getParsesJson()
  {
    FakeDaemonApi api;
    ApiClient client;
    client.setBaseUrl(QUrl(QString("http://127.0.0.1:%1").arg(api.port())));

    bool called = false;
    client.get("/api/version", [&](bool ok, const QJsonDocument& doc){
      called = true;
      QVERIFY(ok);
      QCOMPARE(doc.object().value("version").toString(), QString("9.9.9"));
    });
    QTRY_VERIFY(called);
  }

  void putSendsScalarBody()
  {
    FakeDaemonApi api;
    ApiClient client;
    client.setBaseUrl(QUrl(QString("http://127.0.0.1:%1").arg(api.port())));

    bool called = false;
    client.put("/api/setRefreshRate", 90, [&](bool ok, const QJsonDocument& doc){
      called = true;
      QVERIFY(ok);
      QCOMPARE(doc.object().value("refreshRate").toInt(), 90);
    });
    QTRY_VERIFY(called);
    QCOMPARE(api.requests.last().body, QByteArray("90"));
    QCOMPARE(api.requests.last().method, QString("PUT"));
  }

  void reportsUnreachableServer()
  {
    ApiClient client;
    client.setBaseUrl(QUrl("http://127.0.0.1:1"));

    bool called = false;
    client.get("/api/version", [&](bool ok, const QJsonDocument&){
      called = true;
      QVERIFY(!ok);
    }, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(called, 3000);
  }
};

QTEST_MAIN(TestApiClient)
#include "TestApiClient.moc"
