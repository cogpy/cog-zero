# agentzero-core/tests/CMakeLists.txt
# Phase 1 foundation tests — run under CTest

set(AGENTZERO_CORE_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_agent_zero_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_loop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_task_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_knowledge_integrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_action_executor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_action_scheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_planner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_self_modification.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cogserver_module.cpp
)

add_executable(agentzero_core_tests ${AGENTZERO_CORE_TEST_SOURCES})

target_link_libraries(agentzero_core_tests PRIVATE agentzero-core Threads::Threads)

# Includes come from agentzero-core PUBLIC interface; keep test_runner local
target_include_directories(agentzero_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_core_tests COMMAND agentzero_core_tests)
set_tests_properties(agentzero_core_tests PROPERTIES LABELS "unit;agentzero-core")

add_test(NAME agentzero_core_atomspace_state COMMAND agentzero_core_tests --filter AtomSpaceState)
set_tests_properties(agentzero_core_atomspace_state PROPERTIES LABELS "unit;agentzero-core")

add_test(NAME agentzero_core_module_commands COMMAND agentzero_core_tests --filter CogServer)
set_tests_properties(agentzero_core_module_commands PROPERTIES LABELS "integration;agentzero-core")
