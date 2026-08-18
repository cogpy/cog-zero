/*
 * tests/MetaPlannerTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for MetaPlanner
 * Tests self-reflective planning optimization functionality
 * Part of AZ-PLAN-003: Implement MetaPlanner for self-optimization
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/MetaPlanner.h"
#include "opencog/agentzero/AgentZeroCore.h"
#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/ActionScheduler.h"

using namespace opencog;
using namespace opencog::agentzero;

class MetaPlannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        atomspace = std::make_shared<AtomSpace>();
        
        // Create minimal AgentZeroCore mock for testing
        agent_core = nullptr; // We'll test MetaPlanner in isolation
        
        meta_planner = std::make_unique<MetaPlanner>(agent_core, atomspace);
        
        // Create test context
        test_context = atomspace->add_node(CONCEPT_NODE, "TestPlanningContext");
        test_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    }
    
    void TearDown() override {
        meta_planner.reset();
        atomspace.reset();
    }
    
    std::shared_ptr<AtomSpace> atomspace;
    AgentZeroCore* agent_core;
    std::unique_ptr<MetaPlanner> meta_planner;
    Handle test_context;
};

// Test basic initialization
TEST_F(MetaPlannerTest, Initialization) {
    EXPECT_TRUE(meta_planner->isInitialized());
    EXPECT_EQ(meta_planner->getCurrentStrategy(), PlanningStrategy::ADAPTIVE);
    EXPECT_EQ(meta_planner->getOptimizationObjective(), OptimizationObjective::BALANCED);
}

// Test optimization objective setting
TEST_F(MetaPlannerTest, OptimizationObjectiveSetting) {
    meta_planner->setOptimizationObjective(OptimizationObjective::MINIMIZE_TIME);
    EXPECT_EQ(meta_planner->getOptimizationObjective(), OptimizationObjective::MINIMIZE_TIME);
    
    meta_planner->setOptimizationObjective(OptimizationObjective::MAXIMIZE_SUCCESS);
    EXPECT_EQ(meta_planner->getOptimizationObjective(), OptimizationObjective::MAXIMIZE_SUCCESS);
}

// Test strategy management
TEST_F(MetaPlannerTest, StrategyManagement) {
    // Test manual strategy setting
    meta_planner->setStrategy(PlanningStrategy::GREEDY);
    EXPECT_EQ(meta_planner->getCurrentStrategy(), PlanningStrategy::GREEDY);
    
    meta_planner->setStrategy(PlanningStrategy::HIERARCHICAL);
    EXPECT_EQ(meta_planner->getCurrentStrategy(), PlanningStrategy::HIERARCHICAL);
    
    // Test strategy evaluation retrieval
    auto evaluation = meta_planner->getStrategyEvaluation(PlanningStrategy::GREEDY);
    EXPECT_EQ(evaluation.strategy, PlanningStrategy::GREEDY);
    EXPECT_GE(evaluation.effectiveness_score, 0.0);
    EXPECT_LE(evaluation.effectiveness_score, 1.0);
}

// Test planning effectiveness analysis
TEST_F(MetaPlannerTest, PlanningEffectivenessAnalysis) {
    Handle analysis_result = meta_planner->analyzePlanningEffectiveness(test_context);
    
    EXPECT_NE(analysis_result, Handle::UNDEFINED);
    EXPECT_EQ(analysis_result->get_type(), CONCEPT_NODE);
    
    // Check that analysis is stored in AtomSpace
    HandleSeq analysis_atoms = atomspace->get_handles_by_type(CONCEPT_NODE);
    bool found_analysis = false;
    for (const Handle& atom : analysis_atoms) {
        if (atom->get_name().find("PlanningEffectivenessAnalysis") != std::string::npos) {
            found_analysis = true;
            break;
        }
    }
    EXPECT_TRUE(found_analysis);
}

// Test strategy optimization
TEST_F(MetaPlannerTest, StrategyOptimization) {
    PlanningStrategy original_strategy = meta_planner->getCurrentStrategy();
    
    // Test optimization with context
    PlanningStrategy optimized_strategy = meta_planner->optimizePlanningStrategy(test_context);
    
    // Strategy should be valid
    EXPECT_NE(optimized_strategy, static_cast<PlanningStrategy>(-1));
    
    // Test optimization without context
    PlanningStrategy fallback_strategy = meta_planner->optimizePlanningStrategy(Handle::UNDEFINED);
    EXPECT_EQ(fallback_strategy, meta_planner->getCurrentStrategy());
}

// Test planning episode recording
TEST_F(MetaPlannerTest, PlanningEpisodeRecording) {
    Handle episode = atomspace->add_node(CONCEPT_NODE, "TestPlanningEpisode");
    
    // Record successful episode
    meta_planner->recordPlanningEpisode(episode, true, std::chrono::milliseconds(1000));
    
    // Check that metrics are updated
    auto metrics = meta_planner->getCurrentMetrics();
    EXPECT_GT(metrics.success_rate, 0.0);
    EXPECT_GT(metrics.average_execution_time, 0.0);
    
    // Record failed episode
    Handle episode2 = atomspace->add_node(CONCEPT_NODE, "TestPlanningEpisode2");
    meta_planner->recordPlanningEpisode(episode2, false, std::chrono::milliseconds(2000));
    
    // Success rate should be affected
    auto updated_metrics = meta_planner->getCurrentMetrics();
    EXPECT_LT(updated_metrics.success_rate, 1.0);
}

// Test reflection cycle
TEST_F(MetaPlannerTest, ReflectionCycle) {
    // Record some episodes first
    for (int i = 0; i < 5; i++) {
        Handle episode = atomspace->add_node(CONCEPT_NODE, "Episode_" + std::to_string(i));
        bool success = (i % 2 == 0); // Alternate success/failure
        meta_planner->recordPlanningEpisode(episode, success, std::chrono::milliseconds(1000 + i * 100));
    }
    
    // Trigger reflection
    Handle reflection_result = meta_planner->triggerReflection();
    
    EXPECT_NE(reflection_result, Handle::UNDEFINED);
    EXPECT_EQ(reflection_result->get_type(), CONCEPT_NODE);
    
    // Check that reflection results are stored
    HandleSeq reflection_atoms = atomspace->get_handles_by_type(CONCEPT_NODE);
    bool found_reflection = false;
    for (const Handle& atom : reflection_atoms) {
        if (atom->get_name().find("MetaPlanningReflection") != std::string::npos) {
            found_reflection = true;
            break;
        }
    }
    EXPECT_TRUE(found_reflection);
}

// Test learning optimization patterns
TEST_F(MetaPlannerTest, LearningOptimizationPatterns) {
    // Record several successful episodes
    for (int i = 0; i < 10; i++) {
        Handle episode = atomspace->add_node(CONCEPT_NODE, "LearningEpisode_" + std::to_string(i));
        meta_planner->recordPlanningEpisode(episode, true, std::chrono::milliseconds(800 + i * 50));
    }
    
    // Learn patterns
    int patterns_learned = meta_planner->learnOptimizationPatterns(10);
    EXPECT_GE(patterns_learned, 0);
    
    // Test applying optimizations
    int optimizations_applied = meta_planner->applyOptimizations(test_context);
    EXPECT_GE(optimizations_applied, 0);
}

// Test performance trend analysis
TEST_F(MetaPlannerTest, PerformanceTrendAnalysis) {
    // Record episodes with improving performance
    for (int i = 0; i < 5; i++) {
        Handle episode = atomspace->add_node(CONCEPT_NODE, "TrendEpisode_" + std::to_string(i));
        bool success = (i >= 2); // Later episodes are more successful
        meta_planner->recordPlanningEpisode(episode, success, std::chrono::milliseconds(1000 - i * 100));
    }
    
    // Analyze trend
    Handle trend_result = meta_planner->getPerformanceTrend(std::chrono::hours(1));
    
    EXPECT_NE(trend_result, Handle::UNDEFINED);
    EXPECT_EQ(trend_result->get_type(), CONCEPT_NODE);
}

// Test configuration methods
TEST_F(MetaPlannerTest, Configuration) {
    // Test parameter configuration
    meta_planner->configure(0.2, 0.1, true);
    
    // Test reflection interval setting
    meta_planner->setReflectionInterval(std::chrono::seconds(30));
    
    // Configuration should not break initialization
    EXPECT_TRUE(meta_planner->isInitialized());
}

// Test metrics reset
TEST_F(MetaPlannerTest, MetricsReset) {
    // Record some episodes
    Handle episode = atomspace->add_node(CONCEPT_NODE, "ResetTestEpisode");
    meta_planner->recordPlanningEpisode(episode, true, std::chrono::milliseconds(1000));
    
    // Verify metrics are non-zero
    auto metrics_before = meta_planner->getCurrentMetrics();
    EXPECT_GT(metrics_before.success_rate, 0.0);
    
    // Reset metrics
    meta_planner->resetMetrics();
    
    // Verify metrics are reset
    auto metrics_after = meta_planner->getCurrentMetrics();
    EXPECT_EQ(metrics_after.success_rate, 0.0);
    EXPECT_EQ(metrics_after.average_execution_time, 0.0);
}

// Test component integration
TEST_F(MetaPlannerTest, ComponentIntegration) {
    // Create mock components
    auto task_manager = std::make_shared<TaskManager>(agent_core, atomspace);
    auto action_scheduler = std::make_shared<ActionScheduler>(agent_core, atomspace);
    
    // Set component references
    meta_planner->setComponentReferences(task_manager, action_scheduler);
    
    // MetaPlanner should still be initialized
    EXPECT_TRUE(meta_planner->isInitialized());
}

// Test utility methods
TEST_F(MetaPlannerTest, UtilityMethods) {
    // Test strategy to string conversion
    EXPECT_EQ(MetaPlanner::strategyToString(PlanningStrategy::GREEDY), "Greedy");
    EXPECT_EQ(MetaPlanner::strategyToString(PlanningStrategy::HIERARCHICAL), "Hierarchical");
    EXPECT_EQ(MetaPlanner::strategyToString(PlanningStrategy::TEMPORAL), "Temporal");
    EXPECT_EQ(MetaPlanner::strategyToString(PlanningStrategy::ADAPTIVE), "Adaptive");
    EXPECT_EQ(MetaPlanner::strategyToString(PlanningStrategy::LEARNING_BASED), "LearningBased");
    EXPECT_EQ(MetaPlanner::strategyToString(PlanningStrategy::HYBRID), "Hybrid");
    
    // Test objective to string conversion
    EXPECT_EQ(MetaPlanner::objectiveToString(OptimizationObjective::MINIMIZE_TIME), "MinimizeTime");
    EXPECT_EQ(MetaPlanner::objectiveToString(OptimizationObjective::MINIMIZE_RESOURCES), "MinimizeResources");
    EXPECT_EQ(MetaPlanner::objectiveToString(OptimizationObjective::MAXIMIZE_SUCCESS), "MaximizeSuccess");
    EXPECT_EQ(MetaPlanner::objectiveToString(OptimizationObjective::MINIMIZE_COMPLEXITY), "MinimizeComplexity");
    EXPECT_EQ(MetaPlanner::objectiveToString(OptimizationObjective::BALANCED), "Balanced");
}

// Test error handling
TEST_F(MetaPlannerTest, ErrorHandling) {
    // Test with undefined context
    Handle analysis_result = meta_planner->analyzePlanningEffectiveness(Handle::UNDEFINED);
    EXPECT_EQ(analysis_result, Handle::UNDEFINED);
    
    // Test recording undefined episode
    meta_planner->recordPlanningEpisode(Handle::UNDEFINED, true, std::chrono::milliseconds(1000));
    // Should not crash
    
    // Test with invalid handles
    PlanningStrategy strategy = meta_planner->optimizePlanningStrategy(Handle::UNDEFINED);
    EXPECT_EQ(strategy, meta_planner->getCurrentStrategy());
}

// Performance test for multiple episodes
TEST_F(MetaPlannerTest, PerformanceWithManyEpisodes) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Record many episodes
    for (int i = 0; i < 100; i++) {
        Handle episode = atomspace->add_node(CONCEPT_NODE, "PerfEpisode_" + std::to_string(i));
        bool success = (i % 3 != 0); // ~67% success rate
        meta_planner->recordPlanningEpisode(episode, success, std::chrono::milliseconds(500 + i));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should complete within reasonable time (less than 1 second for 100 episodes)
    EXPECT_LT(duration.count(), 1000);
    
    // Verify final metrics are reasonable
    auto final_metrics = meta_planner->getCurrentMetrics();
    EXPECT_GT(final_metrics.success_rate, 0.5);
    EXPECT_LT(final_metrics.success_rate, 1.0);
}

// Integration test with AtomSpace patterns
TEST_F(MetaPlannerTest, AtomSpaceIntegration) {
    // Record an episode and check AtomSpace content
    Handle episode = atomspace->add_node(CONCEPT_NODE, "AtomSpaceTestEpisode");
    meta_planner->recordPlanningEpisode(episode, true, std::chrono::milliseconds(1500));
    
    // Check for planning-related atoms in AtomSpace
    HandleSeq evaluation_links = atomspace->get_handles_by_type(EVALUATION_LINK);
    bool found_episode_record = false;
    
    for (const Handle& eval : evaluation_links) {
        if (eval->get_arity() >= 2) {
            Handle predicate = eval->getOutgoingAtom(0);
            if (predicate->get_name() == "planning_episode_recorded") {
                found_episode_record = true;
                break;
            }
        }
    }
    
    EXPECT_TRUE(found_episode_record);
    
    // Check for context atoms
    HandleSeq concept_nodes = atomspace->get_handles_by_type(CONCEPT_NODE);
    bool found_metaplanning_context = false;
    
    for (const Handle& concept : concept_nodes) {
        if (concept->get_name() == "MetaPlanningContext") {
            found_metaplanning_context = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_metaplanning_context);
}

// Test strategy effectiveness scoring
TEST_F(MetaPlannerTest, StrategyEffectivenessScoring) {
    // Test different optimization objectives affect scoring
    meta_planner->setOptimizationObjective(OptimizationObjective::MAXIMIZE_SUCCESS);
    
    // Record high-success, slow episodes
    for (int i = 0; i < 5; i++) {
        Handle episode = atomspace->add_node(CONCEPT_NODE, "SuccessEpisode_" + std::to_string(i));
        meta_planner->recordPlanningEpisode(episode, true, std::chrono::milliseconds(5000));
    }
    
    auto success_metrics = meta_planner->getCurrentMetrics();
    
    // Switch to time optimization
    meta_planner->setOptimizationObjective(OptimizationObjective::MINIMIZE_TIME);
    
    // Record fast but failing episodes  
    for (int i = 0; i < 5; i++) {
        Handle episode = atomspace->add_node(CONCEPT_NODE, "FastEpisode_" + std::to_string(i));
        meta_planner->recordPlanningEpisode(episode, false, std::chrono::milliseconds(100));
    }
    
    // Metrics should reflect the mixed performance
    auto final_metrics = meta_planner->getCurrentMetrics();
    EXPECT_NE(final_metrics.success_rate, success_metrics.success_rate);
    EXPECT_NE(final_metrics.average_execution_time, success_metrics.average_execution_time);
}

// Test concurrent access safety (basic thread safety)
TEST_F(MetaPlannerTest, BasicThreadSafety) {
    // This is a basic test - full thread safety would require more complex testing
    std::vector<std::thread> threads;
    
    // Record episodes from multiple "threads" (simulated)
    for (int t = 0; t < 3; t++) {
        for (int i = 0; i < 10; i++) {
            Handle episode = atomspace->add_node(CONCEPT_NODE, 
                "ThreadEpisode_" + std::to_string(t) + "_" + std::to_string(i));
            meta_planner->recordPlanningEpisode(episode, (i % 2 == 0), std::chrono::milliseconds(1000));
        }
    }
    
    // Should not crash and should have recorded all episodes
    auto metrics = meta_planner->getCurrentMetrics();
    EXPECT_GT(metrics.success_rate, 0.0);
    EXPECT_LT(metrics.success_rate, 1.0); // Mixed success/failure
}

// Test plan quality recording and MetaPlanner self-optimisation hook (AZ-PLAN-004)
TEST_F(MetaPlannerTest, PlanQualityRecording) {
    // Initial average plan quality should be zero
    EXPECT_FLOAT_EQ(meta_planner->getAveragePlanQuality(), 0.0f);

    // Record a high-quality plan
    MetaPlanner::PlanQualityReport good_report;
    good_report.optimality_score        = 0.9f;
    good_report.goal_coverage           = 0.95f;
    good_report.temporal_satisfaction   = 1.0f;
    good_report.resource_efficiency     = 0.85f;
    good_report.robustness_score        = 0.88f;
    good_report.action_count            = 3;
    good_report.average_action_confidence = 0.9f;
    good_report.planning_time           = std::chrono::milliseconds(50);

    meta_planner->recordPlanQuality(good_report, test_context);

    float avg_after_one = meta_planner->getAveragePlanQuality();
    EXPECT_GT(avg_after_one, 0.0f);
    EXPECT_LE(avg_after_one, 1.0f);

    // Overall score of good_report is ~0.916 — average should be close
    EXPECT_NEAR(avg_after_one, good_report.overallScore(), 0.2f);

    // Record a low-quality plan; the average should decrease
    MetaPlanner::PlanQualityReport poor_report;
    poor_report.optimality_score        = 0.1f;
    poor_report.goal_coverage           = 0.2f;
    poor_report.temporal_satisfaction   = 0.3f;
    poor_report.resource_efficiency     = 0.15f;
    poor_report.robustness_score        = 0.1f;
    poor_report.action_count            = 15;
    poor_report.average_action_confidence = 0.2f;
    poor_report.planning_time           = std::chrono::milliseconds(500);

    meta_planner->recordPlanQuality(poor_report, test_context);

    float avg_after_two = meta_planner->getAveragePlanQuality();
    EXPECT_LT(avg_after_two, avg_after_one);
    EXPECT_GT(avg_after_two, 0.0f);
}

// Test overallScore() helper of PlanQualityReport
TEST_F(MetaPlannerTest, PlanQualityReportOverallScore) {
    MetaPlanner::PlanQualityReport report;
    report.optimality_score      = 1.0f;
    report.goal_coverage         = 1.0f;
    report.temporal_satisfaction = 1.0f;
    report.resource_efficiency   = 1.0f;
    report.robustness_score      = 1.0f;
    EXPECT_FLOAT_EQ(report.overallScore(), 1.0f);

    MetaPlanner::PlanQualityReport zero_report;
    EXPECT_FLOAT_EQ(zero_report.overallScore(), 0.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}