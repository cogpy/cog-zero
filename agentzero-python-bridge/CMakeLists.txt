# agentzero-python-bridge/CMakeLists.txt
# Phase 9 — Python interoperability bridge (C API + Cython/ctypes)
#
# Builds a shared C API library for the standalone cog0 agent and installs
# a pure-Python ctypes package. Optional Cython extension is built when
# Cython is available.
#
# No OpenCog packages are required.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    list(APPEND CMAKE_MODULE_PATH "${COGZERO_ROOT}/cmake")
    include(CogZeroVersion)
    project(AgentZeroPythonBridge
        VERSION ${COGZERO_VERSION}
        DESCRIPTION "Python Bindings for cog0 / Agent-Zero"
        LANGUAGES CXX
    )
    set(AGENTZERO_PYTHON_BRIDGE_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    set(AGENTZERO_PYTHON_BRIDGE_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
    if(NOT DEFINED COGZERO_VERSION)
        set(COGZERO_VERSION "${PROJECT_VERSION}")
    endif()
    if(NOT DEFINED COGZERO_SOVERSION)
        set(COGZERO_SOVERSION 0)
    endif()
endif()

option(AGENTZERO_PYTHON_BRIDGE_BUILD_CYTHON "Build Cython extension if Cython is available" ON)

find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development QUIET)

if(NOT Python3_FOUND)
    message(WARNING "Python3 not found — agentzero-python-bridge will not be built")
    return()
endif()

message(STATUS "agentzero-python-bridge: Python ${Python3_VERSION} (${Python3_EXECUTABLE})")

# -----------------------------------------------------------------------
# Shared C API library (wraps standalone cog0 Agent via cog0_capi)
# -----------------------------------------------------------------------

set(COG0_SRC_DIR "${COGZERO_ROOT}/src")
set(COG0_INC_DIR "${COGZERO_ROOT}/include")

# Generate cog0/ include tree (same approach as standalone/)
set(COG0_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/cog0_headers")
file(MAKE_DIRECTORY "${COG0_GENERATED_INCLUDE}/cog0")
foreach(_h
    AtomStore Logger Agent cog0_capi TaskManager ReasoningEngine CognitiveLoop
    EpisodicMemory ActionExecutor ActionScheduler KnowledgeIntegrator
    PerceptualProcessor MultiModalSensor TextualSensor AttentionManager
    RaftConsensus RaftLogStore ConflictResolver MonitoringServer
    WebSocketHandler DashboardAssets
)
    if(EXISTS "${COG0_INC_DIR}/${_h}.h")
        file(CREATE_LINK
            "${COG0_INC_DIR}/${_h}.h"
            "${COG0_GENERATED_INCLUDE}/cog0/${_h}.h"
            SYMBOLIC
        )
    endif()
endforeach()

# Build a self-contained shared library. We intentionally do NOT link the
# static cog0lib target here: cog0lib already compiles cog0_capi.cpp and
# linking both would produce duplicate symbols.
set(COG0_CAPI_SOURCES
    ${COG0_SRC_DIR}/AtomStore.cpp
    ${COG0_SRC_DIR}/TaskManager.cpp
    ${COG0_SRC_DIR}/ReasoningEngine.cpp
    ${COG0_SRC_DIR}/CognitiveLoop.cpp
    ${COG0_SRC_DIR}/Agent.cpp
    ${COG0_SRC_DIR}/cog0_capi.cpp
    ${COG0_SRC_DIR}/MultiModalSensor.cpp
    ${COG0_SRC_DIR}/PerceptualProcessor.cpp
    ${COG0_SRC_DIR}/AttentionManager.cpp
    ${COG0_SRC_DIR}/TextualSensor.cpp
    ${COG0_SRC_DIR}/ActionExecutor.cpp
    ${COG0_SRC_DIR}/ActionScheduler.cpp
    ${COG0_SRC_DIR}/KnowledgeIntegrator.cpp
    ${COG0_SRC_DIR}/EpisodicMemory.cpp
    ${COG0_SRC_DIR}/RaftConsensus.cpp
    ${COG0_SRC_DIR}/RaftLogStore.cpp
    ${COG0_SRC_DIR}/ConflictResolver.cpp
    ${COG0_SRC_DIR}/MonitoringServer.cpp
    ${COG0_SRC_DIR}/WebSocketHandler.cpp
    ${COG0_SRC_DIR}/DashboardAssets.cpp
)

add_library(cog0_capi SHARED ${COG0_CAPI_SOURCES})
target_include_directories(cog0_capi PUBLIC
    $<BUILD_INTERFACE:${COG0_GENERATED_INCLUDE}>
    $<BUILD_INTERFACE:${COG0_INC_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(cog0_capi PUBLIC Threads::Threads)
target_compile_definitions(cog0_capi PUBLIC COG0_VERSION="${COGZERO_VERSION}")

set_target_properties(cog0_capi PROPERTIES
    VERSION ${COGZERO_VERSION}
    SOVERSION ${COGZERO_SOVERSION}
    OUTPUT_NAME "cog0_capi"
    POSITION_INDEPENDENT_CODE ON
)

# Generate a version module so the pure-Python package stays in sync
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cog0/_version.py"
    "# Auto-generated from VERSION — do not edit\n__version__ = \"${COGZERO_VERSION}\"\n"
)

# Generate a small _libpath helper for ctypes discovery in the build tree
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cog0_libpath.py"
    CONTENT "# Auto-generated — path to built libcog0_capi\nLIB_DIR = r\"$<TARGET_FILE_DIR:cog0_capi>\"\nLIB_NAME = r\"$<TARGET_FILE_NAME:cog0_capi>\"\n"
)

include(GNUInstallDirs)
install(TARGETS cog0_capi
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Python
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Python
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT Python
)

set(PYTHON_PKG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cog0")
install(DIRECTORY ${PYTHON_PKG_DIR}/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages/cog0
    COMPONENT Python
    FILES_MATCHING PATTERN "*.py" PATTERN "*.pxd" PATTERN "*.pyx"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cog0/_version.py"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages/cog0
    COMPONENT Python
)

# -----------------------------------------------------------------------
# Optional Cython extension
# -----------------------------------------------------------------------

if(AGENTZERO_PYTHON_BRIDGE_BUILD_CYTHON)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import Cython; print(Cython.__version__)"
        RESULT_VARIABLE _cython_rc
        OUTPUT_VARIABLE _cython_ver
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_cython_rc EQUAL 0)
        message(STATUS "agentzero-python-bridge: Cython ${_cython_ver} found — building extension")
        set(_pyx "${CMAKE_CURRENT_SOURCE_DIR}/cog0/_cog0.pyx")
        set(_c_out "${CMAKE_CURRENT_BINARY_DIR}/_cog0.c")
        add_custom_command(
            OUTPUT ${_c_out}
            COMMAND ${Python3_EXECUTABLE} -m cython -3 -o ${_c_out} ${_pyx}
            DEPENDS ${_pyx}
            COMMENT "Cythonizing cog0._cog0"
        )
        Python3_add_library(_cog0 MODULE ${_c_out})
        target_link_libraries(_cog0 PRIVATE cog0_capi Python3::Python)
        target_include_directories(_cog0 PRIVATE
            ${COG0_GENERATED_INCLUDE}
            ${COG0_INC_DIR}
            ${Python3_INCLUDE_DIRS}
        )
        set_target_properties(_cog0 PROPERTIES
            PREFIX ""
            OUTPUT_NAME "_cog0"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cog0"
        )
    else()
        message(STATUS "agentzero-python-bridge: Cython not found — ctypes bindings only")
    endif()
endif()

# -----------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------

if(BUILD_TESTING OR AGENTZERO_PYTHON_BRIDGE_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-python-bridge configured (ctypes + optional Cython)")
