cmake_minimum_required(VERSION 3.18)
project(QU_Src LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Quick Widgets)

# Inject DLL subdirectory
add_subdirectory(inject)

# Bridge plugin (non-invasive loading via QT_IM_MODULE)
add_subdirectory(bridge)

# Main application
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/AppContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ProcessManager.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/engine/ControlTree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/ScriptEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/PipeServer.cpp
)

set(QML_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/qml.qrc
)

add_executable(QU ${SOURCES} ${QML_RESOURCES})

if (MSVC)
    target_compile_options(QU PRIVATE /utf-8)
endif()

target_link_libraries(QU PRIVATE
    Qt5::Core Qt5::Quick Qt5::Widgets
)

target_include_directories(QU PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Copy inject DLL to output directory post-build
add_custom_command(TARGET QU POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:QU_Inject>/$<TARGET_FILE_NAME:QU_Inject>"
        "$<TARGET_FILE_DIR:QU>/"
    COMMENT "Copying QU_Inject.dll to output directory"
)
