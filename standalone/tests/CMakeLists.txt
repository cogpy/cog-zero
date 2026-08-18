# standalone/tests/CMakeLists.txt
#
# Unit, e2e, integration, regression, fuzz, and benchmark tests for cog0.
# All test source files live in the repository's flat tests/ directory.
# Uses the zero-dependency test framework in tests/test_runner.h.

# COG0_REPO_ROOT is set by the parent standalone/CMakeLists.txt and resolves
# to the repository root regardless of which directory cmake was invoked from.
set(COG0_TEST_SRC "${COG0_REPO_ROOT}/tests")

# -----------------------------------------------------------------------
# Core unit + integration tests

add_executable(cog0_tests
    ${COG0_TEST_SRC}/test_main.cpp
    ${COG0_TEST_SRC}/test_atom_store.cpp
    ${COG0_TEST_SRC}/test_task_manager.cpp
    ${COG0_TEST_SRC}/test_reasoning_engine.cpp
    ${COG0_TEST_SRC}/test_cognitive_loop.cpp
    ${COG0_TEST_SRC}/test_agent.cpp
    ${COG0_TEST_SRC}/test_perception.cpp
    # AtomStore save/load snapshot persistence (CLI save/load commands)
    ${COG0_TEST_SRC}/test_persistence.cpp
    # Standalone-compatible ActionExecutor / ActionScheduler tests
    # (test_action_executor.cpp and test_action_scheduler.cpp use OpenCog/gtest
    #  and are only built with the full OpenCog tree)
    ${COG0_TEST_SRC}/test_action_executor_cog0.cpp
    ${COG0_TEST_SRC}/test_action_scheduler_cog0.cpp
)

target_link_libraries(cog0_tests PRIVATE cog0lib)
target_include_directories(cog0_tests PRIVATE "${COG0_TEST_SRC}")

add_test(NAME cog0_tests COMMAND cog0_tests)
set_tests_properties(cog0_tests PROPERTIES LABELS "unit")

# -----------------------------------------------------------------------
# End-to-end tests

if(EXISTS "${COG0_TEST_SRC}/test_e2e.cpp")
    add_executable(cog0_e2e_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_e2e.cpp
    )
    target_link_libraries(cog0_e2e_tests PRIVATE cog0lib)
    target_include_directories(cog0_e2e_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_e2e COMMAND cog0_e2e_tests)
    set_tests_properties(cog0_e2e PROPERTIES LABELS "e2e")
endif()

# -----------------------------------------------------------------------
# Integration tests

if(EXISTS "${COG0_TEST_SRC}/test_integration.cpp")
    add_executable(cog0_integration_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_integration.cpp
    )
    target_link_libraries(cog0_integration_tests PRIVATE cog0lib)
    target_include_directories(cog0_integration_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_integration COMMAND cog0_integration_tests)
    set_tests_properties(cog0_integration PROPERTIES LABELS "integration")
endif()

# -----------------------------------------------------------------------
# Regression tests

if(EXISTS "${COG0_TEST_SRC}/test_regression.cpp")
    add_executable(cog0_regression_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_regression.cpp
    )
    target_link_libraries(cog0_regression_tests PRIVATE cog0lib)
    target_include_directories(cog0_regression_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_regression COMMAND cog0_regression_tests)
    set_tests_properties(cog0_regression PROPERTIES LABELS "regression")
endif()

# -----------------------------------------------------------------------
# Benchmark tests

if(EXISTS "${COG0_TEST_SRC}/test_benchmarks.cpp")
    # test_benchmarks.cpp uses test_runner.h (latency assertions via TEST macros)
    add_executable(cog0_benchmarks
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_benchmarks.cpp
    )
    target_link_libraries(cog0_benchmarks PRIVATE cog0lib)
    target_include_directories(cog0_benchmarks PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_benchmarks COMMAND cog0_benchmarks)
    set_tests_properties(cog0_benchmarks PROPERTIES LABELS "benchmark")
endif()

# bench_atom_store / bench_reasoning_engine use the separate bench_runner.h
# framework (BENCHMARK_N macros, bench_main.cpp entry point)
if(EXISTS "${COG0_TEST_SRC}/bench_atom_store.cpp")
    add_executable(cog0_microbenchmarks
        ${COG0_TEST_SRC}/bench_main.cpp
        ${COG0_TEST_SRC}/bench_atom_store.cpp
        ${COG0_TEST_SRC}/bench_reasoning_engine.cpp
    )
    target_link_libraries(cog0_microbenchmarks PRIVATE cog0lib)
    target_include_directories(cog0_microbenchmarks PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_microbenchmarks COMMAND cog0_microbenchmarks)
    set_tests_properties(cog0_microbenchmarks PROPERTIES LABELS "benchmark")
endif()

# -----------------------------------------------------------------------
# Phase 12 integration tests (structured logging, Raft, conflict resolution,
# monitoring server)

if(EXISTS "${COG0_TEST_SRC}/test_phase12.cpp")
    add_executable(cog0_phase12_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_phase12.cpp
    )
    target_link_libraries(cog0_phase12_tests PRIVATE cog0lib)
    target_include_directories(cog0_phase12_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_phase12 COMMAND cog0_phase12_tests)
    set_tests_properties(cog0_phase12 PROPERTIES
        LABELS "integration"
        TIMEOUT 60
    )
endif()

# -----------------------------------------------------------------------
# Phase 14 persistence tests (Raft log store)

if(EXISTS "${COG0_TEST_SRC}/test_phase14_raft_persistence.cpp")
    add_executable(cog0_phase14_persistence_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_phase14_raft_persistence.cpp
    )
    target_link_libraries(cog0_phase14_persistence_tests PRIVATE cog0lib)
    target_include_directories(cog0_phase14_persistence_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_phase14_persistence COMMAND cog0_phase14_persistence_tests)
    set_tests_properties(cog0_phase14_persistence PROPERTIES
        LABELS "integration"
        TIMEOUT 60
    )
endif()

# -----------------------------------------------------------------------
# Phase 14 Feature 2.1 tests: Agent Migration — Live State Hand-off

if(EXISTS "${COG0_TEST_SRC}/test_phase14_migration.cpp")
    add_executable(cog0_phase14_migration_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_phase14_migration.cpp
    )
    target_link_libraries(cog0_phase14_migration_tests PRIVATE cog0lib)
    target_include_directories(cog0_phase14_migration_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_phase14_migration COMMAND cog0_phase14_migration_tests)
    set_tests_properties(cog0_phase14_migration PROPERTIES
        LABELS "unit"
        TIMEOUT 60
    )
endif()

# -----------------------------------------------------------------------
# Phase 14 Feature 2.3 tests: WebSocket Monitoring Dashboard

if(EXISTS "${COG0_TEST_SRC}/test_phase14_websocket.cpp")
    add_executable(cog0_phase14_websocket_tests
        ${COG0_TEST_SRC}/test_main.cpp
        ${COG0_TEST_SRC}/test_phase14_websocket.cpp
    )
    target_link_libraries(cog0_phase14_websocket_tests PRIVATE cog0lib)
    target_include_directories(cog0_phase14_websocket_tests PRIVATE "${COG0_TEST_SRC}")
    add_test(NAME cog0_phase14_websocket COMMAND cog0_phase14_websocket_tests)
    set_tests_properties(cog0_phase14_websocket PROPERTIES
        LABELS "unit"
        TIMEOUT 60
    )
endif()

# -----------------------------------------------------------------------
# Property-based ("fuzz") tests
#
# These files use the TEST() macros from test_runner.h to drive
# pseudo-random property-based testing — they are NOT libFuzzer harnesses
# (no LLVMFuzzerTestOneInput entry point). Build them as regular tests.
foreach(_fuzz_src
    test_atom_store_fuzz.cpp
    test_reasoning_engine_fuzz.cpp
)
    if(EXISTS "${COG0_TEST_SRC}/${_fuzz_src}")
        string(REPLACE ".cpp" "" _fuzz_target "cog0_${_fuzz_src}")
        add_executable(${_fuzz_target}
            "${COG0_TEST_SRC}/test_main.cpp"
            "${COG0_TEST_SRC}/${_fuzz_src}"
        )
        target_link_libraries(${_fuzz_target} PRIVATE cog0lib)
        target_include_directories(${_fuzz_target} PRIVATE "${COG0_TEST_SRC}")
        add_test(NAME ${_fuzz_target} COMMAND ${_fuzz_target})
        set_tests_properties(${_fuzz_target} PROPERTIES
            LABELS "fuzz"
            TIMEOUT 60
        )
    endif()
endforeach()

# -----------------------------------------------------------------------
# CLI smoke tests — near-term enhancements (--batch, save/load, rule,
# --no-color, multi-command --eval). Requires the cog0 executable.
#
if(TARGET cog0 AND EXISTS "${COG0_TEST_SRC}/cli_smoke.sh")
    add_test(
        NAME cog0_cli_smoke
        COMMAND bash "${COG0_TEST_SRC}/cli_smoke.sh" "$<TARGET_FILE:cog0>"
    )
    set_tests_properties(cog0_cli_smoke PROPERTIES
        LABELS "cli;e2e"
        TIMEOUT 60
    )
endif()
