/*
 * opencog/agentzero/learning/EpisodicMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * EpisodicMemory Implementation
 * Manages episodic memories with temporal sequence support
 * Part of the AGENT-ZERO-GENESIS project - Phase 7: Memory & Context
 * Task ID: AZ-MEM-001
 */

#ifndef _OPENCOG_AGENTZERO_LEARNING_EPISODIC_MEMORY_H
#define _OPENCOG_AGENTZERO_LEARNING_EPISODIC_MEMORY_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <functional>
#include <deque>
#include <mutex>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/atoms/value/Value.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore; // Forward declaration

namespace learning {

/**
 * EpisodicMemory - Manages temporal episodic memories
 *
 * This class provides comprehensive episodic memory management with
 * advanced temporal sequence support. It stores and retrieves episodic
 * memories as temporal sequences, enabling the agent to learn from
 * past episode patterns and make temporally-aware decisions.
 *
 * Key features:
 * - Temporal sequence modeling and storage
 * - Episode context and pattern recognition
 * - AtomSpace integration for persistent storage
 * - Temporal reasoning and sequence analysis
 * - Context-based episode retrieval
 * - Memory consolidation and organization
 * - Integration with attention and spacetime systems
 */
class EpisodicMemory
{
public:
    // Episode structure for temporal sequences
    struct Episode {
        std::string episode_id;
        std::string description;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        std::vector<Handle> sequence_atoms;  // Temporal sequence of atoms
        std::vector<Handle> context_atoms;   // Context atoms for this episode
        Handle episode_atom;                 // AtomSpace representation
        double importance_score;
        double temporal_coherence;           // How coherent the temporal sequence is
        
        Episode() : importance_score(0.5), temporal_coherence(0.5) {}
    };

    // Temporal sequence pattern
    struct TemporalPattern {
        std::string pattern_id;
        std::vector<Handle> pattern_sequence;
        double frequency;                    // How often this pattern occurs
        double predictive_strength;          // How well it predicts outcomes
        std::set<std::string> related_episodes;
        Handle pattern_atom;
        
        TemporalPattern() : frequency(0.0), predictive_strength(0.0) {}
    };

    // Episode context for temporal queries
    struct EpisodeContext {
        std::vector<Handle> environmental_state;
        std::vector<Handle> agent_state;
        std::vector<Handle> active_goals;
        std::chrono::system_clock::time_point query_time;
        double context_weight;
        
        EpisodeContext() : context_weight(1.0) {}
    };

    // Temporal sequence retrieval options
    struct SequenceQuery {
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        std::vector<Handle> context_filters;
        size_t max_episodes;
        bool include_patterns;
        double min_importance;
        
        SequenceQuery() : max_episodes(10), include_patterns(false), min_importance(0.0) {}
    };

private:
    // Core references
    AtomSpacePtr _atomspace;
    
    // Episode storage and organization
    std::map<std::string, Episode> _episodes;
    std::deque<std::string> _episode_order;  // Chronological order
    std::map<std::chrono::system_clock::time_point, std::vector<std::string>> _temporal_index;
    
    // Temporal pattern storage
    std::map<std::string, TemporalPattern> _temporal_patterns;
    std::map<Handle, std::set<std::string>> _atom_to_patterns;
    
    // AtomSpace handles for memory organization
    Handle _episodic_memory_base;
    Handle _temporal_sequences_node;
    Handle _episode_patterns_node;
    Handle _temporal_context_node;
    
    // Configuration
    size_t _max_episodes;
    double _consolidation_threshold;
    bool _enable_pattern_discovery;
    bool _enable_temporal_reasoning;
    std::chrono::milliseconds _temporal_resolution;
    
    // Thread safety
    mutable std::mutex _episodes_mutex;
    mutable std::mutex _patterns_mutex;
    
    // Internal methods
    void initializeEpisodicMemoryStructures();
    Handle createEpisodeAtom(const Episode& episode);
    void indexEpisode(const Episode& episode);
    void updateTemporalPatterns();
    std::vector<TemporalPattern> discoverTemporalPatterns();
    double calculateTemporalCoherence(const std::vector<Handle>& sequence);
    double calculateEpisodeSimilarity(const Episode& ep1, const Episode& ep2);
    void consolidateEpisodes();
    std::string generateEpisodeId() const;
    std::string generatePatternId() const;

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     */
    EpisodicMemory(AtomSpacePtr atomspace);
    
    /**
     * Destructor - cleans up episodic memory resources
     */
    ~EpisodicMemory();
    
    /**
     * Record a new episodic memory
     * @param description Human-readable description of the episode
     * @param sequence_atoms Temporal sequence of atoms representing the episode
     * @param context_atoms Context atoms for this episode
     * @param start_time Start time of the episode
     * @param end_time End time of the episode
     * @return Episode ID if successful, empty string if failed
     */
    std::string recordEpisode(const std::string& description,
                            const std::vector<Handle>& sequence_atoms,
                            const std::vector<Handle>& context_atoms,
                            const std::chrono::system_clock::time_point& start_time,
                            const std::chrono::system_clock::time_point& end_time);
    
    /**
     * Record an episode with automatic time stamping
     * @param description Description of the episode
     * @param sequence_atoms Temporal sequence of atoms
     * @param context_atoms Context atoms
     * @return Episode ID if successful
     */
    std::string recordEpisode(const std::string& description,
                            const std::vector<Handle>& sequence_atoms,
                            const std::vector<Handle>& context_atoms);
    
    /**
     * Retrieve temporal sequences based on query
     * @param query Query parameters for sequence retrieval
     * @return Vector of episodes matching the query
     */
    std::vector<Episode> getTemporalSequences(const SequenceQuery& query);
    
    /**
     * Get episodes similar to current context
     * @param context Current context for similarity matching
     * @param max_results Maximum number of results to return
     * @return Vector of similar episodes
     */
    std::vector<Episode> getSimilarEpisodes(const EpisodeContext& context,
                                          size_t max_results = 10);
    
    /**
     * Get temporal patterns discovered from episodes
     * @param min_frequency Minimum frequency for patterns
     * @param min_predictive_strength Minimum predictive strength
     * @return Vector of temporal patterns
     */
    std::vector<TemporalPattern> getTemporalPatterns(double min_frequency = 0.1,
                                                   double min_predictive_strength = 0.5);
    
    /**
     * Predict likely next atoms in a sequence
     * @param current_sequence Current sequence of atoms
     * @param context Current context
     * @return Vector of predicted next atoms with confidence scores
     */
    std::vector<std::pair<Handle, double>> predictNextInSequence(
        const std::vector<Handle>& current_sequence,
        const EpisodeContext& context);
    
    /**
     * Get episodes occurring within a time range
     * @param start_time Start of the time range
     * @param end_time End of the time range
     * @return Vector of episodes in chronological order
     */
    std::vector<Episode> getEpisodesInTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time);
    
    /**
     * Analyze temporal coherence of a sequence
     * @param sequence_atoms Sequence to analyze
     * @return Coherence score (0.0 to 1.0)
     */
    double analyzeTemporalCoherence(const std::vector<Handle>& sequence_atoms);
    
    /**
     * Get episode statistics
     * @return Map of statistics about stored episodes
     */
    std::map<std::string, double> getEpisodeStatistics();
    
    /**
     * Consolidate similar episodes to optimize memory
     * @return Number of episodes consolidated
     */
    size_t consolidateMemory();
    
    /**
     * Export episodes for analysis
     * @param format Export format ("json", "csv", "atomese")
     * @return Exported data as string
     */
    std::string exportEpisodes(const std::string& format = "json");
    
    /**
     * Process episodic memory (should be called periodically)
     * @return True if processing was successful
     */
    bool processEpisodicMemory();
    
    // Configuration methods
    void setMaxEpisodes(size_t max_episodes) { _max_episodes = max_episodes; }
    void setConsolidationThreshold(double threshold) { _consolidation_threshold = threshold; }
    void enablePatternDiscovery(bool enable) { _enable_pattern_discovery = enable; }
    void enableTemporalReasoning(bool enable) { _enable_temporal_reasoning = enable; }
    void setTemporalResolution(std::chrono::milliseconds resolution) { _temporal_resolution = resolution; }
    
    // Getters
    size_t getEpisodeCount() const;
    size_t getPatternCount() const;
    Handle getEpisodicMemoryBase() const { return _episodic_memory_base; }
};

// Helper function declarations (inside opencog namespace for ValuePtr visibility)
ValuePtr createFloatValue(double value);
ValuePtr createStringValue(const std::string& value);

} // namespace learning
} // namespace agentzero  
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_LEARNING_EPISODIC_MEMORY_H