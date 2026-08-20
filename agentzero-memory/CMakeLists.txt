# agentzero-memory/CMakeLists.txt
# Agent-Zero Memory & Context Management — Phase 7
# Part of the AGENT-ZERO-GENESIS project (AZ-MEM-001..003, AZ-CONTEXT-001)
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.
# Optional attention / atomspace-rocks packages enable HAVE_* flags.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(_az_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    list(APPEND CMAKE_MODULE_PATH "${_az_repo_root}/cmake")
    include(CogZeroVersion)
    project(AgentZeroMemory
        VERSION ${COGZERO_VERSION}
        DESCRIPTION "Agent-Zero Memory & Context Management Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_MEMORY_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_MEMORY_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_MEMORY_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_MEMORY_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_MEMORY_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_MEMORY_HAVE_OPENCOG TRUE)
    endif()
endif()

set(AGENTZERO_MEMORY_HAVE_ATTENTION FALSE)
set(AGENTZERO_MEMORY_HAVE_ROCKS FALSE)

if(AGENTZERO_MEMORY_HAVE_OPENCOG AND PkgConfig_FOUND)
    pkg_check_modules(ATTENTION attention)
    pkg_check_modules(ATOMSPACE_ROCKS atomspace-rocks)
    if(ATTENTION_FOUND)
        set(AGENTZERO_MEMORY_HAVE_ATTENTION TRUE)
    endif()
    if(ATOMSPACE_ROCKS_FOUND)
        set(AGENTZERO_MEMORY_HAVE_ROCKS TRUE)
    endif()
endif()

if(NOT AGENTZERO_MEMORY_HAVE_OPENCOG)
    message(STATUS "agentzero-memory: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_MEMORY_USE_SHIM TRUE)
endif()

set(AGENTZERO_MEMORY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EpisodicMemory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorkingMemory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LongTermMemory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ContextManager.cpp
)

add_library(agentzero-memory SHARED ${AGENTZERO_MEMORY_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_MEMORY_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-memory
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-memory PUBLIC AGENTZERO_MEMORY_SHIM=1)
else()
    target_include_directories(agentzero-memory
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-memory
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-memory
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-memory
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-memory PUBLIC Threads::Threads)

if(AGENTZERO_MEMORY_HAVE_ATTENTION)
    target_include_directories(agentzero-memory PUBLIC ${ATTENTION_INCLUDE_DIRS})
    target_link_directories(agentzero-memory PUBLIC ${ATTENTION_LIBRARY_DIRS})
    target_link_libraries(agentzero-memory PUBLIC ${ATTENTION_LIBRARIES})
    target_compile_definitions(agentzero-memory PRIVATE HAVE_ATTENTION=1 HAVE_ATTENTION_BANK=1)
    message(STATUS "  WorkingMemory: attention bank enabled")
else()
    message(STATUS "  WorkingMemory: attention bank unavailable (optional)")
endif()

if(AGENTZERO_MEMORY_HAVE_ROCKS)
    target_include_directories(agentzero-memory PUBLIC ${ATOMSPACE_ROCKS_INCLUDE_DIRS})
    target_link_directories(agentzero-memory PUBLIC ${ATOMSPACE_ROCKS_LIBRARY_DIRS})
    target_link_libraries(agentzero-memory PUBLIC ${ATOMSPACE_ROCKS_LIBRARIES})
    target_compile_definitions(agentzero-memory PRIVATE HAVE_ATOMSPACE_ROCKS=1)
    message(STATUS "  LongTermMemory: atomspace-rocks persistence enabled")
else()
    message(STATUS "  LongTermMemory: in-memory persistence fallback (atomspace-rocks not found)")
endif()

# Ensure version vars exist when built as subdir without parent include
if(NOT DEFINED COGZERO_VERSION)
    set(COGZERO_VERSION "${PROJECT_VERSION}")
endif()
if(NOT DEFINED COGZERO_SOVERSION)
    set(COGZERO_SOVERSION 0)
endif()

set_target_properties(agentzero-memory PROPERTIES
    VERSION ${COGZERO_VERSION}
    SOVERSION ${COGZERO_SOVERSION}
    OUTPUT_NAME "agentzero-memory"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
# Shim builds are for tests only — do not install as release artifacts.
if(NOT AGENTZERO_MEMORY_USE_SHIM)
    install(TARGETS agentzero-memory
        EXPORT CogZeroTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(NOT AGENTZERO_MEMORY_USE_SHIM)
# Public headers for OpenCog/Development consumers
set(_agentzero_memory_PUBLIC_HEADERS
        EpisodicMemory.h
        EpisodicMemoryMem.h
        ExperienceManager.h
        LongTermMemory.h
        MemoryTypes.h
        WorkingMemory.h
        ContextManager.h
)
foreach(_h ${_agentzero_memory_PUBLIC_HEADERS})
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
if(BUILD_TESTING OR AGENTZERO_MEMORY_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-memory configured (shim=${AGENTZERO_MEMORY_USE_SHIM}, attention=${AGENTZERO_MEMORY_HAVE_ATTENTION}, rocks=${AGENTZERO_MEMORY_HAVE_ROCKS})")
