# agentzero-communication/tests/CMakeLists.txt
# Phase 6 communication tests — run under CTest

set(AGENTZERO_COMMUNICATION_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_language_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_dialogue_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_agent_comms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_human_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_communication_pipeline.cpp
)

add_executable(agentzero_communication_tests ${AGENTZERO_COMMUNICATION_TEST_SOURCES})

target_link_libraries(agentzero_communication_tests PRIVATE agentzero-communication Threads::Threads)

target_include_directories(agentzero_communication_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_communication_tests COMMAND agentzero_communication_tests)
set_tests_properties(agentzero_communication_tests PROPERTIES LABELS "unit;agentzero-communication")

add_test(NAME agentzero_communication_language COMMAND agentzero_communication_tests --filter LanguageProcessor)
set_tests_properties(agentzero_communication_language PROPERTIES LABELS "unit;agentzero-communication")

add_test(NAME agentzero_communication_dialogue COMMAND agentzero_communication_tests --filter DialogueManager)
set_tests_properties(agentzero_communication_dialogue PROPERTIES LABELS "unit;agentzero-communication")

add_test(NAME agentzero_communication_comms COMMAND agentzero_communication_tests --filter AgentComms)
set_tests_properties(agentzero_communication_comms PROPERTIES LABELS "unit;agentzero-communication")

add_test(NAME agentzero_communication_human COMMAND agentzero_communication_tests --filter HumanInterface)
set_tests_properties(agentzero_communication_human PROPERTIES LABELS "unit;agentzero-communication")

add_test(NAME agentzero_communication_pipeline COMMAND agentzero_communication_tests --filter Pipeline)
set_tests_properties(agentzero_communication_pipeline PROPERTIES LABELS "integration;agentzero-communication")
