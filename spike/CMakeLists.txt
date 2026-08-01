cmake_minimum_required(VERSION 3.18)
project(QU_Spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5 REQUIRED COMPONENTS
    Core Quick Widgets
)

# Add private include path for Qt5::Quick (needed for QQuickAccessibleAttached)
get_target_property(_qt5quick_pub_include Qt5::Quick INTERFACE_INCLUDE_DIRECTORIES)
set(Qt5Quick_PRIVATE_INCLUDE "${_qt5quick_pub_include}/5.15.2")

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

set(QML_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml.qrc
)

add_executable(QU_Spike ${SOURCES} ${QML_RESOURCES})

target_include_directories(QU_Spike PRIVATE ${Qt5Quick_PRIVATE_INCLUDE})

target_link_libraries(QU_Spike PRIVATE
    Qt5::Core Qt5::Quick Qt5::Widgets
)
