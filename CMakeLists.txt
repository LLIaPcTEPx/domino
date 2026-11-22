cmake_minimum_required(VERSION 3.21)

project(random_console VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

# Geode CLI exports GEODE_SDK as an environment variable. Copy it into the
# CMake variable when it hasn't been passed explicitly with -DGEODE_SDK=... .
if (NOT DEFINED GEODE_SDK OR GEODE_SDK STREQUAL "")
    if (DEFINED ENV{GEODE_SDK} AND NOT "$ENV{GEODE_SDK}" STREQUAL "")
        set(GEODE_SDK "$ENV{GEODE_SDK}")
    else()
        message(FATAL_ERROR
            "Geode SDK path is missing. Install it with `geode sdk install`, "
            "restart your terminal, or set GEODE_SDK to your SDK directory."
        )
    endif()
endif()

if (NOT IS_DIRECTORY "${GEODE_SDK}")
    message(FATAL_ERROR "GEODE_SDK does not point to a directory: ${GEODE_SDK}")
endif()

add_subdirectory("${GEODE_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/geode")
setup_geode_mod(${PROJECT_NAME})
