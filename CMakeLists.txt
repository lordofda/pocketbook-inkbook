cmake_minimum_required(VERSION 3.21)
project(komga-reader LANGUAGES CXX)

if(NOT POCKETBOOK_TOOLCHAIN_LOADED)
  message(FATAL_ERROR "Pass -DCMAKE_TOOLCHAIN_FILE=<sdk>/cmake/pocketbook.toolchain.cmake")
endif()

set(POCKETBOOK_SDK_CMAKE "/opt/pocketbook-build/pocketbook-sdk-qt6/cmake" CACHE PATH "PocketBook SDK CMake modules")
list(APPEND CMAKE_MODULE_PATH "${POCKETBOOK_SDK_CMAKE}")

include(PocketBookQt)
include(PocketBookSDK)
include(PocketBookApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

pocketbook_add_app(komga-reader
  SOURCES
    src/main.cpp
    src/device.h
    src/device.cpp
    src/settings.h
    src/settings.cpp
    src/models.h
    src/models.cpp
    src/komga_api.h
    src/komga_api.cpp
    src/book_store.h
    src/book_store.cpp
    src/controller.h
    src/controller.cpp
  QRC qml/komga.qrc
  LIBRARIES Qt6::Network
)

install(TARGETS komga-reader RUNTIME DESTINATION "${CMAKE_BINARY_DIR}/dist")
