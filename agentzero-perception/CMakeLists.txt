# agentzero-perception/CMakeLists.txt
# Agent-Zero Perception & Sensory Processing — Phase 2
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroPerception
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Perception & Sensory Processing Module"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-perception will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-perception will not be built")
    return()
endif()

# Optional perception-related packages
pkg_check_modules(SENSORY sensory)
pkg_check_modules(VISION vision)
pkg_check_modules(ATTENTION attention)

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

set(AGENTZERO_PERCEPTION_SOURCES
    "${CMAKE_SOURCE_DIR}/src/PerceptualProcessor (2).cpp"
    "${CMAKE_SOURCE_DIR}/src/MultiModalSensor (2).cpp"
    "${CMAKE_SOURCE_DIR}/src/TextualSensor (2).cpp"
    "${CMAKE_SOURCE_DIR}/src/AttentionManager (2).cpp"
)

add_library(agentzero-perception SHARED ${AGENTZERO_PERCEPTION_SOURCES})

target_include_directories(agentzero-perception PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentzero-perception
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    Threads::Threads
)

if(SENSORY_FOUND)
    target_link_libraries(agentzero-perception ${SENSORY_LIBRARIES})
    target_compile_definitions(agentzero-perception PRIVATE HAVE_SENSORY)
endif()

if(ATTENTION_FOUND)
    target_link_libraries(agentzero-perception ${ATTENTION_LIBRARIES})
    target_compile_definitions(agentzero-perception PRIVATE HAVE_ATTENTION)
endif()

set_target_properties(agentzero-perception PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-perception
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-perception configured")
