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

# Resolve the repository root early so VERSION can be read before project().
get_filename_component(COG0_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${COG0_REPO_ROOT}/cmake")
include(CogZeroVersion)

project(cog0
    VERSION ${COGZERO_VERSION}
    DESCRIPTION "cog0 — Standalone Agent-Zero C++ Application"
    LANGUAGES CXX
)

option(COGZERO_ENABLE_CPACK "Enable CPack packaging targets" ON)

include(GNUInstallDirs)

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
# COG0_REPO_ROOT is resolved above (before project()) so VERSION can be read.
if(NOT DEFINED COG0_ROOT_INCLUDE_DIR)
    set(COG0_ROOT_INCLUDE_DIR "${COG0_REPO_ROOT}/include")
endif()

# Create <binary-dir>/cog0_headers/cog0/ so that the compiler finds
# #include "cog0/Foo.h" without conflicting with the cog0 executable target.
set(COG0_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/cog0_headers")
file(MAKE_DIRECTORY "${COG0_GENERATED_INCLUDE}/cog0")

# All standalone headers now live directly in include/ as clean names.
# (The former "(2)" duplicates have been consolidated into the clean files.)
# Prefer symlinks; COPY_ON_ERROR covers Windows (no symlink privilege) and
# filesystems that reject symlinks.
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
    # Phase 14: gRPC agent interface + TLS
    GrpcAgentServer
    GrpcAgentClient
    AgentServiceJson
    TlsSocket
)
    file(CREATE_LINK
        "${COG0_ROOT_INCLUDE_DIR}/${_h}.h"
        "${COG0_GENERATED_INCLUDE}/cog0/${_h}.h"
        SYMBOLIC
        COPY_ON_ERROR
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
    # Phase 14: gRPC agent interface (JSON fallback) + TLS
    ${COG0_SRC_DIR}/GrpcAgentServer.cpp
    ${COG0_SRC_DIR}/GrpcAgentClient.cpp
    ${COG0_SRC_DIR}/TlsSocket.cpp
)

add_library(cog0lib STATIC ${COG0_SOURCES})

target_include_directories(cog0lib
    PUBLIC
        $<BUILD_INTERFACE:${COG0_GENERATED_INCLUDE}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(cog0lib PUBLIC Threads::Threads)

# Winsock for MonitoringServer / GrpcAgent* on Windows
if(WIN32)
    target_link_libraries(cog0lib PUBLIC ws2_32)
endif()

# -----------------------------------------------------------------------
# Optional OpenSSL for MonitoringServer TLS (Phase 14 Feature 2.5)
option(USE_TLS "Enable TLS support for MonitoringServer (requires OpenSSL)" ON)

set(COG0_BUILT_WITH_OPENSSL FALSE)
set(COG0_PC_REQUIRES "")
set(COG0_PC_LIBS_PRIVATE "")
if(USE_TLS)
    find_package(OpenSSL QUIET)
    # FindOpenSSL sets OPENSSL_FOUND; some CMake versions also set OpenSSL_FOUND.
    if(OPENSSL_FOUND OR OpenSSL_FOUND)
        message(STATUS "  OpenSSL: ${OPENSSL_VERSION} (TLS enabled)")
        target_compile_definitions(cog0lib PUBLIC COG0_HAVE_OPENSSL=1)
        # PUBLIC so installed Cog0::cog0lib exports the dependency for static linking.
        target_link_libraries(cog0lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        set(COG0_BUILT_WITH_OPENSSL TRUE)
        set(COG0_PC_REQUIRES "openssl")
        set(COG0_PC_LIBS_PRIVATE "-lssl -lcrypto")
    else()
        message(STATUS "  OpenSSL: not found (TLS disabled; install libssl-dev to enable)")
    endif()
else()
    message(STATUS "  OpenSSL/TLS: disabled by USE_TLS=OFF")
endif()

# -----------------------------------------------------------------------
# Optional real gRPC (Phase 14 Feature 2.2).  Default is the built-in
# length-prefixed JSON transport which needs no external deps.
option(USE_GRPC "Link against real gRPC/protobuf when available" OFF)

if(USE_GRPC)
    find_package(Protobuf QUIET)
    find_package(gRPC CONFIG QUIET)
    if(gRPC_FOUND AND Protobuf_FOUND)
        message(STATUS "  gRPC: found (real gRPC transport enabled)")
        target_compile_definitions(cog0lib PUBLIC COG0_HAVE_GRPC=1)
        # Real gRPC codegen can be wired here when protoc plugins are present.
        # The JSON fallback remains the default runtime path until then.
    else()
        message(STATUS "  gRPC: not found (using JSON AgentService fallback)")
    endif()
else()
    message(STATUS "  gRPC: disabled (JSON AgentService fallback; set -DUSE_GRPC=ON to probe)")
endif()

# Expose the project version as a preprocessor macro
target_compile_definitions(cog0lib PUBLIC
    COG0_VERSION="${PROJECT_VERSION}"
)

set_target_properties(cog0lib PROPERTIES
    OUTPUT_NAME "cog0"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${COGZERO_SOVERSION}
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME cog0lib
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
# Install (components: Runtime, Development)

include(CMakePackageConfigHelpers)

# Public standalone headers installed under include/cog0/
set(COG0_PUBLIC_HEADERS
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
    WebSocketHandler
    DashboardAssets
    GrpcAgentServer
    GrpcAgentClient
    AgentServiceJson
    TlsSocket
)

set(_cog0_install_header_dir "${CMAKE_CURRENT_BINARY_DIR}/install_headers/cog0")
file(MAKE_DIRECTORY "${_cog0_install_header_dir}")
foreach(_h ${COG0_PUBLIC_HEADERS})
    if(EXISTS "${COG0_ROOT_INCLUDE_DIR}/${_h}.h")
        configure_file(
            "${COG0_ROOT_INCLUDE_DIR}/${_h}.h"
            "${_cog0_install_header_dir}/${_h}.h"
            COPYONLY
        )
    endif()
endforeach()

# Runtime binary is installed but NOT exported — Development-only installs must
# still provide a working find_package(Cog0) for Cog0::cog0lib.
install(TARGETS cog0
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT Runtime
)

install(TARGETS cog0lib
    EXPORT Cog0Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Development
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY "${_cog0_install_header_dir}/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cog0
    COMPONENT Development
    FILES_MATCHING PATTERN "*.h"
)

# LICENSE in every install tree / archive
if(EXISTS "${COG0_REPO_ROOT}/LICENSE")
    install(FILES "${COG0_REPO_ROOT}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT Runtime
    )
endif()

# Minimal README for runtime packages
if(EXISTS "${COG0_REPO_ROOT}/README.md")
    install(FILES "${COG0_REPO_ROOT}/README.md"
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT Runtime
    )
endif()

# pkg-config for the standalone SDK
configure_file(
    "${COG0_REPO_ROOT}/cmake/cog0.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cog0.pc"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cog0.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT Development
)

# CMake package config (find_package(Cog0))
configure_package_config_file(
    "${COG0_REPO_ROOT}/cmake/Cog0Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Cog0Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cog0
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/Cog0ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Cog0Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/Cog0ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cog0
    COMPONENT Development
)

install(EXPORT Cog0Targets
    FILE Cog0Targets.cmake
    NAMESPACE Cog0::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cog0
    COMPONENT Development
)

# -----------------------------------------------------------------------
# CPack (when this file is the top-level project)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND COGZERO_ENABLE_CPACK)
    set(COGZERO_CPACK_PACKAGE_NAME "cog0")
    set(COGZERO_CPACK_SUMMARY "cog0 — Standalone Agent-Zero C++ Application")
    set(COGZERO_REPO_ROOT "${COG0_REPO_ROOT}")
    include(CogZeroCPack)
endif()

# -----------------------------------------------------------------------
# Summary

message(STATUS "")
message(STATUS "cog0 Standalone Configuration:")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  SOVERSION:   ${COGZERO_SOVERSION}")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Install:     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Tests:       ${BUILD_TESTING}")
message(STATUS "  CPack:       ${COGZERO_ENABLE_CPACK}")
message(STATUS "  Dependencies: C++17 stdlib, Threads (no OpenCog required)")
message(STATUS "")
