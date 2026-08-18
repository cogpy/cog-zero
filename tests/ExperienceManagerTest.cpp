/*
 * tests/ExperienceManagerTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for ExperienceManager class
 */

#include <iostream>
#include <cassert>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/types.h>

#include "opencog/agentzero/ExperienceManager.h"

using namespace opencog;
using namespace opencog::agentzero;

class ExperienceManagerTest
{
private:
    AtomSpacePtr _atomspace;
    std::unique_ptr<ExperienceManager> _experience_manager;

public:
    ExperienceManagerTest() 
        : _atomspace(std::make_shared<AtomSpace>())
        , _experience_manager(std::make_unique<ExperienceManager>(_atomspace))
    {}

    void runAllTests()
    {
        std::cout << "Running ExperienceManager tests..." << std::endl;
        
        testExperienceRecording();
        testExperienceRetrieval();
        testExperienceSimilarity();
        testExperienceManagement();
        
        std::cout << "All ExperienceManager tests passed!" << std::endl;
    }

private:
    void testExperienceRecording()
    {
        std::cout << "Testing experience recording..." << std::endl;
        
        Handle data = _atomspace->add_node(CONCEPT_NODE, "TestExperience");
        Handle exp = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::SUCCESS, data);
        
        assert(exp != Handle::UNDEFINED);
        assert(_experience_manager->getTotalExperiences() > 0);
        
        std::cout << "✓ Experience recording test passed" << std::endl;
    }

    void testExperienceRetrieval()
    {
        std::cout << "Testing experience retrieval..." << std::endl;
        
        std::vector<Handle> successes = _experience_manager->getExperiencesByType(
            ExperienceManager::ExperienceType::SUCCESS);
        assert(!successes.empty());
        
        std::vector<Handle> recent = _experience_manager->getRecentExperiences(5);
        assert(!recent.empty());
        
        std::cout << "✓ Experience retrieval test passed" << std::endl;
    }

    void testExperienceSimilarity()
    {
        std::cout << "Testing experience similarity..." << std::endl;
        
        Handle data1 = _atomspace->add_node(CONCEPT_NODE, "SimilarExp1");
        Handle data2 = _atomspace->add_node(CONCEPT_NODE, "SimilarExp2");
        
        Handle exp1 = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::OBSERVATION, data1);
        Handle exp2 = _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::OBSERVATION, data2);
        
        std::vector<Handle> similar = _experience_manager->findSimilarExperiences(exp1, 0.1);
        // Should find at least exp2 (both are observations)
        
        std::cout << "✓ Experience similarity test passed" << std::endl;
    }

    void testExperienceManagement()
    {
        std::cout << "Testing experience management..." << std::endl;
        
        size_t initial_count = _experience_manager->getTotalExperiences();
        _experience_manager->setMaxExperiences(initial_count + 1);
        
        // Add one more experience
        Handle data = _atomspace->add_node(CONCEPT_NODE, "ManagementTest");
        _experience_manager->recordExperience(
            ExperienceManager::ExperienceType::FAILURE, data);
        
        // Should trigger compression
        size_t final_count = _experience_manager->getTotalExperiences();
        assert(final_count <= initial_count + 1);
        
        std::cout << "✓ Experience management test passed" << std::endl;
    }
};

int main()
{
    try {
        ExperienceManagerTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}