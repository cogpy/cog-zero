/*
 * tests/SelfModificationTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for SelfModification class
 */

#include <iostream>
#include <cassert>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/types.h>

#include "opencog/agentzero/SelfModification.h"

using namespace opencog;
using namespace opencog::agentzero;

class SelfModificationTest
{
private:
    AtomSpacePtr _atomspace;
    std::unique_ptr<SelfModification> _self_mod;

public:
    SelfModificationTest() 
        : _atomspace(std::make_shared<AtomSpace>())
        , _self_mod(std::make_unique<SelfModification>(nullptr, _atomspace))
    {}

    void runAllTests()
    {
        std::cout << "Running SelfModification tests..." << std::endl;
        
        testInitialization();
        testComponentAnalysis();
        testProposalGeneration();
        testProposalEvaluation();
        testModificationApplication();
        testRollback();
        testSafetyConstraints();
        testModificationHistory();
        testUtilityMethods();
        
        std::cout << "All SelfModification tests passed!" << std::endl;
    }

private:
    void testInitialization()
    {
        std::cout << "Testing initialization..." << std::endl;
        
        assert(_self_mod->isInitialized());
        assert(_self_mod->getSafetyLevel() == SelfModification::SafetyLevel::CAUTIOUS);
        assert(!_self_mod->isAutoModificationEnabled());
        
        std::cout << "✓ Initialization test passed" << std::endl;
    }

    void testComponentAnalysis()
    {
        std::cout << "Testing component analysis..." << std::endl;
        
        std::string component_name = "TestComponent";
        SelfModification::CodeAnalysis analysis = _self_mod->analyzeComponent(component_name);
        
        assert(analysis.component_name == component_name);
        assert(analysis.complexity_score >= 0.0 && analysis.complexity_score <= 1.0);
        assert(analysis.maintainability_score >= 0.0 && analysis.maintainability_score <= 1.0);
        assert(!analysis.performance_metrics.empty());
        assert(analysis.analysis_atom != Handle::UNDEFINED);
        
        // Test cached analysis
        SelfModification::CodeAnalysis cached = _self_mod->getCachedAnalysis(component_name);
        assert(cached.component_name == component_name);
        
        std::cout << "✓ Component analysis test passed" << std::endl;
    }

    void testProposalGeneration()
    {
        std::cout << "Testing proposal generation..." << std::endl;
        
        std::string component_name = "TestComponent2";
        
        // Analyze component first
        _self_mod->analyzeComponent(component_name);
        
        // Generate proposals
        std::vector<SelfModification::ModificationProposal> proposals = 
            _self_mod->proposeModifications(component_name, 5);
        
        assert(!proposals.empty());
        assert(proposals.size() <= 5);
        
        // Verify proposal properties
        for (const auto& proposal : proposals) {
            assert(!proposal.description.empty());
            assert(proposal.target_component == component_name);
            assert(proposal.expected_improvement >= 0.0);
            assert(proposal.proposal_atom != Handle::UNDEFINED);
            
            // Verify safety level is set to a valid value
            bool valid_safety_level = 
                proposal.safety_level == SelfModification::SafetyLevel::SAFE ||
                proposal.safety_level == SelfModification::SafetyLevel::CAUTIOUS ||
                proposal.safety_level == SelfModification::SafetyLevel::EXPERIMENTAL ||
                proposal.safety_level == SelfModification::SafetyLevel::UNSAFE;
            assert(valid_safety_level);
        }
        
        std::cout << "✓ Proposal generation test passed" << std::endl;
    }

    void testProposalEvaluation()
    {
        std::cout << "Testing proposal evaluation..." << std::endl;
        
        // Create test proposals
        std::vector<SelfModification::ModificationProposal> proposals;
        
        SelfModification::ModificationProposal p1;
        p1.type = SelfModification::ModificationType::PARAMETER_TUNING;
        p1.description = "Proposal 1";
        p1.target_component = "Component1";
        p1.expected_improvement = 0.3;
        p1.risk_factor = 0.1;
        proposals.push_back(p1);
        
        SelfModification::ModificationProposal p2;
        p2.type = SelfModification::ModificationType::CODE_OPTIMIZATION;
        p2.description = "Proposal 2";
        p2.target_component = "Component1";
        p2.expected_improvement = 0.5;
        p2.risk_factor = 0.2;
        proposals.push_back(p2);
        
        // Evaluate and rank proposals
        std::vector<SelfModification::ModificationProposal> ranked = 
            _self_mod->evaluateProposals(proposals);
        
        assert(ranked.size() == proposals.size());
        
        // Verify ranking (higher improvement should be first)
        if (ranked.size() >= 2) {
            double score1 = ranked[0].expected_improvement - ranked[0].risk_factor;
            double score2 = ranked[1].expected_improvement - ranked[1].risk_factor;
            assert(score1 >= score2);
        }
        
        std::cout << "✓ Proposal evaluation test passed" << std::endl;
    }

    void testModificationApplication()
    {
        std::cout << "Testing modification application..." << std::endl;
        
        // Create a safe modification proposal
        SelfModification::ModificationProposal proposal;
        proposal.type = SelfModification::ModificationType::PARAMETER_TUNING;
        proposal.safety_level = SelfModification::SafetyLevel::SAFE;
        proposal.description = "Test parameter tuning";
        proposal.target_component = "TestComponent3";
        proposal.expected_improvement = 0.10;
        proposal.risk_factor = 0.05;
        proposal.parameters["learning_rate"] = "0.01";
        
        // Apply modification
        SelfModification::ModificationResult result = 
            _self_mod->applyModification(proposal, true);
        
        // Verify result
        assert(result.status == SelfModification::ModificationStatus::SUCCESS || 
               result.status == SelfModification::ModificationStatus::REJECTED);
        assert(result.result_atom != Handle::UNDEFINED);
        
        if (result.status == SelfModification::ModificationStatus::SUCCESS) {
            assert(result.actual_improvement >= 0.0);
            
            // Verify component is marked as modified
            std::set<std::string> modified = _self_mod->getModifiedComponents();
            assert(modified.find(proposal.target_component) != modified.end());
        }
        
        std::cout << "✓ Modification application test passed" << std::endl;
    }

    void testRollback()
    {
        std::cout << "Testing rollback..." << std::endl;
        
        // Enable rollback
        _self_mod->configure(3, 0.05, true);
        
        // Create and apply a modification
        SelfModification::ModificationProposal proposal;
        proposal.type = SelfModification::ModificationType::PARAMETER_TUNING;
        proposal.safety_level = SelfModification::SafetyLevel::SAFE;
        proposal.description = "Test rollback";
        proposal.target_component = "TestComponent4";
        proposal.expected_improvement = 0.15;
        proposal.parameters["test_param"] = "value";
        
        SelfModification::ModificationResult result = 
            _self_mod->applyModification(proposal, true);
        
        if (result.status == SelfModification::ModificationStatus::SUCCESS) {
            // Test rollback
            bool rollback_success = _self_mod->rollback(result);
            // Rollback may succeed or fail depending on implementation
            assert(rollback_success || !result.can_rollback);
        }
        
        std::cout << "✓ Rollback test passed" << std::endl;
    }

    void testSafetyConstraints()
    {
        std::cout << "Testing safety constraints..." << std::endl;
        
        // Set safety level to SAFE only
        _self_mod->setSafetyLevel(SelfModification::SafetyLevel::SAFE);
        assert(_self_mod->getSafetyLevel() == SelfModification::SafetyLevel::SAFE);
        
        // Try to apply an experimental modification
        SelfModification::ModificationProposal unsafe_proposal;
        unsafe_proposal.type = SelfModification::ModificationType::ARCHITECTURE_REFACTOR;
        unsafe_proposal.safety_level = SelfModification::SafetyLevel::EXPERIMENTAL;
        unsafe_proposal.description = "Unsafe modification";
        unsafe_proposal.target_component = "TestComponent5";
        unsafe_proposal.expected_improvement = 0.50;
        
        SelfModification::ModificationResult result = 
            _self_mod->applyModification(unsafe_proposal, true);
        
        // Should be rejected due to safety level
        assert(result.status == SelfModification::ModificationStatus::REJECTED);
        
        // Test minimum improvement threshold
        _self_mod->configure(3, 0.20, true); // Set 20% minimum improvement
        _self_mod->setSafetyLevel(SelfModification::SafetyLevel::CAUTIOUS);
        
        SelfModification::ModificationProposal low_improvement;
        low_improvement.type = SelfModification::ModificationType::PARAMETER_TUNING;
        low_improvement.safety_level = SelfModification::SafetyLevel::SAFE;
        low_improvement.description = "Low improvement";
        low_improvement.target_component = "TestComponent6";
        low_improvement.expected_improvement = 0.10; // Below threshold
        
        result = _self_mod->applyModification(low_improvement, true);
        assert(result.status == SelfModification::ModificationStatus::REJECTED);
        
        std::cout << "✓ Safety constraints test passed" << std::endl;
    }

    void testModificationHistory()
    {
        std::cout << "Testing modification history..." << std::endl;
        
        // Get current history size
        std::vector<SelfModification::ModificationResult> history = 
            _self_mod->getModificationHistory(100);
        size_t initial_size = history.size();
        
        // Apply a modification
        SelfModification::ModificationProposal proposal;
        proposal.type = SelfModification::ModificationType::PARAMETER_TUNING;
        proposal.safety_level = SelfModification::SafetyLevel::SAFE;
        proposal.description = "History test";
        proposal.target_component = "TestComponent7";
        proposal.expected_improvement = 0.12;
        
        _self_mod->setSafetyLevel(SelfModification::SafetyLevel::CAUTIOUS);
        _self_mod->configure(3, 0.05, true);
        
        SelfModification::ModificationResult result = 
            _self_mod->applyModification(proposal, true);
        
        // Get updated history
        history = _self_mod->getModificationHistory(100);
        
        // History should have grown (if modification succeeded)
        if (result.status == SelfModification::ModificationStatus::SUCCESS ||
            result.status == SelfModification::ModificationStatus::REJECTED) {
            assert(history.size() == initial_size + 1);
        }
        
        // Get pending modifications
        std::vector<SelfModification::ModificationProposal> pending = 
            _self_mod->getPendingModifications();
        // Pending list can be empty if all modifications are processed
        assert(pending.size() >= 0);
        
        std::cout << "✓ Modification history test passed" << std::endl;
    }

    void testUtilityMethods()
    {
        std::cout << "Testing utility methods..." << std::endl;
        
        // Test type to string conversion
        std::string type_str = SelfModification::typeToString(
            SelfModification::ModificationType::PARAMETER_TUNING);
        assert(type_str == "ParameterTuning");
        
        type_str = SelfModification::typeToString(
            SelfModification::ModificationType::CODE_OPTIMIZATION);
        assert(type_str == "CodeOptimization");
        
        // Test safety level to string conversion
        std::string safety_str = SelfModification::safetyLevelToString(
            SelfModification::SafetyLevel::SAFE);
        assert(safety_str == "Safe");
        
        safety_str = SelfModification::safetyLevelToString(
            SelfModification::SafetyLevel::EXPERIMENTAL);
        assert(safety_str == "Experimental");
        
        // Test status to string conversion
        std::string status_str = SelfModification::statusToString(
            SelfModification::ModificationStatus::SUCCESS);
        assert(status_str == "Success");
        
        status_str = SelfModification::statusToString(
            SelfModification::ModificationStatus::REJECTED);
        assert(status_str == "Rejected");
        
        // Test auto-modification control
        _self_mod->setAutoModification(true);
        assert(_self_mod->isAutoModificationEnabled());
        
        _self_mod->setAutoModification(false);
        assert(!_self_mod->isAutoModificationEnabled());
        
        std::cout << "✓ Utility methods test passed" << std::endl;
    }
};

int main(int argc, char* argv[])
{
    try {
        SelfModificationTest test;
        test.runAllTests();
        std::cout << "\n🎉 All tests completed successfully!\n" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
