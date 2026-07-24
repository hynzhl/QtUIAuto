cmake_minimum_required(VERSION 3.18)
project(QtUIAuto_Inject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

set(SOURCES dllmain.cpp)
add_library(QtUIAuto_Inject SHARED ${SOURCES})
target_link_libraries(QtUIAuto_Inject PRIVATE Qt5::Core Qt5::Widgets)
set_target_properties(QtUIAuto_Inject PROPERTIES AUTOMOC OFF)

target_include_directories(QtUIAuto_Inject PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
