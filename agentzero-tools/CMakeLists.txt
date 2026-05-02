# agentzero-tools/CMakeLists.txt
# Agent-Zero External Tool Integration — Phase 8
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroTools
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Tool Integration Framework for OpenCog"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-tools will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-tools will not be built")
    return()
endif()

find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(AGENTZERO_TOOLS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/CapabilityComposer.cpp
    ${CMAKE_SOURCE_DIR}/src/ResourceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/RestApiAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/RosBehaviorBridge.cpp
    ${CMAKE_SOURCE_DIR}/src/ToolExecutor.cpp
    ${CMAKE_SOURCE_DIR}/src/ToolRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/ToolWrapper.cpp
)

add_library(agentzero-tools SHARED ${AGENTZERO_TOOLS_SOURCES})

target_include_directories(agentzero-tools PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-tools
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    Threads::Threads
)

set_target_properties(agentzero-tools PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-tools
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-tools configured")
