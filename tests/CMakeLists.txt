# Agent-Zero C++ Tests CMakeLists.txt
# Enhanced test framework with comprehensive testing infrastructure
# Part of the AGENT-ZERO-GENESIS project - AZ-TEST-001

cmake_minimum_required(VERSION 3.16)

# Find CxxTest
find_package(CxxTest)

if(CXXTEST_FOUND)
    message(STATUS "CxxTest found - Agent-Zero comprehensive test framework will be built")
    include_directories(${CXXTEST_INCLUDE_DIR})
    enable_testing()
    
    # Include test framework headers
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/framework)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mocks)
    
    # Build test framework library
    add_library(agentzero-test-framework SHARED
        framework/TestFramework.cpp
        mocks/MockObjects.cpp
    )
    
    target_link_libraries(agentzero-test-framework
        ${COGUTIL_LIBRARIES}
        ${ATOMSPACE_LIBRARIES}
        ${COGSERVER_LIBRARIES}
        ${Boost_LIBRARIES}
    )
    
    # Add component test directories (skip if already added)
    # add_subdirectory(../agentzero-core/tests ${CMAKE_CURRENT_BINARY_DIR}/agentzero-core-tests)
    
    # Add comprehensive test suites
    add_subdirectory(unit-tests)
    add_subdirectory(integration-tests)
    add_subdirectory(performance-tests)
    add_subdirectory(regression-tests)
    
    # Create coverage reporting target (if gcov available)
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Coverage reporting enabled")
        
        # Add coverage flags
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} --coverage")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
        
        # Coverage target
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --zerocounters
            COMMAND make test
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*test*' '*mock*' --output-file coverage.info.cleaned
            COMMAND ${GENHTML_PATH} -o coverage coverage.info.cleaned
            COMMAND ${CMAKE_COMMAND} -E remove coverage.info coverage.info.cleaned
            COMMENT "Generating code coverage report"
        )
        
        add_dependencies(coverage agentzero-comprehensive-tests)
    endif()
    
    # Create comprehensive test targets
    add_custom_target(agentzero-unit-tests
        DEPENDS TestFrameworkUTest MockObjectsUTest
        COMMENT "Building Agent-Zero unit tests"
    )
    
    add_custom_target(agentzero-integration-tests
        DEPENDS ComponentIntegrationTest AtomSpaceIntegrationTest
        COMMENT "Building Agent-Zero integration tests"
    )
    
    add_custom_target(agentzero-performance-tests
        DEPENDS PerformanceBenchmarkTest MemoryUsageTest
        COMMENT "Building Agent-Zero performance tests"
    )
    
    add_custom_target(agentzero-regression-tests
        DEPENDS RegressionTest
        COMMENT "Building Agent-Zero regression tests"
    )
    
    # Main comprehensive test target
    add_custom_target(agentzero-comprehensive-tests
        DEPENDS agentzero-unit-tests agentzero-integration-tests agentzero-performance-tests agentzero-regression-tests
        COMMENT "Building all Agent-Zero comprehensive tests"
    )
    
    # Convenience target (backwards compatibility)
    add_custom_target(agentzero-tests
        DEPENDS agentzero-comprehensive-tests
        COMMENT "Building all Agent-Zero tests (comprehensive framework)"
    )
    
    # Test execution targets
    add_custom_target(run-unit-tests
        COMMAND ctest -R ".*UTest" --output-on-failure
        DEPENDS agentzero-unit-tests
        COMMENT "Running Agent-Zero unit tests"
    )
    
    add_custom_target(run-integration-tests
        COMMAND ctest -R ".*IntegrationTest" --output-on-failure
        DEPENDS agentzero-integration-tests
        COMMENT "Running Agent-Zero integration tests"
    )
    
    add_custom_target(run-performance-tests
        COMMAND ctest -R ".*PerformanceTest|.*BenchmarkTest" --output-on-failure
        DEPENDS agentzero-performance-tests
        COMMENT "Running Agent-Zero performance tests"
    )
    
    add_custom_target(run-all-tests
        COMMAND ctest --output-on-failure
        DEPENDS agentzero-comprehensive-tests
        COMMENT "Running all Agent-Zero tests"
    )
    
    # Test reporting
    add_custom_target(test-report
        COMMAND echo "=== Agent-Zero Test Framework Report ==="
        COMMAND ctest --output-on-failure --verbose
        DEPENDS agentzero-comprehensive-tests
        COMMENT "Generating comprehensive test report"
    )
    
    message(STATUS "Agent-Zero comprehensive test framework configured")
    message(STATUS "Available test targets:")
    message(STATUS "  agentzero-tests                - Build all tests")
    message(STATUS "  agentzero-unit-tests          - Build unit tests")
    message(STATUS "  agentzero-integration-tests   - Build integration tests")
    message(STATUS "  agentzero-performance-tests   - Build performance tests")
    message(STATUS "  agentzero-regression-tests    - Build regression tests")
    message(STATUS "  run-unit-tests                - Run unit tests")
    message(STATUS "  run-integration-tests         - Run integration tests")
    message(STATUS "  run-performance-tests         - Run performance tests")
    message(STATUS "  run-all-tests                 - Run all tests")
    message(STATUS "  test-report                   - Generate test report")
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "  coverage                      - Generate coverage report")
    endif()
    
else()
    message(STATUS "CxxTest not found - Agent-Zero tests will not be built")
    message(STATUS "Install CxxTest to enable Agent-Zero comprehensive testing")
    message(STATUS "  Ubuntu/Debian: sudo apt-get install cxxtest")
    message(STATUS "  See: https://cxxtest.com/")
endif()