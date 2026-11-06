#include <QtTest>

#include "installer/PinnedDaemon.hpp"
#include "installer/Requirements.hpp"

namespace
{
  HostProbe probeWith(const QStringList& tools, const QStringList& modules, int mbedtls = 3)
  {
    HostProbe probe;
    probe.hasTool = [tools](const QString& tool){ return tools.contains(tool); };
    probe.hasModules = [modules](const QStringList& wanted){
      for(const auto& module : wanted){
        if(!modules.contains(module)){
          return false;
        }
      }
      return true;
    };
    probe.mbedtlsMajor = [mbedtls](){ return mbedtls; };
    return probe;
  }

  const QStringList AllTools{"git", "cmake", "g++", "make", "tar", "xz", "pkg-config"};
  const QStringList X11Modules{"libcurl", "opencv4", "x11", "xext", "xrandr"};
  const QStringList WaylandModules{"libcurl", "opencv4", "libpipewire-0.3", "glib-2.0", "gio-2.0", "gio-unix-2.0"};

  QStringList missingIds(const QList<Requirement>& requirements)
  {
    QStringList ids;
    for(const auto& requirement : requirements){
      if(!requirement.met){
        ids.append(requirement.id);
      }
    }
    return ids;
  }
}


class TestRequirements : public QObject
{
  Q_OBJECT

private slots:
  void detectsSessionType_data()
  {
    QTest::addColumn<QString>("sessionType");
    QTest::addColumn<QString>("waylandDisplay");
    QTest::addColumn<QString>("display");
    QTest::addColumn<int>("expected");

    QTest::newRow("explicit wayland") << "wayland" << "" << "" << int(SessionType::Wayland);
    QTest::newRow("explicit x11") << "x11" << "" << ":0" << int(SessionType::X11);
    QTest::newRow("case is ignored") << "Wayland" << "" << "" << int(SessionType::Wayland);
    QTest::newRow("xwayland does not make it x11") << "" << "wayland-0" << ":0" << int(SessionType::Wayland);
    QTest::newRow("only display") << "" << "" << ":0" << int(SessionType::X11);
    QTest::newRow("tty is unknown") << "tty" << "" << "" << int(SessionType::Unknown);
    QTest::newRow("nothing") << "" << "" << "" << int(SessionType::Unknown);
  }

  void detectsSessionType()
  {
    QFETCH(QString, sessionType);
    QFETCH(QString, waylandDisplay);
    QFETCH(QString, display);
    QFETCH(int, expected);

    QProcessEnvironment environment;
    environment.insert("XDG_SESSION_TYPE", sessionType);
    environment.insert("WAYLAND_DISPLAY", waylandDisplay);
    environment.insert("DISPLAY", display);
    QCOMPARE(int(Requirements::detectSession(environment)), expected);
  }

  void readsMbedtlsMajorVersion()
  {
    QCOMPARE(Requirements::parseMbedtlsMajor("#define MBEDTLS_VERSION_MAJOR  3\n#define MBEDTLS_VERSION_MINOR 6\n"), 3);
    QCOMPARE(Requirements::parseMbedtlsMajor("/* x */\n#  define MBEDTLS_VERSION_MAJOR 2\n"), 2);
    QCOMPARE(Requirements::parseMbedtlsMajor("#define SOMETHING_ELSE 9\n"), 0);
    QCOMPARE(Requirements::parseMbedtlsMajor({}), 0);
  }

  void cleanEnvironmentDropsForeignPrefixes()
  {
    qputenv("CMAKE_PREFIX_PATH", "/home/someone/miniconda");
    qputenv("PKG_CONFIG_PATH", "/home/someone/miniconda/lib/pkgconfig");
    const auto environment = Requirements::cleanEnvironment();
    QVERIFY(!environment.contains("CMAKE_PREFIX_PATH"));
    QVERIFY(!environment.contains("PKG_CONFIG_PATH"));
    QCOMPARE(environment.value("PATH"), Requirements::systemPath().join(':'));
    qunsetenv("CMAKE_PREFIX_PATH");
    qunsetenv("PKG_CONFIG_PATH");
  }

  void everythingPresentMeansNothingMissing()
  {
    const auto list = Requirements::check(SessionType::X11, probeWith(AllTools, X11Modules));
    QVERIFY(Requirements::allMet(list));
    QVERIFY(Requirements::installHint(PackageKind::Rpm, list).isEmpty());
  }

  void sessionDecidesWhichGrabberLibrariesAreNeeded()
  {
    const auto tools = AllTools;

    // A Wayland session does not need the X11 headers and the other way around
    QVERIFY(Requirements::allMet(Requirements::check(SessionType::Wayland, probeWith(tools, WaylandModules))));
    QVERIFY(Requirements::allMet(Requirements::check(SessionType::X11, probeWith(tools, X11Modules))));

    QCOMPARE(missingIds(Requirements::check(SessionType::Wayland, probeWith(tools, X11Modules))), QStringList{"wayland"});
    QCOMPARE(missingIds(Requirements::check(SessionType::X11, probeWith(tools, WaylandModules))), QStringList{"x11"});

    // Without a known session both are required
    QCOMPARE(missingIds(Requirements::check(SessionType::Unknown, probeWith(tools, X11Modules))), QStringList{"wayland"});
    QVERIFY(Requirements::allMet(Requirements::check(SessionType::Unknown, probeWith(tools, X11Modules + WaylandModules))));
  }

  void oldMbedtlsCountsAsMissing()
  {
    QCOMPARE(missingIds(Requirements::check(SessionType::X11, probeWith(AllTools, X11Modules, 2))), QStringList{"mbedtls"});
    QCOMPARE(missingIds(Requirements::check(SessionType::X11, probeWith(AllTools, X11Modules, 0))), QStringList{"mbedtls"});
  }

  void acceptsAlternativeToolNames()
  {
    const QStringList tools{"git", "cmake", "clang++", "ninja", "tar", "xz", "pkgconf"};
    QVERIFY(Requirements::allMet(Requirements::check(SessionType::X11, probeWith(tools, X11Modules))));
  }

  void hintListsOnlyMissingPackages_data()
  {
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("expected");

    QTest::newRow("fedora") << int(PackageKind::Rpm)
      << "sudo dnf install cmake libX11-devel libXext-devel libXrandr-devel";
    QTest::newRow("debian") << int(PackageKind::Deb)
      << "sudo apt install cmake libx11-dev libxext-dev libxrandr-dev";
    QTest::newRow("arch") << int(PackageKind::Pacman)
      << "sudo pacman -S --needed cmake libx11 libxext libxrandr";
    QTest::newRow("unknown system") << int(PackageKind::None) << "";
  }

  void hintListsOnlyMissingPackages()
  {
    QFETCH(int, kind);
    QFETCH(QString, expected);

    QStringList tools = AllTools;
    tools.removeAll("cmake");
    const auto list = Requirements::check(SessionType::X11, probeWith(tools, {"libcurl", "opencv4"}));
    QCOMPARE(Requirements::installHint(PackageKind(kind), list), expected);
  }

  void everyRequirementMapsToPackagesOnEveryFamily()
  {
    const auto all = Requirements::check(SessionType::Unknown, probeWith(AllTools, X11Modules + WaylandModules));

    for(auto kind : {PackageKind::Rpm, PackageKind::Deb, PackageKind::Pacman}){
      for(int i = 0; i < all.size(); i++){
        auto list = all;
        list[i].met = false;
        QVERIFY2(!Requirements::installHint(kind, list).isEmpty(), qPrintable(all[i].id));
      }
    }
  }

  void pinIsCompleteAndWellFormed()
  {
    const auto pin = PinnedDaemon::current();
    QVERIFY(pin.repoUrl.startsWith("https://"));
    QCOMPARE(pin.commit.size(), 40);
    QVERIFY(QRegularExpression("^[0-9a-f]{40}$").match(pin.commit).hasMatch());
    QCOMPARE(pin.version(), pin.tag.mid(1));
    QVERIFY(!pin.dependencies.isEmpty());

    for(const auto& dependency : pin.dependencies){
      QVERIFY2(dependency.url.startsWith("https://"), qPrintable(dependency.url));
      QVERIFY2(QRegularExpression("^[0-9a-f]{64}$").match(dependency.sha256).hasMatch(), qPrintable(dependency.fetchName));
      QVERIFY(!dependency.packageName.isEmpty());
    }
  }
};

QTEST_GUILESS_MAIN(TestRequirements)
#include "TestRequirements.moc"
