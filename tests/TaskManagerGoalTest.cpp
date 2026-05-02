/*
 * tests/TaskManagerGoalTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple focused test for TaskManager goal decomposition
 * Tests the enhanced goal decomposition functionality
 */

#include <iostream>
#include <memory>
#include <cassert>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

// Mock AgentZeroCore for testing
class MockAgentZeroCore {
public:
    std::string getAgentName() const { return "TestAgent"; }
};

// Include TaskManager after mock
#include "../include/opencog/agentzero/TaskManager.h"

using namespace opencog;
using namespace opencog::agentzero;

void test_goal_creation_and_decomposition()
{
    std::cout << "Testing goal creation and decomposition..." << std::endl;
    
    // Create AtomSpace and mock core
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    
    // Create TaskManager
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Test 1: Basic goal creation
    Handle goal = task_mgr.setGoal("Learn_Programming", true);
    assert(goal != Handle::UNDEFINED);
    std::cout << "✓ Goal creation successful" << std::endl;
    
    // Test 2: Check goal is in AtomSpace
    assert(atomspace->is_valid_handle(goal));
    std::cout << "✓ Goal exists in AtomSpace" << std::endl;
    
    // Test 3: Check current goal is set
    assert(task_mgr.getCurrentGoal() == goal);
    std::cout << "✓ Current goal correctly set" << std::endl;
    
    // Test 4: Goal should have been decomposed (learning type goal)
    // Check for subgoals by looking for INHERITANCE_LINK
    HandleSeq incoming = atomspace->get_incoming_by_type(goal, INHERITANCE_LINK);
    assert(incoming.size() > 0);
    std::cout << "✓ Goal decomposition created subgoals: " << incoming.size() << std::endl;
    
    // Test 5: Test goal achievement calculation
    TruthValuePtr achievement = task_mgr.isGoalAchieved(goal);
    assert(achievement != nullptr);
    std::cout << "✓ Goal achievement calculation: " << achievement->get_mean() << std::endl;
    
    std::cout << "Goal creation and decomposition tests passed!" << std::endl;
}

void test_different_goal_types()
{
    std::cout << "\nTesting different goal types..." << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Test different goal types for specialized decomposition
    std::vector<std::string> test_goals = {
        "solve_math_problem",
        "create_website", 
        "communicate_with_user",
        "generic_task"
    };
    
    for (const std::string& goal_desc : test_goals) {
        Handle goal = task_mgr.setGoal(goal_desc, true);
        assert(goal != Handle::UNDEFINED);
        
        // Check decomposition occurred
        HandleSeq incoming = atomspace->get_incoming_by_type(goal, INHERITANCE_LINK);
        assert(incoming.size() > 0);
        
        std::cout << "✓ Goal '" << goal_desc << "' decomposed into " 
                  << incoming.size() << " subgoals" << std::endl;
    }
    
    std::cout << "Different goal types test passed!" << std::endl;
}

void test_task_creation_and_status()
{
    std::cout << "\nTesting task creation and status management..." << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create a goal and associated task
    Handle goal = task_mgr.setGoal("test_goal", false); // No auto-decompose
    Handle task = task_mgr.createTask("Test_Task", TaskManager::Priority::HIGH, goal);
    
    assert(task != Handle::UNDEFINED);
    assert(atomspace->is_valid_handle(task));
    std::cout << "✓ Task creation successful" << std::endl;
    
    // Test initial status
    auto status = task_mgr.getTaskStatus(task);
    assert(status == TaskManager::TaskStatus::PENDING);
    std::cout << "✓ Initial task status is PENDING" << std::endl;
    
    // Test status update
    bool result = task_mgr.completeTask(task, true);
    assert(result == true);
    
    status = task_mgr.getTaskStatus(task);
    assert(status == TaskManager::TaskStatus::COMPLETED);
    std::cout << "✓ Task completion and status update successful" << std::endl;
    
    std::cout << "Task creation and status tests passed!" << std::endl;
}

void test_subgoal_hierarchy()
{
    std::cout << "\nTesting subgoal hierarchy..." << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create main goal
    Handle main_goal = task_mgr.setGoal("main_goal", false);
    
    // Create subgoals manually
    Handle subgoal1 = task_mgr.addSubgoal(main_goal, "subgoal_1");
    Handle subgoal2 = task_mgr.addSubgoal(main_goal, "subgoal_2");
    
    assert(subgoal1 != Handle::UNDEFINED);
    assert(subgoal2 != Handle::UNDEFINED);
    std::cout << "✓ Subgoal creation successful" << std::endl;
    
    // Check hierarchy in AtomSpace
    HandleSeq incoming1 = atomspace->get_incoming_by_type(main_goal, INHERITANCE_LINK);
    assert(incoming1.size() >= 2);
    std::cout << "✓ Subgoal hierarchy established in AtomSpace" << std::endl;
    
    std::cout << "Subgoal hierarchy tests passed!" << std::endl;
}

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_timestamp_flag(false);
    logger().set_print_level_flag(false);
    
    std::cout << "=== TaskManager Goal Decomposition Tests ===" << std::endl;
    
    try {
        test_goal_creation_and_decomposition();
        test_different_goal_types();
        test_task_creation_and_status();
        test_subgoal_hierarchy();
        
        std::cout << "\n🎉 All tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}