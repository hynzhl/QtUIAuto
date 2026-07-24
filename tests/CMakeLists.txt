cmake_minimum_required(VERSION 3.18)
project(QtUIAuto_Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/5.15.2/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Core Test Quick Widgets)

# TODO: add test source files here
# add_executable(test_accessibility test_accessibility.cpp)
# target_link_libraries(test_accessibility PRIVATE Qt5::Core Qt5::Test)
# add_test(NAME test_accessibility COMMAND test_accessibility)
