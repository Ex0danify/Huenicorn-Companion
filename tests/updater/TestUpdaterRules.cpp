#include <QtTest>

#include "updater/Platform.hpp"
#include "updater/Release.hpp"

class TestUpdaterRules : public QObject
{
  Q_OBJECT

private slots:
  void comparesVersionsNumerically_data()
  {
    QTest::addColumn<QString>("current");
    QTest::addColumn<QString>("candidate");
    QTest::addColumn<bool>("newer");

    QTest::newRow("patch") << "0.2.0" << "v0.2.1" << true;
    QTest::newRow("minor beats patch") << "0.9.0" << "v0.10.0" << true;
    QTest::newRow("same") << "0.2.0" << "v0.2.0" << false;
    QTest::newRow("older") << "0.3.0" << "v0.2.9" << false;
    QTest::newRow("longer tag") << "1.0" << "v1.0.1" << true;
    QTest::newRow("prerelease is ignored") << "0.2.0" << "v0.3.0-rc1" << false;
    QTest::newRow("garbage") << "0.2.0" << "latest" << false;
    QTest::newRow("empty") << "0.2.0" << "" << false;
    QTest::newRow("broken current version") << "dev" << "v0.3.0" << false;
  }

  void comparesVersionsNumerically()
  {
    QFETCH(QString, current);
    QFETCH(QString, candidate);
    QFETCH(bool, newer);
    QCOMPARE(Release::isNewer(current, candidate), newer);
  }

  void onlyTrustsSecureUrls()
  {
    QVERIFY(Release::isTrustedUrl(QUrl("https://github.com/x/y")));
    QVERIFY(Release::isTrustedUrl(QUrl("http://127.0.0.1:8000/x")));
    QVERIFY(Release::isTrustedUrl(QUrl("http://localhost/x")));
    QVERIFY(!Release::isTrustedUrl(QUrl("http://github.com/x/y")));
    QVERIFY(!Release::isTrustedUrl(QUrl("ftp://github.com/x")));
    QVERIFY(!Release::isTrustedUrl(QUrl("https:///nohost")));
    QVERIFY(!Release::isTrustedUrl(QUrl()));
  }

  void findsChecksumByExactName()
  {
    const QByteArray sums =
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  huenicorn-companion-0.3.0-fedora.rpm\n"
      "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB *huenicorn-companion-0.3.0-arch.pkg.tar.zst\n"
      "cccc  too-short-hash.deb\n";

    QCOMPARE(Release::checksumFor(sums, "huenicorn-companion-0.3.0-fedora.rpm").value(), QString(64, 'a'));
    QCOMPARE(Release::checksumFor(sums, "huenicorn-companion-0.3.0-arch.pkg.tar.zst").value(), QString(64, 'b'));
    QVERIFY(!Release::checksumFor(sums, "fedora.rpm").has_value());
    QVERIFY(!Release::checksumFor(sums, "too-short-hash.deb").has_value());
    QVERIFY(!Release::checksumFor({}, "huenicorn-companion-0.3.0-fedora.rpm").has_value());
  }

  void parsesReleaseJson()
  {
    const auto json = QJsonObject{
      {"tag_name", "v0.3.0"},
      {"body", "notes"},
      {"html_url", "https://github.com/Ex0danify/Huenicorn-Companion/releases/tag/v0.3.0"},
      {"assets", QJsonArray{
        QJsonObject{{"name", "good.rpm"}, {"browser_download_url", "https://github.com/good.rpm"}},
        QJsonObject{{"name", "insecure.rpm"}, {"browser_download_url", "http://example.com/insecure.rpm"}},
        QJsonObject{{"name", ""}, {"browser_download_url", "https://github.com/nameless"}}
      }}
    };

    const auto release = ReleaseInfo::fromJson(json);
    QVERIFY(release.has_value());
    QCOMPARE(release->tag, QString("v0.3.0"));
    QCOMPARE(release->assets.size(), 1);
    QVERIFY(release->findAsset("good.rpm").has_value());
    QVERIFY(!release->findAsset("insecure.rpm").has_value());

    QVERIFY(!ReleaseInfo::fromJson(QJsonObject{{"message", "Not Found"}}).has_value());
  }

  void detectsPackageKind_data()
  {
    QTest::addColumn<QByteArray>("osRelease");
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("firstId");

    QTest::newRow("nobara") << QByteArray("ID=nobara\nID_LIKE=\"rhel centos fedora\"\n") << int(PackageKind::Rpm) << "nobara";
    QTest::newRow("fedora") << QByteArray("NAME=\"Fedora Linux\"\nID=fedora\n") << int(PackageKind::Rpm) << "fedora";
    QTest::newRow("ubuntu") << QByteArray("ID=ubuntu\nID_LIKE=debian\n") << int(PackageKind::Deb) << "ubuntu";
    QTest::newRow("mint") << QByteArray("ID=linuxmint\nID_LIKE=\"ubuntu debian\"\n") << int(PackageKind::Deb) << "linuxmint";
    QTest::newRow("arch") << QByteArray("ID=arch\n") << int(PackageKind::Pacman) << "arch";
    QTest::newRow("manjaro") << QByteArray("ID=manjaro\nID_LIKE=arch\n") << int(PackageKind::Pacman) << "manjaro";
    QTest::newRow("quoted id") << QByteArray("ID=\"debian\"\n# comment\n") << int(PackageKind::Deb) << "debian";
    QTest::newRow("unsupported") << QByteArray("ID=gentoo\n") << int(PackageKind::None) << "gentoo";
    QTest::newRow("empty") << QByteArray() << int(PackageKind::None) << "";
  }

  void detectsPackageKind()
  {
    QFETCH(QByteArray, osRelease);
    QFETCH(int, kind);
    QFETCH(QString, firstId);

    const auto info = Platform::parseOsRelease(osRelease);
    QCOMPARE(int(info.kind), kind);
    QCOMPARE(info.ids.value(0), firstId);
  }

  void picksTheAssetForTheSystem()
  {
    const QList<ReleaseAsset> assets = {
      {"huenicorn-companion-0.3.0-fedora.rpm", QUrl("https://x/fedora.rpm")},
      {"huenicorn-companion-0.3.0-ubuntu-24.04.deb", QUrl("https://x/ubuntu.deb")},
      {"huenicorn-companion-0.3.0-debian-13.deb", QUrl("https://x/debian.deb")},
      {"huenicorn-companion-0.3.0-arch.pkg.tar.zst", QUrl("https://x/arch.zst")},
      {"huenicorn-companion-0.3.0-linux-x86_64.tar.gz", QUrl("https://x/tarball")},
      {"SHA256SUMS", QUrl("https://x/sums")}
    };

    auto pick = [&](const QByteArray& osRelease){
      const auto asset = Platform::pickAsset(assets, Platform::parseOsRelease(osRelease));
      return asset ? asset->name : QString();
    };

    QCOMPARE(pick("ID=nobara\nID_LIKE=\"rhel centos fedora\""), QString("huenicorn-companion-0.3.0-fedora.rpm"));
    QCOMPARE(pick("ID=debian"), QString("huenicorn-companion-0.3.0-debian-13.deb"));
    QCOMPARE(pick("ID=ubuntu\nID_LIKE=debian"), QString("huenicorn-companion-0.3.0-ubuntu-24.04.deb"));
    QCOMPARE(pick("ID=linuxmint\nID_LIKE=\"ubuntu debian\""), QString("huenicorn-companion-0.3.0-ubuntu-24.04.deb"));
    QCOMPARE(pick("ID=manjaro\nID_LIKE=arch"), QString("huenicorn-companion-0.3.0-arch.pkg.tar.zst"));
    QCOMPARE(pick("ID=gentoo"), QString());

    // A package kind with no matching asset must not fall back to another kind
    QCOMPARE(Platform::pickAsset({assets.last()}, Platform::parseOsRelease("ID=fedora")).has_value(), false);
  }

  void buildsInstallCommands()
  {
    QCOMPARE(Platform::installCommand(PackageKind::Rpm, "/t/a.rpm"), (QStringList{"dnf", "install", "-y", "/t/a.rpm"}));
    QCOMPARE(Platform::installCommand(PackageKind::Pacman, "/t/a.zst"), (QStringList{"pacman", "-U", "--noconfirm", "/t/a.zst"}));
    const auto deb = Platform::installCommand(PackageKind::Deb, "/t/a.deb");
    QCOMPARE(deb.last(), QString("/t/a.deb"));
    QVERIFY(deb.contains("apt-get"));
    QVERIFY(Platform::installCommand(PackageKind::None, "/t/a").isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestUpdaterRules)
#include "TestUpdaterRules.moc"
