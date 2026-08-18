/*
 * tests/GoalHierarchyEnhancedTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Comprehensive test for enhanced GoalHierarchy management in TaskManager
 * Tests new functionality added for AZ-PLAN-001
 */

#include <iostream>
#include <memory>
#include <cassert>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

// Forward declaration for AgentZeroCore
namespace opencog {
namespace agentzero {
    class AgentZeroCore {
    public:
        virtual ~AgentZeroCore() = default;
        virtual std::string getAgentName() const = 0;
    };
}
}

// Mock AgentZeroCore for testing
class MockAgentZeroCore : public opencog::agentzero::AgentZeroCore {
public:
    std::string getAgentName() const override { return "TestAgent"; }
};

// Include TaskManager after forward declaration
#include "../include/opencog/agentzero/TaskManager.h"

using namespace opencog;
using namespace opencog::agentzero;

void test_goal_hierarchy_navigation()
{
    std::cout << "\n=== Testing Goal Hierarchy Navigation ===" << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create a multi-level goal hierarchy
    Handle root_goal = task_mgr.setGoal("project_completion", false);
    Handle milestone1 = task_mgr.addSubgoal(root_goal, "milestone_1");
    Handle milestone2 = task_mgr.addSubgoal(root_goal, "milestone_2");
    Handle task1_1 = task_mgr.addSubgoal(milestone1, "task_1_1");
    Handle task1_2 = task_mgr.addSubgoal(milestone1, "task_1_2");
    Handle subtask1_1_1 = task_mgr.addSubgoal(task1_1, "subtask_1_1_1");
    
    std::cout << "✓ Created multi-level goal hierarchy" << std::endl;
    
    // Test getSubgoals
    std::vector<Handle> root_subgoals = task_mgr.getSubgoals(root_goal);
    assert(root_subgoals.size() == 2);
    std::cout << "✓ getSubgoals: Root has " << root_subgoals.size() << " subgoals" << std::endl;
    
    std::vector<Handle> milestone1_subgoals = task_mgr.getSubgoals(milestone1);
    assert(milestone1_subgoals.size() == 2);
    std::cout << "✓ getSubgoals: Milestone1 has " << milestone1_subgoals.size() << " subgoals" << std::endl;
    
    // Test getParentGoal
    Handle parent_of_task1_1 = task_mgr.getParentGoal(task1_1);
    assert(parent_of_task1_1 == milestone1);
    std::cout << "✓ getParentGoal: Correctly identified parent of task1_1" << std::endl;
    
    Handle parent_of_subtask = task_mgr.getParentGoal(subtask1_1_1);
    assert(parent_of_subtask == task1_1);
    std::cout << "✓ getParentGoal: Correctly identified parent of subtask" << std::endl;
    
    // Test getGoalAncestors
    std::vector<Handle> ancestors = task_mgr.getGoalAncestors(subtask1_1_1);
    assert(ancestors.size() == 3); // task1_1, milestone1, root_goal
    std::cout << "✓ getGoalAncestors: Found " << ancestors.size() << " ancestors for deepest goal" << std::endl;
    
    // Test getLeafGoals
    std::vector<Handle> leaf_goals = task_mgr.getLeafGoals(root_goal);
    assert(leaf_goals.size() == 3); // task1_2, subtask1_1_1, milestone2
    std::cout << "✓ getLeafGoals: Found " << leaf_goals.size() << " leaf goals" << std::endl;
    
    // Test getGoalHierarchyDepth
    int depth = task_mgr.getGoalHierarchyDepth(root_goal);
    assert(depth == 4); // root -> milestone1 -> task1_1 -> subtask1_1_1
    std::cout << "✓ getGoalHierarchyDepth: Hierarchy depth is " << depth << std::endl;
    
    std::cout << "Goal Hierarchy Navigation tests passed!" << std::endl;
}

void test_hierarchical_goal_achievement()
{
    std::cout << "\n=== Testing Hierarchical Goal Achievement ===" << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create test hierarchy
    Handle root_goal = task_mgr.setGoal("achievement_test", false);
    Handle sub1 = task_mgr.addSubgoal(root_goal, "subgoal_1");
    Handle sub2 = task_mgr.addSubgoal(root_goal, "subgoal_2");
    Handle leaf1 = task_mgr.addSubgoal(sub1, "leaf_1");
    Handle leaf2 = task_mgr.addSubgoal(sub1, "leaf_2");
    
    std::cout << "✓ Created achievement test hierarchy" << std::endl;
    
    // Set leaf goal achievements
    leaf1->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9)); // Fully achieved
    leaf2->setTruthValue(SimpleTruthValue::createTV(0.8, 0.9)); // Mostly achieved
    sub2->setTruthValue(SimpleTruthValue::createTV(0.2, 0.9));  // Not achieved
    
    // Test calculateHierarchicalGoalAchievement
    TruthValuePtr sub1_achievement = task_mgr.calculateHierarchicalGoalAchievement(sub1);
    assert(sub1_achievement != nullptr);
    double sub1_level = sub1_achievement->get_mean();
    assert(sub1_level > 0.5); // Should be high due to leaf achievements
    std::cout << "✓ calculateHierarchicalGoalAchievement: sub1 achievement = " << sub1_level << std::endl;
    
    TruthValuePtr root_achievement = task_mgr.calculateHierarchicalGoalAchievement(root_goal);
    assert(root_achievement != nullptr);
    double root_level = root_achievement->get_mean();
    assert(root_level > 0.0 && root_level < 1.0); // Should be partial
    std::cout << "✓ calculateHierarchicalGoalAchievement: root achievement = " << root_level << std::endl;
    
    // Root should be less achieved than sub1 (due to sub2's low achievement)
    assert(root_level < sub1_level);
    std::cout << "✓ Hierarchical achievement properly weighted by subgoal achievements" << std::endl;
    
    std::cout << "Hierarchical Goal Achievement tests passed!" << std::endl;
}

void test_goal_priority_propagation()
{
    std::cout << "\n=== Testing Goal Priority Propagation ===" << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create test hierarchy
    Handle root_goal = task_mgr.setGoal("priority_test", false);
    Handle sub1 = task_mgr.addSubgoal(root_goal, "priority_sub1");
    Handle sub2 = task_mgr.addSubgoal(root_goal, "priority_sub2");
    Handle leaf = task_mgr.addSubgoal(sub1, "priority_leaf");
    
    std::cout << "✓ Created priority test hierarchy" << std::endl;
    
    // Test propagateGoalPriority
    bool propagated = task_mgr.propagateGoalPriority(root_goal, TaskManager::Priority::HIGH);
    assert(propagated == true);
    std::cout << "✓ propagateGoalPriority: Successfully propagated HIGH priority" << std::endl;
    
    // Verify priority metadata exists in AtomSpace
    HandleSeq eval_links;
    atomspace->get_handles_by_type(eval_links, EVALUATION_LINK);
    
    bool found_priority_metadata = false;
    for (const Handle& eval : eval_links) {
        HandleSeq eval_outgoing = eval->getOutgoingSet();
        if (eval_outgoing.size() >= 3) {
            Handle pred = eval_outgoing[0];
            if (pred->get_name() == "goal_priority") {
                found_priority_metadata = true;
                break;
            }
        }
    }
    
    assert(found_priority_metadata);
    std::cout << "✓ Priority metadata properly stored in AtomSpace" << std::endl;
    
    std::cout << "Goal Priority Propagation tests passed!" << std::endl;
}

void test_goal_hierarchy_synchronization()
{
    std::cout << "\n=== Testing Goal Hierarchy Synchronization ===" << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create test hierarchy
    Handle root_goal = task_mgr.setGoal("sync_test", false);
    Handle sub1 = task_mgr.addSubgoal(root_goal, "sync_sub1");
    Handle leaf = task_mgr.addSubgoal(sub1, "sync_leaf");
    
    std::cout << "✓ Created synchronization test hierarchy" << std::endl;
    
    // Set leaf goal to highly achieved
    leaf->setTruthValue(SimpleTruthValue::createTV(0.9, 0.9));
    
    // Test synchronizeGoalHierarchy
    bool synchronized = task_mgr.synchronizeGoalHierarchy(leaf);
    assert(synchronized == true);
    std::cout << "✓ synchronizeGoalHierarchy: Successfully synchronized from leaf" << std::endl;
    
    // Verify synchronization metadata exists
    HandleSeq eval_links;
    atomspace->get_handles_by_type(eval_links, EVALUATION_LINK);
    
    bool found_sync_metadata = false;
    for (const Handle& eval : eval_links) {
        HandleSeq eval_outgoing = eval->getOutgoingSet();
        if (eval_outgoing.size() >= 2) {
            Handle pred = eval_outgoing[0];
            if (pred->get_name() == "hierarchy_synchronized") {
                found_sync_metadata = true;
                break;
            }
        }
    }
    
    assert(found_sync_metadata);
    std::cout << "✓ Synchronization metadata properly stored" << std::endl;
    
    std::cout << "Goal Hierarchy Synchronization tests passed!" << std::endl;
}

void test_goal_hierarchy_removal()
{
    std::cout << "\n=== Testing Goal Hierarchy Removal ===" << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Create test hierarchy
    Handle root_goal = task_mgr.setGoal("removal_test", false);
    Handle sub1 = task_mgr.addSubgoal(root_goal, "removal_sub1");
    Handle sub2 = task_mgr.addSubgoal(root_goal, "removal_sub2");
    Handle leaf1 = task_mgr.addSubgoal(sub1, "removal_leaf1");
    Handle leaf2 = task_mgr.addSubgoal(sub1, "removal_leaf2");
    
    std::cout << "✓ Created removal test hierarchy" << std::endl;
    
    // Verify initial state
    std::vector<Handle> initial_subgoals = task_mgr.getSubgoals(root_goal);
    assert(initial_subgoals.size() == 2);
    
    std::vector<Handle> sub1_subgoals = task_mgr.getSubgoals(sub1);
    assert(sub1_subgoals.size() == 2);
    
    // Test removeGoalFromHierarchy without preserving orphans
    bool removed = task_mgr.removeGoalFromHierarchy(sub1, false);
    assert(removed == true);
    std::cout << "✓ removeGoalFromHierarchy: Successfully removed intermediate goal" << std::endl;
    
    // Verify sub1 and its children are gone
    std::vector<Handle> remaining_subgoals = task_mgr.getSubgoals(root_goal);
    assert(remaining_subgoals.size() == 1); // Only sub2 should remain
    std::cout << "✓ removeGoalFromHierarchy: Orphaned subgoals properly removed" << std::endl;
    
    // Verify leaf goals are gone
    std::vector<Handle> remaining_leaf_goals = task_mgr.getLeafGoals(root_goal);
    assert(remaining_leaf_goals.size() == 1); // Only sub2 should remain as leaf
    std::cout << "✓ removeGoalFromHierarchy: Hierarchy properly cleaned up" << std::endl;
    
    std::cout << "Goal Hierarchy Removal tests passed!" << std::endl;
}

void test_edge_cases()
{
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;
    
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    MockAgentZeroCore mock_core;
    TaskManager task_mgr(&mock_core, atomspace);
    
    // Test with undefined handles
    std::vector<Handle> empty_subgoals = task_mgr.getSubgoals(Handle::UNDEFINED);
    assert(empty_subgoals.empty());
    std::cout << "✓ getSubgoals handles undefined input gracefully" << std::endl;
    
    Handle undefined_parent = task_mgr.getParentGoal(Handle::UNDEFINED);
    assert(undefined_parent == Handle::UNDEFINED);
    std::cout << "✓ getParentGoal handles undefined input gracefully" << std::endl;
    
    TruthValuePtr undefined_achievement = task_mgr.calculateHierarchicalGoalAchievement(Handle::UNDEFINED);
    assert(undefined_achievement != nullptr);
    assert(undefined_achievement->get_mean() == 0.0);
    std::cout << "✓ calculateHierarchicalGoalAchievement handles undefined input gracefully" << std::endl;
    
    // Test with empty hierarchy
    Handle lone_goal = task_mgr.setGoal("lone_goal", false);
    std::vector<Handle> lone_subgoals = task_mgr.getSubgoals(lone_goal);
    assert(lone_subgoals.empty());
    
    int lone_depth = task_mgr.getGoalHierarchyDepth(lone_goal);
    assert(lone_depth == 1);
    std::cout << "✓ Single goal hierarchy handled correctly" << std::endl;
    
    std::cout << "Edge Cases tests passed!" << std::endl;
}

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_timestamp_flag(false);
    logger().set_print_level_flag(false);
    
    std::cout << "=== Enhanced GoalHierarchy Management Comprehensive Tests ===" << std::endl;
    
    try {
        test_goal_hierarchy_navigation();
        test_hierarchical_goal_achievement();
        test_goal_priority_propagation();
        test_goal_hierarchy_synchronization();
        test_goal_hierarchy_removal();
        test_edge_cases();
        
        std::cout << "\n🎉 All Enhanced GoalHierarchy Management tests passed successfully!" << std::endl;
        std::cout << "\nGoalHierarchy Management Implementation Summary:" << std::endl;
        std::cout << "- ✅ Goal hierarchy navigation (getSubgoals, getParentGoal, getGoalAncestors)" << std::endl;
        std::cout << "- ✅ Hierarchical goal achievement calculation" << std::endl;
        std::cout << "- ✅ Goal priority propagation through hierarchy" << std::endl;
        std::cout << "- ✅ Goal hierarchy synchronization" << std::endl;
        std::cout << "- ✅ Goal hierarchy management (depth, leaf detection, removal)" << std::endl;
        std::cout << "- ✅ Robust error handling and edge case management" << std::endl;
        std::cout << "- ✅ Comprehensive AtomSpace integration" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}