add_library(huenicorn_i18n STATIC Localization.cpp)
target_link_libraries(huenicorn_i18n PUBLIC Qt6::Core huenicorn_settings)

# One catalog per language, every .ts file in translations/ is picked up
file(GLOB TRANSLATION_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/translations/*.ts)

# The update target scans all sources once, regardless of which module they belong to
file(GLOB_RECURSE TRANSLATABLE_SOURCES CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.hpp ${PROJECT_SOURCE_DIR}/src/*.qml)

qt_add_translations(huenicorn_i18n
  TS_FILES ${TRANSLATION_FILES}
  SOURCES ${TRANSLATABLE_SOURCES}
  INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/src
  RESOURCE_PREFIX /i18n
  LUPDATE_OPTIONS -locations none -no-obsolete
)
