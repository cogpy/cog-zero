/*
 * test_GoalHierarchy.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for GoalHierarchy
 * Part of AZ-PLAN-001: Implement GoalHierarchy management
 */

#include <cxxtest/TestSuite.h>
#include <opencog/agentzero/GoalHierarchy.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>

using namespace opencog;
using namespace opencog::agentzero;

class GoalHierarchyTestSuite : public CxxTest::TestSuite
{
private:
    AtomSpacePtr as;
    std::unique_ptr<GoalHierarchy> goal_hierarchy;

public:
    void setUp()
    {
        as = std::make_shared<AtomSpace>();
        goal_hierarchy = std::make_unique<GoalHierarchy>(as);
    }

    void tearDown()
    {
        goal_hierarchy.reset();
        as.reset();
    }

    void test_constructor()
    {
        TS_ASSERT(goal_hierarchy != nullptr);
        TS_ASSERT(goal_hierarchy->getGoalHierarchyContext() != Handle::UNDEFINED);
    }

    void test_add_root_goal()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "test-root-goal");
        
        bool result = goal_hierarchy->addGoal(goal);
        
        TS_ASSERT(result);
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalStatus(goal), GoalHierarchy::GoalStatus::INACTIVE);
        
        auto root_goals = goal_hierarchy->getRootGoals();
        TS_ASSERT_EQUALS(root_goals.size(), 1);
        TS_ASSERT_EQUALS(root_goals[0], goal);
    }

    void test_add_subgoal()
    {
        Handle parent_goal = as->add_node(CONCEPT_NODE, "parent-goal");
        Handle sub_goal = as->add_node(CONCEPT_NODE, "sub-goal");
        
        goal_hierarchy->addGoal(parent_goal);
        goal_hierarchy->addGoal(sub_goal, parent_goal);
        
        auto subgoals = goal_hierarchy->getSubgoals(parent_goal);
        TS_ASSERT_EQUALS(subgoals.size(), 1);
        TS_ASSERT_EQUALS(subgoals[0], sub_goal);
        
        TS_ASSERT_EQUALS(goal_hierarchy->getParentGoal(sub_goal), parent_goal);
    }

    void test_goal_status()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "status-goal");
        goal_hierarchy->addGoal(goal);
        
        TS_ASSERT(goal_hierarchy->setGoalStatus(goal, GoalHierarchy::GoalStatus::ACTIVE));
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalStatus(goal), GoalHierarchy::GoalStatus::ACTIVE);
        
        auto active_goals = goal_hierarchy->getActiveGoals();
        TS_ASSERT_EQUALS(active_goals.size(), 1);
        TS_ASSERT_EQUALS(active_goals[0], goal);
    }

    void test_goal_satisfaction()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "satisfaction-goal");
        goal_hierarchy->addGoal(goal);
        
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalSatisfaction(goal), 0.0f);
        
        TS_ASSERT(goal_hierarchy->setGoalSatisfaction(goal, 0.75f));
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalSatisfaction(goal), 0.75f);
        
        TS_ASSERT(goal_hierarchy->setGoalSatisfaction(goal, 1.0f));
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalStatus(goal), GoalHierarchy::GoalStatus::SATISFIED);
    }

    void test_goal_dependencies()
    {
        Handle goal1 = as->add_node(CONCEPT_NODE, "goal1");
        Handle goal2 = as->add_node(CONCEPT_NODE, "goal2");
        
        goal_hierarchy->addGoal(goal1);
        goal_hierarchy->addGoal(goal2);
        
        TS_ASSERT(goal_hierarchy->addGoalDependency(goal2, goal1));
        
        auto dependencies = goal_hierarchy->getGoalDependencies(goal2);
        TS_ASSERT_EQUALS(dependencies.size(), 1);
        TS_ASSERT_EQUALS(dependencies[0], goal1);
        
        TS_ASSERT(!goal_hierarchy->areGoalDependenciesSatisfied(goal2));
        
        goal_hierarchy->setGoalStatus(goal1, GoalHierarchy::GoalStatus::SATISFIED);
        TS_ASSERT(goal_hierarchy->areGoalDependenciesSatisfied(goal2));
    }

    void test_goal_activation()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "activation-goal");
        goal_hierarchy->addGoal(goal);
        
        TS_ASSERT(goal_hierarchy->activateGoal(goal));
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalStatus(goal), GoalHierarchy::GoalStatus::ACTIVE);
        
        TS_ASSERT(goal_hierarchy->deactivateGoal(goal));
        TS_ASSERT_EQUALS(goal_hierarchy->getGoalStatus(goal), GoalHierarchy::GoalStatus::INACTIVE);
    }

    void test_next_goal_to_plan()
    {
        Handle high_priority = as->add_node(CONCEPT_NODE, "high-priority");
        Handle low_priority = as->add_node(CONCEPT_NODE, "low-priority");
        
        goal_hierarchy->addGoal(high_priority, Handle::UNDEFINED, GoalHierarchy::GoalPriority::HIGH);
        goal_hierarchy->addGoal(low_priority, Handle::UNDEFINED, GoalHierarchy::GoalPriority::LOW);
        
        goal_hierarchy->activateGoal(high_priority);
        goal_hierarchy->activateGoal(low_priority);
        
        Handle next_goal = goal_hierarchy->getNextGoalToPlan();
        TS_ASSERT_EQUALS(next_goal, high_priority);  // Higher priority should be selected first
    }
};