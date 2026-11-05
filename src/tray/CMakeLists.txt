add_library(huenicorn_tray STATIC TrayApp.cpp IconFactory.cpp LogWindow.cpp)
target_link_libraries(huenicorn_tray PUBLIC Qt6::Widgets huenicorn_daemon huenicorn_settings)
