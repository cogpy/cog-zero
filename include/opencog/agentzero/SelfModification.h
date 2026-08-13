/*
 * opencog/agentzero/SelfModification.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * SelfModification - Agent self-modification and meta-programming capabilities
 * Part of AZ-META-001: Implement self-modification capabilities
 * Phase 10: Advanced Features
 */

#ifndef _OPENCOG_AGENTZERO_SELF_MODIFICATION_H
#define _OPENCOG_AGENTZERO_SELF_MODIFICATION_H

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <functional>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;

/**
 * SelfModification - Meta-programming and self-modification system
 *
 * This class provides capabilities for the agent to analyze, evaluate,
 * and modify its own code, strategies, and behaviors. It uses AtomSpace
 * for representing code structures, modification patterns, and performance
 * metrics.
 *
 * Key features:
 * - Code structure analysis and introspection
 * - Safe modification strategy generation
 * - Performance impact prediction
 * - Rollback and version control integration
 * - AtomSpace-based meta-representation
 * - Integration with MetaPlanner and MetaLearning
 */
class SelfModification
{
public:
    // Modification types
    enum class ModificationType {
        PARAMETER_TUNING,       // Adjust algorithm parameters
        STRATEGY_REPLACEMENT,   // Replace execution strategies
        CODE_OPTIMIZATION,      // Optimize code sections
        BEHAVIOR_ADDITION,      // Add new behaviors
        BEHAVIOR_REMOVAL,       // Remove unused behaviors
        ARCHITECTURE_REFACTOR   // Refactor system architecture
    };
    
    // Modification safety levels
    enum class SafetyLevel {
        SAFE,           // Verified safe modifications
        CAUTIOUS,       // Potentially safe, needs monitoring
        EXPERIMENTAL,   // Experimental modifications
        UNSAFE          // Known unsafe modifications
    };
    
    // Modification result status
    enum class ModificationStatus {
        SUCCESS,            // Successfully applied
        FAILED,             // Failed to apply
        ROLLED_BACK,        // Applied but rolled back
        PENDING_VALIDATION, // Waiting for validation
        REJECTED            // Rejected by safety checks
    };
    
    // Code analysis results
    struct CodeAnalysis {
        std::string component_name;
        std::map<std::string, double> performance_metrics;
        std::vector<std::string> bottlenecks;
        std::vector<std::string> improvement_opportunities;
        double complexity_score{0.0};
        double maintainability_score{0.0};
        Handle analysis_atom;
        
        void reset() {
            component_name.clear();
            performance_metrics.clear();
            bottlenecks.clear();
            improvement_opportunities.clear();
            complexity_score = 0.0;
            maintainability_score = 0.0;
            analysis_atom = Handle::UNDEFINED;
        }
    };
    
    // Modification proposal
    struct ModificationProposal {
        ModificationType type;
        SafetyLevel safety_level;
        std::string description;
        std::string target_component;
        std::map<std::string, std::string> parameters;
        double expected_improvement{0.0};
        double risk_factor{0.0};
        std::vector<std::string> prerequisites;
        Handle proposal_atom;
        
        ModificationProposal() : type(ModificationType::PARAMETER_TUNING),
                                safety_level(SafetyLevel::SAFE) {}
    };
    
    // Modification execution result
    struct ModificationResult {
        ModificationStatus status;
        ModificationProposal proposal;
        double actual_improvement{0.0};
        std::string error_message;
        std::chrono::steady_clock::time_point timestamp;
        Handle result_atom;
        bool can_rollback{true};
        
        ModificationResult() : status(ModificationStatus::PENDING_VALIDATION) {
            timestamp = std::chrono::steady_clock::now();
        }
    };

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    
    // Self-modification state
    std::vector<ModificationProposal> _pending_modifications;
    std::vector<ModificationResult> _modification_history;
    std::map<std::string, CodeAnalysis> _component_analyses;
    std::set<std::string> _modified_components;
    
    // Safety and validation
    SafetyLevel _current_safety_level;
    bool _enable_auto_modification;
    bool _enable_rollback;
    int _max_modification_depth;
    double _minimum_improvement_threshold;
    
    // AtomSpace handles
    Handle _selfmod_context;
    Handle _analysis_context;
    Handle _modification_context;
    Handle _safety_context;
    Handle _history_context;
    
    // Modification strategies
    std::map<ModificationType, std::function<bool(const ModificationProposal&)>> _modification_handlers;
    
    // Internal methods - Analysis
    void initializeSelfModification();
    CodeAnalysis analyzeComponentStructure(const std::string& component_name);
    std::vector<std::string> identifyBottlenecks(const CodeAnalysis& analysis);
    std::vector<std::string> findImprovementOpportunities(const CodeAnalysis& analysis);
    double calculateComplexityScore(const std::string& component_name);
    
    // Internal methods - Proposal Generation
    ModificationProposal generateOptimizationProposal(const CodeAnalysis& analysis);
    ModificationProposal generateRefactoringProposal(const std::string& component_name);
    ModificationProposal generateParameterTuningProposal(const std::string& component_name,
                                                        const std::map<std::string, double>& params);
    SafetyLevel assessModificationSafety(const ModificationProposal& proposal);
    double predictImprovement(const ModificationProposal& proposal);
    
    // Internal methods - Execution
    bool validateModification(const ModificationProposal& proposal);
    ModificationResult executeModification(const ModificationProposal& proposal);
    bool applyParameterTuning(const ModificationProposal& proposal);
    bool applyStrategyReplacement(const ModificationProposal& proposal);
    bool applyCodeOptimization(const ModificationProposal& proposal);
    bool applyBehaviorModification(const ModificationProposal& proposal);
    
    // Internal methods - Safety and Rollback
    bool checkSafetyConstraints(const ModificationProposal& proposal);
    bool canRollback(const ModificationResult& result);
    bool rollbackModification(const ModificationResult& result);
    void createCheckpoint(const std::string& component_name);
    void restoreCheckpoint(const std::string& component_name);
    
    // Internal methods - AtomSpace Integration
    Handle createAnalysisAtom(const CodeAnalysis& analysis);
    Handle createProposalAtom(const ModificationProposal& proposal);
    Handle createResultAtom(const ModificationResult& result);
    void recordModificationDecision(const ModificationProposal& proposal, 
                                   const ModificationResult& result);
    
    // Internal methods - Learning Integration
    void learnFromModificationResult(const ModificationResult& result);
    void updateModificationStrategies();
    std::vector<ModificationProposal> rankProposals(const std::vector<ModificationProposal>& proposals);

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    SelfModification(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - ensures cleanup of self-modification resources
     */
    ~SelfModification();
    
    // Core self-modification interface
    /**
     * Analyze a component's code structure and performance
     * @param component_name Name of the component to analyze
     * @return Analysis results
     */
    CodeAnalysis analyzeComponent(const std::string& component_name);
    
    /**
     * Generate modification proposals for improving a component
     * @param component_name Name of the component to improve
     * @param max_proposals Maximum number of proposals to generate
     * @return Vector of modification proposals
     */
    std::vector<ModificationProposal> proposeModifications(const std::string& component_name,
                                                           int max_proposals = 5);
    
    /**
     * Evaluate and rank modification proposals
     * @param proposals Vector of proposals to evaluate
     * @return Ranked vector of proposals (best first)
     */
    std::vector<ModificationProposal> evaluateProposals(const std::vector<ModificationProposal>& proposals);
    
    /**
     * Apply a modification proposal
     * @param proposal The proposal to apply
     * @param validate Whether to validate before applying
     * @return Modification result
     */
    ModificationResult applyModification(const ModificationProposal& proposal, bool validate = true);
    
    /**
     * Rollback a previous modification
     * @param result The modification result to rollback
     * @return True if rollback successful
     */
    bool rollback(const ModificationResult& result);
    
    // Automatic modification control
    /**
     * Enable or disable automatic self-modification
     * @param enable True to enable automatic modification
     */
    void setAutoModification(bool enable) { _enable_auto_modification = enable; }
    
    /**
     * Check if automatic modification is enabled
     * @return True if enabled
     */
    bool isAutoModificationEnabled() const { return _enable_auto_modification; }
    
    /**
     * Set the safety level for modifications
     * @param level The safety level to enforce
     */
    void setSafetyLevel(SafetyLevel level) { _current_safety_level = level; }
    
    /**
     * Get the current safety level
     * @return Current safety level
     */
    SafetyLevel getSafetyLevel() const { return _current_safety_level; }
    
    // Introspection and monitoring
    /**
     * Get all pending modification proposals
     * @return Vector of pending proposals
     */
    std::vector<ModificationProposal> getPendingModifications() const { return _pending_modifications; }
    
    /**
     * Get modification history
     * @param max_results Maximum number of results to return
     * @return Vector of modification results
     */
    std::vector<ModificationResult> getModificationHistory(int max_results = 100) const;
    
    /**
     * Get list of components that have been modified
     * @return Set of component names
     */
    std::set<std::string> getModifiedComponents() const { return _modified_components; }
    
    /**
     * Get cached analysis for a component
     * @param component_name Name of the component
     * @return Analysis results (empty if not cached)
     */
    CodeAnalysis getCachedAnalysis(const std::string& component_name) const;
    
    // Configuration
    /**
     * Configure self-modification parameters
     * @param max_depth Maximum depth of recursive modifications
     * @param min_improvement Minimum improvement threshold (0.0-1.0)
     * @param enable_rollback Enable rollback capability
     */
    void configure(int max_depth, double min_improvement, bool enable_rollback = true);
    
    /**
     * Set modification handler for a specific type
     * @param type Modification type
     * @param handler Function to handle modifications of this type
     */
    void setModificationHandler(ModificationType type,
                               std::function<bool(const ModificationProposal&)> handler);
    
    /**
     * Check if self-modification system is properly initialized
     * @return True if initialized
     */
    bool isInitialized() const;
    
    // Utility methods
    /**
     * Convert modification type to string
     * @param type The type to convert
     * @return String representation
     */
    static std::string typeToString(ModificationType type);
    
    /**
     * Convert safety level to string
     * @param level The level to convert
     * @return String representation
     */
    static std::string safetyLevelToString(SafetyLevel level);
    
    /**
     * Convert status to string
     * @param status The status to convert
     * @return String representation
     */
    static std::string statusToString(ModificationStatus status);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_SELF_MODIFICATION_H
