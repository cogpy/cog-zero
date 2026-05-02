/*
 * ExperienceManagerDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of ExperienceManager functionality
 * Part of the AGENT-ZERO-GENESIS project - Phase 5: Learning & Adaptation
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/learning/ExperienceManager.h"

using namespace opencog;
using namespace opencog::agentzero::learning;

// Mock AgentZeroCore for demonstration
class MockAgentZeroCore {
public:
    MockAgentZeroCore() = default;
    ~MockAgentZeroCore() = default;
};

void demonstrateBasicUsage(ExperienceManager& exp_manager, AtomSpacePtr atomspace) {
    std::cout << "\n=== Basic Experience Recording Demo ===" << std::endl;
    
    // Create some environmental context atoms
    Handle clean_env = atomspace->add_node(CONCEPT_NODE, "CleanEnvironment");
    Handle moving_target = atomspace->add_node(CONCEPT_NODE, "MovingTarget");
    Handle robot_ready = atomspace->add_node(CONCEPT_NODE, "RobotReady");
    
    // Create action atoms
    Handle move_forward = atomspace->add_node(CONCEPT_NODE, "MoveForward");
    Handle turn_left = atomspace->add_node(CONCEPT_NODE, "TurnLeft");
    Handle grab_object = atomspace->add_node(CONCEPT_NODE, "GrabObject");
    
    // Create consequence atoms
    Handle reached_target = atomspace->add_node(CONCEPT_NODE, "ReachedTarget");
    Handle avoided_obstacle = atomspace->add_node(CONCEPT_NODE, "AvoidedObstacle");
    Handle task_completed = atomspace->add_node(CONCEPT_NODE, "TaskCompleted");
    
    // Record several different types of experiences
    
    // 1. Successful navigation experience
    ExperienceManager::ExperienceContext nav_context;
    nav_context.timestamp = std::chrono::system_clock::now();
    nav_context.environmental_state = {clean_env, moving_target};
    nav_context.agent_state = {robot_ready};
    nav_context.confidence_level = 0.9;
    
    Handle nav_exp = exp_manager.recordExperience(
        "Successfully navigated to moving target",
        ExperienceManager::ExperienceType::PROBLEM_SOLVING,
        ExperienceManager::ExperienceOutcome::SUCCESS,
        nav_context,
        {move_forward, turn_left},
        {reached_target, avoided_obstacle}
    );
    
    std::cout << "✅ Navigation experience recorded: " << nav_exp->to_string() << std::endl;
    
    // 2. Failed grab attempt
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay for different timestamp
    
    Handle fail_exp = exp_manager.recordExperience(
        "Failed to grab object - target moved",
        ExperienceManager::ExperienceType::ACTION_OUTCOME,
        ExperienceManager::ExperienceOutcome::FAILURE
    );
    
    std::cout << "✅ Failure experience recorded: " << fail_exp->to_string() << std::endl;
    
    // 3. Unexpected outcome
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ExperienceManager::ExperienceContext unexpected_context;
    unexpected_context.timestamp = std::chrono::system_clock::now();
    unexpected_context.environmental_state = {clean_env};
    unexpected_context.agent_state = {robot_ready};
    unexpected_context.confidence_level = 0.6;
    
    Handle unexpected_exp = exp_manager.recordExperience(
        "Attempted simple move but discovered hidden door",
        ExperienceManager::ExperienceType::UNEXPECTED,
        ExperienceManager::ExperienceOutcome::UNEXPECTED_OUTCOME,
        unexpected_context,
        {move_forward},
        {atomspace->add_node(CONCEPT_NODE, "DiscoveredHiddenDoor")}
    );
    
    std::cout << "✅ Unexpected experience recorded: " << unexpected_exp->to_string() << std::endl;
    
    // 4. Learning episode
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Handle learning_exp = exp_manager.recordExperience(
        "Learned that moving targets require prediction",
        ExperienceManager::ExperienceType::LEARNING_EPISODE,
        ExperienceManager::ExperienceOutcome::LEARNING_OPPORTUNITY
    );
    
    std::cout << "✅ Learning experience recorded: " << learning_exp->to_string() << std::endl;
}

void demonstratePatternDiscovery(ExperienceManager& exp_manager) {
    std::cout << "\n=== Pattern Discovery Demo ===" << std::endl;
    
    // Record a series of related experiences to create patterns
    for (int i = 0; i < 5; ++i) {
        std::string desc = "Navigation attempt " + std::to_string(i + 1);
        auto outcome = (i % 3 == 0) ? ExperienceManager::ExperienceOutcome::SUCCESS :
                      (i % 3 == 1) ? ExperienceManager::ExperienceOutcome::FAILURE :
                                     ExperienceManager::ExperienceOutcome::PARTIAL_SUCCESS;
        
        exp_manager.recordExperience(
            desc,
            ExperienceManager::ExperienceType::ACTION_OUTCOME,
            outcome
        );
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "✅ Recorded 5 navigation experiences with mixed outcomes" << std::endl;
    
    // Discover patterns
    size_t patterns_found = exp_manager.discoverExperiencePatterns(
        ExperienceManager::ExperienceType::ACTION_OUTCOME
    );
    
    std::cout << "✅ Pattern discovery completed - found " << patterns_found << " patterns" << std::endl;
}

void demonstrateExperienceRetrieval(ExperienceManager& exp_manager, AtomSpacePtr atomspace) {
    std::cout << "\n=== Experience Retrieval Demo ===" << std::endl;
    
    // Get statistics
    auto stats = exp_manager.getExperienceStatistics();
    std::cout << "Experience Statistics:" << std::endl;
    for (const auto& stat : stats) {
        std::cout << "  Type " << static_cast<int>(stat.first) << ": " 
                  << stat.second << " experiences" << std::endl;
    }
    
    // Retrieve successful experiences
    std::vector<Handle> successful_exp = exp_manager.getExperiencesByOutcome(
        ExperienceManager::ExperienceOutcome::SUCCESS, 3
    );
    
    std::cout << "✅ Retrieved " << successful_exp.size() << " successful experiences:" << std::endl;
    for (const Handle& exp : successful_exp) {
        std::cout << "  - " << exp->to_string() << std::endl;
    }
    
    // Retrieve failed experiences
    std::vector<Handle> failed_exp = exp_manager.getExperiencesByOutcome(
        ExperienceManager::ExperienceOutcome::FAILURE, 3
    );
    
    std::cout << "✅ Retrieved " << failed_exp.size() << " failed experiences:" << std::endl;
    for (const Handle& exp : failed_exp) {
        std::cout << "  - " << exp->to_string() << std::endl;
    }
    
    // Context-based retrieval
    Handle current_env = atomspace->add_node(CONCEPT_NODE, "CleanEnvironment");
    std::vector<Handle> context = {current_env};
    
    std::vector<Handle> similar_exp = exp_manager.getSimilarExperiences(
        context, 
        ExperienceManager::ExperienceType::PROBLEM_SOLVING, 
        3
    );
    
    std::cout << "✅ Retrieved " << similar_exp.size() << " context-similar experiences" << std::endl;
}

void demonstrateAnalysisAndExport(ExperienceManager& exp_manager) {
    std::cout << "\n=== Analysis and Export Demo ===" << std::endl;
    
    // Get recent learning insights
    std::vector<Handle> insights = exp_manager.getRecentLearningInsights(1); // Last 1 day
    std::cout << "✅ Found " << insights.size() << " recent learning insights" << std::endl;
    
    // Export experiences to JSON
    std::string json_export = exp_manager.exportExperiences(
        ExperienceManager::ExperienceType::ACTION_OUTCOME
    );
    
    std::cout << "✅ Experience export successful" << std::endl;
    std::cout << "JSON Export Sample (first 300 chars):" << std::endl;
    std::cout << json_export.substr(0, 300) << "..." << std::endl;
    
    // Get configuration status
    std::string config_status = exp_manager.getConfigurationStatus();
    std::cout << "\n" << config_status << std::endl;
}

void demonstrateProcessingCycle(ExperienceManager& exp_manager) {
    std::cout << "\n=== Experience Management Processing Demo ===" << std::endl;
    
    // Simulate periodic processing
    bool processing_success = exp_manager.processExperienceManagement();
    
    if (processing_success) {
        std::cout << "✅ Experience management processing completed successfully" << std::endl;
    } else {
        std::cout << "❌ Experience management processing failed" << std::endl;
    }
    
    // Demonstrate memory management
    size_t pruned_count = exp_manager.pruneOldExperiences();
    std::cout << "✅ Pruned " << pruned_count << " old experiences" << std::endl;
}

int main() {
    std::cout << "=== ExperienceManager Comprehensive Demo ===" << std::endl;
    std::cout << "Part of AGENT-ZERO-GENESIS Phase 5: Learning & Adaptation" << std::endl;
    
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    try {
        // Create AtomSpace and mock agent core
        AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
        MockAgentZeroCore mock_core;
        
        // Create ExperienceManager
        ExperienceManager exp_manager(reinterpret_cast<AgentZeroCore*>(&mock_core), atomspace);
        
        std::cout << "\n🎯 ExperienceManager initialized successfully" << std::endl;
        
        // Run demonstrations
        demonstrateBasicUsage(exp_manager, atomspace);
        demonstratePatternDiscovery(exp_manager);
        demonstrateExperienceRetrieval(exp_manager, atomspace);
        demonstrateAnalysisAndExport(exp_manager);
        demonstrateProcessingCycle(exp_manager);
        
        std::cout << "\n🎉 All ExperienceManager demonstrations completed successfully!" << std::endl;
        std::cout << "\nThis demonstrates the core functionality of Phase 5 Learning & Adaptation" << std::endl;
        std::cout << "See README.md for detailed usage instructions and integration examples." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "\n❌ Demo failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "\n❌ Demo failed with unknown exception" << std::endl;
        return 1;
    }
}