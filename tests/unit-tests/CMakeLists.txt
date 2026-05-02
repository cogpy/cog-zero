# Unit Tests CMakeLists.txt
# AZ-TEST-001: Comprehensive unit testing framework

cmake_minimum_required(VERSION 3.16)

if(CXXTEST_FOUND)
    # Test Framework Unit Tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/TestFrameworkUTest.cxxtest")
        cxxtest_add_test(TestFrameworkUTest TestFrameworkUTest.cpp 
                        "${CMAKE_CURRENT_SOURCE_DIR}/TestFrameworkUTest.cxxtest")
        
        target_link_libraries(TestFrameworkUTest
            agentzero-test-framework
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
            ${COGSERVER_LIBRARIES}
            ${Boost_LIBRARIES}
        )
    endif()
    
    # Mock Objects Unit Tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MockObjectsUTest.cxxtest")
        cxxtest_add_test(MockObjectsUTest MockObjectsUTest.cpp 
                        "${CMAKE_CURRENT_SOURCE_DIR}/MockObjectsUTest.cxxtest")
        
        target_link_libraries(MockObjectsUTest
            agentzero-test-framework
            ${COGUTIL_LIBRARIES}
            ${ATOMSPACE_LIBRARIES}
            ${COGSERVER_LIBRARIES}
            ${Boost_LIBRARIES}
        )
    endif()
    
    message(STATUS "Agent-Zero unit tests configured")
endif()