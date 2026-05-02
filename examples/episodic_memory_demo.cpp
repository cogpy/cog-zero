/*
 * episodic_memory_demo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of EpisodicMemory usage
 * Shows how to record episodes, retrieve them, and discover patterns
 * Part of AZ-MEM-001: Implement EpisodicMemory with temporal sequences
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/learning/EpisodicMemory.h"

using namespace opencog;
using namespace opencog::agentzero::learning;

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "🤖 EpisodicMemory Demonstration" << std::endl;
    std::cout << "================================" << std::endl;
    
    // 1. Initialize EpisodicMemory
    std::cout << "\n📚 Step 1: Initialize EpisodicMemory" << std::endl;
    auto atomspace = std::make_shared<AtomSpace>();
    auto episodic_memory = std::make_unique<EpisodicMemory>(atomspace);
    
    // Enable pattern discovery
    episodic_memory->enablePatternDiscovery(true);
    episodic_memory->enableTemporalReasoning(true);
    
    std::cout << "✓ EpisodicMemory initialized with AtomSpace" << std::endl;
    
    // 2. Create Action and Context Atoms
    std::cout << "\n🎭 Step 2: Create Action and Context Atoms" << std::endl;
    
    // Actions for a robot navigation scenario
    Handle move_forward = atomspace->add_node(CONCEPT_NODE, "move_forward");
    Handle turn_left = atomspace->add_node(CONCEPT_NODE, "turn_left");
    Handle turn_right = atomspace->add_node(CONCEPT_NODE, "turn_right");
    Handle scan_area = atomspace->add_node(CONCEPT_NODE, "scan_area");
    Handle grasp_object = atomspace->add_node(CONCEPT_NODE, "grasp_object");
    Handle place_object = atomspace->add_node(CONCEPT_NODE, "place_object");
    
    // Environmental contexts
    Handle indoor_env = atomspace->add_node(CONCEPT_NODE, "indoor_environment");
    Handle outdoor_env = atomspace->add_node(CONCEPT_NODE, "outdoor_environment");
    Handle obstacle_present = atomspace->add_node(CONCEPT_NODE, "obstacle_present");
    Handle object_nearby = atomspace->add_node(CONCEPT_NODE, "object_nearby");
    Handle target_location = atomspace->add_node(CONCEPT_NODE, "target_location_visible");
    
    // Goals
    Handle goal_navigate = atomspace->add_node(CONCEPT_NODE, "goal_navigate_to_target");
    Handle goal_collect = atomspace->add_node(CONCEPT_NODE, "goal_collect_objects");
    
    std::cout << "✓ Created 11 action and context atoms" << std::endl;
    
    // 3. Record Episode Sequences
    std::cout << "\n📝 Step 3: Record Episodic Sequences" << std::endl;
    
    // Episode 1: Simple navigation
    {
        std::vector<Handle> sequence = {move_forward, turn_left, move_forward};
        std::vector<Handle> context = {indoor_env, goal_navigate};
        
        std::string ep_id = episodic_memory->recordEpisode(
            "Simple indoor navigation", sequence, context);
        std::cout << "✓ Recorded Episode 1: " << ep_id << std::endl;
    }
    
    // Small delay to differentiate timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Episode 2: Navigation with obstacle avoidance
    {
        std::vector<Handle> sequence = {move_forward, scan_area, turn_right, move_forward, turn_left};
        std::vector<Handle> context = {indoor_env, obstacle_present, goal_navigate};
        
        std::string ep_id = episodic_memory->recordEpisode(
            "Navigation with obstacle avoidance", sequence, context);
        std::cout << "✓ Recorded Episode 2: " << ep_id << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Episode 3: Object collection sequence
    {
        std::vector<Handle> sequence = {move_forward, scan_area, grasp_object, turn_left, move_forward, place_object};
        std::vector<Handle> context = {indoor_env, object_nearby, goal_collect};
        
        std::string ep_id = episodic_memory->recordEpisode(
            "Object collection sequence", sequence, context);
        std::cout << "✓ Recorded Episode 3: " << ep_id << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Episode 4: Repeated navigation pattern
    {
        std::vector<Handle> sequence = {move_forward, turn_left, move_forward, turn_left};
        std::vector<Handle> context = {outdoor_env, goal_navigate};
        
        std::string ep_id = episodic_memory->recordEpisode(
            "Outdoor navigation pattern", sequence, context);
        std::cout << "✓ Recorded Episode 4: " << ep_id << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Episode 5: Another object collection (to create patterns)
    {
        std::vector<Handle> sequence = {scan_area, move_forward, grasp_object, turn_right, move_forward, place_object};
        std::vector<Handle> context = {indoor_env, object_nearby, goal_collect};
        
        std::string ep_id = episodic_memory->recordEpisode(
            "Second object collection", sequence, context);
        std::cout << "✓ Recorded Episode 5: " << ep_id << std::endl;
    }
    
    // 4. Display Memory Statistics
    std::cout << "\n📊 Step 4: Memory Statistics" << std::endl;
    auto stats = episodic_memory->getEpisodeStatistics();
    
    std::cout << "  Episodes stored: " << static_cast<int>(stats["total_episodes"]) << std::endl;
    std::cout << "  Average sequence length: " << stats["average_sequence_length"] << std::endl;
    std::cout << "  Average importance: " << stats["average_importance"] << std::endl;
    std::cout << "  Average coherence: " << stats["average_coherence"] << std::endl;
    std::cout << "  AtomSpace size: " << atomspace->get_size() << " atoms" << std::endl;
    
    // 5. Context-Based Retrieval
    std::cout << "\n🎯 Step 5: Context-Based Episode Retrieval" << std::endl;
    
    EpisodicMemory::EpisodeContext query_context;
    query_context.environmental_state = {indoor_env, object_nearby};
    query_context.agent_state = {goal_collect};
    query_context.query_time = std::chrono::system_clock::now();
    
    auto similar_episodes = episodic_memory->getSimilarEpisodes(query_context, 3);
    
    std::cout << "  Query: Indoor environment + Object nearby + Collection goal" << std::endl;
    std::cout << "  Found " << similar_episodes.size() << " similar episodes:" << std::endl;
    
    for (const auto& episode : similar_episodes) {
        std::cout << "    - " << episode.description 
                  << " (importance: " << std::fixed << std::setprecision(2) 
                  << episode.importance_score << ")" << std::endl;
    }
    
    // 6. Temporal Range Retrieval
    std::cout << "\n🕐 Step 6: Temporal Range Retrieval" << std::endl;
    
    auto now = std::chrono::system_clock::now();
    auto start_time = now - std::chrono::minutes(1);  // Last minute
    auto end_time = now + std::chrono::seconds(1);
    
    auto recent_episodes = episodic_memory->getEpisodesInTimeRange(start_time, end_time);
    
    std::cout << "  Episodes in the last minute: " << recent_episodes.size() << std::endl;
    for (size_t i = 0; i < recent_episodes.size() && i < 3; ++i) {
        const auto& episode = recent_episodes[i];
        std::cout << "    - " << episode.description 
                  << " (sequence length: " << episode.sequence_atoms.size() << ")" << std::endl;
    }
    
    // 7. Pattern Discovery
    std::cout << "\n🔍 Step 7: Pattern Discovery" << std::endl;
    
    // Process memory to discover patterns
    episodic_memory->processEpisodicMemory();
    
    auto patterns = episodic_memory->getTemporalPatterns(0.1, 0.1); // Low thresholds to find patterns
    
    std::cout << "  Discovered " << patterns.size() << " temporal patterns:" << std::endl;
    
    for (const auto& pattern : patterns) {
        std::cout << "    - Pattern " << pattern.pattern_id 
                  << ": " << pattern.pattern_sequence.size() << " atoms"
                  << " (frequency: " << std::fixed << std::setprecision(2) << pattern.frequency 
                  << ", strength: " << pattern.predictive_strength << ")" << std::endl;
    }
    
    // 8. Sequence Prediction
    std::cout << "\n🔮 Step 8: Sequence Prediction" << std::endl;
    
    // Test prediction with a partial sequence
    std::vector<Handle> partial_sequence = {move_forward, scan_area};
    
    EpisodicMemory::EpisodeContext pred_context;
    pred_context.environmental_state = {indoor_env};
    pred_context.query_time = std::chrono::system_clock::now();
    
    auto predictions = episodic_memory->predictNextInSequence(partial_sequence, pred_context);
    
    std::cout << "  Given sequence: [move_forward, scan_area]" << std::endl;
    std::cout << "  Predicted next atoms (" << predictions.size() << " predictions):" << std::endl;
    
    for (const auto& prediction : predictions) {
        std::cout << "    - Atom " << prediction.first 
                  << " (confidence: " << std::fixed << std::setprecision(2) 
                  << prediction.second << ")" << std::endl;
    }
    
    // 9. Memory Export
    std::cout << "\n💾 Step 9: Memory Export" << std::endl;
    
    std::string json_export = episodic_memory->exportEpisodes("json");
    std::cout << "  JSON export size: " << json_export.length() << " characters" << std::endl;
    
    // Show first 200 characters of JSON
    if (json_export.length() > 200) {
        std::cout << "  Preview: " << json_export.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "  Full export: " << json_export << std::endl;
    }
    
    // 10. Memory Consolidation
    std::cout << "\n🗜️  Step 10: Memory Consolidation" << std::endl;
    
    // Set lower consolidation threshold to demonstrate feature
    episodic_memory->setConsolidationThreshold(0.6);
    size_t consolidated = episodic_memory->consolidateMemory();
    
    std::cout << "  Consolidated " << consolidated << " similar episodes" << std::endl;
    
    auto final_stats = episodic_memory->getEpisodeStatistics();
    std::cout << "  Final episode count: " << static_cast<int>(final_stats["total_episodes"]) << std::endl;
    
    // Summary
    std::cout << "\n🎉 Demonstration Complete!" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Successfully demonstrated all major EpisodicMemory features:" << std::endl;
    std::cout << "  ✓ Episode recording with temporal sequences" << std::endl;
    std::cout << "  ✓ Context-based episode retrieval" << std::endl;  
    std::cout << "  ✓ Temporal range queries" << std::endl;
    std::cout << "  ✓ Pattern discovery from sequences" << std::endl;
    std::cout << "  ✓ Sequence prediction capabilities" << std::endl;
    std::cout << "  ✓ Memory export functionality" << std::endl;
    std::cout << "  ✓ Memory consolidation and optimization" << std::endl;
    std::cout << "  ✓ Full AtomSpace integration" << std::endl;
    
    return 0;
}