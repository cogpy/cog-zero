/*
 * examples/MetaPlannerDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MetaPlanner Integration Demo
 * Demonstrates self-reflective planning optimization with OpenCog integration
 * Part of AZ-PLAN-003: Implement MetaPlanner for self-optimization
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <random>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/MetaPlanner.h"

using namespace opencog;
using namespace opencog::agentzero;

/**
 * Demo simulation of a planning task with varying complexity and success rates
 */
class PlanningSimulator {
private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<> success_dist;
    std::uniform_int_distribution<> time_dist;
    
public:
    PlanningSimulator() : gen(rd()), success_dist(0.0, 1.0), time_dist(500, 3000) {}
    
    struct SimulationResult {
        bool success;
        std::chrono::milliseconds execution_time;
        double complexity_score;
    };
    
    SimulationResult simulatePlanning(MetaPlanner::PlanningStrategy strategy, 
                                     const std::string& context_type) {
        SimulationResult result;
        
        // Simulate strategy-specific performance characteristics
        double base_success_rate = 0.7;
        int base_time = 1000;
        
        switch (strategy) {
            case MetaPlanner::PlanningStrategy::GREEDY:
                base_success_rate = 0.6;  // Lower success but faster
                base_time = 600;
                break;
            case MetaPlanner::PlanningStrategy::HIERARCHICAL:
                base_success_rate = 0.9;  // Higher success but slower
                base_time = 1800;
                break;
            case MetaPlanner::PlanningStrategy::TEMPORAL:
                base_success_rate = 0.8;
                base_time = context_type == "time_critical" ? 700 : 1200;
                break;
            case MetaPlanner::PlanningStrategy::ADAPTIVE:
                base_success_rate = 0.75;
                base_time = 1100;
                break;
            case MetaPlanner::PlanningStrategy::LEARNING_BASED:
                base_success_rate = 0.85;  // Improves with experience
                base_time = 1000;
                break;
            case MetaPlanner::PlanningStrategy::HYBRID:
                base_success_rate = 0.8;
                base_time = 1300;
                break;
        }
        
        // Context-specific adjustments
        if (context_type == "complex_goal") {
            if (strategy == MetaPlanner::PlanningStrategy::HIERARCHICAL) {
                base_success_rate += 0.1;  // Hierarchical excels at complex goals
            } else if (strategy == MetaPlanner::PlanningStrategy::GREEDY) {
                base_success_rate -= 0.2;  // Greedy struggles with complexity
            }
        } else if (context_type == "time_critical") {
            if (strategy == MetaPlanner::PlanningStrategy::GREEDY || 
                strategy == MetaPlanner::PlanningStrategy::TEMPORAL) {
                base_success_rate += 0.1;  // These strategies handle time pressure well
                base_time = static_cast<int>(base_time * 0.8);
            }
        }
        
        // Add some randomness
        result.success = success_dist(gen) < base_success_rate;
        result.execution_time = std::chrono::milliseconds(
            base_time + static_cast<int>((success_dist(gen) - 0.5) * 400));
        result.complexity_score = success_dist(gen);
        
        return result;
    }
};

/**
 * Demo planning context generator
 */
class ContextGenerator {
private:
    std::vector<std::string> context_types = {
        "simple_task", "complex_goal", "time_critical", 
        "resource_constrained", "uncertain_environment"
    };
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> type_dist;
    
public:
    ContextGenerator() : gen(rd()), type_dist(0, context_types.size() - 1) {}
    
    std::pair<Handle, std::string> generateContext(AtomSpacePtr atomspace, int episode_num) {
        std::string context_type = context_types[type_dist(gen)];
        std::string context_name = "PlanningContext_" + context_type + "_" + std::to_string(episode_num);
        
        Handle context = atomspace->add_node(CONCEPT_NODE, context_name);
        context->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
        
        // Add context type information
        Handle type_pred = atomspace->add_node(PREDICATE_NODE, "context_type");
        Handle type_value = atomspace->add_node(CONCEPT_NODE, context_type);
        Handle type_eval = atomspace->add_link(EVALUATION_LINK, {type_pred, context, type_value});
        type_eval->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
        
        return {context, context_type};
    }
};

/**
 * Main demonstration function
 */
void runMetaPlannerDemo() {
    std::cout << "=== MetaPlanner Self-Optimization Demo ===" << std::endl;
    std::cout << "Demonstrating adaptive planning strategy optimization" << std::endl << std::endl;
    
    // Initialize OpenCog AtomSpace
    auto atomspace = std::make_shared<AtomSpace>();
    
    // Create MetaPlanner (AgentZeroCore not needed for this demo)
    AgentZeroCore* agent_core = nullptr;
    auto meta_planner = std::make_unique<MetaPlanner>(agent_core, atomspace);
    
    // Configure for active learning
    meta_planner->configure(0.15, 0.1, true);  // learning_rate, adaptation_threshold, temporal_opt
    meta_planner->setReflectionInterval(std::chrono::seconds(10));
    
    // Initialize simulation components
    PlanningSimulator simulator;
    ContextGenerator context_gen;
    
    std::cout << "Initial Configuration:" << std::endl;
    std::cout << "- Strategy: " << MetaPlanner::strategyToString(meta_planner->getCurrentStrategy()) << std::endl;
    std::cout << "- Objective: " << MetaPlanner::objectiveToString(meta_planner->getOptimizationObjective()) << std::endl;
    std::cout << "- Learning Rate: 0.15" << std::endl;
    std::cout << "- Adaptation Threshold: 0.1" << std::endl << std::endl;
    
    // Simulation parameters
    const int total_episodes = 50;
    const int reflection_interval = 10;
    
    std::cout << "Running " << total_episodes << " planning episodes..." << std::endl;
    std::cout << "Strategy adaptations will occur automatically based on performance." << std::endl << std::endl;
    
    // Track strategy changes for analysis
    MetaPlanner::PlanningStrategy last_strategy = meta_planner->getCurrentStrategy();
    int strategy_changes = 0;
    
    // Main simulation loop
    for (int episode = 1; episode <= total_episodes; ++episode) {
        // Generate planning context
        auto [context, context_type] = context_gen.generateContext(atomspace, episode);
        
        // Get optimized strategy for this context
        auto strategy = meta_planner->optimizePlanningStrategy(context);
        
        // Track strategy changes
        if (strategy != last_strategy) {
            strategy_changes++;
            std::cout << "Episode " << episode << ": Strategy adapted from " 
                     << MetaPlanner::strategyToString(last_strategy) << " to " 
                     << MetaPlanner::strategyToString(strategy) << std::endl;
            last_strategy = strategy;
        }
        
        // Simulate planning execution
        auto result = simulator.simulatePlanning(strategy, context_type);
        
        // Record episode for learning
        Handle episode_atom = atomspace->add_node(CONCEPT_NODE, "PlanningEpisode_" + std::to_string(episode));
        meta_planner->recordPlanningEpisode(episode_atom, result.success, result.execution_time);
        
        // Periodic status updates
        if (episode % 10 == 0) {
            auto metrics = meta_planner->getCurrentMetrics();
            std::cout << "Episode " << episode << " Status:" << std::endl;
            std::cout << "- Success Rate: " << std::fixed << std::setprecision(2) 
                     << (metrics.success_rate * 100) << "%" << std::endl;
            std::cout << "- Avg Execution Time: " << std::fixed << std::setprecision(0) 
                     << metrics.average_execution_time << "ms" << std::endl;
            std::cout << "- Current Strategy: " << MetaPlanner::strategyToString(strategy) << std::endl;
            std::cout << std::endl;
        }
        
        // Trigger reflection periodically
        if (episode % reflection_interval == 0) {
            std::cout << "--- Reflection Cycle " << (episode / reflection_interval) << " ---" << std::endl;
            Handle reflection = meta_planner->triggerReflection();
            
            if (reflection != Handle::UNDEFINED) {
                std::cout << "Reflection completed: Performance analysis stored in AtomSpace" << std::endl;
            }
            
            // Learn optimization patterns
            int patterns = meta_planner->learnOptimizationPatterns(reflection_interval);
            if (patterns > 0) {
                std::cout << "Learned " << patterns << " new optimization patterns" << std::endl;
            }
            std::cout << std::endl;
        }
        
        // Small delay to make the demo more observable
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Final analysis
    std::cout << "=== Final Analysis ===" << std::endl;
    
    auto final_metrics = meta_planner->getCurrentMetrics();
    std::cout << "Final Performance Metrics:" << std::endl;
    std::cout << "- Success Rate: " << std::fixed << std::setprecision(2) 
             << (final_metrics.success_rate * 100) << "%" << std::endl;
    std::cout << "- Average Execution Time: " << std::fixed << std::setprecision(0) 
             << final_metrics.average_execution_time << "ms" << std::endl;
    std::cout << "- Final Strategy: " << MetaPlanner::strategyToString(meta_planner->getCurrentStrategy()) << std::endl;
    std::cout << "- Total Strategy Changes: " << strategy_changes << std::endl;
    
    // Test different optimization objectives
    std::cout << std::endl << "=== Testing Different Optimization Objectives ===" << std::endl;
    
    std::vector<MetaPlanner::OptimizationObjective> objectives = {
        MetaPlanner::OptimizationObjective::MINIMIZE_TIME,
        MetaPlanner::OptimizationObjective::MAXIMIZE_SUCCESS,
        MetaPlanner::OptimizationObjective::BALANCED
    };
    
    Handle test_context = atomspace->add_node(CONCEPT_NODE, "TestContext");
    
    for (auto objective : objectives) {
        meta_planner->setOptimizationObjective(objective);
        auto recommended_strategy = meta_planner->optimizePlanningStrategy(test_context);
        
        std::cout << "Objective: " << MetaPlanner::objectiveToString(objective) 
                 << " -> Recommended Strategy: " << MetaPlanner::strategyToString(recommended_strategy) << std::endl;
    }
    
    // Strategy evaluation summary
    std::cout << std::endl << "=== Strategy Evaluation Summary ===" << std::endl;
    
    std::vector<MetaPlanner::PlanningStrategy> all_strategies = {
        MetaPlanner::PlanningStrategy::GREEDY,
        MetaPlanner::PlanningStrategy::HIERARCHICAL,
        MetaPlanner::PlanningStrategy::TEMPORAL,
        MetaPlanner::PlanningStrategy::ADAPTIVE,
        MetaPlanner::PlanningStrategy::LEARNING_BASED,
        MetaPlanner::PlanningStrategy::HYBRID
    };
    
    for (auto strategy : all_strategies) {
        auto evaluation = meta_planner->getStrategyEvaluation(strategy);
        std::cout << MetaPlanner::strategyToString(strategy) << ": " 
                 << "Effectiveness = " << std::fixed << std::setprecision(3) 
                 << evaluation.effectiveness_score << std::endl;
    }
    
    // AtomSpace statistics
    std::cout << std::endl << "=== AtomSpace Integration ===" << std::endl;
    std::cout << "Total Atoms Created: " << atomspace->get_size() << std::endl;
    
    // Count different atom types
    auto concept_nodes = atomspace->get_handles_by_type(CONCEPT_NODE);
    auto evaluation_links = atomspace->get_handles_by_type(EVALUATION_LINK);
    
    std::cout << "Concept Nodes: " << concept_nodes->size() << std::endl;
    std::cout << "Evaluation Links: " << evaluation_links->size() << std::endl;
    
    std::cout << std::endl << "Demo completed successfully!" << std::endl;
    std::cout << "The MetaPlanner has demonstrated self-optimization capabilities" << std::endl;
    std::cout << "by adapting planning strategies based on performance feedback." << std::endl;
}

/**
 * Interactive demo mode
 */
void runInteractiveDemo() {
    std::cout << "=== Interactive MetaPlanner Demo ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    AgentZeroCore* agent_core = nullptr;
    auto meta_planner = std::make_unique<MetaPlanner>(agent_core, atomspace);
    
    std::string input;
    while (true) {
        std::cout << "\nInteractive Demo Commands:" << std::endl;
        std::cout << "1. Set strategy (greedy|hierarchical|temporal|adaptive|learning|hybrid)" << std::endl;
        std::cout << "2. Set objective (time|resources|success|complexity|balanced)" << std::endl;
        std::cout << "3. Record episode (success/failure)" << std::endl;
        std::cout << "4. Show metrics" << std::endl;
        std::cout << "5. Trigger reflection" << std::endl;
        std::cout << "6. Quit" << std::endl;
        std::cout << "Enter command: ";
        
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "6") {
            break;
        } else if (input.find("strategy") != std::string::npos || input == "1") {
            std::cout << "Enter strategy (greedy|hierarchical|temporal|adaptive|learning|hybrid): ";
            std::getline(std::cin, input);
            
            if (input == "greedy") {
                meta_planner->setStrategy(MetaPlanner::PlanningStrategy::GREEDY);
            } else if (input == "hierarchical") {
                meta_planner->setStrategy(MetaPlanner::PlanningStrategy::HIERARCHICAL);
            } else if (input == "temporal") {
                meta_planner->setStrategy(MetaPlanner::PlanningStrategy::TEMPORAL);
            } else if (input == "adaptive") {
                meta_planner->setStrategy(MetaPlanner::PlanningStrategy::ADAPTIVE);
            } else if (input == "learning") {
                meta_planner->setStrategy(MetaPlanner::PlanningStrategy::LEARNING_BASED);
            } else if (input == "hybrid") {
                meta_planner->setStrategy(MetaPlanner::PlanningStrategy::HYBRID);
            }
            
            std::cout << "Strategy set to: " << MetaPlanner::strategyToString(meta_planner->getCurrentStrategy()) << std::endl;
            
        } else if (input.find("objective") != std::string::npos || input == "2") {
            std::cout << "Enter objective (time|resources|success|complexity|balanced): ";
            std::getline(std::cin, input);
            
            if (input == "time") {
                meta_planner->setOptimizationObjective(MetaPlanner::OptimizationObjective::MINIMIZE_TIME);
            } else if (input == "resources") {
                meta_planner->setOptimizationObjective(MetaPlanner::OptimizationObjective::MINIMIZE_RESOURCES);
            } else if (input == "success") {
                meta_planner->setOptimizationObjective(MetaPlanner::OptimizationObjective::MAXIMIZE_SUCCESS);
            } else if (input == "complexity") {
                meta_planner->setOptimizationObjective(MetaPlanner::OptimizationObjective::MINIMIZE_COMPLEXITY);
            } else if (input == "balanced") {
                meta_planner->setOptimizationObjective(MetaPlanner::OptimizationObjective::BALANCED);
            }
            
            std::cout << "Objective set to: " << MetaPlanner::objectiveToString(meta_planner->getOptimizationObjective()) << std::endl;
            
        } else if (input.find("episode") != std::string::npos || input == "3") {
            std::cout << "Enter episode result (success/failure): ";
            std::getline(std::cin, input);
            
            bool success = (input == "success");
            
            std::cout << "Enter execution time (milliseconds): ";
            std::getline(std::cin, input);
            
            int time_ms = std::stoi(input);
            
            Handle episode = atomspace->add_node(CONCEPT_NODE, "InteractiveEpisode_" + std::to_string(std::time(nullptr)));
            meta_planner->recordPlanningEpisode(episode, success, std::chrono::milliseconds(time_ms));
            
            std::cout << "Episode recorded: " << (success ? "success" : "failure") 
                     << " in " << time_ms << "ms" << std::endl;
            
        } else if (input.find("metrics") != std::string::npos || input == "4") {
            auto metrics = meta_planner->getCurrentMetrics();
            std::cout << "Current Metrics:" << std::endl;
            std::cout << "- Success Rate: " << std::fixed << std::setprecision(2) 
                     << (metrics.success_rate * 100) << "%" << std::endl;
            std::cout << "- Avg Execution Time: " << std::fixed << std::setprecision(0) 
                     << metrics.average_execution_time << "ms" << std::endl;
            std::cout << "- Strategy: " << MetaPlanner::strategyToString(meta_planner->getCurrentStrategy()) << std::endl;
            std::cout << "- Objective: " << MetaPlanner::objectiveToString(meta_planner->getOptimizationObjective()) << std::endl;
            
        } else if (input.find("reflection") != std::string::npos || input == "5") {
            Handle reflection = meta_planner->triggerReflection();
            if (reflection != Handle::UNDEFINED) {
                std::cout << "Reflection completed - analysis stored in AtomSpace" << std::endl;
            } else {
                std::cout << "Reflection failed or no data available" << std::endl;
            }
        }
    }
    
    std::cout << "Interactive demo ended." << std::endl;
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--interactive") {
            runInteractiveDemo();
        } else {
            runMetaPlannerDemo();
        }
    } catch (const std::exception& e) {
        std::cerr << "Demo error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}