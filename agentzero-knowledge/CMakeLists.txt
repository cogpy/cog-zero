# agentzero-knowledge/CMakeLists.txt
# Agent-Zero Knowledge Representation & Reasoning — Phase 3
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroKnowledge
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Knowledge Representation & Reasoning Module"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-knowledge will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-knowledge will not be built")
    return()
endif()

pkg_check_modules(PLN pln)
pkg_check_modules(URE ure)
pkg_check_modules(MINER miner)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(AGENTZERO_KNOWLEDGE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/KnowledgeBase.cpp
    ${CMAKE_SOURCE_DIR}/src/PatternDiscovery.cpp
    ${CMAKE_SOURCE_DIR}/src/ConceptFormation.cpp
    ${CMAKE_SOURCE_DIR}/src/PLNRuleLibrary.cpp
)

add_library(agentzero-knowledge SHARED ${AGENTZERO_KNOWLEDGE_SOURCES})

target_include_directories(agentzero-knowledge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-knowledge
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
)

if(PLN_FOUND)
    target_link_libraries(agentzero-knowledge ${PLN_LIBRARIES})
    target_compile_definitions(agentzero-knowledge PRIVATE HAVE_PLN)
endif()

if(URE_FOUND)
    target_link_libraries(agentzero-knowledge ${URE_LIBRARIES})
    target_compile_definitions(agentzero-knowledge PRIVATE HAVE_URE)
endif()

set_target_properties(agentzero-knowledge PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "agentzero-knowledge"
)

include(GNUInstallDirs)
install(TARGETS agentzero-knowledge
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-knowledge configured (PLN: ${PLN_FOUND}, URE: ${URE_FOUND})")
