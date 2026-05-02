/**
 * experience_management_example.cpp - Example of Experience Management
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Demonstrates basic usage of ExperienceManager for storing and retrieving experiences
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <iostream>
#include <memory>

#include <agentzero/learning/ExperienceManager.h>
#include <agentzero/learning/LearningUtils.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

using namespace opencog;
using namespace opencog::agentzero::learning;

int main() {
    std::cout << "=== Agent-Zero Experience Management Example ===" << std::endl;
    
    // Configure logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        std::cout << "Created AtomSpace" << std::endl;
        
        // Create experience manager with custom configuration
        LearningConfig config = utils::getDefaultConfig("default");
        config.experience_buffer_size = 50;  // Small buffer for demo
        
        auto manager = std::make_unique<ExperienceManager>(atomspace, config);
        std::cout << "Created ExperienceManager with buffer size: " << config.experience_buffer_size << std::endl;
        
        // Create some test atoms representing states and actions
        std::vector<Handle> states;
        std::vector<Handle> actions;
        
        for (int i = 0; i < 5; ++i) {
            states.push_back(atomspace->add_node(CONCEPT_NODE, "State_" + std::to_string(i)));
            actions.push_back(atomspace->add_node(CONCEPT_NODE, "Action_" + std::to_string(i)));
        }
        
        std::cout << "Created " << states.size() << " states and " << actions.size() << " actions" << std::endl;
        
        // Simulate some experiences
        std::cout << "\nSimulating agent experiences..." << std::endl;
        
        std::vector<ExperienceId> experience_ids;
        
        for (int episode = 0; episode < 3; ++episode) {
            std::cout << "Episode " << (episode + 1) << ":" << std::endl;
            
            for (int step = 0; step < 4; ++step) {
                Handle current_state = states[step];
                Handle action = actions[step % actions.size()];
                Handle next_state = states[(step + 1) % states.size()];
                
                // Simulate varying rewards
                double reward = 0.1 * step + 0.2 * episode;
                bool terminal = (step == 3); // Last step is terminal
                
                ExperienceId exp_id = manager->addExperience(
                    current_state, action, next_state, reward, terminal
                );
                
                experience_ids.push_back(exp_id);
                
                std::cout << "  Step " << step << ": " << exp_id 
                          << " (reward: " << reward << ", terminal: " << terminal << ")" << std::endl;
            }
        }
        
        // Check experience count
        std::cout << "\nTotal experiences stored: " << manager->getExperienceCount() << std::endl;
        
        // Get experience statistics
        auto stats = manager->getExperienceStats();
        std::cout << "\nExperience Statistics:" << std::endl;
        for (const auto& pair : stats) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
        
        // Get reward statistics
        auto reward_stats = manager->getRewardStats();
        std::cout << "\nReward Statistics:" << std::endl;
        for (const auto& pair : reward_stats) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
        
        // Demonstrate experience retrieval by different criteria
        std::cout << "\n=== Experience Retrieval Examples ===" << std::endl;
        
        // Get recent experiences
        auto recent = manager->getRecentExperiences(5);
        std::cout << "Recent experiences (" << recent.size() << "):" << std::endl;
        for (const auto& exp : recent) {
            std::cout << "  " << exp->id << " - reward: " << exp->reward << std::endl;
        }
        
        // Get high-reward experiences
        auto high_reward = manager->getExperiencesByRewardRange(0.4, 1.0);
        std::cout << "\nHigh reward experiences (>= 0.4): " << high_reward.size() << std::endl;
        for (const auto& exp : high_reward) {
            std::cout << "  " << exp->id << " - reward: " << exp->reward << std::endl;
        }
        
        // Get experiences by state
        if (!states.empty()) {
            auto state_experiences = manager->getExperiencesByState(states[0]);
            std::cout << "\nExperiences involving State_0: " << state_experiences.size() << std::endl;
            for (const auto& exp : state_experiences) {
                std::cout << "  " << exp->id << " - reward: " << exp->reward << std::endl;
            }
        }
        
        // Get experiences by action
        if (!actions.empty()) {
            auto action_experiences = manager->getExperiencesByAction(actions[1]);
            std::cout << "\nExperiences involving Action_1: " << action_experiences.size() << std::endl;
            for (const auto& exp : action_experiences) {
                std::cout << "  " << exp->id << " - reward: " << exp->reward << std::endl;
            }
        }
        
        // Demonstrate experience sampling
        std::cout << "\n=== Experience Sampling ===" << std::endl;
        
        // Uniform random sampling
        auto uniform_sample = manager->sampleExperiences(3, false);
        std::cout << "Uniform random sample (" << uniform_sample.size() << "):" << std::endl;
        for (const auto& exp : uniform_sample) {
            std::cout << "  " << exp->id << " - reward: " << exp->reward << std::endl;
        }
        
        // Prioritized sampling (may fall back to uniform if not fully implemented)
        auto prioritized_sample = manager->sampleExperiences(3, true);
        std::cout << "\nPrioritized sample (" << prioritized_sample.size() << "):" << std::endl;
        for (const auto& exp : prioritized_sample) {
            std::cout << "  " << exp->id << " - reward: " << exp->reward << std::endl;
        }
        
        // Demonstrate AtomSpace storage
        std::cout << "\n=== AtomSpace Storage ===" << std::endl;
        
        size_t stored_count = manager->storeExperiencesToAtomSpace();
        std::cout << "Stored " << stored_count << " experiences to AtomSpace" << std::endl;
        
        // Demonstrate filtering
        std::cout << "\n=== Custom Filtering ===" << std::endl;
        
        auto terminal_experiences = manager->getExperiencesByFilter(
            [](const Experience& exp) { return exp.terminal; }
        );
        
        std::cout << "Terminal experiences: " << terminal_experiences.size() << std::endl;
        for (const auto& exp : terminal_experiences) {
            std::cout << "  " << exp->id << " - reward: " << exp->reward << " (terminal)" << std::endl;
        }
        
        // Test buffer management
        std::cout << "\n=== Buffer Management Test ===" << std::endl;
        
        std::cout << "Current buffer size: " << manager->getExperienceCount() << std::endl;
        std::cout << "Buffer limit: " << manager->getBufferSizeLimit() << std::endl;
        
        // Add more experiences to test buffer overflow
        std::cout << "Adding more experiences to test buffer overflow..." << std::endl;
        
        for (int i = 0; i < 60; ++i) {
            Handle state = atomspace->add_node(CONCEPT_NODE, "OverflowState_" + std::to_string(i));
            Handle action = atomspace->add_node(CONCEPT_NODE, "OverflowAction_" + std::to_string(i));
            Handle next_state = atomspace->add_node(CONCEPT_NODE, "OverflowNextState_" + std::to_string(i));
            
            manager->addExperience(state, action, next_state, 0.1, false);
        }
        
        std::cout << "Buffer size after overflow test: " << manager->getExperienceCount() << std::endl;
        std::cout << "Should be limited to: " << manager->getBufferSizeLimit() << std::endl;
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}