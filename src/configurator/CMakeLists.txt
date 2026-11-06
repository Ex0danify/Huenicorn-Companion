add_library(huenicorn_configurator STATIC
  ConfiguratorModel.cpp
  ConfiguratorWindow.cpp
  qml.qrc
)
target_link_libraries(huenicorn_configurator PUBLIC Qt6::Quick huenicorn_api huenicorn_daemon huenicorn_setup huenicorn_updater huenicorn_installer huenicorn_i18n huenicorn_preferences)
