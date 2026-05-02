/*
 * standalone/include/cog0/AttentionManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AttentionManager — ECAN-inspired attention allocation for AtomStore atoms.
 * Manages Short-Term Importance (STI) to focus cognitive resources on the
 * most relevant atoms in the current context.
 * Part of Phase 2: Perception & Sensory Processing.
 */

#pragma once

#include <memory>
#include <vector>

#include "AtomStore.h"

namespace cog0 {

// -----------------------------------------------------------------------
// AttentionManager

class AttentionManager {
public:
    explicit AttentionManager(std::shared_ptr<AtomStore> store);

    // Increase the STI of an atom (stimulate it into the focus set).
    void stimulate(const Handle& h, double amount = 0.1);

    // Apply a multiplicative decay to all atoms' STI values.
    // factor should be in (0, 1); e.g. 0.95 decays by 5 % each tick.
    void decay(double factor = 0.95);

    // Retrieve up to n atoms with the highest STI values.
    std::vector<Handle> topN(size_t n) const;

    // Retrieve all atoms whose STI is above the given threshold.
    std::vector<Handle> aboveThreshold(double threshold) const;

    // Normalize all STI values so their mean equals target_mean.
    // This prevents unbounded growth and keeps values in a stable range.
    void normalize(double targetMean = 0.5);

    // Focus set — the top maxSize atoms by STI.
    std::vector<Handle> focusSet(size_t maxSize = 20) const;

    // Clamp all STI values to [minVal, maxVal].
    void clamp(double minVal = 0.0, double maxVal = 1.0);

private:
    std::shared_ptr<AtomStore> _store;

    // Return all atoms currently in the store across all known AtomTypes.
    std::vector<Handle> getAllAtoms() const;
};

} // namespace cog0
