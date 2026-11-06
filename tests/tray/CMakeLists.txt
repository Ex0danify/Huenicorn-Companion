add_executable(TestIconFactory TestIconFactory.cpp)
target_link_libraries(TestIconFactory PRIVATE huenicorn_tray Qt6::Test)
add_test(NAME IconFactory COMMAND TestIconFactory)
set_tests_properties(IconFactory PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TestTrayMenu TestTrayMenu.cpp)
target_link_libraries(TestTrayMenu PRIVATE huenicorn_tray test_support Qt6::Test)
add_test(NAME TrayMenu COMMAND TestTrayMenu)
set_tests_properties(TrayMenu PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
