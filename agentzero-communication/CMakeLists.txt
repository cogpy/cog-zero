# agentzero-communication/CMakeLists.txt
# Agent-Zero Communication & NLP — Phase 6
# Part of the AGENT-ZERO-GENESIS project (AZ-COMM-001, AZ-NLP-001/002, AZ-HUMAN-001)

cmake_minimum_required(VERSION 3.16)

project(AgentZeroCommunication
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Communication & NLP Module"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-communication will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-communication will not be built")
    return()
endif()

pkg_check_modules(LG_ATOMESE lg-atomese)

find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(AGENTZERO_COMMUNICATION_SOURCES
    ${CMAKE_SOURCE_DIR}/src/AgentComms.cpp
    ${CMAKE_SOURCE_DIR}/src/MessageRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/ProtocolManager.cpp
    ${CMAKE_SOURCE_DIR}/src/MessageSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/DialogueManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ConversationState.cpp
    ${CMAKE_SOURCE_DIR}/src/LanguageProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/MessageHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/HumanInterface.cpp
    ${CMAKE_SOURCE_DIR}/src/CommunicationUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/MultiAgentCoordinator.cpp
)

add_library(agentzero-communication SHARED ${AGENTZERO_COMMUNICATION_SOURCES})

target_include_directories(agentzero-communication PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-communication
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    Threads::Threads
)

if(LG_ATOMESE_FOUND)
    target_link_libraries(agentzero-communication ${LG_ATOMESE_LIBRARIES})
    target_compile_definitions(agentzero-communication PRIVATE HAVE_LG_ATOMESE)
    message(STATUS "  agentzero-communication: lg-atomese NLP features enabled")
else()
    message(STATUS "  agentzero-communication: basic NLP only (lg-atomese not found)")
endif()

set_target_properties(agentzero-communication PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-communication
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-communication configured")
