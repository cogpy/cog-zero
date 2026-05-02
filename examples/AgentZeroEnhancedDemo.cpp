/*
 * AgentZeroEnhancedDemo.cpp
 *
 * Demonstration of enhanced AtomSpace operations for Agent-Zero
 * Shows practical usage of advanced reasoning, pattern mining, and semantic search
 */

#include <iostream>
#include <memory>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

// Mock AgentZeroCore for demonstration
class MockAgentZeroCore {
public:
    std::string getAgentName() const { return "DemoAgent"; }
};

#include "opencog/agentzero/KnowledgeIntegrator.h"

using namespace opencog;
using namespace opencog::agentzero;

class AgentZeroEnhancedDemo {
private:
    AtomSpacePtr _atomspace;
    std::unique_ptr<MockAgentZeroCore> _agent_core;
    std::unique_ptr<KnowledgeIntegrator> _knowledge_integrator;

public:
    void initialize() {
        std::cout << "Initializing Agent-Zero Enhanced Demo..." << std::endl;
        
        _atomspace = std::make_shared<AtomSpace>();
        _agent_core = std::make_unique<MockAgentZeroCore>();
        _knowledge_integrator = std::make_unique<KnowledgeIntegrator>(_agent_core.get(), _atomspace);
        
        // Configure enhanced features
        _knowledge_integrator->configureAdvancedReasoning(true, 0.6, 10);
        _knowledge_integrator->configurePatternMining(true, 0.3, true);
        
        std::cout << "✓ Agent-Zero Enhanced Demo initialized" << std::endl << std::endl;
    }
    
    void demonstrateBasicKnowledge() {
        std::cout << "=== Demonstrating Basic Knowledge Operations ===" << std::endl;
        
        // Add facts about animals
        _knowledge_integrator->addFact("Dogs are mammals", KnowledgeIntegrator::ConfidenceLevel::HIGH);
        _knowledge_integrator->addFact("Cats are mammals", KnowledgeIntegrator::ConfidenceLevel::HIGH);
        _knowledge_integrator->addFact("Mammals are warm-blooded", KnowledgeIntegrator::ConfidenceLevel::HIGH);
        
        std::cout << "✓ Added basic knowledge about animals and mammals" << std::endl;
    }
    
    void demonstrateAdvancedReasoning() {
        std::cout << "\n=== Demonstrating Advanced Reasoning ===" << std::endl;
        
        // Create logical premises
        Handle premise1 = _atomspace->add_node(CONCEPT_NODE, "All mammals are warm-blooded");
        Handle premise2 = _atomspace->add_node(CONCEPT_NODE, "Dogs are mammals");
        
        // Perform advanced reasoning
        std::vector<Handle> query_atoms = {premise1, premise2};
        auto reasoning_results = _knowledge_integrator->performAdvancedReasoning(
            query_atoms, "forward", 5);
            
        std::cout << "✓ Advanced reasoning produced " << reasoning_results.size() 
                  << " inferred conclusions" << std::endl;
    }
    
    void demonstratePatternDiscovery() {
        std::cout << "\n=== Demonstrating Pattern Discovery ===" << std::endl;
        
        // Create interaction data
        std::vector<Handle> daily_routine = {
            _atomspace->add_node(CONCEPT_NODE, "wake_up"),
            _atomspace->add_node(CONCEPT_NODE, "drink_coffee"),
            _atomspace->add_node(CONCEPT_NODE, "check_email"),
            _atomspace->add_node(CONCEPT_NODE, "drink_coffee")
        };
        
        auto patterns = _knowledge_integrator->discoverKnowledgePatterns(
            daily_routine, "associative", 0.3);
            
        std::cout << "✓ Discovered " << patterns.size() << " behavioral patterns" << std::endl;
    }
    
    void showStatistics() {
        std::cout << "\n=== Knowledge Base Statistics ===" << std::endl;
        
        auto stats = _knowledge_integrator->getAdvancedKnowledgeStatistics();
        
        std::cout << "Knowledge Base Metrics:" << std::endl;
        for (const auto& stat : stats) {
            std::cout << "  " << stat.first << ": " << stat.second << std::endl;
        }
    }
    
    void runCompleteDemo() {
        initialize();
        demonstrateBasicKnowledge();
        demonstrateAdvancedReasoning();
        demonstratePatternDiscovery();
        showStatistics();
        
        std::cout << "\n===============================================" << std::endl;
        std::cout << "✓ Agent-Zero Enhanced AtomSpace Demo Complete!" << std::endl;
        std::cout << "===============================================" << std::endl;
    }
};

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    try {
        AgentZeroEnhancedDemo demo;
        demo.runCompleteDemo();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Demo failed: " << e.what() << std::endl;
        return 1;
    }
}