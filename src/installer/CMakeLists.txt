add_library(huenicorn_installer STATIC
  PinnedDaemon.cpp
  Requirements.cpp
  CommandRunner.cpp
  DaemonInstaller.cpp
)
target_link_libraries(huenicorn_installer PUBLIC Qt6::Core Qt6::Network huenicorn_updater)
