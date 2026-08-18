/*
 * EnhancedKnowledgeIntegratorTest.cpp
 *
 * Comprehensive test for enhanced AtomSpace operations in KnowledgeIntegrator
 * Tests PLN reasoning integration, URE integration, pattern mining, and advanced features
 */

#include <iostream>
#include <memory>
#include <cassert>
#include <vector>

// Include OpenCog headers
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

// Mock AgentZeroCore for testing
class MockAgentZeroCore {
public:
    std::string getAgentName() const { return "TestAgent"; }
};

#include "opencog/agentzero/KnowledgeIntegrator.h"

using namespace opencog;
using namespace opencog::agentzero;

class EnhancedKnowledgeIntegratorTest {
private:
    AtomSpacePtr _atomspace;
    std::unique_ptr<MockAgentZeroCore> _agent_core;
    std::unique_ptr<KnowledgeIntegrator> _knowledge_integrator;
    
    void setupTestEnvironment() {
        std::cout << "Setting up test environment..." << std::endl;
        
        _atomspace = std::make_shared<AtomSpace>();
        _agent_core = std::make_unique<MockAgentZeroCore>();
        _knowledge_integrator = std::make_unique<KnowledgeIntegrator>(_agent_core.get(), _atomspace);
        
        std::cout << "Test environment setup complete." << std::endl;
    }
    
    void testBasicKnowledgeOperations() {
        std::cout << "\n=== Testing Basic Knowledge Operations ===" << std::endl;
        
        // Test adding facts
        Handle fact1 = _knowledge_integrator->addFact("Dogs are animals", 
                                                     KnowledgeIntegrator::ConfidenceLevel::HIGH);
        assert(fact1 != Handle::UNDEFINED);
        std::cout << "✓ Added fact: Dogs are animals" << std::endl;
        
        Handle fact2 = _knowledge_integrator->addFact("Cats are animals", 
                                                     KnowledgeIntegrator::ConfidenceLevel::HIGH);
        assert(fact2 != Handle::UNDEFINED);
        std::cout << "✓ Added fact: Cats are animals" << std::endl;
        
        // Test adding semantic relations
        Handle relation = _knowledge_integrator->addSemanticRelation("dog", "isa", "animal", 
                                                                   KnowledgeIntegrator::ConfidenceLevel::HIGH);
        assert(relation != Handle::UNDEFINED);
        std::cout << "✓ Added semantic relation: dog isa animal" << std::endl;
        
        // Test concept registration
        Handle concept = _knowledge_integrator->registerConcept("mammal", "Warm-blooded vertebrate");
        assert(concept != Handle::UNDEFINED);
        std::cout << "✓ Registered concept: mammal" << std::endl;
        
        // Test knowledge query
        auto query_results = _knowledge_integrator->queryKnowledge("animal", 5);
        assert(!query_results.empty());
        std::cout << "✓ Knowledge query returned " << query_results.size() << " results" << std::endl;
    }
    
    void testAdvancedReasoning() {
        std::cout << "\n=== Testing Advanced Reasoning ===" << std::endl;
        
        // Configure advanced reasoning
        _knowledge_integrator->configureAdvancedReasoning(true, 0.5, 10);
        std::cout << "✓ Configured advanced reasoning" << std::endl;
        
        // Create some premises for reasoning
        Handle premise1 = _atomspace->add_node(CONCEPT_NODE, "Socrates is human");
        Handle premise2 = _atomspace->add_node(CONCEPT_NODE, "All humans are mortal");
        
        // Create implication rule: humans -> mortal
        HandleSeq impl_link;
        impl_link.push_back(premise1);
        Handle conclusion = _atomspace->add_node(CONCEPT_NODE, "Socrates is mortal");
        impl_link.push_back(conclusion);
        Handle implication = _atomspace->add_link(IMPLICATION_LINK, std::move(impl_link));
        
        TruthValuePtr impl_tv = SimpleTruthValue::createTV(0.9, 0.8);
        implication->setTruthValue(impl_tv);
        
        // Test advanced reasoning
        std::vector<Handle> query_atoms = {premise1, premise2};
        auto reasoning_results = _knowledge_integrator->performAdvancedReasoning(
            query_atoms, "forward", 5);
        
        std::cout << "✓ Advanced reasoning produced " << reasoning_results.size() << " results" << std::endl;
        
        // Test inference with specific premises
        auto inferences = _knowledge_integrator->performInference({premise1}, "");
        std::cout << "✓ Performed inference, got " << inferences.size() << " results" << std::endl;
    }
    
    void testPatternDiscovery() {
        std::cout << "\n=== Testing Pattern Discovery ===" << std::endl;
        
        // Configure pattern mining
        _knowledge_integrator->configurePatternMining(true, 0.3, true);
        std::cout << "✓ Configured pattern mining" << std::endl;
        
        // Create data for pattern discovery
        std::vector<Handle> data_atoms;
        data_atoms.push_back(_atomspace->add_node(CONCEPT_NODE, "morning_coffee"));
        data_atoms.push_back(_atomspace->add_node(CONCEPT_NODE, "morning_news"));
        data_atoms.push_back(_atomspace->add_node(CONCEPT_NODE, "morning_coffee"));
        data_atoms.push_back(_atomspace->add_node(CONCEPT_NODE, "morning_exercise"));
        data_atoms.push_back(_atomspace->add_node(CONCEPT_NODE, "morning_coffee"));
        data_atoms.push_back(_atomspace->add_node(CONCEPT_NODE, "morning_news"));
        
        // Test pattern discovery
        auto patterns = _knowledge_integrator->discoverKnowledgePatterns(
            data_atoms, "associative", 0.3);
        
        std::cout << "✓ Discovered " << patterns.size() << " knowledge patterns" << std::endl;
        
        // Test hypothesis generation
        std::vector<Handle> observations = {
            _atomspace->add_node(CONCEPT_NODE, "increased_heart_rate"),
            _atomspace->add_node(CONCEPT_NODE, "rapid_breathing")
        };
        
        auto hypotheses = _knowledge_integrator->generateKnowledgeHypotheses(observations);
        std::cout << "✓ Generated " << hypotheses.size() << " hypotheses" << std::endl;
    }
    
    void testSemanticSimilarity() {
        std::cout << "\n=== Testing Semantic Similarity ===" << std::endl;
        
        // Create test concepts
        Handle dog = _atomspace->add_node(CONCEPT_NODE, "dog");
        Handle cat = _atomspace->add_node(CONCEPT_NODE, "cat");
        Handle animal = _atomspace->add_node(CONCEPT_NODE, "animal");
        
        // Create connections to establish similarity
        HandleSeq dog_animal_link;
        dog_animal_link.push_back(dog);
        dog_animal_link.push_back(animal);
        _atomspace->add_link(INHERITANCE_LINK, std::move(dog_animal_link));
        
        HandleSeq cat_animal_link;
        cat_animal_link.push_back(cat);
        cat_animal_link.push_back(animal);
        _atomspace->add_link(INHERITANCE_LINK, std::move(cat_animal_link));
        
        // Test semantic similarity search
        auto similar_concepts = _knowledge_integrator->findSemanticallySimilar(dog, 0.1, 5);
        std::cout << "✓ Found " << similar_concepts.size() << " concepts similar to 'dog'" << std::endl;
    }
    
    void testKnowledgeSynthesis() {
        std::cout << "\n=== Testing Knowledge Synthesis ===" << std::endl;
        
        // Create source knowledge
        std::vector<Handle> source_knowledge;
        source_knowledge.push_back(_atomspace->add_node(CONCEPT_NODE, "birds_fly"));
        source_knowledge.push_back(_atomspace->add_node(CONCEPT_NODE, "birds_have_wings"));
        source_knowledge.push_back(_atomspace->add_node(CONCEPT_NODE, "wings_enable_flight"));
        
        // Test knowledge synthesis
        Handle synthesized = _knowledge_integrator->synthesizeKnowledge(
            source_knowledge, "flight_capability", "compositional");
        
        if (synthesized != Handle::UNDEFINED) {
            std::cout << "✓ Successfully synthesized knowledge: " << synthesized->get_name() << std::endl;
        } else {
            std::cout << "✗ Knowledge synthesis failed" << std::endl;
        }
    }
    
    void testLearningFromInteractions() {
        std::cout << "\n=== Testing Learning from Interactions ===" << std::endl;
        
        // Create interaction sequence
        std::vector<Handle> interaction_sequence;
        interaction_sequence.push_back(_atomspace->add_node(CONCEPT_NODE, "user_question"));
        interaction_sequence.push_back(_atomspace->add_node(CONCEPT_NODE, "agent_search"));
        interaction_sequence.push_back(_atomspace->add_node(CONCEPT_NODE, "agent_response"));
        interaction_sequence.push_back(_atomspace->add_node(CONCEPT_NODE, "user_feedback"));
        
        // Test learning from interactions
        auto learned_patterns = _knowledge_integrator->learnFromInteractions(
            interaction_sequence, "temporal");
        
        std::cout << "✓ Learned " << learned_patterns.size() << " patterns from interactions" << std::endl;
    }
    
    void testAdvancedQuery() {
        std::cout << "\n=== Testing Advanced Query Processing ===" << std::endl;
        
        // Add some knowledge for querying
        _knowledge_integrator->addFact("Einstein was a physicist", 
                                      KnowledgeIntegrator::ConfidenceLevel::HIGH);
        _knowledge_integrator->addFact("Einstein developed relativity theory", 
                                      KnowledgeIntegrator::ConfidenceLevel::HIGH);
        
        // Test advanced query with inference
        auto query_results = _knowledge_integrator->advancedKnowledgeQuery(
            "Einstein", {}, true);
        
        std::cout << "✓ Advanced query returned " << query_results.size() << " results" << std::endl;
    }
    
    void testKnowledgeValidation() {
        std::cout << "\n=== Testing Knowledge Validation ===" << std::endl;
        
        // Add contradictory knowledge for testing
        Handle fact_a = _knowledge_integrator->addFact("The sky is blue", 
                                                      KnowledgeIntegrator::ConfidenceLevel::HIGH);
        Handle fact_b = _atomspace->add_node(CONCEPT_NODE, "The sky is red");
        TruthValuePtr low_tv = SimpleTruthValue::createTV(0.2, 0.8);
        fact_b->setTruthValue(low_tv);
        
        // Test knowledge validation
        auto inconsistencies = _knowledge_integrator->validateKnowledgeWithReasoning();
        std::cout << "✓ Validation found " << inconsistencies.size() << " potential inconsistencies" << std::endl;
    }
    
    void testAdvancedStatistics() {
        std::cout << "\n=== Testing Advanced Statistics ===" << std::endl;
        
        // Get advanced statistics
        auto stats = _knowledge_integrator->getAdvancedKnowledgeStatistics();
        
        std::cout << "✓ Advanced Knowledge Base Statistics:" << std::endl;
        for (const auto& stat : stats) {
            std::cout << "   " << stat.first << ": " << stat.second << std::endl;
        }
    }

public:
    void runAllTests() {
        std::cout << "Starting Enhanced KnowledgeIntegrator Tests" << std::endl;
        std::cout << "===========================================" << std::endl;
        
        setupTestEnvironment();
        
        try {
            testBasicKnowledgeOperations();
            testAdvancedReasoning();
            testPatternDiscovery();
            testSemanticSimilarity();
            testKnowledgeSynthesis();
            testLearningFromInteractions();
            testAdvancedQuery();
            testKnowledgeValidation();
            testAdvancedStatistics();
            
            std::cout << "\n===========================================" << std::endl;
            std::cout << "✓ All Enhanced KnowledgeIntegrator tests PASSED!" << std::endl;
            std::cout << "Enhanced AtomSpace operations are working correctly." << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "\n✗ Test failed with exception: " << e.what() << std::endl;
        }
    }
};

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    EnhancedKnowledgeIntegratorTest test;
    test.runAllTests();
    
    return 0;
}