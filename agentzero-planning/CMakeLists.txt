# agentzero-planning/CMakeLists.txt
# Agent-Zero Planning Engine with Temporal Reasoning — Phase 4
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroPlanning
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Planning Engine with Temporal Reasoning"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-planning will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-planning will not be built")
    return()
endif()

pkg_check_modules(SPACETIME spacetime)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(AGENTZERO_PLANNING_SOURCES
    ${CMAKE_SOURCE_DIR}/src/GoalHierarchy.cpp
    ${CMAKE_SOURCE_DIR}/src/PlanningEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/TemporalReasoner.cpp
)

if(SPACETIME_FOUND)
    list(APPEND AGENTZERO_PLANNING_SOURCES
        ${CMAKE_SOURCE_DIR}/src/SpaceTimeIntegrator.cpp)
    message(STATUS "  SpaceTimeIntegrator: enabled (spacetime found)")
else()
    message(STATUS "  SpaceTimeIntegrator: disabled (spacetime not found)")
endif()

add_library(agentzero-planning SHARED ${AGENTZERO_PLANNING_SOURCES})

target_include_directories(agentzero-planning PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-planning
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
)

if(SPACETIME_FOUND)
    target_link_libraries(agentzero-planning ${SPACETIME_LIBRARIES})
    target_compile_definitions(agentzero-planning PRIVATE HAVE_SPACETIME)
endif()

set_target_properties(agentzero-planning PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-planning
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-planning configured")
