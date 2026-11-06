add_subdirectory(support)
add_subdirectory(api)
add_subdirectory(i18n)
add_subdirectory(preferences)
add_subdirectory(daemon)
add_subdirectory(setup)
add_subdirectory(updater)
add_subdirectory(installer)
add_subdirectory(configurator)
add_subdirectory(tray)

# Keeps module boundaries from eroding
add_test(NAME ArchitectureBoundaries
  COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${PROJECT_SOURCE_DIR}/src -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckBoundaries.cmake)
