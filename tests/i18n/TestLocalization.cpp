#include <QtTest>

#include <QXmlStreamReader>

#include "i18n/Localization.hpp"

namespace
{
  QStringList placeholders(const QString& text)
  {
    static const QRegularExpression pattern("%(?:L?\\d+|n)");
    QStringList found;
    for(auto it = pattern.globalMatch(text); it.hasNext();){
      found.append(it.next().captured());
    }
    found.removeDuplicates();
    found.sort();
    return found;
  }

  struct Entry
  {
    QString context;
    QString source;
    QString translation;
    bool finished;
  };

  QList<Entry> readCatalog(const QString& path)
  {
    QFile file(path);
    QList<Entry> entries;
    if(!file.open(QIODevice::ReadOnly)){
      return entries;
    }

    QXmlStreamReader xml(&file);
    QString context;
    Entry entry;
    while(!xml.atEnd()){
      xml.readNext();
      if(!xml.isStartElement()){
        continue;
      }
      if(xml.name() == u"name"){
        context = xml.readElementText();
      }
      else if(xml.name() == u"message"){
        entry = {context, {}, {}, true};
      }
      else if(xml.name() == u"source"){
        entry.source = xml.readElementText();
      }
      else if(xml.name() == u"translation"){
        entry.finished = xml.attributes().value("type").isEmpty();
        entry.translation = xml.readElementText();
        entries.append(entry);
      }
    }
    return entries;
  }
}


class TestLocalization : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    // Settings would otherwise write into the real user config
    QVERIFY(m_config.isValid());
    qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
  }

  void shipsEnglishAndGerman()
  {
    QVERIFY(Localization::available().contains("en"));
    QVERIFY(Localization::available().contains("de"));
  }

  void resolvePicksTheBestMatch_data()
  {
    QTest::addColumn<QString>("setting");
    QTest::addColumn<QStringList>("system");
    QTest::addColumn<QString>("expected");

    const QStringList available{"de", "en", "pt_BR"};
    Q_UNUSED(available)

    QTest::newRow("explicit choice wins") << "de" << QStringList{"en-US"} << "de";
    QTest::newRow("auto uses the system language") << "auto" << QStringList{"de-DE"} << "de";
    QTest::newRow("regional variant falls back to the language") << "auto" << QStringList{"de-AT", "en"} << "de";
    QTest::newRow("exact regional match") << "auto" << QStringList{"pt-BR"} << "pt_BR";
    QTest::newRow("first supported system language") << "auto" << QStringList{"fr-FR", "de-DE"} << "de";
    QTest::newRow("unsupported falls back to English") << "auto" << QStringList{"fr-FR"} << "en";
    QTest::newRow("unknown choice follows the system") << "xx" << QStringList{"de"} << "de";
    QTest::newRow("nothing known") << "auto" << QStringList{} << "en";
  }

  void resolvePicksTheBestMatch()
  {
    QFETCH(QString, setting);
    QFETCH(QStringList, system);
    QFETCH(QString, expected);

    QCOMPARE(Localization::resolve(setting, system, {"de", "en", "pt_BR"}), expected);
  }

  void catalogsKeepPlaceholders()
  {
    const auto files = QDir(TRANSLATIONS_DIR).entryList({"*.ts"}, QDir::Files);
    QVERIFY(!files.isEmpty());

    for(const auto& name : files){
      const auto entries = readCatalog(QDir(TRANSLATIONS_DIR).filePath(name));
      QVERIFY2(!entries.isEmpty(), qPrintable(name));

      for(const auto& entry : entries){
        if(entry.translation.isEmpty()){
          continue;
        }
        QVERIFY2(placeholders(entry.source) == placeholders(entry.translation),
                 qPrintable(QString("%1: %2: \"%3\" vs \"%4\"").arg(name, entry.context, entry.source, entry.translation)));
      }
    }
  }

  void germanIsComplete()
  {
    const auto entries = readCatalog(QDir(TRANSLATIONS_DIR).filePath("huenicorn-qt_de.ts"));
    QVERIFY(!entries.isEmpty());

    for(const auto& entry : entries){
      QVERIFY2(entry.finished && !entry.translation.isEmpty(),
               qPrintable(QString("%1: \"%2\"").arg(entry.context, entry.source)));
    }
  }

  void switchesLanguageAtRuntime()
  {
    Settings settings;
    Localization localization(settings);
    QSignalSpy changed(&localization, &Localization::changed);
    QSignalSpy languageChanged(&localization, &Localization::languageChanged);

    localization.setLanguage("de");
    QCOMPARE(localization.active(), QString("de"));
    QCOMPARE(QCoreApplication::translate("TrayApp", "Quit"), QString("Beenden"));
    QCOMPARE(settings.language(), QString("de"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(languageChanged.count(), 1);

    localization.setLanguage("en");
    QCOMPARE(localization.active(), QString("en"));
    QCOMPARE(QCoreApplication::translate("TrayApp", "Quit"), QString("Quit"));
    QCOMPARE(changed.count(), 2);
  }

  void ignoresRepeatedLanguage()
  {
    Settings settings;
    Localization localization(settings);
    localization.setLanguage("de");

    QSignalSpy changed(&localization, &Localization::changed);
    localization.setLanguage("de");
    QCOMPARE(changed.count(), 0);
  }

  void unknownLanguageKeepsEnglish()
  {
    Settings settings;
    settings.setLanguage("xx");
    Localization localization(settings);

    QCOMPARE(QCoreApplication::translate("TrayApp", "Quit"), localization.active() == "de" ? QString("Beenden") : QString("Quit"));
    QVERIFY(Localization::available().contains(localization.active()));
  }

  void listsLanguagesInTheirOwnName()
  {
    Settings settings;
    Localization localization(settings);

    QStringList names;
    for(const auto& language : localization.languages()){
      names.append(language.toMap().value("name").toString());
    }
    QVERIFY(names.contains("Deutsch"));
    QVERIFY(names.contains("English"));
  }

  void derivesFlagsFromTheLanguage()
  {
    Settings settings;
    Localization localization(settings);

    QMap<QString, QString> flags;
    for(const auto& language : localization.languages()){
      const auto entry = language.toMap();
      flags[entry.value("code").toString()] = entry.value("flag").toString();
    }

    // Regional indicator letters, which render as the flag where an emoji font exists
    QCOMPARE(flags.value("de"), QString::fromUcs4(U"\U0001F1E9\U0001F1EA"));
    QCOMPARE(flags.value("en"), QString::fromUcs4(U"\U0001F1FA\U0001F1F8"));
  }

private:
  QTemporaryDir m_config;
};

QTEST_MAIN(TestLocalization)
#include "TestLocalization.moc"
