/*
 * opencog/agentzero/KnowledgeIntegrator.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Knowledge Integrator Implementation
 * Bridges knowledge representation with AtomSpace operations
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_KNOWLEDGE_INTEGRATOR_H
#define _OPENCOG_AGENTZERO_KNOWLEDGE_INTEGRATOR_H

#include <memory>
#include <string>
#include <vector>
#include <set>
#include <map>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;

/**
 * KnowledgeIntegrator - Bridges knowledge representation with AtomSpace
 *
 * This class provides comprehensive knowledge management capabilities,
 * integrating various forms of knowledge representation with OpenCog's
 * AtomSpace. It handles knowledge acquisition, storage, retrieval,
 * and reasoning integration.
 *
 * Key features:
 * - Knowledge representation in AtomSpace
 * - Concept formation and relationship mapping
 * - Semantic knowledge integration
 * - Memory consolidation and organization
 * - Query and retrieval interfaces
 */
class KnowledgeIntegrator
{
public:
    // Knowledge types for categorization
    enum class KnowledgeType {
        FACTUAL,        // Facts about the world
        PROCEDURAL,     // How to perform actions
        EPISODIC,       // Experience memories
        SEMANTIC,       // Concept relationships
        CONDITIONAL,    // If-then rules
        TEMPORAL        // Time-based knowledge
    };
    
    // Confidence levels for knowledge assertions
    enum class ConfidenceLevel {
        VERY_LOW = 0,
        LOW = 25,
        MEDIUM = 50,
        HIGH = 75,
        VERY_HIGH = 100
    };

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    
    // Knowledge organization structures
    std::map<KnowledgeType, Handle> _knowledge_categories;
    std::map<std::string, Handle> _concept_registry;
    std::set<Handle> _active_knowledge;
    
    // AtomSpace handles for knowledge contexts
    Handle _knowledge_base;
    Handle _working_knowledge;
    Handle _semantic_network;
    Handle _episodic_memory;
    Handle _procedural_memory;
    
    // Configuration
    bool _enable_concept_formation;
    bool _enable_semantic_integration;
    bool _enable_memory_consolidation;
    double _knowledge_threshold;
    
    // Internal methods
    void initializeKnowledgeStructures();
    Handle createKnowledgeAtom(const std::string& content, KnowledgeType type);
    Handle findOrCreateConcept(const std::string& concept_name);
    void establishConceptRelation(const Handle& concept1, const Handle& concept2, 
                                 const std::string& relation_type);
    TruthValuePtr assessKnowledgeReliability(const Handle& knowledge_atom);
    void consolidateMemory();
    std::vector<Handle> findRelatedKnowledge(const Handle& query_atom);
    
// <<<<<<< copilot/fix-33
    // Advanced concept formation methods
    std::vector<Handle> formConceptsByPatterns(const std::vector<Handle>& experience_atoms);
    std::vector<Handle> formConceptsByClustering(const std::vector<Handle>& experience_atoms);
    std::vector<Handle> formHierarchicalConcepts(const std::vector<Handle>& experience_atoms);
    std::vector<Handle> formConceptsByFrequency(const std::vector<Handle>& experience_atoms);
    void validateAndRefineNewConcepts(std::vector<Handle>& new_concepts, 
                                     const std::vector<Handle>& experience_atoms);
    
    // Helper methods for concept formation
    std::string createStructureSignature(const Handle& atom);
    void calculateSemanticSimilarityMatrix(const std::vector<Handle>& atoms, 
                                         std::vector<std::vector<double>>& matrix);
    double calculateAtomSimilarity(const Handle& atom1, const Handle& atom2);
    double calculateStringSimilarity(const std::string& str1, const std::string& str2);
    std::vector<std::vector<int>> performSimpleClustering(
        const std::vector<std::vector<double>>& similarity_matrix, double threshold);
    double calculateClusterCohesion(const std::vector<int>& cluster, 
                                   const std::vector<std::vector<double>>& similarity_matrix);
    std::vector<std::string> extractHierarchyLevels(const Handle& atom);
    std::vector<std::string> tokenizeWithContext(const std::string& text);
    std::string extractTermContext(const std::string& text, const std::string& term);
    double calculateSemanticWeight(const std::string& term);
    double calculateDynamicThreshold(const std::map<std::string, double>& term_significance,
                                    size_t experience_count);
    double evaluateConceptQuality(const Handle& concept, const std::vector<Handle>& experience_atoms);
    double evaluateConceptUniqueness(const Handle& concept);
    void refineConceptRelationships(Handle& concept, const std::vector<Handle>& all_concepts);
    void updateConceptConfidenceAfterValidation(Handle& concept, double quality_score);
// =======
    // Enhanced AtomSpace operations for Agent-Zero
    void initializeAdvancedReasoning();
    void initializePatternMining();
    std::vector<Handle> performInference(const std::vector<Handle>& premises, 
                                        const std::string& target_pattern = "");
    std::vector<Handle> discoverPatterns(const std::vector<Handle>& data_atoms, 
                                        double minimum_support = 0.3);
    std::vector<Handle> applyRules(const std::vector<Handle>& rule_set,
                                  const std::vector<Handle>& facts);
    Handle createInferenceContext(const std::string& reasoning_task);
    std::vector<Handle> performSemanticSearch(const std::string& query,
                                            const std::vector<Handle>& context = {});
    std::vector<Handle> generateHypotheses(const std::vector<Handle>& observations);
    void updateTruthValues(const std::vector<Handle>& atoms, 
                          const std::vector<double>& evidences);
    Handle createCompositeKnowledge(const std::vector<Handle>& component_atoms,
                                   const std::string& composition_type);
    std::vector<Handle> extractImplications(const std::vector<Handle>& premises);
    
    // Advanced query processing
    std::vector<Handle> queryWithInference(const std::string& query_pattern,
                                          const std::vector<Handle>& background_knowledge);
    std::vector<Handle> findSimilarKnowledge(const Handle& target_atom,
                                           double similarity_threshold = 0.7);
    std::map<Handle, double> rankKnowledgeRelevance(const std::vector<Handle>& candidates,
                                                   const Handle& context);
    
    // Pattern-based learning
    void learnFromInteractionPatterns(const std::vector<Handle>& interaction_sequence);
    std::vector<Handle> identifyKnowledgeGaps(const std::vector<Handle>& current_knowledge);
    Handle synthesizeNewKnowledge(const std::vector<Handle>& source_knowledge,
                                 const std::string& synthesis_goal);
    
    // Advanced configuration
    bool _enable_advanced_reasoning;
    bool _enable_pattern_mining;
    bool _enable_hypothesis_generation;
    double _inference_confidence_threshold;
    int _max_inference_steps;
    
    // Advanced AtomSpace handles
    Handle _reasoning_context;
    Handle _inference_history;
    Handle _learned_patterns;
    Handle _hypothesis_space;
    std::map<std::string, Handle> _reasoning_tasks;
    
    // Helper methods for enhanced operations
    double calculateSemanticSimilarity(const Handle& atom1, const Handle& atom2);
    double calculateStringSimilarity(const std::string& str1, const std::string& str2);
    bool atomMatches(const Handle& pattern, const Handle& atom);
    bool isContradictory(const Handle& atom1, const Handle& atom2);
    std::vector<Handle> extractTemporalPatterns(const std::vector<Handle>& sequence);
    std::vector<Handle> extractCausalPatterns(const std::vector<Handle>& sequence);
    std::vector<Handle> extractStatisticalPatterns(const std::vector<Handle>& data);
// >>>>>>> main

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    KnowledgeIntegrator(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - cleans up knowledge management resources
     */
    ~KnowledgeIntegrator();
    
    // Knowledge acquisition
    /**
     * Add new factual knowledge to the knowledge base
     * @param fact_description Description of the fact
     * @param confidence Confidence level in the fact
     * @return Handle to the created knowledge atom
     */
    Handle addFact(const std::string& fact_description, 
                   ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    
    /**
     * Add procedural knowledge (how to do something)
     * @param procedure_description Description of the procedure
     * @param steps Vector of step descriptions
     * @param confidence Confidence in the procedure
     * @return Handle to the created procedural knowledge
     */
    Handle addProcedure(const std::string& procedure_description,
                       const std::vector<std::string>& steps,
                       ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    
    /**
     * Add episodic memory (experience-based knowledge)
     * @param experience_description Description of the experience
     * @param context_atoms Related context atoms
     * @param confidence Confidence in the memory
     * @return Handle to the created episodic memory
     */
    Handle addEpisode(const std::string& experience_description,
                     const std::vector<Handle>& context_atoms,
                     ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    
    /**
     * Add semantic relationship between concepts
     * @param concept1_name Name of the first concept
     * @param relation_type Type of relationship (e.g., "isa", "has", "causes")
     * @param concept2_name Name of the second concept
     * @param confidence Confidence in the relationship
     * @return Handle to the created relationship
     */
    Handle addSemanticRelation(const std::string& concept1_name,
                              const std::string& relation_type,
                              const std::string& concept2_name,
                              ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    
    // Knowledge retrieval
    /**
     * Query knowledge base with natural language query
     * @param query_text Natural language query
     * @param max_results Maximum number of results to return
     * @return Vector of relevant knowledge atoms
     */
    std::vector<Handle> queryKnowledge(const std::string& query_text, 
                                      int max_results = 10);
    
    /**
     * Find facts related to a specific concept
     * @param concept_name Name of the concept
     * @return Vector of related fact atoms
     */
    std::vector<Handle> getFactsAbout(const std::string& concept_name);
    
    /**
     * Get procedural knowledge for a specific task
     * @param task_description Description of the task
     * @return Vector of relevant procedure atoms
     */
    std::vector<Handle> getProceduresFor(const std::string& task_description);
    
    /**
     * Retrieve episodic memories related to a context
     * @param context_atoms Context atoms to match against
     * @return Vector of relevant episodic memory atoms
     */
    std::vector<Handle> getEpisodesRelatedTo(const std::vector<Handle>& context_atoms);
    
    /**
     * Get semantic relationships for a concept
     * @param concept_name Name of the concept
     * @param relation_type Optional specific relation type filter
     * @return Vector of relationship atoms
     */
    std::vector<Handle> getSemanticRelations(const std::string& concept_name,
                                            const std::string& relation_type = "");
    
    // Concept management
    /**
     * Register a new concept in the knowledge base
     * @param concept_name Name of the concept
     * @param concept_description Optional description
     * @return Handle to the concept atom
     */
    Handle registerConcept(const std::string& concept_name, 
                          const std::string& concept_description = "");
    
    /**
     * Check if a concept exists in the knowledge base
     * @param concept_name Name of the concept
     * @return true if concept exists
     */
    bool hasKnowledgeAbout(const std::string& concept_name);
    
    /**
     * Get all known concepts
     * @return Vector of concept atom handles
     */
    std::vector<Handle> getAllConcepts();
    
    /**
     * Form new concepts from experience patterns
     * @param experience_atoms Vector of experience atoms to analyze
     * @return Vector of newly formed concept atoms
     */
    std::vector<Handle> formConceptsFrom(const std::vector<Handle>& experience_atoms);
    
    // Knowledge validation and maintenance
    /**
     * Validate knowledge consistency
     * @return Vector of inconsistent knowledge atoms found
     */
    std::vector<Handle> validateKnowledgeConsistency();
    
    /**
     * Update knowledge confidence based on new evidence
     * @param knowledge_atom Handle to knowledge to update
     * @param supporting_evidence Vector of supporting evidence atoms
     * @return Updated confidence level
     */
    ConfidenceLevel updateKnowledgeConfidence(const Handle& knowledge_atom,
                                             const std::vector<Handle>& supporting_evidence);
    
    /**
     * Remove or deprecate outdated knowledge
     * @param age_threshold_days Knowledge older than this will be reviewed
     * @return Number of knowledge items processed
     */
    int cleanupOutdatedKnowledge(int age_threshold_days = 30);
    
    // Integration interfaces
    /**
     * Import knowledge from external sources
     * @param source_description Description of the knowledge source
     * @param knowledge_data Structured knowledge data
     * @return Number of knowledge items imported
     */
    int importKnowledge(const std::string& source_description,
                       const std::map<std::string, std::string>& knowledge_data);
    
    /**
     * Export knowledge to external format
     * @param export_format Format for export (e.g., "json", "rdf", "text")
     * @param knowledge_filter Optional filter for specific knowledge types
     * @return Serialized knowledge data
     */
    std::string exportKnowledge(const std::string& export_format,
                               KnowledgeType knowledge_filter = KnowledgeType::FACTUAL);
    
    // Statistics and monitoring
    /**
     * Get knowledge base statistics
     * @return Map of statistics (concept_count, fact_count, etc.)
     */
    std::map<std::string, int> getKnowledgeStatistics();
    
    /**
     * Get the most active knowledge concepts
     * @param count Number of top concepts to return
     * @return Vector of most frequently accessed concept atoms
     */
    std::vector<Handle> getMostActiveKnowledge(int count = 10);
    
    // Configuration
    /**
     * Set knowledge acceptance threshold
     * @param threshold Minimum confidence for accepting new knowledge (0.0-1.0)
     */
    void setKnowledgeThreshold(double threshold) { _knowledge_threshold = threshold; }
    
    /**
     * Enable or disable automatic concept formation
     * @param enable Whether to automatically form concepts from patterns
     */
    void setConceptFormationEnabled(bool enable) { _enable_concept_formation = enable; }
    
    // AtomSpace integration
    /**
     * Get the knowledge base root atom
     * @return Handle to knowledge base root
     */
    Handle getKnowledgeBase() const { return _knowledge_base; }
    
    /**
     * Get the semantic network atom
     * @return Handle to semantic network
     */
    Handle getSemanticNetwork() const { return _semantic_network; }
    
    /**
     * Get status information for debugging
     * @return JSON string with status details
     */
    std::string getStatusInfo() const;
    
    /**
     * Process knowledge integration for one cycle
     * Called by the cognitive loop
     * @return true if processing completed successfully
     */
    bool processKnowledgeIntegration();
    
    // Enhanced AtomSpace Operations for Agent-Zero
    
    /**
     * Perform advanced reasoning with inference over knowledge base
     * @param query_atoms The atoms to reason about
     * @param reasoning_type Type of reasoning ("forward", "backward", "mixed")
     * @param max_steps Maximum number of reasoning steps
     * @return Vector of inferred knowledge atoms
     */
    std::vector<Handle> performAdvancedReasoning(const std::vector<Handle>& query_atoms,
                                               const std::string& reasoning_type = "mixed",
                                               int max_steps = 10);
    
    /**
     * Discover knowledge patterns using pattern mining
     * @param data_atoms Vector of atoms to analyze for patterns
     * @param pattern_type Type of pattern to discover ("sequential", "associative", "causal")
     * @param minimum_support Minimum support threshold for pattern acceptance
     * @return Vector of discovered pattern atoms
     */
    std::vector<Handle> discoverKnowledgePatterns(const std::vector<Handle>& data_atoms,
                                                 const std::string& pattern_type = "associative",
                                                 double minimum_support = 0.3);
    
    /**
     * Apply knowledge rules using the Unified Rule Engine
     * @param rule_base Vector of rule atoms to apply
     * @param facts Vector of fact atoms to apply rules to
     * @param rule_selection Strategy for rule selection ("random", "confidence", "relevance")
     * @return Vector of newly derived knowledge atoms
     */
    std::vector<Handle> applyKnowledgeRules(const std::vector<Handle>& rule_base,
                                          const std::vector<Handle>& facts,
                                          const std::string& rule_selection = "confidence");
    
    /**
     * Generate hypotheses from incomplete knowledge
     * @param observations Vector of observation atoms
     * @param hypothesis_templates Optional templates for hypothesis generation
     * @return Vector of generated hypothesis atoms with confidence scores
     */
    std::vector<Handle> generateKnowledgeHypotheses(const std::vector<Handle>& observations,
                                                   const std::vector<Handle>& hypothesis_templates = {});
    
    /**
     * Perform semantic similarity search in knowledge base
     * @param target_concept The concept to find similar items for
     * @param similarity_threshold Minimum similarity score (0.0-1.0)
     * @param max_results Maximum number of results to return
     * @return Vector of similar concept atoms with similarity scores
     */
    std::vector<Handle> findSemanticallySimilar(const Handle& target_concept,
                                              double similarity_threshold = 0.7,
                                              int max_results = 10);
    
    /**
     * Synthesize new knowledge from existing knowledge components
     * @param source_knowledge Vector of source knowledge atoms
     * @param synthesis_goal Description of what new knowledge to synthesize
     * @param synthesis_method Method for synthesis ("analogical", "compositional", "inductive")
     * @return Handle to synthesized knowledge atom
     */
    Handle synthesizeKnowledge(const std::vector<Handle>& source_knowledge,
                              const std::string& synthesis_goal,
                              const std::string& synthesis_method = "compositional");
    
    /**
     * Learn knowledge patterns from interaction sequences
     * @param interaction_atoms Vector of interaction atoms in sequence
     * @param learning_type Type of learning ("temporal", "causal", "statistical")
     * @return Vector of learned pattern atoms
     */
    std::vector<Handle> learnFromInteractions(const std::vector<Handle>& interaction_atoms,
                                            const std::string& learning_type = "temporal");
    
    /**
     * Validate knowledge consistency using reasoning
     * @param knowledge_subset Optional subset to validate (empty = all knowledge)
     * @return Vector of inconsistency atoms found with explanation
     */
    std::vector<Handle> validateKnowledgeWithReasoning(const std::vector<Handle>& knowledge_subset = {});
    
    /**
     * Update knowledge confidence based on reasoning outcomes
     * @param reasoning_results Vector of reasoning result atoms
     * @return Number of knowledge atoms with updated confidence
     */
    int updateConfidenceFromReasoning(const std::vector<Handle>& reasoning_results);
    
    /**
     * Query knowledge with advanced pattern matching and inference
     * @param query_pattern Natural language or logical pattern
     * @param context_atoms Optional context for query interpretation
     * @param enable_inference Whether to use inference for query answering
     * @return Vector of matching knowledge atoms with relevance scores
     */
    std::vector<Handle> advancedKnowledgeQuery(const std::string& query_pattern,
                                             const std::vector<Handle>& context_atoms = {},
                                             bool enable_inference = true);
    
    // Configuration for enhanced operations
    
    /**
     * Configure advanced reasoning parameters
     * @param enable_reasoning Whether to enable advanced reasoning
     * @param confidence_threshold Minimum confidence for reasoning results
     * @param max_inference_steps Maximum steps in inference chains
     */
    void configureAdvancedReasoning(bool enable_reasoning = true,
                                   double confidence_threshold = 0.6,
                                   int max_inference_steps = 15);
    
    /**
     * Configure pattern mining parameters
     * @param enable_mining Whether to enable pattern mining
     * @param min_support Minimum support for pattern acceptance
     * @param enable_hypothesis_generation Whether to generate hypotheses
     */
    void configurePatternMining(bool enable_mining = true,
                               double min_support = 0.3,
                               bool enable_hypothesis_generation = true);
    
    /**
     * Get advanced knowledge statistics including reasoning metrics
     * @return Map of advanced statistics (inference_count, pattern_count, etc.)
     */
    std::map<std::string, int> getAdvancedKnowledgeStatistics();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_KNOWLEDGE_INTEGRATOR_H