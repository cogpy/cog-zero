# standalone/CMakeLists.txt
#
# cog0 — Standalone C++ Agent-Zero application
#
# Builds cog0 using ONLY its own minimal dependencies:
#   - C++17 standard library (required)
#   - Threads (std::thread / pthreads — required)
#
# No OpenCog (cogutil, atomspace, cogserver, etc.) dependencies are needed.
# Source files and headers live in the repository's flat src/ and include/
# directories.  The CMakeLists creates a "cog0/" symlink in the binary dir
# so that #include "cog0/Foo.h" resolves to include/Foo.h at build time.
#
# Build (from repo root via the parent cmake):
#   cmake -B build -DBUILD_OPENCOG_MODULES=OFF
#   cmake --build build --target cog0
#
# Stand-alone build:
#   cmake -B standalone/build -S standalone
#   cmake --build standalone/build

cmake_minimum_required(VERSION 3.14)

project(cog0
    VERSION 0.3.0
    DESCRIPTION "cog0 — Standalone Agent-Zero C++ Application"
    LANGUAGES CXX
)

# -----------------------------------------------------------------------
# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------
# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "cog0 build type: ${CMAKE_BUILD_TYPE}")

# -----------------------------------------------------------------------
# Compiler warnings
add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# -----------------------------------------------------------------------
# Threads — required (std::thread uses pthreads on Linux/macOS)
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------
# Include path fixup:
#   Source files use   #include "cog0/Foo.h"
#   Standalone headers live in the flat include/ directory.
#
#   Strategy: create a  <binary-dir>/cog0_headers/cog0/  directory and
#   populate it with symlinks pointing to <repo-root>/include/Foo.h.
#   Then add <binary-dir>/cog0_headers to the compiler include search path.
#
# Resolve the repository root regardless of whether the build is configured
# from the repo root (cmake -S .) or from this standalone directory
# (cmake -S standalone).  CMAKE_CURRENT_LIST_DIR is always this file's dir.
get_filename_component(COG0_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED COG0_ROOT_INCLUDE_DIR)
    set(COG0_ROOT_INCLUDE_DIR "${COG0_REPO_ROOT}/include")
endif()

# Create <binary-dir>/cog0_headers/cog0/ so that the compiler finds
# #include "cog0/Foo.h" without conflicting with the cog0 executable target.
set(COG0_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/cog0_headers")
file(MAKE_DIRECTORY "${COG0_GENERATED_INCLUDE}/cog0")

# All standalone headers now live directly in include/ as clean names.
# (The former "(2)" duplicates have been consolidated into the clean files.)
foreach(_h
    AtomStore
    Logger
    Agent
    cog0_capi
    RaftConsensus
    RaftLogStore
    ConflictResolver
    MonitoringServer
    TaskManager
    ReasoningEngine
    CognitiveLoop
    EpisodicMemory
    ActionExecutor
    ActionScheduler
    KnowledgeIntegrator
    PerceptualProcessor
    MultiModalSensor
    TextualSensor
    AttentionManager
    # Phase 14: WebSocket dashboard
    WebSocketHandler
    DashboardAssets
)
    file(CREATE_LINK
        "${COG0_ROOT_INCLUDE_DIR}/${_h}.h"
        "${COG0_GENERATED_INCLUDE}/cog0/${_h}.h"
        SYMBOLIC
    )
endforeach()

# -----------------------------------------------------------------------
# cog0 library (static) — all standalone subsystem sources

set(COG0_SRC_DIR "${COG0_REPO_ROOT}/src")

set(COG0_SOURCES
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
    # Phase 12: advanced coordination & observability
    ${COG0_SRC_DIR}/RaftConsensus.cpp
    ${COG0_SRC_DIR}/RaftLogStore.cpp
    ${COG0_SRC_DIR}/ConflictResolver.cpp
    ${COG0_SRC_DIR}/MonitoringServer.cpp
    # Phase 14: WebSocket monitoring dashboard
    ${COG0_SRC_DIR}/WebSocketHandler.cpp
    ${COG0_SRC_DIR}/DashboardAssets.cpp
)

add_library(cog0lib STATIC ${COG0_SOURCES})

target_include_directories(cog0lib
    PUBLIC
        "${COG0_GENERATED_INCLUDE}"
)

target_link_libraries(cog0lib PUBLIC Threads::Threads)

# Expose the project version as a preprocessor macro
target_compile_definitions(cog0lib PUBLIC
    COG0_VERSION="${PROJECT_VERSION}"
)

set_target_properties(cog0lib PROPERTIES
    OUTPUT_NAME "cog0"
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------
# cog0 executable

add_executable(cog0 "${COG0_REPO_ROOT}/main.cpp")
target_link_libraries(cog0 PRIVATE cog0lib)

# -----------------------------------------------------------------------
# Optional: GNU readline for tab-completion in the interactive REPL.

option(USE_READLINE "Use GNU readline for interactive tab-completion" ON)

if(USE_READLINE)
    find_library(READLINE_LIB NAMES readline)
    find_path(READLINE_INC NAMES readline/readline.h)
    if(READLINE_LIB AND READLINE_INC)
        message(STATUS "  readline: ${READLINE_LIB} (tab-completion enabled)")
        target_compile_definitions(cog0 PRIVATE COG0_HAVE_READLINE=1)
        target_include_directories(cog0 PRIVATE ${READLINE_INC})
        target_link_libraries(cog0 PRIVATE ${READLINE_LIB})
    else()
        message(STATUS "  readline: not found (install libreadline-dev to enable tab-completion)")
    endif()
else()
    message(STATUS "  readline: disabled by USE_READLINE=OFF")
endif()

# -----------------------------------------------------------------------
# Tests

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------
# Install

include(GNUInstallDirs)

install(TARGETS cog0
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS cog0lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# -----------------------------------------------------------------------
# Summary

message(STATUS "")
message(STATUS "cog0 Standalone Configuration:")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Install:     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Tests:       ${BUILD_TESTING}")
message(STATUS "  Dependencies: C++17 stdlib, Threads (no OpenCog required)")
message(STATUS "")
