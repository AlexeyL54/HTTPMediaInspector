cmake_minimum_required(VERSION 3.10)
project(httpMediaInspector VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")

set(SOURCES
    src/Inspector.cpp
    src/Server.cpp
    main.cpp
)

set(HEADERS
    src/Inspector.hpp
    src/Server.hpp
    lib/cpp-httplib/httplib.h
)

add_executable(httpMediaInspector ${SOURCES} ${C_SOURCES} ${HEADERS})

target_link_libraries(httpMediaInspector pthread m)
