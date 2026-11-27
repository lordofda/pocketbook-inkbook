cmake_minimum_required(VERSION 3.21)
project(komga-host-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

qt_add_executable(komga-host-tests
  test_settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/book_store.cpp
)

target_include_directories(komga-host-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(komga-host-tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)

add_test(NAME komga-host-tests COMMAND komga-host-tests)
