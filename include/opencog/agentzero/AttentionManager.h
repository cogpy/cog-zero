/*
 * opencog/agentzero/AttentionManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ECAN-inspired attention allocation for incoming percepts.
 * Part of AGENT-ZERO-GENESIS Phase 2 (Perception & Sensory Processing).
 */
#ifndef _OPENCOG_AGENTZERO_ATTENTION_MANAGER_H
#define _OPENCOG_AGENTZERO_ATTENTION_MANAGER_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

#include "opencog/agentzero/MultiModalSensor.h"

namespace opencog {
namespace agentzero {

/**
 * Tunable ECAN-lite parameters for STI allocation and focus selection.
 */
struct AttentionConfig {
    double base_sti = 0.5;
    double decay_rate = 0.05;       // multiplicative loss per decay cycle
    double focus_boundary = 0.7;    // STI threshold for attentional focus
    double min_sti = 0.0;           // atoms below this are untracked after decay
    double max_sti = 1.0;
};

/**
 * Multi-factor salience estimate for a SensoryInput sample.
 */
struct SalienceScore {
    double signal_quality = 0.0;
    double novelty = 0.0;
    double overall = 0.0;
};

/**
 * AttentionManager — short-term importance (STI) tracking, decay, focus set,
 * and lightweight attention spreading over AtomSpace links.
 */
class AttentionManager {
public:
    explicit AttentionManager(AtomSpacePtr atomspace,
                              const AttentionConfig& config = AttentionConfig{});
    ~AttentionManager();

    const AttentionConfig& getConfig() const { return _config; }

    /**
     * Allocate STI for an atom proportional to salience ∈ [0,1].
     * Returns the resulting STI (0 if handle is undefined).
     */
    double allocateAttention(const Handle& atom, double salience);

    /** Overwrite STI for a tracked atom (clamped to [min_sti, max_sti]). */
    void updateAttention(const Handle& atom, double sti);

    double getSTI(const Handle& atom) const;
    size_t trackedAtomCount() const;

    /** Multiply all tracked STI by (1 - decay_rate); drop below min_sti. */
    void decayAttention();

    /** Atoms whose STI is at or above focus_boundary. */
    HandleSeq getAttentionFocus() const;
    bool isInAttentionFocus(const Handle& atom) const;

    /** Estimate salience of a raw sensory sample. */
    SalienceScore calculateSalience(const SensoryInput& input);

    /**
     * Spread a fraction of attention from a source link's outgoing set
     * (or the atom itself if it has no outgoing set).
     */
    void spreadAttention(const Handle& source, double amount);

    std::string getStats() const;
    void reset();

private:
    static double clamp01(double v);
    void setSTIUnlocked(const Handle& atom, double sti);

    AtomSpacePtr _atomspace;
    AttentionConfig _config;

    mutable std::mutex _mu;
    std::map<Handle, double> _sti;
    std::map<std::string, size_t> _modality_seen;

    size_t _allocations = 0;
    size_t _decay_cycles = 0;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ATTENTION_MANAGER_H
