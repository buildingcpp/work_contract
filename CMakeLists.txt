cmake_minimum_required(VERSION 3.16.3)
set(CMAKE_CXX_STANDARD 20)

project(work_contract)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("*** Build type not set.  defaulting to Release")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(
        -g
        -O0
        -march=native
    #    -fno-omit-frame-pointer
    )
else()
    add_compile_options(
        -O3
        -march=native
    #    -pg
    #    -g
    )
endif()

message("Build Type = ${CMAKE_BUILD_TYPE}")

###############################################################################
# Fetch the main bcpp scripts which allows us to fetch other dependencies more easily
###############################################################################

include(FetchContent)

if ("${_scripts_src_path}" STREQUAL "")
    set(_scripts_src_path ${CMAKE_BINARY_DIR}/scripts-src CACHE STRING "")
    message("Fetching Content: scripts.git")

    FetchContent_Declare(
        scripts
        GIT_REPOSITORY git@github.com:buildingcpp/scripts.git
        GIT_TAG main
        SOURCE_DIR        "${CMAKE_BINARY_DIR}/scripts-src"
        BINARY_DIR        "${CMAKE_BINARY_DIR}/scripts-build"
        INSTALL_DIR       "${CMAKE_BINARY_DIR}"
        INSTALL_COMMAND   ""
        )
    FetchContent_MakeAvailable(scripts)
    FetchContent_GetProperties(scripts)
endif()

include("${_scripts_src_path}/cmake/fetch_dependencies.cmake")

fetch_dependency("https://github.com/buildingcpp/include.git;main")


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME})
set(_${PROJECT_NAME}_dir ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "")

add_subdirectory(src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
