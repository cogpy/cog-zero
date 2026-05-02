/*
 * opencog/agentzero/learning/EpisodicMemory.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * EpisodicMemory Implementation
 * Manages episodic memories with temporal sequence support
 * Part of the AGENT-ZERO-GENESIS project - Phase 7: Memory & Context
 * Task ID: AZ-MEM-001
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <random>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/atoms/value/FloatValue.h>
#include <opencog/atoms/value/StringValue.h>
#include <opencog/atoms/value/Value.h>

#include "opencog/agentzero/learning/EpisodicMemory.h"

namespace opencog {
namespace agentzero {
namespace learning {

EpisodicMemory::EpisodicMemory(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _episodic_memory_base(Handle::UNDEFINED)
    , _temporal_sequences_node(Handle::UNDEFINED)
    , _episode_patterns_node(Handle::UNDEFINED)
    , _temporal_context_node(Handle::UNDEFINED)
    , _max_episodes(1000)
    , _consolidation_threshold(0.8)
    , _enable_pattern_discovery(true)
    , _enable_temporal_reasoning(true)
    , _temporal_resolution(std::chrono::milliseconds(100))
{
    if (!_atomspace) {
        throw std::invalid_argument("EpisodicMemory requires valid AtomSpace");
    }
    
    initializeEpisodicMemoryStructures();
    logger().info("EpisodicMemory: Initialized temporal episodic memory system");
}

EpisodicMemory::~EpisodicMemory()
{
    logger().info("EpisodicMemory: Destroyed with %zu episodes and %zu patterns", 
                  _episodes.size(), _temporal_patterns.size());
}

void EpisodicMemory::initializeEpisodicMemoryStructures()
{
    // Create base structure in AtomSpace
    _episodic_memory_base = _atomspace->add_node(CONCEPT_NODE, "EpisodicMemorySystem");
    _temporal_sequences_node = _atomspace->add_node(CONCEPT_NODE, "TemporalSequences");
    _episode_patterns_node = _atomspace->add_node(CONCEPT_NODE, "EpisodePatterns");
    _temporal_context_node = _atomspace->add_node(CONCEPT_NODE, "TemporalContext");
    
    // Create structural links
    _atomspace->add_link(INHERITANCE_LINK, {_temporal_sequences_node, _episodic_memory_base});
    _atomspace->add_link(INHERITANCE_LINK, {_episode_patterns_node, _episodic_memory_base});
    _atomspace->add_link(INHERITANCE_LINK, {_temporal_context_node, _episodic_memory_base});
    
    logger().debug("EpisodicMemory: Initialized AtomSpace structures");
}

std::string EpisodicMemory::recordEpisode(const std::string& description,
                                        const std::vector<Handle>& sequence_atoms,
                                        const std::vector<Handle>& context_atoms,
                                        const std::chrono::system_clock::time_point& start_time,
                                        const std::chrono::system_clock::time_point& end_time)
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    try {
        Episode episode;
        episode.episode_id = generateEpisodeId();
        episode.description = description;
        episode.start_time = start_time;
        episode.end_time = end_time;
        episode.sequence_atoms = sequence_atoms;
        episode.context_atoms = context_atoms;
        episode.temporal_coherence = calculateTemporalCoherence(sequence_atoms);
        
        // Calculate importance based on sequence length and coherence
        episode.importance_score = (sequence_atoms.size() / 10.0) * episode.temporal_coherence;
        if (episode.importance_score > 1.0) episode.importance_score = 1.0;
        
        // Create AtomSpace representation
        episode.episode_atom = createEpisodeAtom(episode);
        
        // Store and index the episode
        _episodes[episode.episode_id] = episode;
        _episode_order.push_back(episode.episode_id);
        indexEpisode(episode);
        
        // Enforce memory limits
        while (_episodes.size() > _max_episodes) {
            auto oldest_id = _episode_order.front();
            _episode_order.pop_front();
            _episodes.erase(oldest_id);
        }
        
        // Update patterns if enabled
        if (_enable_pattern_discovery && _episodes.size() % 10 == 0) {
            updateTemporalPatterns();
        }
        
        logger().debug("EpisodicMemory: Recorded episode %s with %zu atoms", 
                      episode.episode_id.c_str(), sequence_atoms.size());
        
        return episode.episode_id;
        
    } catch (const std::exception& e) {
        logger().error("EpisodicMemory: Error recording episode: %s", e.what());
        return "";
    }
}

std::string EpisodicMemory::recordEpisode(const std::string& description,
                                        const std::vector<Handle>& sequence_atoms,
                                        const std::vector<Handle>& context_atoms)
{
    auto now = std::chrono::system_clock::now();
    return recordEpisode(description, sequence_atoms, context_atoms, now, now);
}

Handle EpisodicMemory::createEpisodeAtom(const Episode& episode)
{
    // Create episode node
    Handle episode_node = _atomspace->add_node(CONCEPT_NODE, 
        "Episode_" + episode.episode_id);
    
    // Add description
    _atomspace->set_value(episode_node, 
        _atomspace->add_node(PREDICATE_NODE, "description"),
        createStringValue(episode.description));
    
    // Add temporal information
    auto start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        episode.start_time.time_since_epoch()).count();
    auto end_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        episode.end_time.time_since_epoch()).count();
    
    _atomspace->set_value(episode_node,
        _atomspace->add_node(PREDICATE_NODE, "start_time"),
        createFloatValue(static_cast<double>(start_time_ms)));
    
    _atomspace->set_value(episode_node,
        _atomspace->add_node(PREDICATE_NODE, "end_time"),
        createFloatValue(static_cast<double>(end_time_ms)));
    
    // Add importance and coherence scores
    _atomspace->set_value(episode_node,
        _atomspace->add_node(PREDICATE_NODE, "importance"),
        createFloatValue(episode.importance_score));
    
    _atomspace->set_value(episode_node,
        _atomspace->add_node(PREDICATE_NODE, "temporal_coherence"),
        createFloatValue(episode.temporal_coherence));
    
    // Create temporal sequence link
    if (!episode.sequence_atoms.empty()) {
        HandleSeq sequence_copy = episode.sequence_atoms;
        Handle sequence_link = _atomspace->add_link(LIST_LINK, std::move(sequence_copy));
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "temporal_sequence"),
            _atomspace->add_link(LIST_LINK, {episode_node, sequence_link})
        });
    }
    
    // Link context atoms
    for (const auto& context_atom : episode.context_atoms) {
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "episode_context"),
            _atomspace->add_link(LIST_LINK, {episode_node, context_atom})
        });
    }
    
    // Link to episodic memory base
    _atomspace->add_link(MEMBER_LINK, {episode_node, _temporal_sequences_node});
    
    return episode_node;
}

void EpisodicMemory::indexEpisode(const Episode& episode)
{
    // Index by time
    _temporal_index[episode.start_time].push_back(episode.episode_id);
    
    // Index patterns by atoms
    for (const auto& atom : episode.sequence_atoms) {
        _atom_to_patterns[atom].insert(episode.episode_id);
    }
}

double EpisodicMemory::calculateTemporalCoherence(const std::vector<Handle>& sequence)
{
    if (sequence.size() < 2) return 1.0;
    
    // Simple coherence calculation based on atom type transitions
    int coherent_transitions = 0;
    int total_transitions = sequence.size() - 1;
    
    for (size_t i = 0; i < sequence.size() - 1; ++i) {
        Handle current = sequence[i];
        Handle next = sequence[i + 1];
        
        // Check if transition makes sense (placeholder logic)
        if (current != Handle::UNDEFINED && next != Handle::UNDEFINED) {
            coherent_transitions++;
        }
    }
    
    return static_cast<double>(coherent_transitions) / total_transitions;
}

std::vector<EpisodicMemory::Episode> EpisodicMemory::getTemporalSequences(const SequenceQuery& query)
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    std::vector<Episode> results;
    
    for (const auto& ep_pair : _episodes) {
        const Episode& episode = ep_pair.second;
        
        // Filter by time range
        if (episode.start_time >= query.start_time && episode.end_time <= query.end_time) {
            // Filter by importance
            if (episode.importance_score >= query.min_importance) {
                // Filter by context if specified
                bool context_matches = query.context_filters.empty();
                if (!context_matches) {
                    for (const auto& filter : query.context_filters) {
                        for (const auto& context_atom : episode.context_atoms) {
                            if (context_atom == filter) {
                                context_matches = true;
                                break;
                            }
                        }
                        if (context_matches) break;
                    }
                }
                
                if (context_matches) {
                    results.push_back(episode);
                }
            }
        }
        
        // Limit results
        if (results.size() >= query.max_episodes) {
            break;
        }
    }
    
    // Sort by start time
    std::sort(results.begin(), results.end(),
              [](const Episode& a, const Episode& b) {
                  return a.start_time < b.start_time;
              });
    
    return results;
}

std::vector<EpisodicMemory::Episode> EpisodicMemory::getSimilarEpisodes(const EpisodeContext& context,
                                                                      size_t max_results)
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    std::vector<std::pair<Episode, double>> scored_episodes;
    
    for (const auto& ep_pair : _episodes) {
        const Episode& episode = ep_pair.second;
        double similarity = 0.0;
        
        // Calculate context similarity
        int matching_contexts = 0;
        for (const auto& env_atom : context.environmental_state) {
            for (const auto& ep_context : episode.context_atoms) {
                if (env_atom == ep_context) {
                    matching_contexts++;
                }
            }
        }
        
        if (!episode.context_atoms.empty()) {
            similarity = static_cast<double>(matching_contexts) / episode.context_atoms.size();
        }
        
        // Weight by temporal proximity
        auto time_diff = std::abs(std::chrono::duration_cast<std::chrono::hours>(
            context.query_time - episode.start_time).count());
        double time_weight = 1.0 / (1.0 + time_diff / 24.0); // Decay over days
        
        similarity = (similarity * context.context_weight + time_weight * 0.2) / 1.2;
        
        scored_episodes.emplace_back(episode, similarity);
    }
    
    // Sort by similarity
    std::sort(scored_episodes.begin(), scored_episodes.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    // Extract top results
    std::vector<Episode> results;
    for (size_t i = 0; i < std::min(max_results, scored_episodes.size()); ++i) {
        results.push_back(scored_episodes[i].first);
    }
    
    return results;
}

std::vector<EpisodicMemory::TemporalPattern> EpisodicMemory::getTemporalPatterns(double min_frequency,
                                                                               double min_predictive_strength)
{
    std::lock_guard<std::mutex> lock(_patterns_mutex);
    
    std::vector<TemporalPattern> results;
    
    for (const auto& pattern_pair : _temporal_patterns) {
        const TemporalPattern& pattern = pattern_pair.second;
        
        if (pattern.frequency >= min_frequency && 
            pattern.predictive_strength >= min_predictive_strength) {
            results.push_back(pattern);
        }
    }
    
    // Sort by predictive strength
    std::sort(results.begin(), results.end(),
              [](const TemporalPattern& a, const TemporalPattern& b) {
                  return a.predictive_strength > b.predictive_strength;
              });
    
    return results;
}

void EpisodicMemory::updateTemporalPatterns()
{
    if (!_enable_pattern_discovery) return;
    
    std::lock_guard<std::mutex> lock(_patterns_mutex);
    
    // Discover new patterns
    auto new_patterns = discoverTemporalPatterns();
    
    for (const auto& pattern : new_patterns) {
        _temporal_patterns[pattern.pattern_id] = pattern;
    }
    
    logger().debug("EpisodicMemory: Updated patterns, now have %zu patterns", 
                  _temporal_patterns.size());
}

std::vector<EpisodicMemory::TemporalPattern> EpisodicMemory::discoverTemporalPatterns()
{
    std::vector<TemporalPattern> patterns;
    std::map<std::vector<Handle>, int> sequence_counts;
    
    // Find common subsequences of length 2-4
    for (const auto& ep_pair : _episodes) {
        const Episode& episode = ep_pair.second;
        
        for (size_t len = 2; len <= std::min(size_t(4), episode.sequence_atoms.size()); ++len) {
            for (size_t i = 0; i <= episode.sequence_atoms.size() - len; ++i) {
                std::vector<Handle> subseq(episode.sequence_atoms.begin() + i,
                                         episode.sequence_atoms.begin() + i + len);
                sequence_counts[subseq]++;
            }
        }
    }
    
    // Convert frequent sequences to patterns
    double total_episodes = static_cast<double>(_episodes.size());
    
    for (const auto& seq_count : sequence_counts) {
        if (seq_count.second >= 3) { // At least 3 occurrences
            TemporalPattern pattern;
            pattern.pattern_id = generatePatternId();
            pattern.pattern_sequence = seq_count.first;
            pattern.frequency = seq_count.second / total_episodes;
            pattern.predictive_strength = pattern.frequency; // Simplified calculation
            
            patterns.push_back(pattern);
        }
    }
    
    return patterns;
}

std::vector<std::pair<Handle, double>> EpisodicMemory::predictNextInSequence(
    const std::vector<Handle>& current_sequence,
    const EpisodeContext& context)
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    std::map<Handle, int> next_atom_counts;
    int total_matches = 0;
    
    // Find episodes with matching subsequences
    for (const auto& ep_pair : _episodes) {
        const Episode& episode = ep_pair.second;
        
        // Look for current_sequence within episode sequence
        if (episode.sequence_atoms.size() > current_sequence.size()) {
            for (size_t i = 0; i <= episode.sequence_atoms.size() - current_sequence.size(); ++i) {
                bool matches = true;
                for (size_t j = 0; j < current_sequence.size(); ++j) {
                    if (episode.sequence_atoms[i + j] != current_sequence[j]) {
                        matches = false;
                        break;
                    }
                }
                
                if (matches && (i + current_sequence.size() < episode.sequence_atoms.size())) {
                    Handle next_atom = episode.sequence_atoms[i + current_sequence.size()];
                    next_atom_counts[next_atom]++;
                    total_matches++;
                }
            }
        }
    }
    
    // Convert to probability scores
    std::vector<std::pair<Handle, double>> predictions;
    for (const auto& count_pair : next_atom_counts) {
        double probability = static_cast<double>(count_pair.second) / total_matches;
        predictions.emplace_back(count_pair.first, probability);
    }
    
    // Sort by probability
    std::sort(predictions.begin(), predictions.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    return predictions;
}

std::vector<EpisodicMemory::Episode> EpisodicMemory::getEpisodesInTimeRange(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time)
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    std::vector<Episode> results;
    
    for (const auto& ep_pair : _episodes) {
        const Episode& episode = ep_pair.second;
        
        if (episode.start_time >= start_time && episode.end_time <= end_time) {
            results.push_back(episode);
        }
    }
    
    // Sort chronologically
    std::sort(results.begin(), results.end(),
              [](const Episode& a, const Episode& b) {
                  return a.start_time < b.start_time;
              });
    
    return results;
}

double EpisodicMemory::analyzeTemporalCoherence(const std::vector<Handle>& sequence_atoms)
{
    return calculateTemporalCoherence(sequence_atoms);
}

std::map<std::string, double> EpisodicMemory::getEpisodeStatistics()
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    std::map<std::string, double> stats;
    stats["total_episodes"] = static_cast<double>(_episodes.size());
    stats["total_patterns"] = static_cast<double>(_temporal_patterns.size());
    
    if (!_episodes.empty()) {
        double total_importance = 0.0;
        double total_coherence = 0.0;
        size_t total_atoms = 0;
        
        for (const auto& ep_pair : _episodes) {
            const Episode& episode = ep_pair.second;
            total_importance += episode.importance_score;
            total_coherence += episode.temporal_coherence;
            total_atoms += episode.sequence_atoms.size();
        }
        
        stats["average_importance"] = total_importance / _episodes.size();
        stats["average_coherence"] = total_coherence / _episodes.size();
        stats["average_sequence_length"] = static_cast<double>(total_atoms) / _episodes.size();
    } else {
        stats["average_importance"] = 0.0;
        stats["average_coherence"] = 0.0;
        stats["average_sequence_length"] = 0.0;
    }
    
    return stats;
}

size_t EpisodicMemory::consolidateMemory()
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    size_t consolidated = 0;
    
    // Simple consolidation: merge very similar episodes
    std::vector<std::string> to_remove;
    
    for (auto it1 = _episodes.begin(); it1 != _episodes.end(); ++it1) {
        for (auto it2 = std::next(it1); it2 != _episodes.end(); ++it2) {
            double similarity = calculateEpisodeSimilarity(it1->second, it2->second);
            
            if (similarity > _consolidation_threshold) {
                // Mark lower importance episode for removal
                if (it1->second.importance_score < it2->second.importance_score) {
                    to_remove.push_back(it1->first);
                } else {
                    to_remove.push_back(it2->first);
                }
                consolidated++;
                break; // Avoid double-marking
            }
        }
    }
    
    // Remove marked episodes
    for (const auto& id : to_remove) {
        _episodes.erase(id);
        _episode_order.erase(
            std::remove(_episode_order.begin(), _episode_order.end(), id),
            _episode_order.end());
    }
    
    logger().info("EpisodicMemory: Consolidated %zu similar episodes", consolidated);
    return consolidated;
}

double EpisodicMemory::calculateEpisodeSimilarity(const Episode& ep1, const Episode& ep2)
{
    // Calculate similarity based on sequence overlap and context
    double sequence_similarity = 0.0;
    double context_similarity = 0.0;
    
    // Sequence similarity (simplified Jaccard index)
    std::set<Handle> set1(ep1.sequence_atoms.begin(), ep1.sequence_atoms.end());
    std::set<Handle> set2(ep2.sequence_atoms.begin(), ep2.sequence_atoms.end());
    
    std::set<Handle> intersection;
    std::set_intersection(set1.begin(), set1.end(),
                         set2.begin(), set2.end(),
                         std::inserter(intersection, intersection.begin()));
    
    std::set<Handle> union_set;
    std::set_union(set1.begin(), set1.end(),
                   set2.begin(), set2.end(),
                   std::inserter(union_set, union_set.begin()));
    
    if (!union_set.empty()) {
        sequence_similarity = static_cast<double>(intersection.size()) / union_set.size();
    }
    
    // Context similarity
    std::set<Handle> ctx_set1(ep1.context_atoms.begin(), ep1.context_atoms.end());
    std::set<Handle> ctx_set2(ep2.context_atoms.begin(), ep2.context_atoms.end());
    
    std::set<Handle> ctx_intersection;
    std::set_intersection(ctx_set1.begin(), ctx_set1.end(),
                         ctx_set2.begin(), ctx_set2.end(),
                         std::inserter(ctx_intersection, ctx_intersection.begin()));
    
    std::set<Handle> ctx_union;
    std::set_union(ctx_set1.begin(), ctx_set1.end(),
                   ctx_set2.begin(), ctx_set2.end(),
                   std::inserter(ctx_union, ctx_union.begin()));
    
    if (!ctx_union.empty()) {
        context_similarity = static_cast<double>(ctx_intersection.size()) / ctx_union.size();
    }
    
    // Combine similarities
    return (sequence_similarity * 0.7 + context_similarity * 0.3);
}

std::string EpisodicMemory::exportEpisodes(const std::string& format)
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    
    if (format == "json") {
        std::ostringstream oss;
        oss << "{\n  \"episodes\": [\n";
        
        bool first = true;
        for (const auto& ep_pair : _episodes) {
            const Episode& episode = ep_pair.second;
            
            if (!first) oss << ",\n";
            first = false;
            
            oss << "    {\n";
            oss << "      \"id\": \"" << episode.episode_id << "\",\n";
            oss << "      \"description\": \"" << episode.description << "\",\n";
            oss << "      \"sequence_length\": " << episode.sequence_atoms.size() << ",\n";
            oss << "      \"importance\": " << episode.importance_score << ",\n";
            oss << "      \"coherence\": " << episode.temporal_coherence << "\n";
            oss << "    }";
        }
        
        oss << "\n  ]\n}";
        return oss.str();
    }
    
    return "Unsupported export format";
}

bool EpisodicMemory::processEpisodicMemory()
{
    try {
        // Periodic pattern discovery
        if (_enable_pattern_discovery && _episodes.size() % 50 == 0) {
            updateTemporalPatterns();
        }
        
        // Memory consolidation
        if (_episodes.size() > _max_episodes * 0.9) {
            consolidateMemory();
        }
        
        return true;
    } catch (const std::exception& e) {
        logger().error("EpisodicMemory: Error in processing: %s", e.what());
        return false;
    }
}

size_t EpisodicMemory::getEpisodeCount() const
{
    std::lock_guard<std::mutex> lock(_episodes_mutex);
    return _episodes.size();
}

size_t EpisodicMemory::getPatternCount() const
{
    std::lock_guard<std::mutex> lock(_patterns_mutex);
    return _temporal_patterns.size();
}

std::string EpisodicMemory::generateEpisodeId() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return "episode_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::string EpisodicMemory::generatePatternId() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100, 999);
    
    return "pattern_" + std::to_string(dis(gen));
}

// Helper methods for AtomSpace value creation
ValuePtr createFloatValue(double value) {
    return std::make_shared<FloatValue>(std::vector<double>{value});
}

ValuePtr createStringValue(const std::string& value) {
    return std::make_shared<StringValue>(std::vector<std::string>{value});
}

} // namespace learning
} // namespace agentzero
} // namespace opencog