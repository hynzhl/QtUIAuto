cmake_minimum_required(VERSION 3.18)
project(QtUIAuto_TestTarget LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Quick)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

set(QML_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml.qrc
)

add_executable(QtUIAuto_TestTarget ${SOURCES} ${QML_RESOURCES})

target_link_libraries(QtUIAuto_TestTarget PRIVATE
    Qt5::Core Qt5::Quick
)
