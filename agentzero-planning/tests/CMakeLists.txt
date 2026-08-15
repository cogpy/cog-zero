# agentzero-planning/tests/CMakeLists.txt
# Phase 4 planning tests — run under CTest

set(AGENTZERO_PLANNING_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_goal_hierarchy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_planning_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_temporal_reasoner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_spacetime_integrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_meta_planner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_planning_pipeline.cpp
)

add_executable(agentzero_planning_tests ${AGENTZERO_PLANNING_TEST_SOURCES})

target_link_libraries(agentzero_planning_tests PRIVATE agentzero-planning Threads::Threads)

target_include_directories(agentzero_planning_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME agentzero_planning_tests COMMAND agentzero_planning_tests)
set_tests_properties(agentzero_planning_tests PROPERTIES LABELS "unit;agentzero-planning")

add_test(NAME agentzero_planning_goals COMMAND agentzero_planning_tests --filter GoalHierarchy)
set_tests_properties(agentzero_planning_goals PROPERTIES LABELS "unit;agentzero-planning")

add_test(NAME agentzero_planning_engine COMMAND agentzero_planning_tests --filter PlanningEngine)
set_tests_properties(agentzero_planning_engine PROPERTIES LABELS "unit;agentzero-planning")

add_test(NAME agentzero_planning_temporal COMMAND agentzero_planning_tests --filter Temporal)
set_tests_properties(agentzero_planning_temporal PROPERTIES LABELS "unit;agentzero-planning")

add_test(NAME agentzero_planning_spacetime COMMAND agentzero_planning_tests --filter SpaceTime)
set_tests_properties(agentzero_planning_spacetime PROPERTIES LABELS "unit;agentzero-planning")

add_test(NAME agentzero_planning_meta COMMAND agentzero_planning_tests --filter MetaPlanner)
set_tests_properties(agentzero_planning_meta PROPERTIES LABELS "unit;agentzero-planning")

add_test(NAME agentzero_planning_pipeline COMMAND agentzero_planning_tests --filter Pipeline)
set_tests_properties(agentzero_planning_pipeline PROPERTIES LABELS "integration;agentzero-planning")
