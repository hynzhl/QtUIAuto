cmake_minimum_required(VERSION 3.18)
project(QU_Bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Gui Quick)

# Qt private headers for QPlatformInputContextPlugin
# Required for <qpa/qplatforminputcontext.h> and <QtGui/private/...>
set(Qt5_PRIVATE_INCLUDES
    "D:/Qt/5.15.2/msvc2019_64/include/QtCore/5.15.2"
    "D:/Qt/5.15.2/msvc2019_64/include/QtCore/5.15.2/QtCore"
    "D:/Qt/5.15.2/msvc2019_64/include/QtGui/5.15.2"
    "D:/Qt/5.15.2/msvc2019_64/include/QtGui/5.15.2/QtGui"
)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/QUBridgePlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/QUBridgeInputContext.cpp

    # Reuse agent core from inject module
    ${CMAKE_CURRENT_SOURCE_DIR}/../inject/ControlScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../inject/InputSimulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../inject/CommandHandler.cpp
)

add_library(QU_BridgePlugin SHARED ${SOURCES})

# 复用的 inject 源码带中文注释，缺 /utf-8 时 MSVC 会按本地代码页（936/GBK）解读
# UTF-8 字节，注释会“吃掉”后续代码并引发 C2001 / C1057 一类跟踪不到根因的错误。
# inject target 已有此选项，本 target 复用同一批源码，必须保持一致。
if (MSVC)
    target_compile_options(QU_BridgePlugin PRIVATE /utf-8)
endif()

target_include_directories(QU_BridgePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Qt5_PRIVATE_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../inject
)

target_link_libraries(QU_BridgePlugin PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
)

# Qt plugins must be placed under <QT_PLUGIN_PATH>/platforminputcontexts/
# Force flat output on multi-config generators (MSVC) so Release/Debug do not
# create extra subdirectories and Qt can locate the plugin by QT_PLUGIN_PATH.
set(_PLUGIN_OUTDIR "${CMAKE_BINARY_DIR}/plugins/platforminputcontexts")
set_target_properties(QU_BridgePlugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_PLUGIN_OUTDIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_PLUGIN_OUTDIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_PLUGIN_OUTDIR}"
    LIBRARY_OUTPUT_DIRECTORY "${_PLUGIN_OUTDIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_PLUGIN_OUTDIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_PLUGIN_OUTDIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${_PLUGIN_OUTDIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${_PLUGIN_OUTDIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${_PLUGIN_OUTDIR}"
)
