/*
 * test_PlanningEngine.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for PlanningEngine
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

#include <cxxtest/TestSuite.h>
#include <opencog/agentzero/PlanningEngine.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>

using namespace opencog;
using namespace opencog::agentzero;

class PlanningEngineTestSuite : public CxxTest::TestSuite
{
private:
    AtomSpacePtr as;
    std::unique_ptr<PlanningEngine> planning_engine;

public:
    void setUp()
    {
        as = std::make_shared<AtomSpace>();
        planning_engine = std::make_unique<PlanningEngine>(as);
    }

    void tearDown()
    {
        planning_engine.reset();
        as.reset();
    }

    void test_constructor()
    {
        TS_ASSERT(planning_engine != nullptr);
        TS_ASSERT(planning_engine->getPlanningContext() != Handle::UNDEFINED);
        TS_ASSERT(planning_engine->getTemporalContext() != Handle::UNDEFINED);
    }

    void test_create_simple_plan()
    {
        // Create a simple goal
        Handle goal = as->add_node(CONCEPT_NODE, "test-goal");
        
        // Create plan
        auto result = planning_engine->createPlan(goal);
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);
        
        // Verify plan exists
        const auto* plan = planning_engine->getPlan(goal);
        TS_ASSERT(plan != nullptr);
        TS_ASSERT_EQUALS(plan->goal_atom, goal);
        TS_ASSERT(!plan->action_sequence.empty());
    }

    void test_invalid_goal()
    {
        Handle invalid_goal = Handle::UNDEFINED;
        
        auto result = planning_engine->createPlan(invalid_goal);
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::GOAL_INVALID);
    }

    void test_temporal_plan()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "temporal-goal");
        
        // Set deadline 5 seconds from now
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        
        auto result = planning_engine->createTemporalPlan(goal, deadline);
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);
        
        const auto* plan = planning_engine->getPlan(goal);
        TS_ASSERT(plan != nullptr);
        TS_ASSERT(plan->end_time <= deadline);
    }

    void test_plan_status()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "status-goal");
        
        planning_engine->createPlan(goal);
        const auto* plan = planning_engine->getPlan(goal);
        
        TS_ASSERT(plan != nullptr);
        auto status = planning_engine->getPlanStatus(plan->plan_atom);
        TS_ASSERT_EQUALS(status, PlanningEngine::ExecutionStatus::NOT_STARTED);
    }

    void test_performance_targets()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "performance-goal");
        
        auto start_time = std::chrono::steady_clock::now();
        auto result = planning_engine->createPlan(goal);
        auto end_time = std::chrono::steady_clock::now();
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);
        
        // Check if planning time is under 100ms target for routine decisions
        auto planning_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // Allow some flexibility for test environment
        TS_ASSERT_LESS_THAN(planning_time.count(), 200);  // 200ms max for tests
    }

    void test_hierarchical_planning()
    {
        // Create a goal with subgoals
        Handle main_goal = as->add_node(CONCEPT_NODE, "main-goal");
        Handle subgoal1 = as->add_node(CONCEPT_NODE, "subgoal1");
        Handle subgoal2 = as->add_node(CONCEPT_NODE, "subgoal2");
        
        // Create AND link to represent goal decomposition
        as->add_link(AND_LINK, {subgoal1, subgoal2, main_goal});
        
        auto result = planning_engine->createPlan(main_goal, 
                                                PlanningEngine::PlanningStrategy::HIERARCHICAL);
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);
        
        const auto* plan = planning_engine->getPlan(main_goal);
        TS_ASSERT(plan != nullptr);
        TS_ASSERT(!plan->action_sequence.empty());
    }

    void test_planning_timeout()
    {
        // Set very short timeout for testing
        planning_engine->setPlanningTimeout(1);  // 1ms
        
        Handle complex_goal = as->add_node(CONCEPT_NODE, "complex-goal");
        
        // This might timeout depending on system performance
        auto result = planning_engine->createPlan(complex_goal);
        
        // Should either succeed quickly or timeout
        TS_ASSERT(result == PlanningEngine::PlanResult::SUCCESS ||
                 result == PlanningEngine::PlanResult::TIMEOUT);
    }

    void test_configuration()
    {
        planning_engine->setMaxPlanDepth(5);
        planning_engine->setMaxActionsPerPlan(10);
        planning_engine->setMinConfidenceThreshold(0.5f);
        planning_engine->configureFeatures(true, true);
        
        // Test that configuration is applied
        Handle goal = as->add_node(CONCEPT_NODE, "config-goal");
        auto result = planning_engine->createPlan(goal);
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);
        
        const auto* plan = planning_engine->getPlan(goal);
        TS_ASSERT(plan != nullptr);
        TS_ASSERT_LESS_THAN_EQUALS(plan->action_sequence.size(), 10);
        TS_ASSERT_LESS_THAN_EQUALS(0.5f, plan->confidence);
    }

    void test_performance_stats()
    {
        // Create several plans to generate statistics
        for (int i = 0; i < 5; ++i) {
            Handle goal = as->add_node(CONCEPT_NODE, "stats-goal-" + std::to_string(i));
            planning_engine->createPlan(goal);
        }
        
        std::string stats = planning_engine->getPerformanceStats();
        TS_ASSERT(!stats.empty());
        
        float success_rate = planning_engine->getPlanningSuccessRate();
        TS_ASSERT_LESS_THAN(0.0f, success_rate);
        TS_ASSERT_LESS_THAN_EQUALS(success_rate, 100.0f);
        
        auto avg_time = planning_engine->getAveragePlanningTime();
        TS_ASSERT_LESS_THAN(std::chrono::milliseconds(0), avg_time);
    }

    void test_active_plans()
    {
        Handle goal1 = as->add_node(CONCEPT_NODE, "active-goal1");
        Handle goal2 = as->add_node(CONCEPT_NODE, "active-goal2");
        
        planning_engine->createPlan(goal1);
        planning_engine->createPlan(goal2);
        
        auto active_plans = planning_engine->getActivePlans();
        TS_ASSERT_EQUALS(active_plans.size(), 2);
    }

    void test_temporal_reasoning_integration()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "temporal-integration-goal");
        
        // Create temporal plan
        auto result = planning_engine->createPlan(goal, 
                                                PlanningEngine::PlanningStrategy::TEMPORAL_FIRST);
        
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);
        
        // Update temporal reasoning
        int updates = planning_engine->updateTemporalReasoning();
        TS_ASSERT_LESS_THAN_EQUALS(0, updates);
    }

    void test_compute_plan_quality()
    {
        Handle goal = as->add_node(CONCEPT_NODE, "quality-goal");

        auto result = planning_engine->createPlan(goal);
        TS_ASSERT_EQUALS(result, PlanningEngine::PlanResult::SUCCESS);

        const auto* plan = planning_engine->getPlan(goal);
        TS_ASSERT(plan != nullptr);

        // Compute quality metrics
        auto metrics = planning_engine->computePlanQuality(*plan);

        // Scores must be in [0, 1]
        TS_ASSERT_LESS_THAN_EQUALS(0.0f, metrics.optimality_score);
        TS_ASSERT_LESS_THAN_EQUALS(metrics.optimality_score, 1.0f);

        TS_ASSERT_LESS_THAN_EQUALS(0.0f, metrics.goal_coverage);
        TS_ASSERT_LESS_THAN_EQUALS(metrics.goal_coverage, 1.0f);

        TS_ASSERT_LESS_THAN_EQUALS(0.0f, metrics.temporal_satisfaction);
        TS_ASSERT_LESS_THAN_EQUALS(metrics.temporal_satisfaction, 1.0f);

        TS_ASSERT_LESS_THAN_EQUALS(0.0f, metrics.resource_efficiency);
        TS_ASSERT_LESS_THAN_EQUALS(metrics.resource_efficiency, 1.0f);

        TS_ASSERT_LESS_THAN_EQUALS(0.0f, metrics.robustness_score);
        TS_ASSERT_LESS_THAN_EQUALS(metrics.robustness_score, 1.0f);

        // action_count must match the plan's action sequence length
        TS_ASSERT_EQUALS(metrics.action_count,
                         static_cast<int>(plan->action_sequence.size()));

        // Overall score must also be in [0, 1]
        float overall = metrics.overallScore();
        TS_ASSERT_LESS_THAN_EQUALS(0.0f, overall);
        TS_ASSERT_LESS_THAN_EQUALS(overall, 1.0f);
    }
};