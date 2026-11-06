cmake_minimum_required(VERSION 3.16)

# Allowed #include directions between modules under src/.
# A module may include itself and the modules listed after its name.
set(ALLOWED_api "")
set(ALLOWED_settings "")
set(ALLOWED_updater "")
set(ALLOWED_installer "updater")
set(ALLOWED_daemon "api")
set(ALLOWED_setup "api;daemon")
set(ALLOWED_tray "daemon;settings")
set(ALLOWED_configurator "api;daemon;setup;updater;installer")
set(ALLOWED_app "api;daemon;setup;settings;updater;installer;tray;configurator")

set(MODULES api settings updater installer daemon setup tray configurator app)
set(violations "")

foreach(module ${MODULES})
  file(GLOB_RECURSE files "${SRC_DIR}/${module}/*.cpp" "${SRC_DIR}/${module}/*.hpp")
  foreach(file ${files})
    file(STRINGS "${file}" includes REGEX "^#include \"[a-z]+/")
    foreach(line ${includes})
      string(REGEX REPLACE "^#include \"([a-z]+)/.*" "\\1" target "${line}")
      if(NOT target STREQUAL module AND NOT target IN_LIST ALLOWED_${module})
        list(APPEND violations "${file}: ${module} must not depend on ${target}")
      endif()
    endforeach()
  endforeach()
endforeach()

if(violations)
  string(REPLACE ";" "\n" report "${violations}")
  message(FATAL_ERROR "Module boundary violations:\n${report}")
endif()
