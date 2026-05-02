# agentzero-core/CMakeLists.txt
# Agent-Zero Core Orchestration Engine — Phase 1
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroCore
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Core Orchestration Engine for OpenCog Integration"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-core will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-core will not be built")
    return()
endif()

pkg_check_modules(COGSERVER cogserver)
pkg_check_modules(ATTENTION attention)
pkg_check_modules(PLN pln)

find_package(Threads REQUIRED)

# Include paths
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
    ${COGSERVER_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(AGENTZERO_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/AgentZeroCore.cpp
    "${CMAKE_SOURCE_DIR}/src/CognitiveLoop (2).cpp"
    ${CMAKE_SOURCE_DIR}/src/KnowledgeIntegrator.cpp
    ${CMAKE_SOURCE_DIR}/src/ReasoningEngine.cpp
    "${CMAKE_SOURCE_DIR}/src/ActionExecutor (2).cpp"
    "${CMAKE_SOURCE_DIR}/src/ActionScheduler (2).cpp"
    ${CMAKE_SOURCE_DIR}/src/MetaPlanner.cpp
    ${CMAKE_SOURCE_DIR}/src/SelfModification.cpp
)

add_library(agentzero-core SHARED ${AGENTZERO_CORE_SOURCES})

target_include_directories(agentzero-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-core
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    Threads::Threads
)

if(COGSERVER_FOUND)
    target_link_libraries(agentzero-core ${COGSERVER_LIBRARIES})
endif()

set_target_properties(agentzero-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-core configured (cogutil ${COGUTIL_VERSION}, atomspace ${ATOMSPACE_VERSION})")
