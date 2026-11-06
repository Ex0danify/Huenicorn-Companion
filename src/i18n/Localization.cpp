#include "i18n/Localization.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QLocale>

namespace
{
  const QString Prefix = ":/i18n/";
  const QString Basename = "huenicorn-companion_";

  QString normalized(const QString& code)
  {
    return QString(code).replace('-', '_').toLower();
  }

  // Flag emoji of the territory Qt assumes for the language, e.g. "de" is Germany
  QString flagFor(const QString& code)
  {
    const auto territory = QLocale(code).name().section('_', 1, 1);
    if(territory.size() != 2){
      return {};
    }

    const auto upper = territory.toUpper();
    const char32_t points[2] = {
      static_cast<char32_t>(0x1F1E6 + (upper[0].unicode() - u'A')),
      static_cast<char32_t>(0x1F1E6 + (upper[1].unicode() - u'A'))
    };
    return QString::fromUcs4(points, 2);
  }
}


Localization::Localization(Settings& settings, QObject* parent)
  : QObject(parent)
  , m_settings(settings)
{
  // Resources in a static library are dropped by the linker unless initialized by hand
  Q_INIT_RESOURCE(huenicorn_i18n_translations);
  apply();
}


QString Localization::language() const
{
  return m_settings.language();
}


void Localization::setLanguage(const QString& language)
{
  if(language == m_settings.language()){
    return;
  }

  m_settings.setLanguage(language);
  apply();
  emit languageChanged();
}


QVariantList Localization::languages() const
{
  QVariantList result;
  for(const auto& code : available()){
    // Qt would say "American English" here
    auto name = code == SourceLanguage ? QString("English") : QLocale(code).nativeLanguageName();
    if(!name.isEmpty()){
      name[0] = name[0].toUpper();
    }
    result.append(QVariantMap{{"code", code}, {"name", name}, {"flag", flagFor(code)}});
  }
  return result;
}


QString Localization::active() const
{
  return m_active;
}


QStringList Localization::available()
{
  QStringList codes;
  const auto files = QDir(Prefix).entryList({Basename + "*.qm"}, QDir::Files);
  for(const auto& file : files){
    codes.append(file.mid(Basename.size(), file.size() - Basename.size() - 3));
  }
  codes.sort();
  return codes;
}


QString Localization::resolve(const QString& setting, const QStringList& systemLanguages, const QStringList& available)
{
  auto find = [&](const QString& code) -> QString {
    for(const auto& candidate : available){
      if(normalized(candidate) == normalized(code)){
        return candidate;
      }
    }
    return {};
  };

  if(setting != "auto"){
    if(const auto match = find(setting); !match.isEmpty()){
      return match;
    }
  }

  for(const auto& language : systemLanguages){
    if(const auto match = find(language); !match.isEmpty()){
      return match;
    }
    if(const auto match = find(language.section('-', 0, 0)); !match.isEmpty()){
      return match;
    }
  }
  return SourceLanguage;
}


void Localization::apply()
{
  QCoreApplication::removeTranslator(&m_translator);

  m_active = resolve(m_settings.language(), QLocale::system().uiLanguages(), available());
  if(m_active != SourceLanguage && m_translator.load(Prefix + Basename + m_active + ".qm")){
    QCoreApplication::installTranslator(&m_translator);
  }
  else{
    m_active = SourceLanguage;
  }

  emit changed();
}
