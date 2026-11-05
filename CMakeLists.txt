cmake_minimum_required(VERSION 3.16)

project(HuenicornQt VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_TESTS "Build tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Quick)

# Every module includes as "<module>/Header.hpp"
include_directories(${PROJECT_SOURCE_DIR}/src)
add_compile_options(-Wall -Wextra -pedantic)

add_subdirectory(src/api)
add_subdirectory(src/daemon)
add_subdirectory(src/settings)
add_subdirectory(src/tray)
add_subdirectory(src/configurator)
add_subdirectory(src/app)

install(FILES resources/huenicorn-qt.desktop DESTINATION share/applications)
install(FILES resources/logo.svg DESTINATION share/icons/hicolor/scalable/apps RENAME huenicorn-qt.svg)

if(BUILD_TESTS)
  enable_testing()
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_subdirectory(tests)
endif()
