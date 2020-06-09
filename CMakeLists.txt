cmake_minimum_required(VERSION 3.5)
project(tcpshm C CXX)

#set( CMAKE_VERBOSE_MAKEFILE on )

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified, defaulting Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
else(NOT CMAKE_BUILD_TYPE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Build type: Debug")
        add_definitions(-DDBG_TIME)
        set(CMAKE_CXX_FLAGS "-g -O0")
    endif()
endif()
set(CMAKE_CXX_FLAGS "--std=c++11")

add_executable(
    camera_server
    ${PROJECT_SOURCE_DIR}/test/camera_server.cc
    ${PROJECT_SOURCE_DIR}/test/frame_dispatcher.cc
    ${PROJECT_SOURCE_DIR}/test/logging.cc
    )

target_link_libraries(
    camera_server
    rt
    pthread
    )

add_executable(
    camera_client
    ${PROJECT_SOURCE_DIR}/test/camera_client.cc
    ${PROJECT_SOURCE_DIR}/test/logging.cc
    )

target_link_libraries(
    camera_client
    rt
    pthread
    )

