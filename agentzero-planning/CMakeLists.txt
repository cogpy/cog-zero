# agentzero-planning/CMakeLists.txt
# Agent-Zero Hierarchical Planning & Goal Management — Phase 4
# Part of the AGENT-ZERO-GENESIS project
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.
# Optional spacetime package enables HAVE_SPACETIME for native octomap.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AgentZeroPlanning
        VERSION 0.1.0
        DESCRIPTION "Agent-Zero Hierarchical Planning & Goal Management Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_PLANNING_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_PLANNING_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_PLANNING_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_PLANNING_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_PLANNING_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_PLANNING_HAVE_OPENCOG TRUE)
    endif()
endif()

set(AGENTZERO_PLANNING_HAVE_SPACETIME FALSE)
if(AGENTZERO_PLANNING_HAVE_OPENCOG AND PkgConfig_FOUND)
    pkg_check_modules(SPACETIME spacetime)
    if(SPACETIME_FOUND)
        set(AGENTZERO_PLANNING_HAVE_SPACETIME TRUE)
    endif()
endif()

if(NOT AGENTZERO_PLANNING_HAVE_OPENCOG)
    message(STATUS "agentzero-planning: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_PLANNING_USE_SHIM TRUE)
endif()

set(AGENTZERO_PLANNING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GoalHierarchy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TemporalReasoner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlanningEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceTimeIntegrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MetaPlanner.cpp
)

add_library(agentzero-planning SHARED ${AGENTZERO_PLANNING_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_PLANNING_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-planning
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-planning PUBLIC AGENTZERO_PLANNING_SHIM=1)
else()
    target_include_directories(agentzero-planning
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-planning
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-planning
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-planning
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-planning PUBLIC Threads::Threads)

if(AGENTZERO_PLANNING_HAVE_SPACETIME)
    target_include_directories(agentzero-planning PUBLIC ${SPACETIME_INCLUDE_DIRS})
    target_link_directories(agentzero-planning PUBLIC ${SPACETIME_LIBRARY_DIRS})
    target_link_libraries(agentzero-planning PUBLIC ${SPACETIME_LIBRARIES})
    target_compile_definitions(agentzero-planning PRIVATE HAVE_SPACETIME=1)
    message(STATUS "  SpaceTimeIntegrator: enabled (spacetime found)")
else()
    message(STATUS "  SpaceTimeIntegrator: fallback (spacetime not found)")
endif()

set_target_properties(agentzero-planning PROPERTIES
    VERSION 0.1.0
    SOVERSION 1
    OUTPUT_NAME "agentzero-planning"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
install(TARGETS agentzero-planning
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING OR AGENTZERO_PLANNING_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-planning configured (shim=${AGENTZERO_PLANNING_USE_SHIM}, spacetime=${AGENTZERO_PLANNING_HAVE_SPACETIME})")
