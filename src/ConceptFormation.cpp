/*
 * opencog/agentzero/knowledge/ConceptFormation.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ConceptFormation Implementation
 * Part of Agent-Zero Knowledge Representation & Reasoning module
 * Part of the AGENT-ZERO-GENESIS project - Phase 3
 */

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <numeric>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/knowledge/ConceptFormation.h"

using namespace opencog;
using namespace opencog::agentzero::knowledge;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ConceptFormation::ConceptFormation(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("ConceptFormation requires a valid AtomSpace");
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool ConceptFormation::initialize()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_initialized) return true;
    logger().info() << "[ConceptFormation] Initializing";
    _initialized = true;
    return true;
}

bool ConceptFormation::shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);
    logger().info() << "[ConceptFormation] Shutting down";
    _initialized = false;
    return true;
}

// ---------------------------------------------------------------------------
// Exemplar Observation
// ---------------------------------------------------------------------------

bool ConceptFormation::observeExemplar(const Handle& exemplar,
                                        const std::string& label)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!exemplar) return false;
    _candidates[label].name = label;
    _candidates[label].exemplars.push_back(exemplar);
    return true;
}

bool ConceptFormation::observeCounterExemplar(const Handle& counter,
                                               const std::string& label)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!counter) return false;
    _candidates[label].name = label;
    _candidates[label].counter_exemplars.push_back(counter);
    return true;
}

size_t ConceptFormation::exemplarCount(const std::string& label) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _candidates.find(label);
    if (it == _candidates.end()) return 0;
    return it->second.exemplars.size();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

double ConceptFormation::structuralSimilarity(const Handle& a,
                                               const Handle& b) const
{
    if (!a || !b) return 0.0;
    if (a == b) return 1.0;
    if (a->get_type() != b->get_type()) return 0.25;
    if (a->is_node() && b->is_node())
        return (a->get_name() == b->get_name()) ? 1.0 : 0.5;
    // Link: partial credit for arity match
    if (a->getArity() == b->getArity()) return 0.5;
    return 0.25;
}

double ConceptFormation::jaccardSimilarity(const std::vector<Handle>& a,
                                            const std::vector<Handle>& b) const
{
    if (a.empty() && b.empty()) return 1.0;
    std::set<Handle> sa(a.begin(), a.end());
    std::set<Handle> sb(b.begin(), b.end());
    size_t intersection = 0;
    for (auto& h : sa)
        if (sb.count(h)) ++intersection;
    size_t union_size = sa.size() + sb.size() - intersection;
    return union_size == 0 ? 1.0 :
        static_cast<double>(intersection) / static_cast<double>(union_size);
}

double ConceptFormation::computeCoherence(
    const ConceptCandidate& candidate) const
{
    auto& ex = candidate.exemplars;
    if (ex.size() <= 1) return ex.empty() ? 0.0 : 1.0;

    double total = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < ex.size(); ++i) {
        for (size_t j = i + 1; j < ex.size(); ++j) {
            total += structuralSimilarity(ex[i], ex[j]);
            ++count;
        }
    }
    return count == 0 ? 1.0 : total / static_cast<double>(count);
}

double ConceptFormation::computeNovelty(
    const ConceptCandidate& candidate) const
{
    // Compare against all other promoted concepts
    double min_sim = 1.0;
    for (auto& [label, other] : _candidates) {
        if (label == candidate.name) continue;
        if (!other.promoted) continue;
        double sim = jaccardSimilarity(candidate.exemplars, other.exemplars);
        min_sim = std::min(min_sim, sim);
    }
    return 1.0 - min_sim; // high novelty = low max similarity to existing
}

bool ConceptFormation::promoteConcept(ConceptCandidate& candidate)
{
    // Create a ConceptNode
    Handle concept = _atomspace->add_node(CONCEPT_NODE, candidate.name);
    if (!concept) return false;

    // Set truth value based on coherence
    double strength = candidate.coherence;
    double confidence = std::min(0.9, static_cast<double>(candidate.exemplars.size()) / 10.0);
    concept->setTruthValue(
        SimpleTruthValue::createTV(strength, confidence));

    // Add InheritanceLinks: exemplar isa concept
    for (auto& ex : candidate.exemplars) {
        _atomspace->add_link(INHERITANCE_LINK, {ex, concept});
    }

    candidate.promoted = true;
    candidate.concept_handle = concept;
    logger().info() << "[ConceptFormation] Promoted concept: " << candidate.name
                    << " exemplars=" << candidate.exemplars.size()
                    << " coherence=" << candidate.coherence;

    RefinementRecord rec;
    rec.concept_handle = concept;
    rec.concept_name = candidate.name;
    rec.change_type = "promote";
    rec.old_strength = 0.0;
    rec.new_strength = strength;
    rec.refined_at = std::chrono::system_clock::now();
    rec.rationale = "initial formation";
    _refinement_history.push_back(std::move(rec));

    return true;
}

// ---------------------------------------------------------------------------
// Concept Formation (public)
// ---------------------------------------------------------------------------

size_t ConceptFormation::formConcepts(const ConceptFormationConfig& config)
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t count = 0;

    for (auto& [label, candidate] : _candidates) {
        if (candidate.exemplars.size() < config.min_exemplars) continue;

        candidate.coherence = computeCoherence(candidate);
        candidate.novelty = computeNovelty(candidate);
        candidate.coverage = static_cast<double>(candidate.exemplars.size()) /
                             std::max<double>(1.0, static_cast<double>(_candidates.size()));

        if (!candidate.promoted) {
            if (candidate.coherence >= config.min_coherence &&
                candidate.coverage  >= config.min_coverage &&
                candidate.novelty   >= config.novelty_threshold)
            {
                if (promoteConcept(candidate)) ++count;
            }
        } else {
            // Refinement: check for split opportunity
            if (config.auto_split &&
                candidate.coherence < config.split_incoherence &&
                candidate.exemplars.size() >= 4)
            {
                // Simple split: partition exemplars by structural type
                std::vector<Handle> group_a, group_b;
                for (size_t i = 0; i < candidate.exemplars.size(); ++i) {
                    if (i % 2 == 0) group_a.push_back(candidate.exemplars[i]);
                    else             group_b.push_back(candidate.exemplars[i]);
                }
                std::string la = label + "_a", lb = label + "_b";
                _candidates[la] = {la, group_a, {}, 0, 0, 0, false, Handle::UNDEFINED};
                _candidates[lb] = {lb, group_b, {}, 0, 0, 0, false, Handle::UNDEFINED};

                RefinementRecord rec;
                rec.concept_handle = candidate.concept_handle;
                rec.concept_name   = label;
                rec.change_type    = "split";
                rec.old_strength   = candidate.coherence;
                rec.new_strength   = 0.0;
                rec.refined_at     = std::chrono::system_clock::now();
                rec.rationale      = "low coherence split into " + la + " + " + lb;
                _refinement_history.push_back(std::move(rec));
                ++count;
            }

            // Update truth value
            double new_strength = computeCoherence(candidate);
            if (candidate.concept_handle) {
                double new_conf = std::min(0.99,
                    static_cast<double>(candidate.exemplars.size()) / 10.0);
                candidate.concept_handle->setTruthValue(
                    SimpleTruthValue::createTV(new_strength, new_conf));
            }
        }

        // Auto-merge pass
        if (config.auto_merge && candidate.promoted) {
            for (auto& [other_label, other] : _candidates) {
                if (other_label == label || !other.promoted) continue;
                double sim = jaccardSimilarity(candidate.exemplars, other.exemplars);
                if (sim >= config.merge_similarity) {
                    // Merge other into candidate (keep candidate)
                    for (auto& h : other.exemplars)
                        candidate.exemplars.push_back(h);
                    other.promoted = false;
                    other.exemplars.clear();

                    RefinementRecord rec;
                    rec.concept_handle = candidate.concept_handle;
                    rec.concept_name   = label;
                    rec.change_type    = "merge";
                    rec.old_strength   = candidate.coherence;
                    rec.new_strength   = computeCoherence(candidate);
                    rec.refined_at     = std::chrono::system_clock::now();
                    rec.rationale      = "merged with " + other_label;
                    _refinement_history.push_back(std::move(rec));
                    ++count;
                    break; // one merge at a time
                }
            }
        }
    }

    logger().info() << "[ConceptFormation] formConcepts: changes=" << count;
    return count;
}

bool ConceptFormation::formConcept(const std::string& label)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _candidates.find(label);
    if (it == _candidates.end()) return false;

    auto& candidate = it->second;
    candidate.coherence = computeCoherence(candidate);
    candidate.novelty   = computeNovelty(candidate);

    if (!candidate.promoted)
        return promoteConcept(candidate);

    // If already promoted, update truth value
    if (candidate.concept_handle) {
        double new_strength = candidate.coherence;
        double new_conf = std::min(0.99,
            static_cast<double>(candidate.exemplars.size()) / 10.0);
        candidate.concept_handle->setTruthValue(
            SimpleTruthValue::createTV(new_strength, new_conf));
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Retrieval
// ---------------------------------------------------------------------------

Handle ConceptFormation::getConceptHandle(const std::string& label) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _candidates.find(label);
    if (it == _candidates.end()) return Handle::UNDEFINED;
    return it->second.concept_handle;
}

std::vector<Handle> ConceptFormation::getAllConcepts() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Handle> result;
    for (auto& [label, cand] : _candidates)
        if (cand.promoted && cand.concept_handle)
            result.push_back(cand.concept_handle);
    return result;
}

std::vector<std::string> ConceptFormation::getCandidateLabels() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::string> labels;
    labels.reserve(_candidates.size());
    for (auto& [label, _] : _candidates)
        labels.push_back(label);
    return labels;
}

std::optional<ConceptCandidate> ConceptFormation::getCandidate(
    const std::string& label) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _candidates.find(label);
    if (it == _candidates.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------------------
// Refinement operations
// ---------------------------------------------------------------------------

Handle ConceptFormation::mergeConcepts(const std::string& label_a,
                                        const std::string& label_b,
                                        const std::string& merged_label)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it_a = _candidates.find(label_a);
    auto it_b = _candidates.find(label_b);
    if (it_a == _candidates.end() || it_b == _candidates.end())
        return Handle::UNDEFINED;

    std::string target_label = merged_label.empty() ? label_a : merged_label;

    ConceptCandidate merged;
    merged.name = target_label;
    merged.exemplars = it_a->second.exemplars;
    for (auto& h : it_b->second.exemplars)
        merged.exemplars.push_back(h);
    merged.counter_exemplars = it_a->second.counter_exemplars;
    for (auto& h : it_b->second.counter_exemplars)
        merged.counter_exemplars.push_back(h);
    merged.coherence = computeCoherence(merged);

    if (!promoteConcept(merged)) return Handle::UNDEFINED;

    _candidates[target_label] = std::move(merged);

    // Mark source candidates as retired
    it_a->second.promoted = false;
    it_b->second.promoted = false;

    return _candidates[target_label].concept_handle;
}

bool ConceptFormation::splitConcept(const std::string& label,
                                     const std::string& label_a,
                                     const std::string& label_b)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _candidates.find(label);
    if (it == _candidates.end()) return false;

    auto& ex = it->second.exemplars;
    if (ex.size() < 2) return false;

    size_t half = ex.size() / 2;
    std::vector<Handle> group_a(ex.begin(), ex.begin() + half);
    std::vector<Handle> group_b(ex.begin() + half, ex.end());

    _candidates[label_a] = {label_a, group_a, {}, 0, 0, 0, false, Handle::UNDEFINED};
    _candidates[label_b] = {label_b, group_b, {}, 0, 0, 0, false, Handle::UNDEFINED};

    it->second.promoted = false;
    return true;
}

std::vector<RefinementRecord> ConceptFormation::getRefinementHistory() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _refinement_history;
}

// ---------------------------------------------------------------------------
// Hierarchy Building
// ---------------------------------------------------------------------------

size_t ConceptFormation::buildConceptHierarchy()
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t added = 0;

    // Collect all promoted concepts
    std::vector<std::pair<std::string, const ConceptCandidate*>> promoted;
    for (auto& [label, cand] : _candidates)
        if (cand.promoted && cand.concept_handle) promoted.emplace_back(label, &cand);

    // For each pair, if A's exemplar set is a proper subset of B's, A isa B
    for (size_t i = 0; i < promoted.size(); ++i) {
        for (size_t j = 0; j < promoted.size(); ++j) {
            if (i == j) continue;
            auto& [la, ca] = promoted[i];
            auto& [lb, cb] = promoted[j];

            std::set<Handle> sa(ca->exemplars.begin(), ca->exemplars.end());
            std::set<Handle> sb(cb->exemplars.begin(), cb->exemplars.end());

            // Check if sa ⊂ sb (proper subset)
            if (sa.size() < sb.size() &&
                std::includes(sb.begin(), sb.end(), sa.begin(), sa.end()))
            {
                _atomspace->add_link(INHERITANCE_LINK,
                                     {ca->concept_handle, cb->concept_handle});
                ++added;
            }
        }
    }

    logger().info() << "[ConceptFormation] buildConceptHierarchy: added "
                    << added << " InheritanceLinks";
    return added;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::string ConceptFormation::getStatsSummary() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t promoted_count = 0;
    for (auto& [label, cand] : _candidates)
        if (cand.promoted) ++promoted_count;

    std::ostringstream ss;
    ss << "[ConceptFormation] candidates=" << _candidates.size()
       << " promoted=" << promoted_count
       << " refinements=" << _refinement_history.size()
       << " initialized=" << _initialized;
    return ss.str();
}

bool ConceptFormation::isHealthy() const
{
    return _initialized && (_atomspace != nullptr);
}
