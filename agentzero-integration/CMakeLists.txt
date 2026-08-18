# agentzero-integration/CMakeLists.txt
# Phase 9 — Full system integration, benchmarks, and regression baselines
# Links all agentzero-* modules together under CTest.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AgentZeroIntegration
        VERSION 0.1.0
        DESCRIPTION "Agent-Zero Phase 9 full-system integration tests"
        LANGUAGES CXX
    )
    set(AGENTZERO_INTEGRATION_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    option(BUILD_TESTING "Build tests" ON)
    # When built alone, pull in all modules with the AtomSpace shim.
    set(BUILD_OPENCOG_MODULES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${COGZERO_ROOT}/agentzero-core ${CMAKE_BINARY_DIR}/agentzero-core)
    add_subdirectory(${COGZERO_ROOT}/agentzero-perception ${CMAKE_BINARY_DIR}/agentzero-perception)
    add_subdirectory(${COGZERO_ROOT}/agentzero-knowledge ${CMAKE_BINARY_DIR}/agentzero-knowledge)
    add_subdirectory(${COGZERO_ROOT}/agentzero-planning ${CMAKE_BINARY_DIR}/agentzero-planning)
    add_subdirectory(${COGZERO_ROOT}/agentzero-learning ${CMAKE_BINARY_DIR}/agentzero-learning)
    add_subdirectory(${COGZERO_ROOT}/agentzero-communication ${CMAKE_BINARY_DIR}/agentzero-communication)
    add_subdirectory(${COGZERO_ROOT}/agentzero-memory ${CMAKE_BINARY_DIR}/agentzero-memory)
    add_subdirectory(${COGZERO_ROOT}/agentzero-tools ${CMAKE_BINARY_DIR}/agentzero-tools)
else()
    set(AGENTZERO_INTEGRATION_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

find_package(Threads REQUIRED)

# Require all Phase 1–8 module targets.
foreach(_mod IN ITEMS
    agentzero-core
    agentzero-perception
    agentzero-knowledge
    agentzero-planning
    agentzero-learning
    agentzero-communication
    agentzero-memory
    agentzero-tools
)
    if(NOT TARGET ${_mod})
        message(WARNING "agentzero-integration: target '${_mod}' not found — skipping Phase 9 system tests")
        return()
    endif()
endforeach()

if(BUILD_TESTING OR AGENTZERO_INTEGRATION_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-integration configured (full system Phase 9 tests)")
