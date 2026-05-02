# Agent-Zero Learning Examples

# Include directories
include_directories(
# Comprehensive Examples for Agent-Zero Learning Module
# Combines MOSES Policy Optimization with Advanced Experience Management

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(COGUTIL REQUIRED cogutil)
pkg_check_modules(ATOMSPACE REQUIRED atomspace)

# Optional MOSES dependencies
pkg_check_modules(MOSES moses)
pkg_check_modules(ASMOSES asmoses)

# Policy optimization example (requires MOSES)
if(MOSES_FOUND OR ASMOSES_FOUND)
    add_executable(policy_optimization_example
        policy_optimization_example.cpp
    )
    
    target_link_libraries(policy_optimization_example
        agentzero-learning
        ${COGUTIL_LIBRARIES}
        ${ATOMSPACE_LIBRARIES}
    )
    
    if(MOSES_LIBRARIES)
        target_link_libraries(policy_optimization_example ${MOSES_LIBRARIES})
    endif()
    
    if(ASMOSES_LIBRARIES)
        target_link_libraries(policy_optimization_example ${ASMOSES_LIBRARIES})
    endif()
    
    target_include_directories(policy_optimization_example PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${COGUTIL_INCLUDE_DIRS}
        ${ATOMSPACE_INCLUDE_DIRS}
    )
    
    message(STATUS "Policy optimization example configured")
else()
    message(STATUS "Skipping policy optimization example - MOSES not found")
endif()

# Advanced experience management example
add_executable(experience_management_example
    experience_management_example.cpp
)

target_link_libraries(experience_management_example
    agentzero-learning
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
)

target_include_directories(experience_management_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
)

# Example programs
add_executable(asmoses_evolution_example
    asmoses_evolution_example.cpp
)

target_link_libraries(asmoses_evolution_example
    agentzero-learning
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    pthread
)

# Install examples
install(TARGETS asmoses_evolution_example
    RUNTIME DESTINATION bin/examples
)
# Experience manager demo (from main branch)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ExperienceManagerDemo.cpp)
    add_executable(experience_manager_demo
        ExperienceManagerDemo.cpp
    )
    
    target_link_libraries(experience_manager_demo
        agentzero-learning
        ${COGUTIL_LIBRARIES}
        ${ATOMSPACE_LIBRARIES}
        ${Boost_LIBRARIES}
    )
    
    target_include_directories(experience_manager_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${COGUTIL_INCLUDE_DIRS}
        ${ATOMSPACE_INCLUDE_DIRS}
    )
    
    set_target_properties(experience_manager_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    message(STATUS "Experience manager demo configured")
endif()

message(STATUS "Comprehensive Agent-Zero Learning examples configured")
# Examples for Agent-Zero Learning Module

cmake_minimum_required(VERSION 3.16)

# Create experience manager demo
add_executable(experience_manager_demo
    ExperienceManagerDemo.cpp
)

target_link_libraries(experience_manager_demo
    agentzero-learning
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    ${Boost_LIBRARIES}
)

# Set output directory
set_target_properties(experience_manager_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

message(STATUS "ExperienceManager examples configured")
