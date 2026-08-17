# agentzero-learning/tests/CMakeLists.txt
# Phase 5 learning tests — run under CTest

set(AGENTZERO_LEARNING_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_experience_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_skill_acquisition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_policy_optimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_learning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_learning_pipeline.cpp
)

add_executable(agentzero_learning_tests ${AGENTZERO_LEARNING_TEST_SOURCES})

target_link_libraries(agentzero_learning_tests PRIVATE agentzero-learning Threads::Threads)

target_include_directories(agentzero_learning_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_learning_tests COMMAND agentzero_learning_tests)
set_tests_properties(agentzero_learning_tests PROPERTIES LABELS "unit;agentzero-learning")

add_test(NAME agentzero_learning_experience COMMAND agentzero_learning_tests --filter ExperienceManager)
set_tests_properties(agentzero_learning_experience PROPERTIES LABELS "unit;agentzero-learning")

add_test(NAME agentzero_learning_skills COMMAND agentzero_learning_tests --filter SkillAcquisition)
set_tests_properties(agentzero_learning_skills PROPERTIES LABELS "unit;agentzero-learning")

add_test(NAME agentzero_learning_policy COMMAND agentzero_learning_tests --filter PolicyOptimizer)
set_tests_properties(agentzero_learning_policy PROPERTIES LABELS "unit;agentzero-learning")

add_test(NAME agentzero_learning_meta COMMAND agentzero_learning_tests --filter MetaLearning)
set_tests_properties(agentzero_learning_meta PROPERTIES LABELS "unit;agentzero-learning")

add_test(NAME agentzero_learning_pipeline COMMAND agentzero_learning_tests --filter Pipeline)
set_tests_properties(agentzero_learning_pipeline PROPERTIES LABELS "integration;agentzero-learning")
