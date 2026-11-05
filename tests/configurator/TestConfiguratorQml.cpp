#include <QtTest>

#include <QQuickWindow>

#include "FakeDaemonApi.hpp"
#include "configurator/ConfiguratorWindow.hpp"

// Catches QML syntax, import and binding errors that only show up at load time
class TestConfiguratorQml : public QObject
{
  Q_OBJECT

private slots:
  void loadsAndRendersWithoutWarnings()
  {
    QStringList warnings;
    static QStringList* sink;
    sink = &warnings;
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message){
      if(type == QtWarningMsg || type == QtCriticalMsg){
        sink->append(message);
      }
    });

    {
      FakeDaemonApi api;
      DaemonController controller;
      controller.setPort(api.port());
      controller.setPollIntervalMs(100);
      ConfiguratorWindow configurator(controller);

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

    qInstallMessageHandler(nullptr);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join("\n")));
  }
};

QTEST_MAIN(TestConfiguratorQml)
#include "TestConfiguratorQml.moc"
