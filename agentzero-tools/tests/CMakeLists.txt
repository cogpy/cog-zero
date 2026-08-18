# agentzero-tools/tests/CMakeLists.txt
# Phase 8 external tool integration tests — run under CTest

set(AGENTZERO_TOOLS_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tool_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tool_executor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rest_api_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ros_behavior_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tools_pipeline.cpp
)

add_executable(agentzero_tools_tests ${AGENTZERO_TOOLS_TEST_SOURCES})

target_link_libraries(agentzero_tools_tests PRIVATE agentzero-tools Threads::Threads)

target_include_directories(agentzero_tools_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_tools_tests COMMAND agentzero_tools_tests)
set_tests_properties(agentzero_tools_tests PROPERTIES LABELS "unit;agentzero-tools")

add_test(NAME agentzero_tools_registry COMMAND agentzero_tools_tests --filter ToolRegistry)
set_tests_properties(agentzero_tools_registry PROPERTIES LABELS "unit;agentzero-tools")

add_test(NAME agentzero_tools_executor COMMAND agentzero_tools_tests --filter ToolExecutor)
set_tests_properties(agentzero_tools_executor PROPERTIES LABELS "unit;agentzero-tools")

add_test(NAME agentzero_tools_rest COMMAND agentzero_tools_tests --filter RestApi)
set_tests_properties(agentzero_tools_rest PROPERTIES LABELS "unit;agentzero-tools")

add_test(NAME agentzero_tools_ros COMMAND agentzero_tools_tests --filter RosBehavior)
set_tests_properties(agentzero_tools_ros PROPERTIES LABELS "unit;agentzero-tools")

add_test(NAME agentzero_tools_pipeline COMMAND agentzero_tools_tests --filter ToolsPipeline)
set_tests_properties(agentzero_tools_pipeline PROPERTIES LABELS "unit;agentzero-tools")
