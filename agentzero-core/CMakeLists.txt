# agentzero-core/CMakeLists.txt
# Agent-Zero Core Orchestration Engine — Phase 1
# Part of the AGENT-ZERO-GENESIS project
#
# Builds against real OpenCog (cogutil/atomspace/cogserver) when available.
# Otherwise uses the in-tree AtomSpace shim so the module and its tests can
# still be built and exercised under CTest.

cmake_minimum_required(VERSION 3.16)

# When included from the monorepo root this is a subdir project; when built
# standalone it becomes the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AgentZeroCore
        VERSION 0.1.0
        DESCRIPTION "Agent-Zero Core Orchestration Engine for OpenCog Integration"
        LANGUAGES CXX
    )
    set(AGENTZERO_CORE_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    # Resolve monorepo root (parent of agentzero-core/)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_CORE_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_CORE_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_CORE_HAVE_OPENCOG FALSE)
set(AGENTZERO_CORE_HAVE_COGSERVER FALSE)

if(NOT AGENTZERO_CORE_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    pkg_check_modules(COGSERVER cogserver)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_CORE_HAVE_OPENCOG TRUE)
        if(COGSERVER_FOUND)
            set(AGENTZERO_CORE_HAVE_COGSERVER TRUE)
        endif()
    endif()
endif()

if(NOT AGENTZERO_CORE_HAVE_OPENCOG)
    message(STATUS "agentzero-core: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_CORE_USE_SHIM TRUE)
    # Enable CogServer Module API via shim so registration paths are testable
    set(AGENTZERO_CORE_HAVE_COGSERVER TRUE)
endif()

set(AGENTZERO_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AgentZeroCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CognitiveLoop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TaskManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/KnowledgeIntegrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ReasoningEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ActionExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ActionScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MetaPlanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SelfModification.cpp
)

add_library(agentzero-core SHARED ${AGENTZERO_CORE_SOURCES})

# When using the shim, it MUST come before ${COGZERO_ROOT}/include so that
# <opencog/atoms/...> resolves to shim headers rather than partial monorepo stubs.
if(AGENTZERO_CORE_USE_SHIM)
    target_include_directories(agentzero-core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shim>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-core PUBLIC AGENTZERO_CORE_SHIM=1)
else()
    target_include_directories(agentzero-core
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
endif()

if(AGENTZERO_CORE_USE_SHIM)
    # already configured above
    set(_AGENTZERO_SHIM_CONFIGURED TRUE)
else()
    target_include_directories(agentzero-core
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
            ${COGSERVER_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-core
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
            ${COGSERVER_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-core
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
    if(AGENTZERO_CORE_HAVE_COGSERVER AND COGSERVER_FOUND)
        target_link_libraries(agentzero-core PUBLIC ${COGSERVER_LIBRARIES})
    endif()
endif()

target_link_libraries(agentzero-core PUBLIC Threads::Threads)

if(AGENTZERO_CORE_HAVE_COGSERVER)
    target_compile_definitions(agentzero-core PUBLIC HAVE_COGSERVER=1)
endif()

set_target_properties(agentzero-core PROPERTIES
    VERSION 0.1.0
    SOVERSION 1
    OUTPUT_NAME "agentzero-core"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
install(TARGETS agentzero-core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING OR AGENTZERO_CORE_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-core configured (shim=${AGENTZERO_CORE_USE_SHIM}, cogserver=${AGENTZERO_CORE_HAVE_COGSERVER})")
