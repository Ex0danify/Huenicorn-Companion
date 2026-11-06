add_executable(TestLocalization TestLocalization.cpp)
target_link_libraries(TestLocalization PRIVATE huenicorn_i18n Qt6::Test)
target_compile_definitions(TestLocalization PRIVATE TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/translations")
add_test(NAME Localization COMMAND TestLocalization)
