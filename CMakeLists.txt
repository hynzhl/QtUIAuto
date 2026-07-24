cmake_minimum_required(VERSION 3.18)

# ============================================================
# QtUIAuto - QML UI Automation Testing Tool
# ============================================================
project(QtUIAuto
    VERSION 0.1.0
    DESCRIPTION "A lightweight, non-invasive UI automation testing tool for Qt/QML applications"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Quick Widgets)

add_subdirectory(src)

option(BUILD_SPIKE "Build the QAccessibility verification spike" OFF)
if(BUILD_SPIKE)
    add_subdirectory(spike)
endif()

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
