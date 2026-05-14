cmake_minimum_required(VERSION 3.10)
project(mediaInspector VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")

set(SOURCES
    src/Inspector.cpp
    src/Server.cpp
    main.cpp
)

set(C_SOURCES
    lib/cJSON/cJSON.c
)

set(HEADERS
    src/Inspector.hpp
    src/Server.hpp
    lib/cJSON/cJSON.h
    lib/cpp-httplib/httplib.h
)

add_executable(mediaInspector ${SOURCES} ${C_SOURCES} ${HEADERS})

set_source_files_properties(lib/cJSON/cJSON.c PROPERTIES LANGUAGE C)

target_link_libraries(mediaInspector pthread m)

# Установка (опционально)
# install(TARGETS fileInspector DESTINATION bin)
