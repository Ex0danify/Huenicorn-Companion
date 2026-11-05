#include <QtTest>

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>

#include "updater/UpdateController.hpp"

// Serves canned responses by path, just enough HTTP for the updater
class FakeGitHub : public QObject
{
public:
  struct Response
  {
    int status{200};
    QByteArray body;
  };

  FakeGitHub()
  {
    connect(&m_server, &QTcpServer::newConnection, this, [this](){
      while(auto* socket = m_server.nextPendingConnection()){
        connect(socket, &QTcpSocket::readyRead, this, [this, socket](){ answer(socket); });
      }
    });
    m_server.listen(QHostAddress::LocalHost);
  }

  QString url(const QString& path) const { return QString("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path); }

  QMap<QString, Response> routes;
  QStringList requested;

private:
  void answer(QTcpSocket* socket)
  {
    const auto head = socket->peek(socket->bytesAvailable());
    if(!head.contains("\r\n\r\n")){
      return;
    }
    socket->readAll();

    const auto path = QString::fromLatin1(head.split(' ').value(1));
    requested.append(path);
    const auto response = routes.value(path, Response{404, "not found"});

    socket->write("HTTP/1.1 " + QByteArray::number(response.status) + " X\r\nContent-Length: "
                  + QByteArray::number(response.body.size()) + "\r\nConnection: close\r\n\r\n" + response.body);
    socket->disconnectFromHost();
  }

  QTcpServer m_server;
};


class TestUpdateController : public QObject
{
  Q_OBJECT

private:
  static constexpr const char* AssetName = "huenicorn-qt-0.3.0-fedora.rpm";
  const QByteArray m_package{"pretend this is an rpm"};

  void serveRelease(FakeGitHub& github, const QString& tag, const QByteArray& sums = {})
  {
    const QJsonObject release{
      {"tag_name", tag},
      {"body", "## Features\n* something"},
      {"html_url", "https://github.com/Ex0danify/Huenicorn-GUI/releases/tag/" + tag},
      {"assets", QJsonArray{
        QJsonObject{{"name", AssetName}, {"browser_download_url", github.url("/download/package")}},
        QJsonObject{{"name", "SHA256SUMS"}, {"browser_download_url", github.url("/download/sums")}}
      }}
    };
    github.routes["/latest"] = {200, QJsonDocument(release).toJson()};
    github.routes["/download/package"] = {200, m_package};

    const auto hash = QCryptographicHash::hash(m_package, QCryptographicHash::Sha256).toHex();
    github.routes["/download/sums"] = {200, sums.isNull() ? hash + "  " + AssetName + "\n" : sums};
  }

  static void configure(UpdateController& updater, const FakeGitHub& github, const QString& elevation = "true")
  {
    updater.setApiUrl(QUrl(github.url("/latest")));
    updater.setSystem(Platform::parseOsRelease("ID=fedora"));
    updater.setElevationProgram(elevation);
  }

private slots:
  void announcesANewerRelease()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github);

    updater.check();
    QCOMPARE(updater.phase(), QString("checking"));
    QTRY_COMPARE(updater.phase(), QString("available"));
    QCOMPARE(updater.latestVersion(), QString("0.3.0"));
    QVERIFY(updater.releaseNotes().contains("something"));
    QVERIFY(updater.canInstall());
  }

  void reportsUpToDate()
  {
    FakeGitHub github;
    serveRelease(github, "v0.2.0");
    UpdateController updater("0.2.0");
    configure(updater, github);

    QSignalSpy completed(&updater, &UpdateController::checkCompleted);
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("upToDate"));
    QCOMPARE(completed.count(), 1);
  }

  void reportsRateLimit()
  {
    FakeGitHub github;
    github.routes["/latest"] = {403, "{}"};
    UpdateController updater("0.2.0");
    configure(updater, github);

    updater.check();
    QTRY_COMPARE(updater.phase(), QString("failed"));
    QVERIFY(updater.errorText().contains("rate limit"));
  }

  void rejectsMalformedAnswers()
  {
    FakeGitHub github;
    github.routes["/latest"] = {200, "<html>captive portal</html>"};
    UpdateController updater("0.2.0");
    configure(updater, github);

    updater.check();
    QTRY_COMPARE(updater.phase(), QString("failed"));
  }

  void refusesInsecureApiAddress()
  {
    UpdateController updater("0.2.0");
    updater.setApiUrl(QUrl("http://example.com/latest"));

    updater.check();
    QCOMPARE(updater.phase(), QString("failed"));
  }

  void failedCheckCanBeRetried()
  {
    FakeGitHub github;
    UpdateController updater("0.2.0");
    configure(updater, github);

    updater.check();
    QTRY_COMPARE(updater.phase(), QString("failed"));

    serveRelease(github, "v0.3.0");
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));
  }

  void announcesInTheBackgroundOnlyForNewVersions()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github);
    QSignalSpy found(&updater, &UpdateController::backgroundUpdateFound);

    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));
    QCOMPARE(found.count(), 0);

    updater.setSkippedVersion("0.3.0");
    updater.checkInBackground();
    QTRY_COMPARE(updater.phase(), QString("available"));
    QCOMPARE(found.count(), 0);

    updater.setSkippedVersion("0.2.5");
    updater.checkInBackground();
    QTRY_COMPARE(found.count(), 1);
    QCOMPARE(found.first().first().toString(), QString("0.3.0"));
  }

  void skipRemembersTheLatestVersion()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github);
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    QSignalSpy skipped(&updater, &UpdateController::skippedVersionChanged);
    updater.skipVersion();
    QCOMPARE(updater.skippedVersion(), QString("0.3.0"));
    QCOMPARE(skipped.count(), 1);
  }

  void installsAVerifiedPackage()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github);
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    updater.install();
    QTRY_COMPARE(updater.phase(), QString("installed"));
    QCOMPARE(updater.progress(), 1.0);

    QSignalSpy restart(&updater, &UpdateController::restartRequested);
    updater.restart();
    QCOMPARE(restart.count(), 1);
  }

  void refusesAPackageWithTheWrongChecksum()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0", QByteArray(64, '0') + "  " + AssetName + "\n");
    UpdateController updater("0.2.0");
    // "false" would fail the install too, so a checksum error proves it never got that far
    configure(updater, github, "false");
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    updater.install();
    QTRY_COMPARE(updater.phase(), QString("failed"));
    QVERIFY(updater.errorText().contains("checksum"));
  }

  void refusesAPackageWithoutChecksumEntry()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0", QByteArray(64, 'a') + "  other-file.rpm\n");
    UpdateController updater("0.2.0");
    configure(updater, github);
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    updater.install();
    QTRY_COMPARE(updater.phase(), QString("failed"));
    QVERIFY(!github.requested.contains("/download/package"));
  }

  void reportsAFailingInstall()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github, "false");
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    updater.install();
    QTRY_COMPARE(updater.phase(), QString("failed"));
    QVERIFY(updater.errorText().contains("Installation failed"));
  }

  void reportsAMissingElevationProgram()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github, "/nonexistent/pkexec");
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    QVERIFY(!updater.canInstall());
    updater.install();
    QCOMPARE(updater.phase(), QString("available"));
  }

  void doesNotInstallOnUnsupportedSystems()
  {
    FakeGitHub github;
    serveRelease(github, "v0.3.0");
    UpdateController updater("0.2.0");
    configure(updater, github);
    updater.setSystem(Platform::parseOsRelease("ID=gentoo"));
    updater.check();
    QTRY_COMPARE(updater.phase(), QString("available"));

    QVERIFY(!updater.canInstall());
    updater.install();
    QCOMPARE(updater.phase(), QString("available"));
    QVERIFY(!github.requested.contains("/download/package"));
  }

  void restartNeedsAnInstalledUpdate()
  {
    UpdateController updater("0.2.0");
    QSignalSpy restart(&updater, &UpdateController::restartRequested);
    updater.restart();
    QCOMPARE(restart.count(), 0);
  }

  void startupChecksAreThrottled()
  {
    const auto now = QDateTime(QDate(2026, 11, 5), QTime(12, 0));
    QVERIFY(UpdateController::checkDue(QDateTime(), now));
    QVERIFY(!UpdateController::checkDue(now.addSecs(-3600), now));
    QVERIFY(UpdateController::checkDue(now.addSecs(-24 * 3600), now));
    QVERIFY(UpdateController::checkDue(now.addSecs(3600), now));
  }
};

QTEST_GUILESS_MAIN(TestUpdateController)
#include "TestUpdateController.moc"
