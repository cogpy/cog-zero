/*
 * ReasoningEngineExample.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple example demonstrating ReasoningEngine usage
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <iostream>
#include <memory>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

// Note: In a real application, you would use the full Agent-Zero framework
// This is a simplified example for demonstration purposes

// Mock AgentZeroCore for this example
class MockAgentZeroCore {
    // Minimal mock implementation for demonstration
};

#include <opencog/agentzero/ReasoningEngine.h>

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    std::cout << "=== ReasoningEngine Example ===" << std::endl;
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        
        // Create mock agent core (in real usage, this would be your AgentZeroCore instance)
        MockAgentZeroCore* mock_agent = nullptr;
        
        // Create ReasoningEngine
        auto reasoning_engine = std::make_unique<ReasoningEngine>(mock_agent, atomspace);
        
        std::cout << "✓ ReasoningEngine created successfully" << std::endl;
        
        // Example 1: Create knowledge base
        std::cout << "\n--- Example 1: Basic Forward Chaining ---" << std::endl;
        
        // Create concepts: Socrates, Human, Mortal
        Handle socrates = atomspace->add_node(CONCEPT_NODE, "Socrates");
        Handle human = atomspace->add_node(CONCEPT_NODE, "Human");
        Handle mortal = atomspace->add_node(CONCEPT_NODE, "Mortal");
        
        // Socrates is a Human (with high confidence)
        Handle socrates_is_human = atomspace->add_link(INHERITANCE_LINK, socrates, human);
        socrates_is_human->setTruthValue(SimpleTruthValue::createTV(1.0, 0.95));
        
        // Humans are Mortal (with high confidence)
        Handle humans_are_mortal = atomspace->add_link(INHERITANCE_LINK, human, mortal);
        humans_are_mortal->setTruthValue(SimpleTruthValue::createTV(0.99, 0.90));
        
        std::cout << "Created knowledge base:" << std::endl;
        std::cout << "  - Socrates is Human (TV: 1.0, 0.95)" << std::endl;
        std::cout << "  - Human → Mortal (TV: 0.99, 0.90)" << std::endl;
        
        // Perform forward chaining reasoning
        std::vector<Handle> premises = {socrates_is_human, humans_are_mortal};
        auto results = reasoning_engine->reason(premises, ReasoningEngine::ReasoningMode::FORWARD_CHAINING, 5);
        
        std::cout << "\nReasoning Results:" << std::endl;
        for (const auto& result : results) {
            std::cout << "  ✓ " << result.explanation << std::endl;
        }
        
        // Example 2: Hypothesis Generation
        std::cout << "\n--- Example 2: Hypothesis Generation ---" << std::endl;
        
        // Create some observations
        Handle observation1 = atomspace->add_node(CONCEPT_NODE, "RainObserved");
        Handle observation2 = atomspace->add_node(CONCEPT_NODE, "WetGroundObserved");
        
        observation1->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
        observation2->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
        
        std::vector<Handle> observations = {observation1, observation2};
        
        std::cout << "Observations to explain:" << std::endl;
        std::cout << "  - Rain observed" << std::endl;
        std::cout << "  - Wet ground observed" << std::endl;
        
        auto hypotheses = reasoning_engine->generateHypotheses(observations);
        
        std::cout << "\nGenerated Hypotheses:" << std::endl;
        for (const auto& hypothesis : hypotheses) {
            std::cout << "  → " << hypothesis.explanation << std::endl;
        }
        
        // Example 3: Configuration and Statistics
        std::cout << "\n--- Example 3: Configuration ---" << std::endl;
        
        // Configure reasoning parameters
        reasoning_engine->configurePLN(true, 0.7, 0.6);
        reasoning_engine->setReasoningLimits(10, 0.5);
        
        // Get statistics
        auto stats = reasoning_engine->getReasoningStatistics();
        
        std::cout << "Reasoning Engine Statistics:" << std::endl;
        for (const auto& stat : stats) {
            std::cout << "  " << stat.first << ": " << stat.second << std::endl;
        }
        
        // Get status info
        std::cout << "\nStatus: " << reasoning_engine->getStatusInfo() << std::endl;
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}