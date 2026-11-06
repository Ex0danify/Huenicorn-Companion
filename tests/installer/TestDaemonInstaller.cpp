#include <QtTest>

#include "FakeCommandRunner.hpp"
#include "installer/DaemonInstaller.hpp"

namespace
{
  const QString Commit = "0123456789abcdef0123456789abcdef01234567";

  HostProbe allPresent()
  {
    HostProbe probe;
    probe.hasTool = [](const QString&){ return true; };
    probe.hasModules = [](const QStringList&){ return true; };
    probe.mbedtlsMajor = [](){ return 3; };
    return probe;
  }


  QByteArray hashOf(const QByteArray& body)
  {
    return QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
  }
}


class TestDaemonInstaller : public QObject
{
  Q_OBJECT

private:
  std::unique_ptr<QTemporaryDir> m_rootDir;
  QByteArray m_alpha{"alpha archive"};
  QByteArray m_beta{"beta archive"};
  FakeRunner* m_runner{};
  std::unique_ptr<DaemonInstaller> m_installer;
  QMap<QString, QByteArray> m_downloads;
  QStringList m_requested;

  QString binDir() const { return m_rootDir->path() + "/bin"; }
  QString workRoot() const { return m_rootDir->path() + "/work"; }

  DaemonPin pin() const
  {
    DaemonPin pin;
    pin.repoUrl = "https://example.invalid/daemon.git";
    pin.tag = "v9.9.9";
    pin.commit = Commit;
    pin.dependencies = {
      {"alpha_lib", "alpha", "https://example.invalid/alpha.tar.gz", hashOf(m_alpha)},
      {"beta_lib", "beta", "https://example.invalid/beta.tar.xz", hashOf(m_beta)},
    };
    pin.compilerFlags = {"-include", "array"};
    return pin;
  }

  void setUpInstaller(const DaemonPin& usedPin)
  {
    m_installer = std::make_unique<DaemonInstaller>(usedPin);
    auto runner = std::make_unique<FakeRunner>();
    m_runner = runner.get();

    // A healthy run: the source has the pinned commit and the build produces a binary
    m_runner->handler = [this](const Command& command, QString& output){
      if(command.arguments.contains("rev-parse")){
        output = "some warning\n" + Commit + "\n";
      }
      if(command.program == "cmake" && command.arguments.value(0) == "--build"){
        QDir().mkpath(command.arguments.value(1));
        QFile binary(command.arguments.value(1) + "/huenicorn");
        if(binary.open(QIODevice::WriteOnly)){
          binary.write("new daemon");
        }
      }
      return 0;
    };

    m_installer->setRunner(std::move(runner));
    m_installer->setProbe(allPresent());
    m_installer->setSession(SessionType::X11);
    m_installer->setSystem({PackageKind::Rpm, {"fedora"}});
    m_installer->setInstallDir(binDir());
    m_installer->setWorkRoot(workRoot());

    m_requested.clear();
    m_downloads = {{"https://example.invalid/alpha.tar.gz", m_alpha}, {"https://example.invalid/beta.tar.xz", m_beta}};
    m_installer->setFetcher([this](const QUrl& url, DaemonInstaller::FetchDone done){
      m_requested.append(url.toString());
      const auto body = m_downloads.value(url.toString());
      const QString error = m_downloads.contains(url.toString()) ? QString() : QString("not found");
      QTimer::singleShot(0, [done, body, error](){ done(body, error); });
    });
  }

  QString leftoversInWorkRoot() const
  {
    return QDir(workRoot()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).join(',');
  }

private slots:
  void init()
  {
    m_rootDir = std::make_unique<QTemporaryDir>();
    setUpInstaller(pin());
  }

  void reportsMissingRequirementsAndRefusesToBuild()
  {
    auto probe = allPresent();
    probe.hasTool = [](const QString& tool){ return tool != "cmake"; };
    m_installer->setProbe(probe);

    m_installer->refresh();
    QCOMPARE(m_installer->phase(), QString("missing"));
    QVERIFY(m_installer->installHint().contains("cmake"));

    m_installer->build();
    QCOMPARE(m_installer->phase(), QString("missing"));
    QVERIFY(m_runner->commands.isEmpty());
    QVERIFY(m_requested.isEmpty());
  }

  void becomesReadyOnceRequirementsAreMet()
  {
    auto probe = allPresent();
    probe.mbedtlsMajor = [](){ return 2; };
    m_installer->setProbe(probe);
    m_installer->refresh();
    QCOMPARE(m_installer->phase(), QString("missing"));

    m_installer->setProbe(allPresent());
    m_installer->refresh();
    QCOMPARE(m_installer->phase(), QString("ready"));
  }

  void buildsAndInstallsInOrder()
  {
    QSignalSpy installed(m_installer.get(), &DaemonInstaller::installed);
    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("installed"), 3000);

    QCOMPARE(installed.count(), 1);
    QCOMPARE(installed.first().first().toString(), binDir() + "/huenicorn");

    QStringList sequence;
    for(const auto& command : m_runner->commands){
      sequence.append(command.program + " " + command.arguments.value(0));
    }
    // The checkout is verified before anything downloaded gets unpacked or built
    QCOMPARE(sequence, (QStringList{"git clone", "git -C", "tar -xf", "tar -xf", "cmake -S", "cmake --build"}));
    QCOMPARE(m_requested, (QStringList{"https://example.invalid/alpha.tar.gz", "https://example.invalid/beta.tar.xz"}));

    QFile binary(binDir() + "/huenicorn");
    QVERIFY(binary.open(QIODevice::ReadOnly));
    QCOMPARE(binary.readAll(), QByteArray("new daemon"));
    QVERIFY(QFileInfo(binary).isExecutable());
    QCOMPARE(m_installer->progress(), 1.0);
    QVERIFY(leftoversInWorkRoot().isEmpty());
  }

  void clonesOnlyThePinnedTag()
  {
    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("installed"), 3000);

    const auto clone = describe(m_runner->commands.first());
    QVERIFY(clone.contains("--depth 1"));
    QVERIFY(clone.contains("--branch v9.9.9"));
    QVERIFY(clone.contains("https://example.invalid/daemon.git"));
  }

  void configuresOnlyWithVerifiedDependencies()
  {
    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("installed"), 3000);

    QString configure;
    for(const auto& command : m_runner->commands){
      if(command.program == "cmake" && command.arguments.value(0) == "-S"){
        configure = describe(command);
      }
    }
    QVERIFY(!configure.isEmpty());
    QVERIFY(configure.contains("-DCMAKE_BUILD_TYPE=Release"));
    QVERIFY(configure.contains("-DFETCHCONTENT_FULLY_DISCONNECTED=ON"));
    QVERIFY(configure.contains("-DCMAKE_DISABLE_FIND_PACKAGE_alpha=ON"));
    QVERIFY(configure.contains("-DCMAKE_DISABLE_FIND_PACKAGE_beta=ON"));
    QVERIFY(configure.contains("-DFETCHCONTENT_SOURCE_DIR_ALPHA_LIB="));
    QVERIFY(configure.contains("-DFETCHCONTENT_SOURCE_DIR_BETA_LIB="));
    QVERIFY(configure.contains("-DCMAKE_CXX_FLAGS=-include array"));
  }

  void abortsWhenTheCheckoutIsNotThePinnedCommit()
  {
    m_runner->handler = [](const Command& command, QString& output){
      if(command.arguments.contains("rev-parse")){
        output = "ffffffffffffffffffffffffffffffffffffffff\n";
      }
      return 0;
    };

    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("failed"), 3000);

    QVERIFY(m_installer->errorText().contains("expected commit"));
    QVERIFY(m_requested.isEmpty());
    QVERIFY(!m_runner->ran("tar"));
    QVERIFY(!m_runner->ran("cmake"));
    QVERIFY(!QFileInfo::exists(binDir() + "/huenicorn"));
    QVERIFY(leftoversInWorkRoot().isEmpty());
  }

  void abortsWhenADownloadDoesNotMatchItsChecksum()
  {
    m_downloads["https://example.invalid/beta.tar.xz"] = "tampered";

    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("failed"), 3000);

    QVERIFY(m_installer->errorText().contains("beta_lib"));
    QVERIFY(m_installer->errorText().contains("checksum"));
    // The tampered archive is never unpacked and nothing gets configured
    int unpacked = 0;
    for(const auto& command : m_runner->commands){
      unpacked += command.program == "tar";
    }
    QCOMPARE(unpacked, 1);
    QVERIFY(!m_runner->ran("cmake"));
    QVERIFY(!QFileInfo::exists(binDir() + "/huenicorn"));
  }

  void abortsWhenADownloadFails()
  {
    m_downloads.remove("https://example.invalid/alpha.tar.gz");

    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("failed"), 3000);
    QVERIFY(m_installer->errorText().contains("alpha_lib"));
    QVERIFY(!m_runner->ran("cmake"));
  }

  void refusesInsecureDependencyUrls()
  {
    auto insecure = pin();
    insecure.dependencies[0].url = "http://example.invalid/alpha.tar.gz";
    setUpInstaller(insecure);

    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("failed"), 3000);
    QVERIFY(m_requested.isEmpty());
  }

  void failedCompileKeepsExistingInstall()
  {
    QDir().mkpath(binDir());
    QFile old(binDir() + "/huenicorn");
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("old daemon");
    old.close();

    auto healthy = m_runner->handler;
    m_runner->handler = [healthy](const Command& command, QString& output){
      if(command.program == "cmake" && command.arguments.value(0) == "--build"){
        output = "line one\nerror: something broke\n";
        return 2;
      }
      return healthy(command, output);
    };

    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("failed"), 3000);

    // The reason shows up for the user
    QVERIFY(m_installer->errorText().contains("something broke"));
    QVERIFY(old.open(QIODevice::ReadOnly));
    QCOMPARE(old.readAll(), QByteArray("old daemon"));
    QVERIFY(leftoversInWorkRoot().isEmpty());
  }

  void canBuildAgainAfterAFailure()
  {
    bool fail = true;
    auto healthy = m_runner->handler;
    m_runner->handler = [&fail, healthy](const Command& command, QString& output){
      if(fail && command.program == "git" && command.arguments.value(0) == "clone"){
        return 128;
      }
      return healthy(command, output);
    };

    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("failed"), 3000);

    fail = false;
    m_installer->build();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("installed"), 3000);
    QVERIFY(m_installer->errorText().isEmpty());
  }

  void cancelStopsTheRunningCommandAndInstallsNothing()
  {
    m_runner->hang = true;
    m_installer->build();
    QCOMPARE(m_installer->phase(), QString("building"));
    QVERIFY(m_installer->busy());

    m_installer->cancel();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("ready"), 3000);

    QVERIFY(m_runner->cancelled);
    QVERIFY(m_installer->errorText().isEmpty());
    QVERIFY(!QFileInfo::exists(binDir() + "/huenicorn"));
    QVERIFY(leftoversInWorkRoot().isEmpty());
    QCOMPARE(m_installer->progress(), 0.0);
  }

  void cancelDuringDownloadIgnoresTheLateAnswer()
  {
    DaemonInstaller::FetchDone pending;
    m_installer->setFetcher([&pending](const QUrl&, DaemonInstaller::FetchDone done){ pending = std::move(done); });

    m_installer->build();
    QTRY_VERIFY_WITH_TIMEOUT(static_cast<bool>(pending), 3000);

    m_installer->cancel();
    QCOMPARE(m_installer->phase(), QString("ready"));

    pending(m_alpha, {});
    QTest::qWait(50);
    QCOMPARE(m_installer->phase(), QString("ready"));
    QVERIFY(!m_runner->ran("cmake"));
    QVERIFY(!QFileInfo::exists(binDir() + "/huenicorn"));
  }

  void secondBuildWhileBusyIsIgnored()
  {
    m_runner->hang = true;
    m_installer->build();
    const auto count = m_runner->commands.size();
    m_installer->build();
    QCOMPARE(m_runner->commands.size(), count);
    m_installer->cancel();
    QTRY_COMPARE_WITH_TIMEOUT(m_installer->phase(), QString("ready"), 3000);
  }
};

QTEST_GUILESS_MAIN(TestDaemonInstaller)
#include "TestDaemonInstaller.moc"
