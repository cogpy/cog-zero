# Agent-Zero C++ Implementation for OpenCog Integration
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroCpp
    VERSION 0.1.0
    DESCRIPTION "C++ Agent-Zero Implementation Optimized for OpenCog Integration"
    LANGUAGES CXX
)

# Add the cmake/ directory to CMake's module search path so that the
# standard OpenCog cmake modules (Summary, AddCxxtest, etc.) are found.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_DOCS "Build documentation" OFF)

# Standalone cog0 application — builds without any OpenCog dependencies.
# This is the default when no OpenCog packages are available on the system.
# It can also be built independently from the standalone/ directory.
option(BUILD_STANDALONE "Build the standalone cog0 C++ application" ON)

# OpenCog-dependent modules — requires cogutil, atomspace (and optionally
# cogserver, pln, ure, moses, attention, spacetime, sensory, lg-atomese).
# When OFF the build succeeds without any OpenCog packages installed.
# Default ON; automatically falls back to OFF if cogutil/atomspace are absent.
option(BUILD_OPENCOG_MODULES "Build OpenCog-dependent modules" ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Fix for modern CMake Boost module policy
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.30)
    cmake_policy(SET CMP0167 OLD)
endif()

message(STATUS "Building Agent-Zero C++ for OpenCog Integration")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Global compiler settings
add_compile_options(-Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O3 -DNDEBUG)
endif()

# ----------------------------------------------------------
# OpenCog-dependent external dependencies (Boost, cogutil, atomspace, …)
# These are only needed when building the OpenCog modules.
# The standalone cog0 build has no external dependencies beyond C++17 + Threads.
#
if(BUILD_OPENCOG_MODULES)
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_MULTITHREADED ON)
    set(MIN_BOOST 1.46)

    find_package(Boost ${MIN_BOOST} COMPONENTS system filesystem thread REQUIRED)

    if(Boost_FOUND)
        message(STATUS "Found Boost ${Boost_VERSION}: ${Boost_INCLUDE_DIRS}")
        include_directories(${Boost_INCLUDE_DIRS})
        link_directories(${Boost_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "Boost ${MIN_BOOST} or newer is needed to build OpenCog modules!")
    endif()

    find_package(PkgConfig REQUIRED)

    # Core OpenCog dependencies
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    pkg_check_modules(COGSERVER cogserver)
endif()

if(BUILD_OPENCOG_MODULES)
    if(NOT COGUTIL_FOUND)
        message(WARNING "\n"
            "OpenCog dependency 'cogutil' not found.\n"
            "Disabling OpenCog modules (BUILD_OPENCOG_MODULES -> OFF).\n"
            "To build OpenCog modules install cogutil first:\n"
            "  See: https://github.com/opencog/cogutil\n")
        set(BUILD_OPENCOG_MODULES OFF CACHE BOOL "" FORCE)
    endif()

    if(BUILD_OPENCOG_MODULES AND NOT ATOMSPACE_FOUND)
        message(WARNING "\n"
            "OpenCog dependency 'atomspace' not found.\n"
            "Disabling OpenCog modules (BUILD_OPENCOG_MODULES -> OFF).\n"
            "  See: https://github.com/opencog/atomspace\n")
        set(BUILD_OPENCOG_MODULES OFF CACHE BOOL "" FORCE)
    endif()

    if(BUILD_OPENCOG_MODULES AND NOT COGSERVER_FOUND)
        message(WARNING "\n"
            "OpenCog dependency 'cogserver' not found.\n"
            "Some features may be disabled.\n"
            "  See: https://github.com/opencog/cogserver\n")
        set(COGSERVER_FOUND FALSE)
    endif()
endif()

# Optional OpenCog dependencies (for specific modules)
if(BUILD_OPENCOG_MODULES)
    pkg_check_modules(PLN pln)
    pkg_check_modules(URE ure)
    pkg_check_modules(MINER miner)
    pkg_check_modules(MOSES moses)
    pkg_check_modules(ATTENTION attention)
    pkg_check_modules(SPACETIME spacetime)
    pkg_check_modules(SENSORY sensory)
    pkg_check_modules(LG_ATOMESE lg-atomese)

    # Report found dependencies
    message(STATUS "OpenCog Dependencies Found:")
    message(STATUS "  cogutil: ${COGUTIL_VERSION}")
    message(STATUS "  atomspace: ${ATOMSPACE_VERSION}")
    message(STATUS "  cogserver: ${COGSERVER_VERSION}")

    if(PLN_FOUND)
        message(STATUS "  pln: ${PLN_VERSION}")
    endif()

    if(URE_FOUND)
        message(STATUS "  ure: ${URE_VERSION}")
    endif()

    if(ATTENTION_FOUND)
        message(STATUS "  attention: ${ATTENTION_VERSION}")
    endif()

    # Global include directories (OpenCog modules)
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${COGUTIL_INCLUDE_DIRS}
        ${ATOMSPACE_INCLUDE_DIRS}
        ${COGSERVER_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
    )

    # Global link directories (OpenCog modules)
    link_directories(
        ${COGUTIL_LIBRARY_DIRS}
        ${ATOMSPACE_LIBRARY_DIRS}
        ${COGSERVER_LIBRARY_DIRS}
        ${Boost_LIBRARY_DIRS}
    )
endif()

# Add subdirectories for each Agent-Zero module
# These are added in dependency order

if(BUILD_OPENCOG_MODULES)
    # Phase 1: Foundation Layer
    add_subdirectory(agentzero-core)

    # Phase 2: Perception & Action (depends on core)
    # Note: agentzero-perception has its own integration with optional OpenCog perception components
    add_subdirectory(agentzero-perception)

    # Phase 3: Knowledge & Reasoning (depends on core)
    # Optional pln/ure/miner enable HAVE_* flags; built-in fallbacks always work
    add_subdirectory(agentzero-knowledge)

    # Phase 4: Planning & Goals (depends on core)
    # Planning module works with or without spacetime; spacetime enables full spatial-temporal features
    add_subdirectory(agentzero-planning)

    # Phase 5: Learning & Adaptation (depends on core)
    # Note: agentzero-learning now exists and integrates MOSES for policy optimization (AZ-LEARN-003)
    if(MOSES_FOUND)
        add_subdirectory(agentzero-learning)
    else()
        message(STATUS "Skipping agentzero-learning: MOSES dependency not found")
    endif()

    # Phase 6: Communication & NLP (depends on core)
    # Note: HumanInterface (AZ-HUMAN-001) has basic functionality without lg-atomese
    add_subdirectory(agentzero-communication)
    if(NOT LG_ATOMESE_FOUND)
        message(STATUS "agentzero-communication: lg-atomese not found - NLP features will be basic")
    endif()

    # Phase 7: Memory & Context (depends on core)
    if(ATTENTION_FOUND)
        add_subdirectory(agentzero-memory)
    else()
        message(STATUS "Skipping agentzero-memory: attention dependency not found")
    endif()

    # Phase 8: Tool Integration (depends on core)
    add_subdirectory(agentzero-tools)

    # Phase 9: Profiling Infrastructure (AZ-PERF-001)
    add_subdirectory(profiling)

    # Phase 10: Advanced Features - Distributed Computing (AZ-SCALE-001)
    add_subdirectory(agentzero-distributed)

    # OpenCog integration layer — the opencog/ tree contains the library
    # that is installed and consumed by other OpenCog projects.
    add_subdirectory(opencog)

    # pkg-config / CMake package files
    add_subdirectory(lib)

    # Examples and tests (OpenCog modules)
    if(BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()

    if(BUILD_TESTING)
        enable_testing()
        add_subdirectory(tests)
    endif()
else()
    message(STATUS "Skipping OpenCog modules (BUILD_OPENCOG_MODULES=OFF)")
endif()

# Documentation
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
    else()
        message(STATUS "Doxygen not found, documentation will not be built")
    endif()
endif()

# Package configuration
include(GNUInstallDirs)

if(BUILD_OPENCOG_MODULES)
    # Set datadir for cmake scripts installation (mirrors cogutil convention)
    if(NOT DEFINED DATADIR)
        set(DATADIR "${CMAKE_INSTALL_PREFIX}/share/opencog")
    endif()
    add_definitions(-DDATADIR="${DATADIR}")

    # Generate the CogZero pkg-config / CMake config file
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CogZeroConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CogZeroConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CogZero
        PATH_VARS CMAKE_INSTALL_PREFIX
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CogZeroConfig.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CogZero
    )

    # Export targets for installation
    install(EXPORT CogZeroTargets
        FILE CogZeroTargets.cmake
        NAMESPACE CogZero::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CogZero
    )

    # Install the cmake modules so downstream packages can find them
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/
        DESTINATION ${DATADIR}/cmake
        FILES_MATCHING PATTERN "*.cmake"
    )
endif()

# Phase 1–3 modules — build with real OpenCog or in-tree AtomSpace shim.
# Always available so module tests can run under CTest without system
# OpenCog packages installed.
if(NOT BUILD_OPENCOG_MODULES)
    message(STATUS "Adding agentzero-core with AtomSpace shim (OpenCog modules disabled)")
    add_subdirectory(agentzero-core)
    message(STATUS "Adding agentzero-perception with AtomSpace shim (OpenCog modules disabled)")
    add_subdirectory(agentzero-perception)
    message(STATUS "Adding agentzero-knowledge with AtomSpace shim (OpenCog modules disabled)")
    add_subdirectory(agentzero-knowledge)
    message(STATUS "Adding agentzero-planning with AtomSpace shim (OpenCog modules disabled)")
    add_subdirectory(agentzero-planning)
endif()

# Standalone cog0 application (no OpenCog dependencies required)
if(BUILD_STANDALONE)
    message(STATUS "Building standalone cog0 application (no OpenCog deps)")
    # enable_testing() must be called from the root CMakeLists so that CTest
    # discovers all test targets registered by add_subdirectory(standalone).
    if(BUILD_TESTING)
        enable_testing()
    endif()
    add_subdirectory(standalone)
else()
    message(STATUS "Skipping standalone cog0 build (BUILD_STANDALONE=OFF)")
endif()

# Status summary
message(STATUS "")
message(STATUS "Agent-Zero C++ Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build testing: ${BUILD_TESTING}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build documentation: ${BUILD_DOCS}")
message(STATUS "  Build OpenCog modules: ${BUILD_OPENCOG_MODULES}")
message(STATUS "  Build standalone: ${BUILD_STANDALONE}")
message(STATUS "")
message(STATUS "This is part of the AGENT-ZERO-GENESIS project.")
message(STATUS "See AGENT-ZERO-GENESIS.md for complete roadmap and development tasks.")
message(STATUS "")