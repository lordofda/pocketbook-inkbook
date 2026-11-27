cmake_minimum_required(VERSION 3.21)
project(komga-host-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

enable_testing()

function(add_komga_host_test target)
  qt_add_executable(${target} ${ARGN})
  target_include_directories(${target} PRIVATE ../../src)
  target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Network Qt6::Test)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_komga_host_test(settings_tests
  test_settings.cpp
  ../../src/settings.h
  ../../src/settings.cpp
)

add_komga_host_test(book_store_tests
  test_book_store.cpp
  ../../src/settings.h
  ../../src/settings.cpp
  ../../src/book_store.h
  ../../src/book_store.cpp
)

add_komga_host_test(list_model_tests
  test_list_model.cpp
  ../../src/models.h
  ../../src/models.cpp
)
