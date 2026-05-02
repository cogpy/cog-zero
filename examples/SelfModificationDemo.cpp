/*
 * examples/SelfModificationDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of Agent-Zero Self-Modification capabilities
 * Part of AZ-META-001: Implement self-modification capabilities
 */

#include <iostream>
#include <memory>
#include <iomanip>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/types.h>

#include "opencog/agentzero/SelfModification.h"

using namespace opencog;
using namespace opencog::agentzero;

/**
 * Utility function to print a separator line
 */
void printSeparator(const std::string& title = "")
{
    std::cout << "\n" << std::string(70, '=') << std::endl;
    if (!title.empty()) {
        std::cout << "  " << title << std::endl;
        std::cout << std::string(70, '=') << std::endl;
    }
}

/**
 * Utility function to print code analysis results
 */
void printAnalysis(const SelfModification::CodeAnalysis& analysis)
{
    std::cout << "\nCode Analysis Results:" << std::endl;
    std::cout << "  Component: " << analysis.component_name << std::endl;
    std::cout << "  Complexity Score: " << std::fixed << std::setprecision(2) 
              << analysis.complexity_score << std::endl;
    std::cout << "  Maintainability Score: " << std::fixed << std::setprecision(2)
              << analysis.maintainability_score << std::endl;
    
    std::cout << "\n  Performance Metrics:" << std::endl;
    for (const auto& metric : analysis.performance_metrics) {
        std::cout << "    " << metric.first << ": " << metric.second << std::endl;
    }
    
    if (!analysis.bottlenecks.empty()) {
        std::cout << "\n  Bottlenecks Identified:" << std::endl;
        for (const auto& bottleneck : analysis.bottlenecks) {
            std::cout << "    - " << bottleneck << std::endl;
        }
    }
    
    if (!analysis.improvement_opportunities.empty()) {
        std::cout << "\n  Improvement Opportunities:" << std::endl;
        for (const auto& opportunity : analysis.improvement_opportunities) {
            std::cout << "    - " << opportunity << std::endl;
        }
    }
}

/**
 * Utility function to print modification proposal
 */
void printProposal(const SelfModification::ModificationProposal& proposal, int index)
{
    std::cout << "\nProposal #" << index << ":" << std::endl;
    std::cout << "  Type: " << SelfModification::typeToString(proposal.type) << std::endl;
    std::cout << "  Safety Level: " << SelfModification::safetyLevelToString(proposal.safety_level) << std::endl;
    std::cout << "  Target: " << proposal.target_component << std::endl;
    std::cout << "  Description: " << proposal.description << std::endl;
    std::cout << "  Expected Improvement: " << std::fixed << std::setprecision(1) 
              << (proposal.expected_improvement * 100) << "%" << std::endl;
    std::cout << "  Risk Factor: " << std::fixed << std::setprecision(2) 
              << proposal.risk_factor << std::endl;
}

/**
 * Utility function to print modification result
 */
void printResult(const SelfModification::ModificationResult& result)
{
    std::cout << "\nModification Result:" << std::endl;
    std::cout << "  Status: " << SelfModification::statusToString(result.status) << std::endl;
    std::cout << "  Target: " << result.proposal.target_component << std::endl;
    
    if (result.status == SelfModification::ModificationStatus::SUCCESS) {
        std::cout << "  Actual Improvement: " << std::fixed << std::setprecision(1)
                  << (result.actual_improvement * 100) << "%" << std::endl;
        std::cout << "  Can Rollback: " << (result.can_rollback ? "Yes" : "No") << std::endl;
    } else if (result.status == SelfModification::ModificationStatus::REJECTED ||
               result.status == SelfModification::ModificationStatus::FAILED) {
        std::cout << "  Error: " << result.error_message << std::endl;
    }
}

/**
 * Demonstrate basic component analysis
 */
void demonstrateComponentAnalysis(SelfModification& self_mod)
{
    printSeparator("1. Component Analysis");
    
    std::cout << "\nAnalyzing cognitive components..." << std::endl;
    
    std::vector<std::string> components = {
        "CognitiveLoop",
        "ReasoningEngine",
        "PerceptionProcessor"
    };
    
    for (const auto& component : components) {
        auto analysis = self_mod.analyzeComponent(component);
        printAnalysis(analysis);
    }
}

/**
 * Demonstrate proposal generation and evaluation
 */
void demonstrateProposalGeneration(SelfModification& self_mod)
{
    printSeparator("2. Modification Proposal Generation");
    
    std::string component = "CognitiveLoop";
    
    std::cout << "\nGenerating modification proposals for: " << component << std::endl;
    
    auto proposals = self_mod.proposeModifications(component, 3);
    
    std::cout << "\nGenerated " << proposals.size() << " proposals:" << std::endl;
    
    for (size_t i = 0; i < proposals.size(); ++i) {
        printProposal(proposals[i], i + 1);
    }
    
    // Evaluate and rank proposals
    std::cout << "\n\nEvaluating and ranking proposals..." << std::endl;
    auto ranked = self_mod.evaluateProposals(proposals);
    
    std::cout << "\nRanked proposals (best first):" << std::endl;
    for (size_t i = 0; i < ranked.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << ranked[i].description 
                  << " (Score: " << std::fixed << std::setprecision(2)
                  << (ranked[i].expected_improvement - ranked[i].risk_factor) << ")" 
                  << std::endl;
    }
}

/**
 * Demonstrate safe modification application
 */
void demonstrateSafeModification(SelfModification& self_mod)
{
    printSeparator("3. Safe Modification Application");
    
    // Configure for safe operations
    self_mod.setSafetyLevel(SelfModification::SafetyLevel::CAUTIOUS);
    self_mod.configure(3, 0.05, true); // max_depth=3, min_improvement=5%, enable_rollback
    
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Safety Level: " << SelfModification::safetyLevelToString(self_mod.getSafetyLevel()) << std::endl;
    std::cout << "  Rollback Enabled: Yes" << std::endl;
    std::cout << "  Minimum Improvement: 5%" << std::endl;
    
    // Create a safe parameter tuning proposal
    SelfModification::ModificationProposal proposal;
    proposal.type = SelfModification::ModificationType::PARAMETER_TUNING;
    proposal.safety_level = SelfModification::SafetyLevel::SAFE;
    proposal.description = "Optimize attention allocation parameters";
    proposal.target_component = "AttentionManager";
    proposal.expected_improvement = 0.15;
    proposal.risk_factor = 0.02;
    proposal.parameters["attention_threshold"] = "0.5";
    proposal.parameters["spreading_factor"] = "0.3";
    
    std::cout << "\nApplying safe modification:" << std::endl;
    printProposal(proposal, 1);
    
    auto result = self_mod.applyModification(proposal, true);
    printResult(result);
}

/**
 * Demonstrate safety constraint enforcement
 */
void demonstrateSafetyConstraints(SelfModification& self_mod)
{
    printSeparator("4. Safety Constraint Enforcement");
    
    // Set strict safety level
    self_mod.setSafetyLevel(SelfModification::SafetyLevel::SAFE);
    
    std::cout << "\nSetting strict safety level: SAFE only" << std::endl;
    
    // Try to apply an experimental modification
    SelfModification::ModificationProposal risky_proposal;
    risky_proposal.type = SelfModification::ModificationType::ARCHITECTURE_REFACTOR;
    risky_proposal.safety_level = SelfModification::SafetyLevel::EXPERIMENTAL;
    risky_proposal.description = "Major architecture refactoring";
    risky_proposal.target_component = "CognitiveArchitecture";
    risky_proposal.expected_improvement = 0.50;
    risky_proposal.risk_factor = 0.40;
    
    std::cout << "\nAttempting to apply experimental modification:" << std::endl;
    printProposal(risky_proposal, 1);
    
    auto result = self_mod.applyModification(risky_proposal, true);
    printResult(result);
    
    std::cout << "\nExplanation: The modification was rejected because its safety level " 
              << "(EXPERIMENTAL) exceeds the current threshold (SAFE)." << std::endl;
}

/**
 * Demonstrate modification history tracking
 */
void demonstrateModificationHistory(SelfModification& self_mod)
{
    printSeparator("5. Modification History");
    
    auto history = self_mod.getModificationHistory(10);
    
    std::cout << "\nRecent modifications (" << history.size() << " total):" << std::endl;
    
    int success_count = 0;
    int rejected_count = 0;
    int failed_count = 0;
    
    for (size_t i = 0; i < history.size(); ++i) {
        const auto& result = history[i];
        std::cout << "\n  #" << (i + 1) << " - " << result.proposal.target_component << std::endl;
        std::cout << "      Type: " << SelfModification::typeToString(result.proposal.type) << std::endl;
        std::cout << "      Status: " << SelfModification::statusToString(result.status) << std::endl;
        
        switch (result.status) {
            case SelfModification::ModificationStatus::SUCCESS:
                success_count++;
                break;
            case SelfModification::ModificationStatus::REJECTED:
                rejected_count++;
                break;
            case SelfModification::ModificationStatus::FAILED:
                failed_count++;
                break;
            default:
                break;
        }
    }
    
    std::cout << "\n\nSummary:" << std::endl;
    std::cout << "  Successful: " << success_count << std::endl;
    std::cout << "  Rejected: " << rejected_count << std::endl;
    std::cout << "  Failed: " << failed_count << std::endl;
    
    auto modified_components = self_mod.getModifiedComponents();
    std::cout << "\n  Modified Components: " << modified_components.size() << std::endl;
    for (const auto& component : modified_components) {
        std::cout << "    - " << component << std::endl;
    }
}

/**
 * Demonstrate AtomSpace integration
 */
void demonstrateAtomSpaceIntegration(AtomSpacePtr atomspace)
{
    printSeparator("6. AtomSpace Integration");
    
    std::cout << "\nAtomSpace Statistics:" << std::endl;
    std::cout << "  Total Atoms: " << atomspace->get_size() << std::endl;
    std::cout << "  Nodes: " << atomspace->get_num_nodes() << std::endl;
    std::cout << "  Links: " << atomspace->get_num_links() << std::endl;
    
    std::cout << "\nSelf-modification data is stored in AtomSpace using:" << std::endl;
    std::cout << "  - ConceptNodes for components and contexts" << std::endl;
    std::cout << "  - TruthValues for metrics and scores" << std::endl;
    std::cout << "  - Links for relationships between proposals and results" << std::endl;
    std::cout << "\nThis enables integration with PLN reasoning and learning systems." << std::endl;
}

/**
 * Main demonstration program
 */
int main(int argc, char* argv[])
{
    try {
        std::cout << "\n";
        printSeparator("Agent-Zero Self-Modification Demonstration");
        std::cout << "\nThis demonstration showcases the self-modification capabilities" << std::endl;
        std::cout << "implemented as part of Phase 10: Advanced Features (AZ-META-001)" << std::endl;
        
        // Create AtomSpace
        AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
        
        // Create SelfModification instance
        SelfModification self_mod(nullptr, atomspace);
        
        // Run demonstrations
        demonstrateComponentAnalysis(self_mod);
        demonstrateProposalGeneration(self_mod);
        demonstrateSafeModification(self_mod);
        demonstrateSafetyConstraints(self_mod);
        demonstrateModificationHistory(self_mod);
        demonstrateAtomSpaceIntegration(atomspace);
        
        printSeparator("Demonstration Complete");
        std::cout << "\nAll self-modification capabilities have been demonstrated." << std::endl;
        std::cout << "The system is ready for integration with the full Agent-Zero" << std::endl;
        std::cout << "cognitive architecture.\n" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nUnknown error occurred" << std::endl;
        return 1;
    }
}
