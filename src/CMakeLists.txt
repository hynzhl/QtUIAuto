cmake_minimum_required(VERSION 3.18)
project(QtUIAuto_Src LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Quick Widgets)

# Inject DLL subdirectory
add_subdirectory(inject)

# Main application
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ProcessManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/AccessibilityController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/ControlTree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/ScriptEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/PipeServer.cpp
)

set(QML_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/qml.qrc
)

add_executable(QtUIAuto ${SOURCES} ${QML_RESOURCES})

target_link_libraries(QtUIAuto PRIVATE
    Qt5::Core Qt5::Quick Qt5::Widgets
)

target_include_directories(QtUIAuto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Copy inject DLL to output directory post-build
add_custom_command(TARGET QtUIAuto POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:QtUIAuto_Inject>/$<TARGET_FILE_NAME:QtUIAuto_Inject>"
        "$<TARGET_FILE_DIR:QtUIAuto>/"
    COMMENT "Copying QtUIAuto_Inject.dll to output directory"
)
