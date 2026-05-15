cmake_minimum_required(VERSION 3.10)
project(test VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)

set(SOURCES
    ../src/Inspector.cpp
    ../src/Server.cpp
    test.cpp
)

set(HEADERS
    ../src/Inspector.hpp
    ../src/Server.hpp
    ../lib/cpp-httplib/httplib.h
)

add_executable(test ${SOURCES} ${C_SOURCES} ${HEADERS})

target_include_directories(test PRIVATE
    ${HTTPLIB_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/..
)

target_link_libraries(test 
    GTest::gtest
    GTest::gtest_main
    pthread
)
