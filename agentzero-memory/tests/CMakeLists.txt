# agentzero-memory/tests/CMakeLists.txt
# Phase 7 memory tests — run under CTest

set(AGENTZERO_MEMORY_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_episodic_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_working_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_long_term_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_context_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_pipeline.cpp
)

add_executable(agentzero_memory_tests ${AGENTZERO_MEMORY_TEST_SOURCES})

target_link_libraries(agentzero_memory_tests PRIVATE agentzero-memory Threads::Threads)

target_include_directories(agentzero_memory_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_memory_tests COMMAND agentzero_memory_tests)
set_tests_properties(agentzero_memory_tests PROPERTIES LABELS "unit;agentzero-memory")

add_test(NAME agentzero_memory_episodic COMMAND agentzero_memory_tests --filter EpisodicMemory)
set_tests_properties(agentzero_memory_episodic PROPERTIES LABELS "unit;agentzero-memory")

add_test(NAME agentzero_memory_working COMMAND agentzero_memory_tests --filter WorkingMemory)
set_tests_properties(agentzero_memory_working PROPERTIES LABELS "unit;agentzero-memory")

add_test(NAME agentzero_memory_longterm COMMAND agentzero_memory_tests --filter LongTermMemory)
set_tests_properties(agentzero_memory_longterm PROPERTIES LABELS "unit;agentzero-memory")

add_test(NAME agentzero_memory_context COMMAND agentzero_memory_tests --filter ContextManager)
set_tests_properties(agentzero_memory_context PROPERTIES LABELS "unit;agentzero-memory")

add_test(NAME agentzero_memory_pipeline COMMAND agentzero_memory_tests --filter MemoryPipeline)
set_tests_properties(agentzero_memory_pipeline PROPERTIES LABELS "unit;agentzero-memory")
