# agentzero-distributed/tests/CMakeLists.txt
# Phase 10 distributed computing tests — run under CTest

set(AGENTZERO_DISTRIBUTED_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cluster_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_distributed_coordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_load_balancer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_coordination_protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_distributed_pipeline.cpp
)

add_executable(agentzero_distributed_tests ${AGENTZERO_DISTRIBUTED_TEST_SOURCES})

target_link_libraries(agentzero_distributed_tests PRIVATE agentzero-distributed Threads::Threads)

target_include_directories(agentzero_distributed_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_distributed_tests COMMAND agentzero_distributed_tests)
set_tests_properties(agentzero_distributed_tests PROPERTIES LABELS "unit;agentzero-distributed")

add_test(NAME agentzero_distributed_cluster COMMAND agentzero_distributed_tests --filter ClusterManager)
set_tests_properties(agentzero_distributed_cluster PROPERTIES LABELS "unit;agentzero-distributed")

add_test(NAME agentzero_distributed_coordinator COMMAND agentzero_distributed_tests --filter DistributedCoordinator)
set_tests_properties(agentzero_distributed_coordinator PROPERTIES LABELS "unit;agentzero-distributed")

add_test(NAME agentzero_distributed_balancer COMMAND agentzero_distributed_tests --filter LoadBalancer)
set_tests_properties(agentzero_distributed_balancer PROPERTIES LABELS "unit;agentzero-distributed")

add_test(NAME agentzero_distributed_protocol COMMAND agentzero_distributed_tests --filter CoordinationProtocol)
set_tests_properties(agentzero_distributed_protocol PROPERTIES LABELS "unit;agentzero-distributed")

add_test(NAME agentzero_distributed_pipeline COMMAND agentzero_distributed_tests --filter DistributedPipeline)
set_tests_properties(agentzero_distributed_pipeline PROPERTIES LABELS "unit;agentzero-distributed")
