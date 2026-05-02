/*
 * include/opencog/agentzero/AttentionManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AttentionManager - ECAN-based attention allocation for incoming percepts
 * Part of the AGENT-ZERO-GENESIS project - Phase 2 Perception
 */

#ifndef _OPENCOG_AGENTZERO_ATTENTION_MANAGER_H
#define _OPENCOG_AGENTZERO_ATTENTION_MANAGER_H

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

#include <opencog/atoms/base/Handle.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "PerceptualProcessor.h"

namespace opencog
{
namespace agentzero
{

/**
 * @brief Configuration for the AttentionManager
 */
struct AttentionConfig
{
    double base_sti = 0.5;          ///< Base STI assigned to new percepts
    double decay_rate = 0.05;       ///< STI decay per update cycle (0.0-1.0)
    double max_sti = 1.0;           ///< Maximum allowable STI value
    double min_sti = 0.0;           ///< Minimum allowable STI value
    double focus_boundary = 0.7;    ///< STI threshold for entering attention focus
    size_t max_focus_size = 100;    ///< Maximum atoms in the attention focus set
    double spread_factor = 0.1;     ///< Fraction of STI spread to neighbours
    double novelty_boost = 0.2;     ///< Extra STI bonus for novel percepts

    AttentionConfig() = default;
};

/**
 * @brief Salience score for a sensory input
 */
struct SalienceScore
{
    double signal_quality;   ///< Quality of the raw signal (0.0-1.0)
    double novelty;          ///< How novel/unexpected the input is (0.0-1.0)
    double relevance;        ///< Relevance to current goals (0.0-1.0)
    double overall;          ///< Combined salience score (0.0-1.0)

    SalienceScore()
        : signal_quality(0.5), novelty(0.5), relevance(0.5), overall(0.5) {}
    SalienceScore(double sq, double n, double rel, double ov)
        : signal_quality(sq), novelty(n), relevance(rel), overall(ov) {}
};

/**
 * @brief AttentionManager — ECAN-based attention allocation for incoming percepts
 *
 * Implements a simplified version of OpenCog's Economic Attention Networks (ECAN)
 * for allocating Short-Term Importance (STI) to perception atoms based on their
 * salience, novelty, and relevance.  Atoms with STI above the focus_boundary
 * enter the "attentional focus" and receive priority downstream processing.
 *
 * Key responsibilities:
 * - Assign STI to newly created perception atoms
 * - Decay STI over update cycles to model fading attention
 * - Spread attention from high-STI atoms to their neighbours
 * - Maintain a bounded attentional focus set
 * - Expose salience scoring for the PerceptualProcessor
 */
class AttentionManager
{
private:
    AtomSpacePtr _atomspace;
    AttentionConfig _config;

    // STI registry (atom handle id -> current STI)
    mutable std::mutex _sti_mutex;
    std::unordered_map<Handle, double> _sti_map;

    // Novelty tracking: previously seen sensor_type + modality combos
    mutable std::mutex _novelty_mutex;
    std::unordered_map<std::string, size_t> _seen_modalities;

    // Statistics
    std::atomic<size_t> _allocations{0};
    std::atomic<size_t> _decay_cycles{0};
    std::atomic<size_t> _focus_evictions{0};

    // Internal helpers
    double clampSTI(double sti) const;
    void evictFromFocus(std::vector<std::pair<Handle, double>>& focus_list);

public:
    /**
     * @brief Constructor
     * @param atomspace Shared pointer to the AtomSpace
     * @param config    Attention configuration parameters
     */
    AttentionManager(AtomSpacePtr atomspace,
                     AttentionConfig config = AttentionConfig{});

    /**
     * @brief Destructor
     */
    ~AttentionManager();

    // -----------------------------------------------------------------------
    // Attention allocation
    // -----------------------------------------------------------------------

    /**
     * @brief Allocate attention (STI) to a percept atom
     *
     * Computes the initial STI from the supplied salience value and the
     * configured base_sti, stores it in the internal registry and also
     * as an atom value in the AtomSpace.
     *
     * @param percept  Handle to the perception atom
     * @param salience Salience of this percept (0.0-1.0)
     * @return         The allocated STI value
     */
    double allocateAttention(const Handle& percept, double salience);

    /**
     * @brief Update stored STI for a percept (e.g. after external adjustment)
     * @param percept Handle to the perception atom
     * @param new_sti New STI value (will be clamped to [min_sti, max_sti])
     */
    void updateAttention(const Handle& percept, double new_sti);

    /**
     * @brief Decay all tracked STI values by one cycle
     *
     * Applies exponential decay:  sti_new = sti_old * (1 - decay_rate)
     * Atoms whose STI falls to min_sti are removed from tracking.
     */
    void decayAttention();

    // -----------------------------------------------------------------------
    // Attention focus
    // -----------------------------------------------------------------------

    /**
     * @brief Return handles currently in the attentional focus
     *
     * The attentional focus contains all tracked atoms with STI >= focus_boundary,
     * sorted by STI descending, and capped at max_focus_size.
     *
     * @return HandleSeq ordered by descending STI
     */
    HandleSeq getAttentionFocus() const;

    /**
     * @brief Check whether an atom is currently in the attentional focus
     * @param atom Handle to check
     * @return true if the atom's STI >= focus_boundary
     */
    bool isInAttentionFocus(const Handle& atom) const;

    /**
     * @brief Get the current STI of a tracked atom (0.0 if not tracked)
     * @param atom Handle to query
     * @return current STI value
     */
    double getSTI(const Handle& atom) const;

    // -----------------------------------------------------------------------
    // Salience scoring
    // -----------------------------------------------------------------------

    /**
     * @brief Calculate salience of a raw sensory input
     *
     * Considers: signal confidence (data quality), novelty of the
     * modality combination, and a uniform relevance baseline.
     *
     * @param input Sensory input to score
     * @return Decomposed SalienceScore
     */
    SalienceScore calculateSalience(const SensoryInput& input);

    // -----------------------------------------------------------------------
    // Attention spreading
    // -----------------------------------------------------------------------

    /**
     * @brief Spread a fraction of an atom's STI to its outgoing neighbours
     *
     * Implements one step of ECAN "Importance Diffusion": the source atom
     * loses spread_factor * sti, which is divided equally among its
     * outgoing set atoms (capped at max_sti).
     *
     * @param source Handle of the atom from which attention spreads
     * @param amount Amount of STI to spread (0.0 uses spread_factor * current STI)
     */
    void spreadAttention(const Handle& source, double amount = 0.0);

    // -----------------------------------------------------------------------
    // Diagnostics
    // -----------------------------------------------------------------------

    /**
     * @brief Return JSON-formatted processing statistics
     */
    std::string getStats() const;

    /**
     * @brief Reset all statistics and clear the STI registry
     */
    void reset();

    /**
     * @brief Return number of atoms currently being tracked
     */
    size_t trackedAtomCount() const;

    /**
     * @brief Get the current configuration
     */
    const AttentionConfig& getConfig() const { return _config; }

    /**
     * @brief Update configuration (takes effect on next operation)
     */
    void setConfig(const AttentionConfig& config) { _config = config; }
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ATTENTION_MANAGER_H
