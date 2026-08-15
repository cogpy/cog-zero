# agentzero-knowledge/tests/CMakeLists.txt
# Phase 3 knowledge tests — run under CTest

set(AGENTZERO_KNOWLEDGE_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_knowledge_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_pattern_discovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_concept_formation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_pln_rule_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_knowledge_pipeline.cpp
)

add_executable(agentzero_knowledge_tests ${AGENTZERO_KNOWLEDGE_TEST_SOURCES})

target_link_libraries(agentzero_knowledge_tests PRIVATE agentzero-knowledge Threads::Threads)

target_include_directories(agentzero_knowledge_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_knowledge_tests COMMAND agentzero_knowledge_tests)
set_tests_properties(agentzero_knowledge_tests PROPERTIES LABELS "unit;agentzero-knowledge")

add_test(NAME agentzero_knowledge_base COMMAND agentzero_knowledge_tests --filter KnowledgeBase)
set_tests_properties(agentzero_knowledge_base PROPERTIES LABELS "unit;agentzero-knowledge")

add_test(NAME agentzero_knowledge_patterns COMMAND agentzero_knowledge_tests --filter PatternDiscovery)
set_tests_properties(agentzero_knowledge_patterns PROPERTIES LABELS "unit;agentzero-knowledge")

add_test(NAME agentzero_knowledge_concepts COMMAND agentzero_knowledge_tests --filter ConceptFormation)
set_tests_properties(agentzero_knowledge_concepts PROPERTIES LABELS "unit;agentzero-knowledge")

add_test(NAME agentzero_knowledge_pln COMMAND agentzero_knowledge_tests --filter PLN)
set_tests_properties(agentzero_knowledge_pln PROPERTIES LABELS "unit;agentzero-knowledge")

add_test(NAME agentzero_knowledge_pipeline COMMAND agentzero_knowledge_tests --filter Pipeline)
set_tests_properties(agentzero_knowledge_pipeline PROPERTIES LABELS "integration;agentzero-knowledge")
