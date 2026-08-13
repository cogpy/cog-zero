# agentzero-perception/CMakeLists.txt
# Agent-Zero Perception & Sensory Processing — Phase 2
# Part of the AGENT-ZERO-GENESIS project
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AgentZeroPerception
        VERSION 0.1.0
        DESCRIPTION "Agent-Zero Perception & Sensory Processing Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_PERCEPTION_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_PERCEPTION_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_PERCEPTION_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_PERCEPTION_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_PERCEPTION_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_PERCEPTION_HAVE_OPENCOG TRUE)
    endif()
endif()

# Optional perception-related packages (real OpenCog only)
set(AGENTZERO_PERCEPTION_HAVE_SENSORY FALSE)
set(AGENTZERO_PERCEPTION_HAVE_VISION FALSE)
set(AGENTZERO_PERCEPTION_HAVE_ATTENTION FALSE)

if(AGENTZERO_PERCEPTION_HAVE_OPENCOG AND PkgConfig_FOUND)
    pkg_check_modules(SENSORY sensory)
    pkg_check_modules(VISION vision)
    pkg_check_modules(ATTENTION attention)
    if(SENSORY_FOUND)
        set(AGENTZERO_PERCEPTION_HAVE_SENSORY TRUE)
    endif()
    if(VISION_FOUND)
        set(AGENTZERO_PERCEPTION_HAVE_VISION TRUE)
    endif()
    if(ATTENTION_FOUND)
        set(AGENTZERO_PERCEPTION_HAVE_ATTENTION TRUE)
    endif()
endif()

if(NOT AGENTZERO_PERCEPTION_HAVE_OPENCOG)
    message(STATUS "agentzero-perception: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_PERCEPTION_USE_SHIM TRUE)
endif()

set(AGENTZERO_PERCEPTION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MultiModalSensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PerceptualProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AttentionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TextualSensor.cpp
)

add_library(agentzero-perception SHARED ${AGENTZERO_PERCEPTION_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_PERCEPTION_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-perception
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-perception PUBLIC AGENTZERO_PERCEPTION_SHIM=1)
else()
    target_include_directories(agentzero-perception
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-perception
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-perception
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-perception
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-perception PUBLIC Threads::Threads)

if(AGENTZERO_PERCEPTION_HAVE_SENSORY)
    target_link_libraries(agentzero-perception PUBLIC ${SENSORY_LIBRARIES})
    target_compile_definitions(agentzero-perception PRIVATE HAVE_SENSORY=1)
endif()
if(AGENTZERO_PERCEPTION_HAVE_VISION)
    target_link_libraries(agentzero-perception PUBLIC ${VISION_LIBRARIES})
    target_compile_definitions(agentzero-perception PRIVATE HAVE_VISION=1)
endif()
if(AGENTZERO_PERCEPTION_HAVE_ATTENTION)
    target_link_libraries(agentzero-perception PUBLIC ${ATTENTION_LIBRARIES})
    target_compile_definitions(agentzero-perception PRIVATE HAVE_ATTENTION=1)
endif()

set_target_properties(agentzero-perception PROPERTIES
    VERSION 0.1.0
    SOVERSION 1
    OUTPUT_NAME "agentzero-perception"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
install(TARGETS agentzero-perception
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING OR AGENTZERO_PERCEPTION_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-perception configured (shim=${AGENTZERO_PERCEPTION_USE_SHIM})")
