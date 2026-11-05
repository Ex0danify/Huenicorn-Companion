add_executable(TestIconFactory TestIconFactory.cpp)
target_link_libraries(TestIconFactory PRIVATE huenicorn_tray Qt6::Test)
add_test(NAME IconFactory COMMAND TestIconFactory)
set_tests_properties(IconFactory PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
