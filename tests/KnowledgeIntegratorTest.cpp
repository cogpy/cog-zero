/*
 * KnowledgeIntegratorTest.cpp
 *
 * Simple test for KnowledgeIntegrator functionality
 * Tests core AtomSpace integration and knowledge management
 */

#include <iostream>
#include <memory>
#include <cassert>

// Include OpenCog headers
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#ifdef MOCK_AGENT_CORE
// Forward declaration for AgentZeroCore
namespace opencog { namespace agentzero { class AgentZeroCore; } }

// Mock AgentZeroCore for testing
class MockAgentZeroCore {
public:
    std::string getAgentName() const { return "TestAgent"; }
};

// Simple mock implementation - just define the required interface
namespace opencog {
namespace agentzero {

class KnowledgeIntegrator
{
public:
    enum class KnowledgeType {
        FACTUAL, PROCEDURAL, EPISODIC, SEMANTIC, CONDITIONAL, TEMPORAL
    };
    
    enum class ConfidenceLevel {
        VERY_LOW = 0, LOW = 25, MEDIUM = 50, HIGH = 75, VERY_HIGH = 100
    };

private:
    AtomSpacePtr _atomspace;
    std::map<std::string, Handle> _concept_registry;
    std::set<Handle> _active_knowledge;
    Handle _knowledge_base;
    Handle _semantic_network;

public:
    KnowledgeIntegrator(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
        : _atomspace(atomspace), _knowledge_base(Handle::UNDEFINED), _semantic_network(Handle::UNDEFINED)
    {
        _knowledge_base = _atomspace->add_node(CONCEPT_NODE, "TestAgent_KnowledgeBase");
        _semantic_network = _atomspace->add_node(CONCEPT_NODE, "TestAgent_SemanticNetwork");
    }
    
    Handle addFact(const std::string& fact_description, ConfidenceLevel confidence = ConfidenceLevel::MEDIUM) {
        Handle fact_atom = _atomspace->add_node(CONCEPT_NODE, "Fact_" + fact_description);
        double confidence_value = static_cast<double>(confidence) / 100.0;
        TruthValuePtr fact_tv = SimpleTruthValue::createTV(confidence_value, 0.9);
        fact_atom->setTruthValue(fact_tv);
        return fact_atom;
    }
    
    Handle registerConcept(const std::string& concept_name, const std::string& concept_description = "") {
        Handle concept = _atomspace->add_node(CONCEPT_NODE, std::string(concept_name));
        _concept_registry[concept_name] = concept;
        return concept;
    }
    
    bool hasKnowledgeAbout(const std::string& concept_name) {
        return _concept_registry.find(concept_name) != _concept_registry.end();
    }
    
    Handle addSemanticRelation(const std::string& concept1_name, const std::string& relation_type, const std::string& concept2_name) {
        Handle concept1 = registerConcept(concept1_name);
        Handle concept2 = registerConcept(concept2_name);
        Handle relation_atom = _atomspace->add_node(CONCEPT_NODE, relation_type + "_" + concept1_name + "_" + concept2_name);
        return relation_atom;
    }
    
    std::vector<Handle> queryKnowledge(const std::string& query_text, int max_results = 10) {
        std::vector<Handle> results;
        HandleSeq all_atoms;
        _atomspace->get_handles_by_type(all_atoms, ATOM, true);
        
        for (const Handle& atom : all_atoms) {
            std::string atom_name = atom->get_name();
            if (atom_name.find(query_text) != std::string::npos) {
                results.push_back(atom);
                if (results.size() >= static_cast<size_t>(max_results)) break;
            }
        }
        return results;
    }
    
    std::map<std::string, int> getKnowledgeStatistics() {
        std::map<std::string, int> stats;
        stats["total_concepts"] = _concept_registry.size();
        stats["active_knowledge"] = _active_knowledge.size();
        stats["total_atoms"] = _atomspace->get_size();
        return stats;
    }
    
    std::string getStatusInfo() const {
        return "{\"status\":\"ok\"}";
    }
    
    Handle getKnowledgeBase() const { return _knowledge_base; }
    Handle getSemanticNetwork() const { return _semantic_network; }
    
    std::vector<Handle> formConceptsFrom(const std::vector<Handle>& experience_atoms) {
        return std::vector<Handle>(); // Simple stub
    }
    
    std::vector<Handle> validateKnowledgeConsistency() {
        return std::vector<Handle>(); // Simple stub
    }
    
    std::string exportKnowledge(const std::string& export_format) {
        if (export_format == "json") return "{\"test\":\"data\"}";
        return "test data";
    }
    
    int importKnowledge(const std::string& source_description, const std::map<std::string, std::string>& knowledge_data) {
        return knowledge_data.size();
    }
};

} // namespace agentzero
} // namespace opencog

#else
// Include real headers when not mocking
#include "opencog/agentzero/KnowledgeIntegrator.h"
#endif

using namespace opencog;
using namespace opencog::agentzero;

class KnowledgeIntegratorTest {
private:
    AtomSpacePtr _atomspace;
    std::unique_ptr<MockAgentZeroCore> _mock_core;
    std::unique_ptr<KnowledgeIntegrator> _integrator;

public:
    KnowledgeIntegratorTest() {
        _atomspace = std::make_shared<AtomSpace>();
        _mock_core = std::make_unique<MockAgentZeroCore>();
        // We need to cast to AgentZeroCore* - this is just for testing
        _integrator = std::make_unique<KnowledgeIntegrator>(
            reinterpret_cast<AgentZeroCore*>(_mock_core.get()), _atomspace);
    }

    void testBasicFunctionality() {
        std::cout << "Testing basic KnowledgeIntegrator functionality..." << std::endl;
        
        // Test 1: Add a fact
        Handle fact = _integrator->addFact("The sky is blue", KnowledgeIntegrator::ConfidenceLevel::HIGH);
        assert(fact != Handle::UNDEFINED);
        std::cout << "✓ Successfully added fact" << std::endl;
        
        // Test 2: Register a concept
        Handle concept = _integrator->registerConcept("Weather", "Atmospheric conditions");
        assert(concept != Handle::UNDEFINED);
        std::cout << "✓ Successfully registered concept" << std::endl;
        
        // Test 3: Check knowledge existence
        bool hasWeather = _integrator->hasKnowledgeAbout("Weather");
        assert(hasWeather);
        std::cout << "✓ Successfully checked knowledge existence" << std::endl;
        
        // Test 4: Add semantic relation
        Handle relation = _integrator->addSemanticRelation("Sky", "isa", "Weather");
        assert(relation != Handle::UNDEFINED);
        std::cout << "✓ Successfully added semantic relation" << std::endl;
        
        // Test 5: Query knowledge
        std::vector<Handle> results = _integrator->queryKnowledge("sky", 5);
        std::cout << "✓ Query returned " << results.size() << " results" << std::endl;
        
        // Test 6: Get knowledge statistics
        auto stats = _integrator->getKnowledgeStatistics();
        std::cout << "✓ Knowledge statistics: " << stats.size() << " categories" << std::endl;
        
        // Test 7: Get status info
        std::string status = _integrator->getStatusInfo();
        assert(!status.empty());
        std::cout << "✓ Status info retrieved" << std::endl;
        
        std::cout << "All basic tests passed!" << std::endl;
    }

    void testAdvancedFunctionality() {
        std::cout << "\nTesting advanced KnowledgeIntegrator functionality..." << std::endl;
        
        // Test concept formation
        std::vector<Handle> experiences;
        experiences.push_back(_atomspace->add_node(CONCEPT_NODE, "walking in the park"));
        experiences.push_back(_atomspace->add_node(CONCEPT_NODE, "running in the park"));
        experiences.push_back(_atomspace->add_node(CONCEPT_NODE, "sitting in the park"));
        
        std::vector<Handle> new_concepts = _integrator->formConceptsFrom(experiences);
        std::cout << "✓ Concept formation created " << new_concepts.size() << " new concepts" << std::endl;
        
        // Test knowledge validation
        std::vector<Handle> inconsistencies = _integrator->validateKnowledgeConsistency();
        std::cout << "✓ Knowledge validation found " << inconsistencies.size() << " inconsistencies" << std::endl;
        
        // Test knowledge export
        std::string exported_json = _integrator->exportKnowledge("json");
        assert(!exported_json.empty());
        std::cout << "✓ Knowledge export to JSON successful" << std::endl;
        
        std::string exported_text = _integrator->exportKnowledge("text");
        assert(!exported_text.empty());
        std::cout << "✓ Knowledge export to text successful" << std::endl;
        
        // Test knowledge import
        std::map<std::string, std::string> test_data;
        test_data["Temperature"] = "Hot";
        test_data["Season"] = "Summer";
        test_data["Activity"] = "Swimming";
        
        int imported = _integrator->importKnowledge("TestSource", test_data);
        std::cout << "✓ Imported " << imported << " knowledge items" << std::endl;
        
        std::cout << "All advanced tests passed!" << std::endl;
    }

    void testAtomSpaceIntegration() {
        std::cout << "\nTesting AtomSpace integration..." << std::endl;
        
        // Test that knowledge is properly stored in AtomSpace
        size_t initial_size = _atomspace->get_size();
        
        _integrator->addFact("Test fact for AtomSpace", KnowledgeIntegrator::ConfidenceLevel::MEDIUM);
        
        size_t final_size = _atomspace->get_size();
        assert(final_size > initial_size);
        std::cout << "✓ AtomSpace size increased from " << initial_size << " to " << final_size << std::endl;
        
        // Test knowledge base structure
        Handle kb = _integrator->getKnowledgeBase();
        assert(kb != Handle::UNDEFINED);
        std::cout << "✓ Knowledge base atom exists: " << kb << std::endl;
        
        Handle semantic_net = _integrator->getSemanticNetwork();
        assert(semantic_net != Handle::UNDEFINED);
        std::cout << "✓ Semantic network atom exists: " << semantic_net << std::endl;
        
        std::cout << "AtomSpace integration tests passed!" << std::endl;
    }

    void runAllTests() {
        std::cout << "=== KnowledgeIntegrator Test Suite ===" << std::endl;
        
        try {
            testBasicFunctionality();
            testAdvancedFunctionality();
            testAtomSpaceIntegration();
            
            std::cout << "\n=== All tests completed successfully! ===" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Test failed with exception: " << e.what() << std::endl;
            exit(1);
        }
    }
};

int main() {
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    KnowledgeIntegratorTest test;
    test.runAllTests();
    
    return 0;
}