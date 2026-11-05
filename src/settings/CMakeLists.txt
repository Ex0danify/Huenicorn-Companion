add_library(huenicorn_settings STATIC Settings.cpp Autostart.cpp)
target_link_libraries(huenicorn_settings PUBLIC Qt6::Core)
