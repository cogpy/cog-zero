cmake_minimum_required(VERSION 3.16)
project(cog0-bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the cog0 static library from the standalone build
set(COG0_LIB "${CMAKE_SOURCE_DIR}/../standalone/build/libcog0.a")
set(COG0_INC "${CMAKE_SOURCE_DIR}/../standalone/build/cog0_headers")
set(COG0_ROOT_INC "${CMAKE_SOURCE_DIR}/../include")

add_library(cog0-bridge SHARED cog0_atomspace_bridge.cpp)

target_include_directories(cog0-bridge PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${COG0_INC}
    ${COG0_ROOT_INC}
)

target_link_libraries(cog0-bridge PRIVATE
    ${COG0_LIB}
    Threads::Threads
)

find_package(Threads REQUIRED)

set_target_properties(cog0-bridge PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

install(TARGETS cog0-bridge LIBRARY DESTINATION lib)
install(FILES cog0_atomspace_bridge.h DESTINATION include)
