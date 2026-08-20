# agentzero-tools/CMakeLists.txt
# Agent-Zero External Tool Integration — Phase 8
# Part of the AGENT-ZERO-GENESIS project
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.
# Optional external-tools / ros-behavior-scripting packages enable HAVE_* flags.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(_az_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    list(APPEND CMAKE_MODULE_PATH "${_az_repo_root}/cmake")
    include(CogZeroVersion)
    project(AgentZeroTools
        VERSION ${COGZERO_VERSION}
        DESCRIPTION "Agent-Zero External Tool Integration Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_TOOLS_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_TOOLS_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_TOOLS_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_TOOLS_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_TOOLS_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_TOOLS_HAVE_OPENCOG TRUE)
    endif()
endif()

set(AGENTZERO_TOOLS_HAVE_EXTERNAL_TOOLS FALSE)
set(AGENTZERO_TOOLS_HAVE_ROS_BEHAVIOR FALSE)

if(AGENTZERO_TOOLS_HAVE_OPENCOG AND PkgConfig_FOUND)
    pkg_check_modules(EXTERNAL_TOOLS external-tools)
    pkg_check_modules(ROS_BEHAVIOR ros-behavior-scripting)
    if(EXTERNAL_TOOLS_FOUND)
        set(AGENTZERO_TOOLS_HAVE_EXTERNAL_TOOLS TRUE)
    endif()
    if(ROS_BEHAVIOR_FOUND)
        set(AGENTZERO_TOOLS_HAVE_ROS_BEHAVIOR TRUE)
    endif()
endif()

if(NOT AGENTZERO_TOOLS_HAVE_OPENCOG)
    message(STATUS "agentzero-tools: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_TOOLS_USE_SHIM TRUE)
endif()

set(AGENTZERO_TOOLS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ToolRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ToolWrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ToolExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RestApiAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RosBehaviorBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CapabilityComposer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceManager.cpp
)

add_library(agentzero-tools SHARED ${AGENTZERO_TOOLS_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_TOOLS_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-tools
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-tools PUBLIC AGENTZERO_TOOLS_SHIM=1)
else()
    target_include_directories(agentzero-tools
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-tools
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-tools
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-tools
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-tools PUBLIC Threads::Threads)

if(AGENTZERO_TOOLS_HAVE_EXTERNAL_TOOLS)
    target_include_directories(agentzero-tools PUBLIC ${EXTERNAL_TOOLS_INCLUDE_DIRS})
    target_link_directories(agentzero-tools PUBLIC ${EXTERNAL_TOOLS_LIBRARY_DIRS})
    target_link_libraries(agentzero-tools PUBLIC ${EXTERNAL_TOOLS_LIBRARIES})
    target_compile_definitions(agentzero-tools PRIVATE HAVE_EXTERNAL_TOOLS=1)
    message(STATUS "  ToolRegistry: external-tools package enabled")
else()
    message(STATUS "  ToolRegistry: built-in discovery fallback (external-tools not found)")
endif()

if(AGENTZERO_TOOLS_HAVE_ROS_BEHAVIOR)
    target_include_directories(agentzero-tools PUBLIC ${ROS_BEHAVIOR_INCLUDE_DIRS})
    target_link_directories(agentzero-tools PUBLIC ${ROS_BEHAVIOR_LIBRARY_DIRS})
    target_link_libraries(agentzero-tools PUBLIC ${ROS_BEHAVIOR_LIBRARIES})
    target_compile_definitions(agentzero-tools PRIVATE HAVE_ROS_BEHAVIOR_SCRIPTING=1)
    message(STATUS "  RosBehaviorBridge: ros-behavior-scripting package enabled")
else()
    message(STATUS "  RosBehaviorBridge: simulation-mode fallback (ros-behavior-scripting not found)")
endif()

# Ensure version vars exist when built as subdir without parent include
if(NOT DEFINED COGZERO_VERSION)
    set(COGZERO_VERSION "${PROJECT_VERSION}")
endif()
if(NOT DEFINED COGZERO_SOVERSION)
    set(COGZERO_SOVERSION 0)
endif()

set_target_properties(agentzero-tools PROPERTIES
    VERSION ${COGZERO_VERSION}
    SOVERSION ${COGZERO_SOVERSION}
    OUTPUT_NAME "agentzero-tools"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
# Shim builds are for tests only — do not install as release artifacts.
if(NOT AGENTZERO_TOOLS_USE_SHIM)
    install(TARGETS agentzero-tools
        EXPORT CogZeroTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(NOT AGENTZERO_TOOLS_USE_SHIM)
# Public headers for OpenCog/Development consumers
set(_agentzero_tools_PUBLIC_HEADERS
        ToolExecutor.h
        ToolRegistry.h
        ToolWrapper.h
        CapabilityComposer.h
        RestApiAdapter.h
        RosBehaviorBridge.h
        ResourceManager.h
)
foreach(_h ${_agentzero_tools_PUBLIC_HEADERS})
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
if(BUILD_TESTING OR AGENTZERO_TOOLS_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-tools configured (shim=${AGENTZERO_TOOLS_USE_SHIM}, external-tools=${AGENTZERO_TOOLS_HAVE_EXTERNAL_TOOLS}, ros=${AGENTZERO_TOOLS_HAVE_ROS_BEHAVIOR})")
