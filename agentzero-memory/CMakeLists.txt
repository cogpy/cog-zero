# agentzero-memory/CMakeLists.txt
# Agent-Zero Memory & Context Management — Phase 7
# Part of the AGENT-ZERO-GENESIS project (AZ-CONTEXT-001)

cmake_minimum_required(VERSION 3.16)

project(AgentZeroMemory
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Memory & Context Management Module"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-memory will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-memory will not be built")
    return()
endif()

pkg_check_modules(ATTENTION attention)
pkg_check_modules(ATOMSPACE_ROCKS atomspace-rocks)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(AGENTZERO_MEMORY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/LongTermMemory.cpp
    ${CMAKE_SOURCE_DIR}/src/EpisodicMemory.cpp
    ${CMAKE_SOURCE_DIR}/src/WorkingMemory.cpp
    ${CMAKE_SOURCE_DIR}/src/ContextManager.cpp
)

add_library(agentzero-memory SHARED ${AGENTZERO_MEMORY_SOURCES})

target_include_directories(agentzero-memory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-memory
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
)

if(ATTENTION_FOUND)
    target_link_libraries(agentzero-memory ${ATTENTION_LIBRARIES})
    target_compile_definitions(agentzero-memory PRIVATE HAVE_ATTENTION)
endif()

if(ATOMSPACE_ROCKS_FOUND)
    target_link_libraries(agentzero-memory ${ATOMSPACE_ROCKS_LIBRARIES})
    target_compile_definitions(agentzero-memory PRIVATE HAVE_ATOMSPACE_ROCKS)
endif()

set_target_properties(agentzero-memory PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "agentzero-memory"
)

include(GNUInstallDirs)
install(TARGETS agentzero-memory
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-memory configured (attention: ${ATTENTION_FOUND})")
