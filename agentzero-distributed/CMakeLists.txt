# agentzero-distributed/CMakeLists.txt
# Agent-Zero Distributed Computing Integration — Phase 10 (AZ-SCALE-001)
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroDistributed
    VERSION 0.1.0
    DESCRIPTION "Distributed Computing Integration for Agent-Zero"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-distributed will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-distributed will not be built")
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

set(AGENTZERO_DISTRIBUTED_SOURCES
    ${CMAKE_SOURCE_DIR}/src/DistributedCoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/ClusterManager.cpp
    ${CMAKE_SOURCE_DIR}/src/LoadBalancer.cpp
    ${CMAKE_SOURCE_DIR}/src/CoordinationProtocol.cpp
)

add_library(agentzero-distributed SHARED ${AGENTZERO_DISTRIBUTED_SOURCES})

target_include_directories(agentzero-distributed PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-distributed
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    Threads::Threads
)

set_target_properties(agentzero-distributed PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-distributed
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-distributed configured")
