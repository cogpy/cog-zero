/*
 * opencog/agentzero/knowledge/PatternDiscovery.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PatternDiscovery - Unsupervised pattern mining over episode history.
 * Part of the AGENT-ZERO-GENESIS project - Phase 3 Knowledge Module
 */

#ifndef _OPENCOG_AGENTZERO_KNOWLEDGE_PATTERN_DISCOVERY_H
#define _OPENCOG_AGENTZERO_KNOWLEDGE_PATTERN_DISCOVERY_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>
#include <chrono>
#include <optional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_MINER
#include <opencog/miner/MinerUtils.h>
#include <opencog/miner/Surprisingness.h>
#endif

namespace opencog {
namespace agentzero {
namespace knowledge {

/**
 * DiscoveredPattern - A frequent or surprising sub-structure mined from
 * the episode history.
 */
struct DiscoveredPattern {
    Handle pattern;           ///< The abstract pattern atom (e.g. a LambdaLink)
    double frequency{0.0};    ///< Empirical frequency in [0,1]
    double surprisingness{0.0}; ///< I-Surprisingness score
    size_t support{0};        ///< Number of episodes that instantiate the pattern
    std::vector<Handle> instances; ///< Concrete instances found
    std::chrono::system_clock::time_point discovered_at;
    std::string description;
};

/**
 * MiningConfig - Configuration for the pattern mining process
 */
struct MiningConfig {
    size_t min_support{2};         ///< Minimum episode count for a pattern to qualify
    size_t max_pattern_size{5};    ///< Maximum number of atoms in a pattern
    size_t max_iterations{1000};   ///< Hard cap on candidate patterns evaluated
    double min_surprisingness{0.0}; ///< Only keep patterns above this I-Surprisingness
    bool mine_conjuncts{true};      ///< Mine conjunction patterns
    bool mine_abstractions{true};   ///< Mine abstract (variable-containing) patterns
    size_t max_results{100};        ///< Maximum number of patterns to return
};

/**
 * EpisodeRecord - A single recorded experience/episode stored for mining
 */
struct EpisodeRecord {
    Handle root;               ///< Root atom of the episode
    std::string episode_id;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;
};

/**
 * PatternDiscovery - Unsupervised pattern mining over episode history
 *
 * This class mines frequently recurring structural patterns from a
 * collection of experience episodes stored in the AtomSpace.  It
 * integrates with OpenCog's Miner when available, and falls back to
 * a built-in frequency-based mining algorithm otherwise.
 *
 * Typical usage:
 * @code
 *   PatternDiscovery pd(atomspace);
 *   pd.initialize();
 *   pd.recordEpisode(my_root_handle, "ep-1");
 *   // … record more episodes …
 *   auto patterns = pd.minePatterns();
 *   for (auto& p : patterns)
 *       std::cout << p.description << "  freq=" << p.frequency << "\n";
 * @endcode
 */
class PatternDiscovery
{
public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the backing AtomSpace
     */
    explicit PatternDiscovery(AtomSpacePtr atomspace);

    /**
     * Destructor
     */
    ~PatternDiscovery() = default;

    // =========================================================
    // Lifecycle
    // =========================================================

    /**
     * Initialize and verify AtomSpace connectivity.
     * Must be called once before any other method.
     */
    bool initialize();

    /**
     * Gracefully shut down.
     */
    bool shutdown();

    bool isInitialized() const { return _initialized; }

    // =========================================================
    // Episode Management
    // =========================================================

    /**
     * Record a new episode root in the history corpus.
     *
     * @param root       Root atom of the episode (e.g. an AndLink grouping
     *                   percepts + actions + outcomes)
     * @param episode_id Unique identifier string for this episode
     * @param metadata   Optional key-value metadata (e.g. reward, context)
     * @return True if the episode was successfully added
     */
    bool recordEpisode(const Handle& root,
                       const std::string& episode_id,
                       const std::map<std::string, std::string>& metadata = {});

    /**
     * Remove an episode by its identifier.
     * @return True if found and removed
     */
    bool removeEpisode(const std::string& episode_id);

    /**
     * Return the number of recorded episodes.
     */
    size_t episodeCount() const;

    /**
     * Return all recorded episode records.
     */
    std::vector<EpisodeRecord> getEpisodes() const;

    /**
     * Clear all episode history.
     */
    void clearEpisodes();

    // =========================================================
    // Pattern Mining
    // =========================================================

    /**
     * Mine patterns from the current episode corpus.
     *
     * The algorithm:
     * 1. Extract all sub-atoms from each episode root.
     * 2. Count how many episodes each sub-atom appears in (support).
     * 3. Apply abstraction: replace concrete node names with variables
     *    to form abstract patterns.
     * 4. Compute surprisingness for abstract patterns.
     * 5. Filter by min_support and min_surprisingness.
     * 6. Return top-K results sorted by surprisingness × frequency.
     *
     * @param config Mining configuration (uses defaults if not provided)
     * @return Vector of discovered patterns, sorted by quality
     */
    std::vector<DiscoveredPattern> minePatterns(
        const MiningConfig& config = MiningConfig{});

    /**
     * Mine patterns but restrict the search to episodes recorded after
     * the given time-point.
     */
    std::vector<DiscoveredPattern> mineRecentPatterns(
        std::chrono::system_clock::time_point since,
        const MiningConfig& config = MiningConfig{});

    /**
     * Incrementally update patterns given a single new episode.
     * More efficient than re-mining the full corpus when streaming.
     *
     * @param root New episode root handle
     * @return Updated list of patterns whose support changed
     */
    std::vector<DiscoveredPattern> updatePatterns(const Handle& root);

    // =========================================================
    // Result Access
    // =========================================================

    /**
     * Return the most recently mined patterns.
     */
    std::vector<DiscoveredPattern> getCachedPatterns() const;

    /**
     * Find patterns that are instantiated by a specific atom.
     */
    std::vector<DiscoveredPattern> findMatchingPatterns(
        const Handle& atom) const;

    /**
     * Return patterns sorted by frequency (descending).
     */
    std::vector<DiscoveredPattern> getPatternsByFrequency(
        size_t top_k = 10) const;

    /**
     * Return patterns sorted by surprisingness (descending).
     */
    std::vector<DiscoveredPattern> getPatternsBySurprisingness(
        size_t top_k = 10) const;

    // =========================================================
    // Statistics
    // =========================================================

    /**
     * Return a human-readable summary of mining statistics.
     */
    std::string getStatsSummary() const;

    bool isHealthy() const;

private:
    // -------------------------
    // Internal helpers
    // -------------------------

    /**
     * Recursively collect all sub-handles of `root` up to `max_depth`.
     */
    void collectSubHandles(const Handle& root,
                           std::set<Handle>& out,
                           size_t current_depth,
                           size_t max_depth) const;

    /**
     * Build the support map: atom → set of episode indices that contain it.
     */
    std::map<Handle, std::set<size_t>> buildSupportMap(
        const std::vector<EpisodeRecord>& episodes,
        size_t max_depth) const;

    /**
     * Mine patterns from an explicit episode corpus (does not mutate _episodes).
     * Caller must hold _mutex if updating _cached_patterns is desired via
     * the update_cache flag.
     */
    std::vector<DiscoveredPattern> minePatternsFrom(
        const std::vector<EpisodeRecord>& episodes,
        const MiningConfig& config,
        bool update_cache) const;

    /**
     * Generate abstract patterns by replacing leaf node names in `templ`
     * with variable nodes, up to `max_vars` substitutions.
     */
    std::vector<Handle> abstractify(const Handle& templ,
                                    size_t max_vars = 2) const;

    /**
     * Compute a simple I-Surprisingness proxy:
     *   surprisingness = freq(pattern) / product(freq(sub-patterns))
     * clamped to [0, 1].
     */
    double computeSurprisingness(
        const Handle& pattern,
        double pattern_freq,
        const std::map<Handle, double>& freq_map) const;

    /**
     * Build a human-readable description of the pattern.
     */
    std::string describePattern(const DiscoveredPattern& p) const;

    // -------------------------
    // State
    // -------------------------

    AtomSpacePtr _atomspace;
    bool _initialized{false};
    mutable std::mutex _mutex;

    std::vector<EpisodeRecord> _episodes;
    mutable std::vector<DiscoveredPattern> _cached_patterns;
};

} // namespace knowledge
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_KNOWLEDGE_PATTERN_DISCOVERY_H
