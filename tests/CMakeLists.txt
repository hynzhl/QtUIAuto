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
