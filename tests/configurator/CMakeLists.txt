add_executable(TestConfiguratorModel TestConfiguratorModel.cpp)
target_link_libraries(TestConfiguratorModel PRIVATE huenicorn_configurator test_support)
add_test(NAME ConfiguratorModel COMMAND TestConfiguratorModel)

# Loads the real QML against the fake daemon and fails on any QML error
add_executable(TestConfiguratorQml TestConfiguratorQml.cpp)
target_link_libraries(TestConfiguratorQml PRIVATE huenicorn_configurator test_support)
add_test(NAME ConfiguratorQml COMMAND TestConfiguratorQml)
set_tests_properties(ConfiguratorQml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

# Developer tool, not a test: renders pages to PNG for visual review
add_executable(RenderConfigurator RenderConfigurator.cpp)
target_link_libraries(RenderConfigurator PRIVATE huenicorn_configurator test_support)
