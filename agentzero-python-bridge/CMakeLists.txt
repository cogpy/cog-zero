# agentzero-python-bridge/CMakeLists.txt
# Agent-Zero Python Bindings — C API + Cython
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroPythonBridge
    VERSION 0.1.0
    DESCRIPTION "Python Bindings for Agent-Zero OpenCog Integration"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python3 COMPONENTS Interpreter Development QUIET)
if(NOT Python3_FOUND)
    message(WARNING "Python3 not found — agentzero-python-bridge will not be built")
    return()
endif()

message(STATUS "Found Python ${Python3_VERSION}: ${Python3_EXECUTABLE}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-python-bridge will not be built")
    return()
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${Python3_INCLUDE_DIRS}
    ${COGUTIL_INCLUDE_DIRS}
)

# C API bridge library
add_library(cog0_capi SHARED
    ${CMAKE_SOURCE_DIR}/src/cog0_capi.cpp
)

target_include_directories(cog0_capi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(cog0_capi
    ${Python3_LIBRARIES}
    ${COGUTIL_LIBRARIES}
)

set_target_properties(cog0_capi PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PREFIX ""
)

include(GNUInstallDirs)
install(TARGETS cog0_capi
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-python-bridge configured (Python ${Python3_VERSION})")
