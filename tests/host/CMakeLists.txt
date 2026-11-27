cmake_minimum_required(VERSION 3.21)
project(komga-host-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

enable_testing()

qt_add_executable(settings_tests
  test_settings.cpp
  ../../src/settings.h
  ../../src/settings.cpp
  ../../src/book_store.h
  ../../src/book_store.cpp
)

target_include_directories(settings_tests PRIVATE ../../src)
target_link_libraries(settings_tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)

add_test(NAME settings_tests COMMAND settings_tests)
