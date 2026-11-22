cmake_minimum_required(VERSION 3.21)

project(random_console VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

if (NOT DEFINED GEODE_SDK)
    message(FATAL_ERROR "GEODE_SDK is not set. Install Geode CLI/SDK, or pass -DGEODE_SDK=<path>.")
endif()

add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})
