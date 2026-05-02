/*
 * simple_planning_demo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple demonstration of PlanningEngine with temporal reasoning
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

#include <iostream>
#include <chrono>
#include <thread>

#include <opencog/agentzero/PlanningEngine.h>
#include <opencog/agentzero/GoalHierarchy.h>
#include <opencog/agentzero/TemporalReasoner.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    std::cout << "=== Agent-Zero Planning Engine Demo ===" << std::endl;
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        std::cout << "✓ Created AtomSpace" << std::endl;
        
        // Create PlanningEngine
        auto planning_engine = std::make_unique<PlanningEngine>(atomspace);
        std::cout << "✓ Created PlanningEngine with temporal reasoning" << std::endl;
        
        // Create some goals
        Handle main_goal = atomspace->add_node(CONCEPT_NODE, "achieve-objective");
        Handle subgoal1 = atomspace->add_node(CONCEPT_NODE, "gather-resources");
        Handle subgoal2 = atomspace->add_node(CONCEPT_NODE, "execute-action");
        
        std::cout << "✓ Created goal hierarchy:" << std::endl;
        std::cout << "  - Main goal: " << main_goal->to_string() << std::endl;
        std::cout << "  - Subgoal 1: " << subgoal1->to_string() << std::endl;
        std::cout << "  - Subgoal 2: " << subgoal2->to_string() << std::endl;
        
        // Create goal decomposition structure
        atomspace->add_link(AND_LINK, {subgoal1, subgoal2, main_goal});
        
        // Test basic planning
        std::cout << "\n--- Testing Basic Planning ---" << std::endl;
        auto start_time = std::chrono::steady_clock::now();
        
        auto result = planning_engine->createPlan(main_goal);
        
        auto end_time = std::chrono::steady_clock::now();
        auto planning_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        if (result == PlanningEngine::PlanResult::SUCCESS) {
            std::cout << "✓ Planning succeeded!" << std::endl;
            std::cout << "  Planning time: " << planning_time.count() << " ms" << std::endl;
            
            const auto* plan = planning_engine->getPlan(main_goal);
            if (plan) {
                std::cout << "  Plan contains " << plan->action_sequence.size() << " actions" << std::endl;
                std::cout << "  Plan confidence: " << plan->confidence << std::endl;
                std::cout << "  Plan status: " << (int)plan->status << std::endl;
            }
        } else {
            std::cout << "✗ Planning failed with result: " << (int)result << std::endl;
            return 1;
        }
        
        // Test temporal planning
        std::cout << "\n--- Testing Temporal Planning ---" << std::endl;
        Handle temporal_goal = atomspace->add_node(CONCEPT_NODE, "time-critical-objective");
        
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        result = planning_engine->createTemporalPlan(temporal_goal, deadline);
        
        if (result == PlanningEngine::PlanResult::SUCCESS) {
            std::cout << "✓ Temporal planning succeeded!" << std::endl;
            const auto* temporal_plan = planning_engine->getPlan(temporal_goal);
            if (temporal_plan) {
                std::cout << "  Temporal plan contains " << temporal_plan->action_sequence.size() << " actions" << std::endl;
                std::cout << "  Temporal plan confidence: " << temporal_plan->confidence << std::endl;
            }
        } else {
            std::cout << "✗ Temporal planning failed with result: " << (int)result << std::endl;
        }
        
        // Test goal hierarchy functionality
        std::cout << "\n--- Testing Goal Hierarchy ---" << std::endl;
        auto goal_hierarchy = planning_engine->getGoalHierarchy();
        if (goal_hierarchy) {
            std::cout << "✓ Goal hierarchy available" << std::endl;
            
            // Add goals to hierarchy
            goal_hierarchy->addGoal(main_goal);
            goal_hierarchy->addGoal(subgoal1, main_goal);
            goal_hierarchy->addGoal(subgoal2, main_goal);
            
            // Set dependencies
            goal_hierarchy->addGoalDependency(subgoal2, subgoal1);
            
            std::cout << "  Added goals to hierarchy with dependencies" << std::endl;
            
            // Activate main goal
            goal_hierarchy->activateGoal(main_goal);
            auto active_goals = goal_hierarchy->getActiveGoals();
            std::cout << "  Active goals: " << active_goals.size() << std::endl;
            
            // Get next goal to plan
            Handle next_goal = goal_hierarchy->getNextGoalToPlan();
            if (next_goal != Handle::UNDEFINED) {
                std::cout << "  Next goal to plan: " << next_goal->to_string() << std::endl;
            }
        }
        
        // Test performance metrics
        std::cout << "\n--- Performance Metrics ---" << std::endl;
        std::cout << planning_engine->getPerformanceStats() << std::endl;
        
        // Test temporal reasoning update
        std::cout << "\n--- Testing Temporal Reasoning Update ---" << std::endl;
        int temporal_updates = planning_engine->updateTemporalReasoning();
        std::cout << "✓ Temporal reasoning updated " << temporal_updates << " constraints" << std::endl;
        
        std::cout << "\n=== Demo Completed Successfully ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }
}