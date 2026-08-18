/*
 * EpisodicMemorySimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple standalone test for EpisodicMemory class compilation and basic functionality
 * Part of AZ-MEM-001: Implement EpisodicMemory with temporal sequences
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <cassert>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

// Use relative include to test our new implementation
#include "../include/opencog/agentzero/learning/EpisodicMemory.h"

using namespace opencog;
using namespace opencog::agentzero::learning;

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "=== EpisodicMemory Simple Test ===" << std::endl;
    
    try {
        // Test 1: Basic initialization
        std::cout << "\n1. Testing initialization..." << std::endl;
        
        auto atomspace = std::make_shared<AtomSpace>();
        auto episodic_memory = std::make_unique<EpisodicMemory>(atomspace);
        
        assert(episodic_memory != nullptr);
        assert(episodic_memory->getEpisodeCount() == 0);
        assert(episodic_memory->getPatternCount() == 0);
        
        std::cout << "✓ Initialization successful" << std::endl;
        
        // Test 2: Basic episode recording
        std::cout << "\n2. Testing basic episode recording..." << std::endl;
        
        // Create some test atoms
        Handle action1 = atomspace->add_node(CONCEPT_NODE, "move_forward");
        Handle action2 = atomspace->add_node(CONCEPT_NODE, "turn_left");
        Handle action3 = atomspace->add_node(CONCEPT_NODE, "grab_object");
        
        Handle context1 = atomspace->add_node(CONCEPT_NODE, "indoor_environment");
        Handle context2 = atomspace->add_node(CONCEPT_NODE, "obstacle_present");
        
        // Create sequences and contexts
        std::vector<Handle> sequence1 = {action1, action2, action3};
        std::vector<Handle> context = {context1, context2};
        
        // Record episode
        std::string episode_id = episodic_memory->recordEpisode(
            "Test navigation sequence",
            sequence1,
            context
        );
        
        assert(!episode_id.empty());
        assert(episodic_memory->getEpisodeCount() == 1);
        
        std::cout << "✓ Episode recorded with ID: " << episode_id << std::endl;
        
        // Test 3: Statistics
        std::cout << "\n3. Testing statistics..." << std::endl;
        
        auto stats = episodic_memory->getEpisodeStatistics();
        assert(stats["total_episodes"] == 1.0);
        assert(stats["average_sequence_length"] > 0.0);
        
        std::cout << "✓ Statistics: " << stats.size() << " metrics available" << std::endl;
        std::cout << "  - Total episodes: " << stats["total_episodes"] << std::endl;
        std::cout << "  - Average sequence length: " << stats["average_sequence_length"] << std::endl;
        
        // Test 4: Temporal coherence analysis
        std::cout << "\n4. Testing temporal coherence..." << std::endl;
        
        double coherence = episodic_memory->analyzeTemporalCoherence(sequence1);
        assert(coherence > 0.0 && coherence <= 1.0);
        
        std::cout << "✓ Temporal coherence: " << coherence << std::endl;
        
        // Test 5: Episode retrieval by time range
        std::cout << "\n5. Testing time range retrieval..." << std::endl;
        
        auto now = std::chrono::system_clock::now();
        auto start_range = now - std::chrono::minutes(1);
        auto end_range = now + std::chrono::minutes(1);
        
        auto episodes_in_range = episodic_memory->getEpisodesInTimeRange(start_range, end_range);
        assert(episodes_in_range.size() == 1);
        
        std::cout << "✓ Retrieved " << episodes_in_range.size() << " episodes in time range" << std::endl;
        
        // Test 6: Context-based similarity
        std::cout << "\n6. Testing context-based retrieval..." << std::endl;
        
        EpisodicMemory::EpisodeContext query_context;
        query_context.environmental_state = {context1};
        query_context.query_time = std::chrono::system_clock::now();
        
        auto similar_episodes = episodic_memory->getSimilarEpisodes(query_context, 5);
        assert(similar_episodes.size() >= 1);
        
        std::cout << "✓ Retrieved " << similar_episodes.size() << " similar episodes" << std::endl;
        
        // Test 7: Multiple episodes and pattern discovery
        std::cout << "\n7. Testing multiple episodes and patterns..." << std::endl;
        
        episodic_memory->enablePatternDiscovery(true);
        
        // Record several more episodes with similar patterns
        for (int i = 0; i < 5; ++i) {
            std::vector<Handle> pattern = {action1, action2};
            episodic_memory->recordEpisode(
                "Pattern episode " + std::to_string(i),
                pattern,
                context
            );
        }
        
        assert(episodic_memory->getEpisodeCount() == 6); // 1 original + 5 new
        
        // Process for pattern discovery
        episodic_memory->processEpisodicMemory();
        
        auto patterns = episodic_memory->getTemporalPatterns(0.1, 0.1);
        
        std::cout << "✓ Total episodes: " << episodic_memory->getEpisodeCount() << std::endl;
        std::cout << "✓ Discovered patterns: " << patterns.size() << std::endl;
        
        // Test 8: Sequence prediction
        std::cout << "\n8. Testing sequence prediction..." << std::endl;
        
        std::vector<Handle> partial_sequence = {action1};
        auto predictions = episodic_memory->predictNextInSequence(partial_sequence, query_context);
        
        std::cout << "✓ Generated " << predictions.size() << " predictions for next atom" << std::endl;
        
        if (predictions.size() > 0) {
            std::cout << "  - Most likely next atom confidence: " << predictions[0].second << std::endl;
        }
        
        // Test 9: Export functionality
        std::cout << "\n9. Testing export functionality..." << std::endl;
        
        std::string json_export = episodic_memory->exportEpisodes("json");
        assert(!json_export.empty());
        assert(json_export.find("episodes") != std::string::npos);
        
        std::cout << "✓ JSON export generated (" << json_export.length() << " characters)" << std::endl;
        
        // Test 10: AtomSpace integration
        std::cout << "\n10. Testing AtomSpace integration..." << std::endl;
        
        Handle memory_base = episodic_memory->getEpisodicMemoryBase();
        assert(memory_base != Handle::UNDEFINED);
        assert(memory_base->get_type() == CONCEPT_NODE);
        
        std::cout << "✓ AtomSpace integration verified" << std::endl;
        std::cout << "  - Memory base handle: " << memory_base << std::endl;
        std::cout << "  - AtomSpace size: " << atomspace->get_size() << std::endl;
        
        // Final statistics
        std::cout << "\n=== Final Test Summary ===" << std::endl;
        auto final_stats = episodic_memory->getEpisodeStatistics();
        
        for (const auto& stat : final_stats) {
            std::cout << "  " << stat.first << ": " << stat.second << std::endl;
        }
        
        std::cout << "\n🎉 All tests passed successfully!" << std::endl;
        std::cout << "EpisodicMemory implementation is working correctly." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}