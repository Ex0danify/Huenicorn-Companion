#pragma once

#include <QObject>
#include <QQmlApplicationEngine>

#include "configurator/ConfiguratorModel.hpp"
#include "i18n/Localization.hpp"
#include "installer/DaemonInstaller.hpp"
#include "updater/UpdateController.hpp"

class QQuickWindow;

// Owns the QML engine and loads it on first use
class ConfiguratorWindow : public QObject
{
  Q_OBJECT

public:
  ConfiguratorWindow(DaemonController& controller, UpdateController& updater, DaemonInstaller& installer, Localization& localization, QObject* parent = nullptr);

  ConfiguratorModel& model();
  QQuickWindow* window();

public slots:
  void show();

private:
  bool ensureLoaded();

  ConfiguratorModel m_model;
  QQmlApplicationEngine m_engine;
  QQuickWindow* m_window{};
};
