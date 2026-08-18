# Regression Tests CMakeLists.txt
# AZ-TEST-001: Regression testing framework

cmake_minimum_required(VERSION 3.16)

if(CXXTEST_FOUND)
    # Regression Tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RegressionTest.cxxtest")
        cxxtest_add_test(RegressionTest RegressionTest.cpp 
                        "${CMAKE_CURRENT_SOURCE_DIR}/RegressionTest.cxxtest")
        
        target_link_libraries(RegressionTest
            agentzero-test-framework
            agentzero-core
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
            ${COGSERVER_LIBRARIES}
            ${Boost_LIBRARIES}
        )
    endif()
    
    message(STATUS "Agent-Zero regression tests configured")
endif()