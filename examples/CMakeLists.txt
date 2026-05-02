# Agent-Zero Examples CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Demo for PerceptualProcessor
add_executable(perceptual_processor_demo PerceptualProcessorDemo.cpp)

# Link libraries
target_link_libraries(perceptual_processor_demo
    agentzero-perception
    ${COGUTIL_LIBRARIES}
    ${ATOMSPACE_LIBRARIES}
    ${Boost_LIBRARIES}
)

# Add additional required libraries
target_link_libraries(perceptual_processor_demo
    /usr/local/lib/opencog/libatombase.so
    /usr/local/lib/opencog/libatom_types.so
    /usr/local/lib/opencog/libvalue.so
    /usr/local/lib/opencog/libtruthvalue.so
)

# Include directories  
target_include_directories(perceptual_processor_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../agentzero-perception/include
    ${COGUTIL_INCLUDE_DIRS}
    ${ATOMSPACE_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Installation
install(TARGETS perceptual_processor_demo
    RUNTIME DESTINATION bin
)

message(STATUS "Agent-Zero Examples configured - PerceptualProcessor demo available")