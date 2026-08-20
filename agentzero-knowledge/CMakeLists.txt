# agentzero-knowledge/CMakeLists.txt
# Agent-Zero Knowledge Representation & Reasoning — Phase 3
# Part of the AGENT-ZERO-GENESIS project
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.
# Optional PLN / URE / miner packages enable HAVE_* compile definitions.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(_az_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    list(APPEND CMAKE_MODULE_PATH "${_az_repo_root}/cmake")
    include(CogZeroVersion)
    project(AgentZeroKnowledge
        VERSION ${COGZERO_VERSION}
        DESCRIPTION "Agent-Zero Knowledge Representation & Reasoning Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_KNOWLEDGE_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_KNOWLEDGE_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_KNOWLEDGE_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_KNOWLEDGE_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_KNOWLEDGE_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_KNOWLEDGE_HAVE_OPENCOG TRUE)
    endif()
endif()

# Optional reasoning packages (real OpenCog only)
set(AGENTZERO_KNOWLEDGE_HAVE_PLN FALSE)
set(AGENTZERO_KNOWLEDGE_HAVE_URE FALSE)
set(AGENTZERO_KNOWLEDGE_HAVE_MINER FALSE)

if(AGENTZERO_KNOWLEDGE_HAVE_OPENCOG AND PkgConfig_FOUND)
    pkg_check_modules(PLN pln)
    pkg_check_modules(URE ure)
    pkg_check_modules(MINER miner)
    if(PLN_FOUND)
        set(AGENTZERO_KNOWLEDGE_HAVE_PLN TRUE)
    endif()
    if(URE_FOUND)
        set(AGENTZERO_KNOWLEDGE_HAVE_URE TRUE)
    endif()
    if(MINER_FOUND)
        set(AGENTZERO_KNOWLEDGE_HAVE_MINER TRUE)
    endif()
endif()

if(NOT AGENTZERO_KNOWLEDGE_HAVE_OPENCOG)
    message(STATUS "agentzero-knowledge: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_KNOWLEDGE_USE_SHIM TRUE)
endif()

set(AGENTZERO_KNOWLEDGE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/KnowledgeBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PatternDiscovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ConceptFormation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PLNRuleLibrary.cpp
)

add_library(agentzero-knowledge SHARED ${AGENTZERO_KNOWLEDGE_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_KNOWLEDGE_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-knowledge
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-knowledge PUBLIC AGENTZERO_KNOWLEDGE_SHIM=1)
else()
    target_include_directories(agentzero-knowledge
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-knowledge
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-knowledge
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-knowledge
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-knowledge PUBLIC Threads::Threads)

if(AGENTZERO_KNOWLEDGE_HAVE_PLN)
    target_link_libraries(agentzero-knowledge PUBLIC ${PLN_LIBRARIES})
    target_compile_definitions(agentzero-knowledge PRIVATE HAVE_PLN=1)
endif()
if(AGENTZERO_KNOWLEDGE_HAVE_URE)
    target_link_libraries(agentzero-knowledge PUBLIC ${URE_LIBRARIES})
    target_compile_definitions(agentzero-knowledge PRIVATE HAVE_URE=1)
endif()
if(AGENTZERO_KNOWLEDGE_HAVE_MINER)
    target_link_libraries(agentzero-knowledge PUBLIC ${MINER_LIBRARIES})
    target_compile_definitions(agentzero-knowledge PRIVATE HAVE_MINER=1)
endif()

# Ensure version vars exist when built as subdir without parent include
if(NOT DEFINED COGZERO_VERSION)
    set(COGZERO_VERSION "${PROJECT_VERSION}")
endif()
if(NOT DEFINED COGZERO_SOVERSION)
    set(COGZERO_SOVERSION 0)
endif()

set_target_properties(agentzero-knowledge PROPERTIES
    VERSION ${COGZERO_VERSION}
    SOVERSION ${COGZERO_SOVERSION}
    OUTPUT_NAME "agentzero-knowledge"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
# Shim builds are for tests only — do not install as release artifacts.
if(NOT AGENTZERO_KNOWLEDGE_USE_SHIM)
    install(TARGETS agentzero-knowledge
        EXPORT CogZeroTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(NOT AGENTZERO_KNOWLEDGE_USE_SHIM)
# Public headers for OpenCog/Development consumers
set(_agentzero_knowledge_PUBLIC_HEADERS
        KnowledgeBase.h
        PLNRuleLibrary.h
        TemporalReasoner.h
        ConceptFormation.h
        PatternDiscovery.h
)
foreach(_h ${_agentzero_knowledge_PUBLIC_HEADERS})
    if(EXISTS "${COGZERO_ROOT}/include/${_h}")
        install(FILES "${COGZERO_ROOT}/include/${_h}"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cog0
            COMPONENT OpenCog
        )
    endif()
endforeach()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING OR AGENTZERO_KNOWLEDGE_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-knowledge configured (shim=${AGENTZERO_KNOWLEDGE_USE_SHIM}, PLN=${AGENTZERO_KNOWLEDGE_HAVE_PLN}, URE=${AGENTZERO_KNOWLEDGE_HAVE_URE}, miner=${AGENTZERO_KNOWLEDGE_HAVE_MINER})")
