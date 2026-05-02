/*
 * standalone/src/AttentionManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "cog0/AttentionManager.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace cog0 {

AttentionManager::AttentionManager(std::shared_ptr<AtomStore> store)
    : _store(std::move(store))
{
}

// private helper — collect every atom from the store across all known types
std::vector<Handle> AttentionManager::getAllAtoms() const
{
    static const AtomType kTypes[] = {
        AtomType::CONCEPT, AtomType::PREDICATE, AtomType::VARIABLE,
        AtomType::LIST, AtomType::EVALUATION, AtomType::INHERITANCE,
        AtomType::SIMILARITY, AtomType::IMPLICATION,
        AtomType::EXECUTION, AtomType::STATE, AtomType::CUSTOM
    };
    std::vector<Handle> all;
    for (auto t : kTypes) {
        auto atoms = _store->getByType(t);
        all.insert(all.end(), atoms.begin(), atoms.end());
    }
    return all;
}

void AttentionManager::stimulate(const Handle& h, double amount)
{
    if (!h) return;
    h->setSTI(h->sti() + amount);
}

void AttentionManager::decay(double factor)
{
    for (auto& h : getAllAtoms())
        h->setSTI(h->sti() * factor);
}

std::vector<Handle> AttentionManager::topN(size_t n) const
{
    auto all = getAllAtoms();

    if (n >= all.size()) {
        std::sort(all.begin(), all.end(),
                  [](const Handle& a, const Handle& b) {
                      return a->sti() > b->sti();
                  });
        return all;
    }

    std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(n),
                      all.end(),
                      [](const Handle& a, const Handle& b) {
                          return a->sti() > b->sti();
                      });
    all.resize(n);
    return all;
}

std::vector<Handle> AttentionManager::aboveThreshold(double threshold) const
{
    std::vector<Handle> result;
    for (auto& h : getAllAtoms())
        if (h->sti() > threshold)
            result.push_back(h);
    return result;
}

void AttentionManager::normalize(double targetMean)
{
    auto all = getAllAtoms();
    if (all.empty()) return;

    double sum = 0.0;
    for (auto& h : all) sum += h->sti();
    double mean = sum / static_cast<double>(all.size());

    if (std::fabs(mean) < 1e-9) return; // avoid divide-by-zero

    double scale = targetMean / mean;
    for (auto& h : all)
        h->setSTI(h->sti() * scale);
}

std::vector<Handle> AttentionManager::focusSet(size_t maxSize) const
{
    return topN(maxSize);
}

void AttentionManager::clamp(double minVal, double maxVal)
{
    for (auto& h : getAllAtoms()) {
        double v = h->sti();
        if (v < minVal)      h->setSTI(minVal);
        else if (v > maxVal) h->setSTI(maxVal);
    }
}

} // namespace cog0
