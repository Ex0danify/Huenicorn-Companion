#pragma once

#include <QObject>
#include <QStringList>
#include <QTranslator>
#include <QVariantList>

#include "settings/Settings.hpp"

// Loads the embedded catalogs and switches them at runtime
class Localization : public QObject
{
  Q_OBJECT

  // "auto" or a language code, as stored in the settings
  Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
  Q_PROPERTY(QVariantList languages READ languages CONSTANT)

public:
  explicit Localization(Settings& settings, QObject* parent = nullptr);

  QString language() const;
  void setLanguage(const QString& language);

  // [{code, name, flag}] with every language in its own name
  QVariantList languages() const;

  // Language that is actually in use
  QString active() const;

  // Codes of all embedded catalogs, including the English source language
  static QStringList available();

  // Explicit choice if it exists, otherwise the best match for the system languages, otherwise English
  static QString resolve(const QString& setting, const QStringList& systemLanguages, const QStringList& available);

  static constexpr const char* SourceLanguage = "en";

signals:
  void languageChanged();

  // Emitted after the catalog was swapped, so widgets can rebuild their texts
  void changed();

private:
  void apply();

  Settings& m_settings;
  QTranslator m_translator;
  QString m_active{SourceLanguage};
};
