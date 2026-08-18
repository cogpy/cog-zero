# agentzero-integration/tests/CMakeLists.txt
# Phase 9 full-system tests — integration, benchmarks, regression

set(AGENTZERO_INTEGRATION_LIBS
    agentzero-core
    agentzero-perception
    agentzero-knowledge
    agentzero-planning
    agentzero-learning
    agentzero-communication
    agentzero-memory
    agentzero-tools
    Threads::Threads
)

# Full system integration (all modules together)
add_executable(agentzero_system_integration_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_full_system.cpp
)
target_link_libraries(agentzero_system_integration_tests PRIVATE ${AGENTZERO_INTEGRATION_LIBS})
target_include_directories(agentzero_system_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME agentzero_system_integration COMMAND agentzero_system_integration_tests)
set_tests_properties(agentzero_system_integration PROPERTIES
    LABELS "integration;phase9;agentzero-integration"
    TIMEOUT 120
)

# Performance benchmarks (< 100 ms routine decisions)
add_executable(agentzero_system_benchmark_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_system_benchmarks.cpp
)
target_link_libraries(agentzero_system_benchmark_tests PRIVATE ${AGENTZERO_INTEGRATION_LIBS})
target_include_directories(agentzero_system_benchmark_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME agentzero_system_benchmarks COMMAND agentzero_system_benchmark_tests)
set_tests_properties(agentzero_system_benchmarks PROPERTIES
    LABELS "benchmark;phase9;agentzero-integration"
    TIMEOUT 120
)

# Regression baseline establishment
add_executable(agentzero_system_regression_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_system_regression.cpp
)
target_link_libraries(agentzero_system_regression_tests PRIVATE ${AGENTZERO_INTEGRATION_LIBS})
target_include_directories(agentzero_system_regression_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME agentzero_system_regression COMMAND agentzero_system_regression_tests)
set_tests_properties(agentzero_system_regression PROPERTIES
    LABELS "regression;phase9;agentzero-integration"
    TIMEOUT 120
)
