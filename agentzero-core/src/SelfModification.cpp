/*
 * opencog/agentzero/SelfModification.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Implementation of SelfModification class
 */

#include "opencog/agentzero/SelfModification.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <algorithm>
#include <sstream>
#include <iostream>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

namespace opencog {
namespace agentzero {

// Configuration constants
namespace {
    constexpr double PARAMETER_REDUCTION_FACTOR = 0.9;  // 10% reduction
    constexpr double EXPECTED_IMPROVEMENT_PARAMETER_TUNING = 0.10;  // 10%
    constexpr double EXPECTED_IMPROVEMENT_CODE_OPTIMIZATION = 0.25;  // 25%
    constexpr double EXPECTED_IMPROVEMENT_ARCHITECTURE_REFACTOR = 0.40;  // 40%
    constexpr double DEFAULT_EXPECTED_IMPROVEMENT = 0.15;  // 15%
}

SelfModification::SelfModification(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(atomspace)
    , _current_safety_level(SafetyLevel::CAUTIOUS)
    , _enable_auto_modification(false)
    , _enable_rollback(true)
    , _max_modification_depth(3)
    , _minimum_improvement_threshold(0.05)
{
    logger().info("[SelfModification] Initializing self-modification system");
    initializeSelfModification();
}

SelfModification::~SelfModification()
{
    logger().info("[SelfModification] Shutting down self-modification system");
}

void SelfModification::initializeSelfModification()
{
    if (!_atomspace) {
        logger().error("[SelfModification] AtomSpace is null, cannot initialize");
        return;
    }
    
    // Create context nodes in AtomSpace
    _selfmod_context = _atomspace->add_node(CONCEPT_NODE, "SelfModificationContext");
    _analysis_context = _atomspace->add_node(CONCEPT_NODE, "CodeAnalysisContext");
    _modification_context = _atomspace->add_node(CONCEPT_NODE, "ModificationContext");
    _safety_context = _atomspace->add_node(CONCEPT_NODE, "SafetyContext");
    _history_context = _atomspace->add_node(CONCEPT_NODE, "ModificationHistoryContext");
    
    // Initialize modification handlers
    _modification_handlers[ModificationType::PARAMETER_TUNING] = 
        [this](const ModificationProposal& p) { return applyParameterTuning(p); };
    _modification_handlers[ModificationType::STRATEGY_REPLACEMENT] = 
        [this](const ModificationProposal& p) { return applyStrategyReplacement(p); };
    _modification_handlers[ModificationType::CODE_OPTIMIZATION] = 
        [this](const ModificationProposal& p) { return applyCodeOptimization(p); };
    _modification_handlers[ModificationType::BEHAVIOR_ADDITION] = 
        [this](const ModificationProposal& p) { return applyBehaviorModification(p); };
    _modification_handlers[ModificationType::BEHAVIOR_REMOVAL] = 
        [this](const ModificationProposal& p) { return applyBehaviorModification(p); };
    
    logger().info("[SelfModification] Initialization complete");
}

// ============================================================================
// Core Interface Implementation
// ============================================================================

SelfModification::CodeAnalysis 
SelfModification::analyzeComponent(const std::string& component_name)
{
    logger().info("[SelfModification] Analyzing component: " + component_name);
    
    // Check if we have a cached analysis
    auto it = _component_analyses.find(component_name);
    if (it != _component_analyses.end()) {
        logger().info("[SelfModification] Using cached analysis for " + component_name);
        return it->second;
    }
    
    // Perform new analysis
    CodeAnalysis analysis = analyzeComponentStructure(component_name);
    
    // Store in cache
    _component_analyses[component_name] = analysis;
    
    // Create AtomSpace representation
    analysis.analysis_atom = createAnalysisAtom(analysis);
    
    logger().info("[SelfModification] Analysis complete for " + component_name);
    return analysis;
}

std::vector<SelfModification::ModificationProposal> 
SelfModification::proposeModifications(const std::string& component_name, int max_proposals)
{
    logger().info("[SelfModification] Generating modification proposals for: " + component_name);
    
    std::vector<ModificationProposal> proposals;
    
    // Analyze the component
    CodeAnalysis analysis = analyzeComponent(component_name);
    
    // Generate optimization proposals based on bottlenecks
    if (!analysis.bottlenecks.empty()) {
        ModificationProposal opt_proposal = generateOptimizationProposal(analysis);
        proposals.push_back(opt_proposal);
    }
    
    // Generate parameter tuning proposals
    if (!analysis.performance_metrics.empty()) {
        ModificationProposal param_proposal = 
            generateParameterTuningProposal(component_name, analysis.performance_metrics);
        proposals.push_back(param_proposal);
    }
    
    // Generate refactoring proposals if complexity is high
    if (analysis.complexity_score > 0.7) {
        ModificationProposal refactor_proposal = generateRefactoringProposal(component_name);
        proposals.push_back(refactor_proposal);
    }
    
    // Limit to max_proposals
    if (proposals.size() > static_cast<size_t>(max_proposals)) {
        proposals.resize(max_proposals);
    }
    
    // Assess safety and predict improvement for each proposal
    for (auto& proposal : proposals) {
        proposal.safety_level = assessModificationSafety(proposal);
        proposal.expected_improvement = predictImprovement(proposal);
        proposal.proposal_atom = createProposalAtom(proposal);
    }
    
    logger().info("[SelfModification] Generated " + std::to_string(proposals.size()) + " proposals");
    return proposals;
}

std::vector<SelfModification::ModificationProposal> 
SelfModification::evaluateProposals(const std::vector<ModificationProposal>& proposals)
{
    logger().info("[SelfModification] Evaluating " + std::to_string(proposals.size()) + " proposals");
    
    return rankProposals(proposals);
}

SelfModification::ModificationResult 
SelfModification::applyModification(const ModificationProposal& proposal, bool validate)
{
    logger().info("[SelfModification] Applying modification: " + proposal.description);
    
    ModificationResult result;
    result.proposal = proposal;
    result.timestamp = std::chrono::steady_clock::now();
    
    // Validate if requested
    if (validate && !validateModification(proposal)) {
        logger().warn("[SelfModification] Modification validation failed");
        result.status = ModificationStatus::REJECTED;
        result.error_message = "Validation failed";
        result.can_rollback = false;
        return result;
    }
    
    // Check safety constraints
    if (!checkSafetyConstraints(proposal)) {
        logger().warn("[SelfModification] Safety constraints not met");
        result.status = ModificationStatus::REJECTED;
        result.error_message = "Safety constraints not met";
        result.can_rollback = false;
        return result;
    }
    
    // Create checkpoint if rollback is enabled
    if (_enable_rollback) {
        createCheckpoint(proposal.target_component);
    }
    
    // Execute the modification
    result = executeModification(proposal);
    
    // Record in history
    _modification_history.push_back(result);
    if (result.status == ModificationStatus::SUCCESS) {
        _modified_components.insert(proposal.target_component);
    }
    
    // Create AtomSpace representation
    result.result_atom = createResultAtom(result);
    
    // Learn from the result
    learnFromModificationResult(result);
    
    logger().info("[SelfModification] Modification " + statusToString(result.status));
    return result;
}

bool SelfModification::rollback(const ModificationResult& result)
{
    logger().info("[SelfModification] Attempting rollback of modification");
    
    if (!result.can_rollback) {
        logger().error("[SelfModification] Modification cannot be rolled back");
        return false;
    }
    
    if (!_enable_rollback) {
        logger().error("[SelfModification] Rollback is disabled");
        return false;
    }
    
    return rollbackModification(result);
}

// ============================================================================
// Analysis Methods
// ============================================================================

SelfModification::CodeAnalysis 
SelfModification::analyzeComponentStructure(const std::string& component_name)
{
    CodeAnalysis analysis;
    analysis.component_name = component_name;
    
    // Calculate complexity score (simplified analysis)
    analysis.complexity_score = calculateComplexityScore(component_name);
    
    // Calculate maintainability score (inverse of complexity)
    analysis.maintainability_score = 1.0 - analysis.complexity_score;
    
    // Add some example performance metrics
    analysis.performance_metrics["execution_time"] = 100.0;  // milliseconds
    analysis.performance_metrics["memory_usage"] = 1024.0;   // KB
    analysis.performance_metrics["cpu_utilization"] = 0.5;   // 0-1
    
    // Identify bottlenecks and opportunities
    analysis.bottlenecks = identifyBottlenecks(analysis);
    analysis.improvement_opportunities = findImprovementOpportunities(analysis);
    
    return analysis;
}

std::vector<std::string> 
SelfModification::identifyBottlenecks(const CodeAnalysis& analysis)
{
    std::vector<std::string> bottlenecks;
    
    // Check for performance bottlenecks
    auto exec_time_it = analysis.performance_metrics.find("execution_time");
    if (exec_time_it != analysis.performance_metrics.end() && exec_time_it->second > 500.0) {
        bottlenecks.push_back("High execution time detected");
    }
    
    auto mem_it = analysis.performance_metrics.find("memory_usage");
    if (mem_it != analysis.performance_metrics.end() && mem_it->second > 10240.0) {
        bottlenecks.push_back("High memory usage detected");
    }
    
    if (analysis.complexity_score > 0.8) {
        bottlenecks.push_back("High code complexity");
    }
    
    return bottlenecks;
}

std::vector<std::string> 
SelfModification::findImprovementOpportunities(const CodeAnalysis& analysis)
{
    std::vector<std::string> opportunities;
    
    if (analysis.complexity_score > 0.6) {
        opportunities.push_back("Refactor to reduce complexity");
    }
    
    if (!analysis.bottlenecks.empty()) {
        opportunities.push_back("Optimize identified bottlenecks");
    }
    
    if (analysis.maintainability_score < 0.5) {
        opportunities.push_back("Improve code maintainability");
    }
    
    return opportunities;
}

double SelfModification::calculateComplexityScore(const std::string& component_name)
{
    // Simplified complexity calculation
    // In a real implementation, this would analyze actual code metrics
    
    // Base complexity on component name length (placeholder)
    double base_complexity = std::min(component_name.length() / 50.0, 1.0);
    
    // Add some randomness for demonstration (would be actual metrics in production)
    return std::min(base_complexity + 0.2, 1.0);
}

// ============================================================================
// Proposal Generation Methods
// ============================================================================

SelfModification::ModificationProposal 
SelfModification::generateOptimizationProposal(const CodeAnalysis& analysis)
{
    ModificationProposal proposal;
    proposal.type = ModificationType::CODE_OPTIMIZATION;
    proposal.target_component = analysis.component_name;
    proposal.description = "Optimize " + analysis.component_name + " to reduce bottlenecks";
    
    // Set parameters based on bottlenecks
    for (const auto& bottleneck : analysis.bottlenecks) {
        proposal.parameters["bottleneck"] = bottleneck;
    }
    
    return proposal;
}

SelfModification::ModificationProposal 
SelfModification::generateRefactoringProposal(const std::string& component_name)
{
    ModificationProposal proposal;
    proposal.type = ModificationType::ARCHITECTURE_REFACTOR;
    proposal.target_component = component_name;
    proposal.description = "Refactor " + component_name + " to reduce complexity";
    
    return proposal;
}

SelfModification::ModificationProposal 
SelfModification::generateParameterTuningProposal(const std::string& component_name,
                                                  const std::map<std::string, double>& params)
{
    ModificationProposal proposal;
    proposal.type = ModificationType::PARAMETER_TUNING;
    proposal.target_component = component_name;
    proposal.description = "Tune parameters for " + component_name;
    
    // Convert numeric parameters to strings
    for (const auto& param : params) {
        proposal.parameters[param.first] = std::to_string(param.second * PARAMETER_REDUCTION_FACTOR);
    }
    
    return proposal;
}

SelfModification::SafetyLevel 
SelfModification::assessModificationSafety(const ModificationProposal& proposal)
{
    // Parameter tuning is generally safe
    if (proposal.type == ModificationType::PARAMETER_TUNING) {
        return SafetyLevel::SAFE;
    }
    
    // Code optimization can be cautious
    if (proposal.type == ModificationType::CODE_OPTIMIZATION) {
        return SafetyLevel::CAUTIOUS;
    }
    
    // Architecture changes are experimental
    if (proposal.type == ModificationType::ARCHITECTURE_REFACTOR) {
        return SafetyLevel::EXPERIMENTAL;
    }
    
    return SafetyLevel::CAUTIOUS;
}

double SelfModification::predictImprovement(const ModificationProposal& proposal)
{
    // Simplified improvement prediction
    // In production, this would use machine learning models
    
    switch (proposal.type) {
        case ModificationType::PARAMETER_TUNING:
            return EXPECTED_IMPROVEMENT_PARAMETER_TUNING;
        case ModificationType::CODE_OPTIMIZATION:
            return EXPECTED_IMPROVEMENT_CODE_OPTIMIZATION;
        case ModificationType::ARCHITECTURE_REFACTOR:
            return EXPECTED_IMPROVEMENT_ARCHITECTURE_REFACTOR;
        default:
            return DEFAULT_EXPECTED_IMPROVEMENT;
    }
}

// ============================================================================
// Execution Methods
// ============================================================================

bool SelfModification::validateModification(const ModificationProposal& proposal)
{
    logger().info("[SelfModification] Validating modification proposal");
    
    // Check if component exists
    if (proposal.target_component.empty()) {
        logger().error("[SelfModification] Target component is empty");
        return false;
    }
    
    // Check if prerequisites are met
    for (const auto& prereq : proposal.prerequisites) {
        logger().debug("[SelfModification] Checking prerequisite: " + prereq);
        // In production, actually check if prerequisites are satisfied
    }
    
    return true;
}

SelfModification::ModificationResult 
SelfModification::executeModification(const ModificationProposal& proposal)
{
    ModificationResult result;
    result.proposal = proposal;
    result.timestamp = std::chrono::steady_clock::now();
    
    // Find and execute the appropriate handler
    auto handler_it = _modification_handlers.find(proposal.type);
    if (handler_it == _modification_handlers.end()) {
        logger().error("[SelfModification] No handler for modification type");
        result.status = ModificationStatus::FAILED;
        result.error_message = "No handler for modification type";
        result.can_rollback = false;
        return result;
    }
    
    try {
        bool success = handler_it->second(proposal);
        
        if (success) {
            result.status = ModificationStatus::SUCCESS;
            result.actual_improvement = proposal.expected_improvement * 0.8; // Assume 80% of predicted
        } else {
            result.status = ModificationStatus::FAILED;
            result.error_message = "Handler returned false";
        }
    } catch (const std::exception& e) {
        logger().error("[SelfModification] Exception during modification: " + std::string(e.what()));
        result.status = ModificationStatus::FAILED;
        result.error_message = e.what();
    }
    
    return result;
}

bool SelfModification::applyParameterTuning(const ModificationProposal& proposal)
{
    logger().info("[SelfModification] Applying parameter tuning");
    
    // In production, actually modify parameters
    for (const auto& param : proposal.parameters) {
        logger().debug("[SelfModification] Tuning parameter: " + param.first + " = " + param.second);
    }
    
    return true;
}

bool SelfModification::applyStrategyReplacement(const ModificationProposal& proposal)
{
    logger().info("[SelfModification] Applying strategy replacement");
    
    // In production, actually replace strategies
    return true;
}

bool SelfModification::applyCodeOptimization(const ModificationProposal& proposal)
{
    logger().info("[SelfModification] Applying code optimization");
    
    // In production, actually optimize code
    return true;
}

bool SelfModification::applyBehaviorModification(const ModificationProposal& proposal)
{
    logger().info("[SelfModification] Applying behavior modification");
    
    // In production, actually modify behaviors
    return true;
}

// ============================================================================
// Safety and Rollback Methods
// ============================================================================

bool SelfModification::checkSafetyConstraints(const ModificationProposal& proposal)
{
    // Helper function to compare safety levels
    auto isMoreRestrictiveThan = [](SafetyLevel a, SafetyLevel b) -> bool {
        return static_cast<std::underlying_type_t<SafetyLevel>>(a) > 
               static_cast<std::underlying_type_t<SafetyLevel>>(b);
    };
    
    // Check if proposal safety level is acceptable
    if (isMoreRestrictiveThan(proposal.safety_level, _current_safety_level)) {
        logger().warn("[SelfModification] Proposal safety level exceeds current threshold");
        return false;
    }
    
    // Check if improvement meets minimum threshold
    if (proposal.expected_improvement < _minimum_improvement_threshold) {
        logger().warn("[SelfModification] Expected improvement below threshold");
        return false;
    }
    
    return true;
}

bool SelfModification::canRollback(const ModificationResult& result)
{
    return result.can_rollback && _enable_rollback;
}

bool SelfModification::rollbackModification(const ModificationResult& result)
{
    logger().info("[SelfModification] Rolling back modification");
    
    if (!canRollback(result)) {
        return false;
    }
    
    try {
        restoreCheckpoint(result.proposal.target_component);
        return true;
    } catch (const std::exception& e) {
        logger().error("[SelfModification] Rollback failed: " + std::string(e.what()));
        return false;
    }
}

void SelfModification::createCheckpoint(const std::string& component_name)
{
    logger().debug("[SelfModification] Creating checkpoint for: " + component_name);
    // In production, save current state
}

void SelfModification::restoreCheckpoint(const std::string& component_name)
{
    logger().debug("[SelfModification] Restoring checkpoint for: " + component_name);
    // In production, restore saved state
}

// ============================================================================
// AtomSpace Integration Methods
// ============================================================================

Handle SelfModification::createAnalysisAtom(const CodeAnalysis& analysis)
{
    if (!_atomspace) return Handle::UNDEFINED;
    
    Handle analysis_node = _atomspace->add_node(CONCEPT_NODE, 
        "Analysis:" + analysis.component_name);
    
    // Link to analysis context
    HandleSeq members = {analysis_node, _analysis_context};
    _atomspace->add_link(MEMBER_LINK, members);
    
    // Store complexity as truth value
    TruthValuePtr tv = SimpleTruthValue::createTV(analysis.complexity_score, 0.9);
    analysis_node->setTruthValue(tv);
    
    return analysis_node;
}

Handle SelfModification::createProposalAtom(const ModificationProposal& proposal)
{
    if (!_atomspace) return Handle::UNDEFINED;
    
    Handle proposal_node = _atomspace->add_node(CONCEPT_NODE,
        "Proposal:" + proposal.description);
    
    // Link to modification context
    HandleSeq members = {proposal_node, _modification_context};
    _atomspace->add_link(MEMBER_LINK, members);
    
    // Store expected improvement as truth value
    TruthValuePtr tv = SimpleTruthValue::createTV(proposal.expected_improvement, 0.8);
    proposal_node->setTruthValue(tv);
    
    return proposal_node;
}

Handle SelfModification::createResultAtom(const ModificationResult& result)
{
    if (!_atomspace) return Handle::UNDEFINED;
    
    Handle result_node = _atomspace->add_node(CONCEPT_NODE,
        "Result:" + result.proposal.target_component);
    
    // Link to history context
    HandleSeq members = {result_node, _history_context};
    _atomspace->add_link(MEMBER_LINK, members);
    
    // Store actual improvement as truth value
    TruthValuePtr tv = SimpleTruthValue::createTV(result.actual_improvement, 0.9);
    result_node->setTruthValue(tv);
    
    return result_node;
}

void SelfModification::recordModificationDecision(const ModificationProposal& proposal,
                                                 const ModificationResult& result)
{
    if (!_atomspace) return;
    
    logger().debug("[SelfModification] Recording modification decision in AtomSpace");
    
    // Create link between proposal and result
    if (proposal.proposal_atom != Handle::UNDEFINED && 
        result.result_atom != Handle::UNDEFINED) {
        HandleSeq pair = {proposal.proposal_atom, result.result_atom};
        _atomspace->add_link(EVALUATION_LINK, pair);
    }
}

// ============================================================================
// Learning Methods
// ============================================================================

void SelfModification::learnFromModificationResult(const ModificationResult& result)
{
    logger().debug("[SelfModification] Learning from modification result");
    
    // Update modification strategies based on result
    if (result.status == ModificationStatus::SUCCESS) {
        // Reinforce successful modification patterns
        updateModificationStrategies();
    }
    
    // Record decision in AtomSpace
    recordModificationDecision(result.proposal, result);
}

void SelfModification::updateModificationStrategies()
{
    logger().debug("[SelfModification] Updating modification strategies");
    
    // In production, use machine learning to update strategies
}

std::vector<SelfModification::ModificationProposal> 
SelfModification::rankProposals(const std::vector<ModificationProposal>& proposals)
{
    std::vector<ModificationProposal> ranked = proposals;
    
    // Sort by expected improvement (descending) and risk (ascending)
    std::sort(ranked.begin(), ranked.end(), 
        [](const ModificationProposal& a, const ModificationProposal& b) {
            double score_a = a.expected_improvement - a.risk_factor;
            double score_b = b.expected_improvement - b.risk_factor;
            return score_a > score_b;
        });
    
    return ranked;
}

// ============================================================================
// Query Methods
// ============================================================================

std::vector<SelfModification::ModificationResult> 
SelfModification::getModificationHistory(int max_results) const
{
    int count = std::min(max_results, static_cast<int>(_modification_history.size()));
    
    std::vector<ModificationResult> history;
    auto it = _modification_history.rbegin();
    for (int i = 0; i < count && it != _modification_history.rend(); ++i, ++it) {
        history.push_back(*it);
    }
    
    return history;
}

SelfModification::CodeAnalysis 
SelfModification::getCachedAnalysis(const std::string& component_name) const
{
    auto it = _component_analyses.find(component_name);
    if (it != _component_analyses.end()) {
        return it->second;
    }
    
    // Return empty analysis
    CodeAnalysis empty;
    return empty;
}

// ============================================================================
// Configuration Methods
// ============================================================================

void SelfModification::configure(int max_depth, double min_improvement, bool enable_rollback)
{
    _max_modification_depth = max_depth;
    _minimum_improvement_threshold = min_improvement;
    _enable_rollback = enable_rollback;
    
    logger().info("[SelfModification] Configuration updated");
}

void SelfModification::setModificationHandler(ModificationType type,
                                             std::function<bool(const ModificationProposal&)> handler)
{
    _modification_handlers[type] = handler;
}

bool SelfModification::isInitialized() const
{
    return _atomspace != nullptr && 
           _selfmod_context != Handle::UNDEFINED &&
           !_modification_handlers.empty();
}

// ============================================================================
// Utility Methods
// ============================================================================

std::string SelfModification::typeToString(ModificationType type)
{
    switch (type) {
        case ModificationType::PARAMETER_TUNING: return "ParameterTuning";
        case ModificationType::STRATEGY_REPLACEMENT: return "StrategyReplacement";
        case ModificationType::CODE_OPTIMIZATION: return "CodeOptimization";
        case ModificationType::BEHAVIOR_ADDITION: return "BehaviorAddition";
        case ModificationType::BEHAVIOR_REMOVAL: return "BehaviorRemoval";
        case ModificationType::ARCHITECTURE_REFACTOR: return "ArchitectureRefactor";
        default: return "Unknown";
    }
}

std::string SelfModification::safetyLevelToString(SafetyLevel level)
{
    switch (level) {
        case SafetyLevel::SAFE: return "Safe";
        case SafetyLevel::CAUTIOUS: return "Cautious";
        case SafetyLevel::EXPERIMENTAL: return "Experimental";
        case SafetyLevel::UNSAFE: return "Unsafe";
        default: return "Unknown";
    }
}

std::string SelfModification::statusToString(ModificationStatus status)
{
    switch (status) {
        case ModificationStatus::SUCCESS: return "Success";
        case ModificationStatus::FAILED: return "Failed";
        case ModificationStatus::ROLLED_BACK: return "RolledBack";
        case ModificationStatus::PENDING_VALIDATION: return "PendingValidation";
        case ModificationStatus::REJECTED: return "Rejected";
        default: return "Unknown";
    }
}

} // namespace agentzero
} // namespace opencog
