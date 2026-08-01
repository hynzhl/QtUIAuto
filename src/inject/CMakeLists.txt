cmake_minimum_required(VERSION 3.18)
project(QU_Inject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Quick)

set(SOURCES
    dllmain.cpp
    ControlScanner.cpp
    InputSimulator.cpp
    CommandHandler.cpp
)

add_library(QU_Inject SHARED ${SOURCES})

if (MSVC)
    target_compile_options(QU_Inject PRIVATE /utf-8)
endif()

target_link_libraries(QU_Inject PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Quick
)

target_include_directories(QU_Inject PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
