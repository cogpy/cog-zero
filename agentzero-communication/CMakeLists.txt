# agentzero-communication/CMakeLists.txt
# Agent-Zero Communication & NLP — Phase 6
# Part of the AGENT-ZERO-GENESIS project (AZ-COMM-001, AZ-NLP-001/002, AZ-HUMAN-001)
#
# Builds against real OpenCog (cogutil/atomspace) when available.
# Otherwise uses the in-tree AtomSpace shim from agentzero-core so the
# module and its tests can still be built and exercised under CTest.
# Optional lg-atomese package enables HAVE_LG_ATOMESE for Link Grammar NLP.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(_az_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    list(APPEND CMAKE_MODULE_PATH "${_az_repo_root}/cmake")
    include(CogZeroVersion)
    project(AgentZeroCommunication
        VERSION ${COGZERO_VERSION}
        DESCRIPTION "Agent-Zero Communication & NLP Module"
        LANGUAGES CXX
    )
    set(AGENTZERO_COMMUNICATION_STANDALONE_BUILD TRUE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    get_filename_component(COGZERO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
else()
    set(AGENTZERO_COMMUNICATION_STANDALONE_BUILD FALSE)
    set(COGZERO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

option(AGENTZERO_COMMUNICATION_USE_SHIM "Force use of in-tree AtomSpace shim" OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(AGENTZERO_COMMUNICATION_HAVE_OPENCOG FALSE)

if(NOT AGENTZERO_COMMUNICATION_USE_SHIM AND PkgConfig_FOUND)
    pkg_check_modules(COGUTIL cogutil)
    pkg_check_modules(ATOMSPACE atomspace)
    if(COGUTIL_FOUND AND ATOMSPACE_FOUND)
        set(AGENTZERO_COMMUNICATION_HAVE_OPENCOG TRUE)
    endif()
endif()

set(AGENTZERO_COMMUNICATION_HAVE_LG_ATOMESE FALSE)
if(AGENTZERO_COMMUNICATION_HAVE_OPENCOG AND PkgConfig_FOUND)
    pkg_check_modules(LG_ATOMESE lg-atomese)
    if(LG_ATOMESE_FOUND)
        set(AGENTZERO_COMMUNICATION_HAVE_LG_ATOMESE TRUE)
    endif()
endif()

if(NOT AGENTZERO_COMMUNICATION_HAVE_OPENCOG)
    message(STATUS "agentzero-communication: OpenCog not found — building with in-tree AtomSpace shim")
    set(AGENTZERO_COMMUNICATION_USE_SHIM TRUE)
endif()

set(AGENTZERO_COMMUNICATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LanguageProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DialogueManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AgentComms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HumanInterface.cpp
)

add_library(agentzero-communication SHARED ${AGENTZERO_COMMUNICATION_SOURCES})

# Shim must precede monorepo include/ so partial stubs do not win.
if(AGENTZERO_COMMUNICATION_USE_SHIM)
    set(AGENTZERO_SHIM_DIR "${COGZERO_ROOT}/agentzero-core/shim")
    target_include_directories(agentzero-communication
        PUBLIC
            $<BUILD_INTERFACE:${AGENTZERO_SHIM_DIR}>
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(agentzero-communication PUBLIC AGENTZERO_COMMUNICATION_SHIM=1)
else()
    target_include_directories(agentzero-communication
        PUBLIC
            $<BUILD_INTERFACE:${COGZERO_ROOT}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_include_directories(agentzero-communication
        PUBLIC
            ${COGUTIL_INCLUDE_DIRS}
            ${ATOMSPACE_INCLUDE_DIRS}
    )
    target_link_directories(agentzero-communication
        PUBLIC
            ${COGUTIL_LIBRARY_DIRS}
            ${ATOMSPACE_LIBRARY_DIRS}
    )
    target_link_libraries(agentzero-communication
        PUBLIC
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
    )
endif()

target_link_libraries(agentzero-communication PUBLIC Threads::Threads)

if(AGENTZERO_COMMUNICATION_HAVE_LG_ATOMESE)
    target_include_directories(agentzero-communication PUBLIC ${LG_ATOMESE_INCLUDE_DIRS})
    target_link_directories(agentzero-communication PUBLIC ${LG_ATOMESE_LIBRARY_DIRS})
    target_link_libraries(agentzero-communication PUBLIC ${LG_ATOMESE_LIBRARIES})
    target_compile_definitions(agentzero-communication PRIVATE HAVE_LG_ATOMESE=1)
    message(STATUS "  LanguageProcessor: lg-atomese enabled")
else()
    message(STATUS "  LanguageProcessor: basic NLP fallback (lg-atomese not found)")
endif()

# Ensure version vars exist when built as subdir without parent include
if(NOT DEFINED COGZERO_VERSION)
    set(COGZERO_VERSION "${PROJECT_VERSION}")
endif()
if(NOT DEFINED COGZERO_SOVERSION)
    set(COGZERO_SOVERSION 0)
endif()

set_target_properties(agentzero-communication PROPERTIES
    VERSION ${COGZERO_VERSION}
    SOVERSION ${COGZERO_SOVERSION}
    OUTPUT_NAME "agentzero-communication"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
# Shim builds are for tests only — do not install as release artifacts.
if(NOT AGENTZERO_COMMUNICATION_USE_SHIM)
    install(TARGETS agentzero-communication
        EXPORT CogZeroTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OpenCog
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(NOT AGENTZERO_COMMUNICATION_USE_SHIM)
# Public headers for OpenCog/Development consumers
set(_agentzero_communication_PUBLIC_HEADERS
        AgentComms.h
        CommTypes.h
        CommunicationTypes.h
        CommunicationUtils.h
        ConversationState.h
        DialogueManager.h
        HumanInterface.h
        LanguageProcessor.h
        MessageHandler.h
        MessageRouter.h
        MessageSerializer.h
        ProtocolManager.h
)
foreach(_h ${_agentzero_communication_PUBLIC_HEADERS})
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
if(BUILD_TESTING OR AGENTZERO_COMMUNICATION_STANDALONE_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "agentzero-communication configured (shim=${AGENTZERO_COMMUNICATION_USE_SHIM}, lg-atomese=${AGENTZERO_COMMUNICATION_HAVE_LG_ATOMESE})")
