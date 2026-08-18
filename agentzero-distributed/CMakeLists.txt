# agentzero-distributed/CMakeLists.txt
# Agent-Zero Distributed Computing Integration — Phase 10 (AZ-SCALE-001)
# Part of the AGENT-ZERO-GENESIS project
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AgentZeroDistributed
        VERSION 0.1.0
        DESCRIPTION "Agent-Zero Distributed Computing Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_DISTRIBUTED_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_DISTRIBUTED_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_DISTRIBUTED_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_DISTRIBUTED_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_DISTRIBUTED_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_DISTRIBUTED_HAVE_OPENCOG TRUE)
    endif()
endif()

if(NOT AGENTZERO_DISTRIBUTED_HAVE_OPENCOG)
    message(STATUS "agentzero-distributed: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_DISTRIBUTED_USE_SHIM TRUE)
endif()

set(AGENTZERO_DISTRIBUTED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ClusterManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DistributedCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LoadBalancer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CoordinationProtocol.cpp
)

add_library(agentzero-distributed SHARED ${AGENTZERO_DISTRIBUTED_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_DISTRIBUTED_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-distributed
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-distributed PUBLIC AGENTZERO_DISTRIBUTED_SHIM=1)
else()
    target_include_directories(agentzero-distributed
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-distributed
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-distributed
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-distributed
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-distributed PUBLIC Threads::Threads)

set_target_properties(agentzero-distributed PROPERTIES
    VERSION 0.1.0
    SOVERSION 1
    OUTPUT_NAME "agentzero-distributed"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
install(TARGETS agentzero-distributed
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING OR AGENTZERO_DISTRIBUTED_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-distributed configured (shim=${AGENTZERO_DISTRIBUTED_USE_SHIM})")
