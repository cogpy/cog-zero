# agentzero-learning/CMakeLists.txt
# Agent-Zero Learning & Adaptation — Phase 5
# Part of the AGENT-ZERO-GENESIS project

cmake_minimum_required(VERSION 3.16)

project(AgentZeroLearning
    VERSION 0.1.0
    DESCRIPTION "Agent-Zero Learning & Adaptation Module"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(COGUTIL cogutil)
if(NOT COGUTIL_FOUND)
    message(WARNING "cogutil not found — agentzero-learning will not be built")
    return()
endif()

pkg_check_modules(ATOMSPACE atomspace)
if(NOT ATOMSPACE_FOUND)
    message(WARNING "atomspace not found — agentzero-learning will not be built")
    return()
endif()

pkg_check_modules(MOSES moses)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

link_directories(
    ${COGUTIL_LIBRARY_DIRS}
    ${ATOMSPACE_LIBRARY_DIRS}
)

set(LEARNING_SOURCES
    ${CMAKE_SOURCE_DIR}/src/EpisodicMemory.cpp
    ${CMAKE_SOURCE_DIR}/src/ExperienceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/SkillAcquisition.cpp
    ${CMAKE_SOURCE_DIR}/src/PolicyOptimizer.cpp
    ${CMAKE_SOURCE_DIR}/src/MetaLearning.cpp
    ${CMAKE_SOURCE_DIR}/src/LearningUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/ASMOSESIntegrator.cpp
    ${CMAKE_SOURCE_DIR}/src/AtomSpaceEvolver.cpp
)

add_library(agentzero-learning SHARED ${LEARNING_SOURCES})

target_include_directories(agentzero-learning PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(agentzero-learning PUBLIC
    $<$<BOOL:${MOSES_FOUND}>:HAVE_MOSES>
)

target_link_libraries(agentzero-learning
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
)

if(MOSES_FOUND)
    target_link_libraries(agentzero-learning ${MOSES_LIBRARIES})
endif()

set_target_properties(agentzero-learning PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

include(GNUInstallDirs)
install(TARGETS agentzero-learning
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "agentzero-learning configured (MOSES: ${MOSES_FOUND})")
