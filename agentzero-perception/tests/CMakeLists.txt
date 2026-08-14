# agentzero-perception/tests/CMakeLists.txt
# Phase 2 perception tests — run under CTest

set(AGENTZERO_PERCEPTION_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_modal_sensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_perceptual_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_attention_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_textual_sensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_perception_pipeline.cpp
)

add_executable(agentzero_perception_tests ${AGENTZERO_PERCEPTION_TEST_SOURCES})

target_link_libraries(agentzero_perception_tests PRIVATE agentzero-perception Threads::Threads)

target_include_directories(agentzero_perception_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_perception_tests COMMAND agentzero_perception_tests)
set_tests_properties(agentzero_perception_tests PROPERTIES LABELS "unit;agentzero-perception")

add_test(NAME agentzero_perception_multimodal COMMAND agentzero_perception_tests --filter MultiModal)
set_tests_properties(agentzero_perception_multimodal PROPERTIES LABELS "unit;agentzero-perception")

add_test(NAME agentzero_perception_processor COMMAND agentzero_perception_tests --filter Perceptual)
set_tests_properties(agentzero_perception_processor PROPERTIES LABELS "unit;agentzero-perception")

add_test(NAME agentzero_perception_attention COMMAND agentzero_perception_tests --filter Attention)
set_tests_properties(agentzero_perception_attention PROPERTIES LABELS "unit;agentzero-perception")

add_test(NAME agentzero_perception_textual COMMAND agentzero_perception_tests --filter Textual)
set_tests_properties(agentzero_perception_textual PROPERTIES LABELS "unit;agentzero-perception")

add_test(NAME agentzero_perception_pipeline COMMAND agentzero_perception_tests --filter Pipeline)
set_tests_properties(agentzero_perception_pipeline PROPERTIES LABELS "integration;agentzero-perception")
