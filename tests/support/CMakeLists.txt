# Header-only test double shared by the module tests
add_library(test_support INTERFACE)
target_include_directories(test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_support INTERFACE Qt6::Network Qt6::Test)
