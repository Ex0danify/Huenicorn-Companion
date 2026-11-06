# Composition root: the only place that knows every module
add_executable(huenicorn-companion main.cpp)
target_link_libraries(huenicorn-companion PRIVATE
  huenicorn_tray
  huenicorn_configurator
  huenicorn_settings
  huenicorn_daemon
  huenicorn_updater
  huenicorn_installer
  huenicorn_i18n
  huenicorn_preferences
)
target_compile_definitions(huenicorn-companion PRIVATE HUENICORN_COMPANION_VERSION="${PROJECT_VERSION}")

# Keep the binary at the build root, where it has always been
set_target_properties(huenicorn-companion PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

install(TARGETS huenicorn-companion RUNTIME DESTINATION bin)
