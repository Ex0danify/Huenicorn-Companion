# Composition root: the only place that knows every module
add_executable(huenicorn-qt main.cpp)
target_link_libraries(huenicorn-qt PRIVATE
  huenicorn_tray
  huenicorn_configurator
  huenicorn_settings
  huenicorn_daemon
  huenicorn_updater
)
target_compile_definitions(huenicorn-qt PRIVATE HUENICORN_QT_VERSION="${PROJECT_VERSION}")

# Keep the binary at the build root, where it has always been
set_target_properties(huenicorn-qt PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

install(TARGETS huenicorn-qt RUNTIME DESTINATION bin)
