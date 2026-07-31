cmake_minimum_required(VERSION 3.18)
project(QtUIAuto_Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Network Test Quick Widgets)

add_executable(QtUIAuto_E2E e2e_test.cpp)

# 源码中含中文断言期望值，必须按 UTF-8 编译，否则字面量会被错误解码
if (MSVC)
    target_compile_options(QtUIAuto_E2E PRIVATE /utf-8)
endif()

target_link_libraries(QtUIAuto_E2E PRIVATE
    Qt5::Core Qt5::Network Qt5::Test
)

# 把被测目标与注入 DLL 同步到 E2E 可执行文件目录，
# 避免靠手工拷贝导致运行到过期的二进制
add_dependencies(QtUIAuto_E2E QtUIAuto_TestTarget QtUIAuto_Inject)
add_custom_command(TARGET QtUIAuto_E2E POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:QtUIAuto_TestTarget> $<TARGET_FILE_DIR:QtUIAuto_E2E>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:QtUIAuto_Inject> $<TARGET_FILE_DIR:QtUIAuto_E2E>
    COMMENT "Sync QtUIAuto_TestTarget.exe / QtUIAuto_Inject.dll into E2E output dir"
)

add_test(NAME e2e_test COMMAND QtUIAuto_E2E)

# ─────────────────────────────────────────────────────────────
# 宿主端链路测试：直接编译进主程序的真实源文件。
# E2E 自己实现了一套管道客户端，绕过了这三个类，所以它们需要单独覆盖。
# ─────────────────────────────────────────────────────────────
set(HOST_CHAIN_SOURCES
    host_chain_test.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ProcessManager.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ProcessManager.h
    ${CMAKE_SOURCE_DIR}/src/engine/PipeServer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/PipeServer.h
    ${CMAKE_SOURCE_DIR}/src/engine/ScriptEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/ScriptEngine.h
)

add_executable(QtUIAuto_HostChain ${HOST_CHAIN_SOURCES})

# 源码与日志含中文，同样必须按 UTF-8 编译
if (MSVC)
    target_compile_options(QtUIAuto_HostChain PRIVATE /utf-8)
endif()

target_link_libraries(QtUIAuto_HostChain PRIVATE
    Qt5::Core Qt5::Network
)

target_include_directories(QtUIAuto_HostChain PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_dependencies(QtUIAuto_HostChain QtUIAuto_TestTarget QtUIAuto_Inject)
add_custom_command(TARGET QtUIAuto_HostChain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:QtUIAuto_TestTarget> $<TARGET_FILE_DIR:QtUIAuto_HostChain>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:QtUIAuto_Inject> $<TARGET_FILE_DIR:QtUIAuto_HostChain>
    COMMENT "Sync QtUIAuto_TestTarget.exe / QtUIAuto_Inject.dll into HostChain output dir"
)

add_test(NAME host_chain_test COMMAND QtUIAuto_HostChain)
