/*
 * opencog/agentzero/knowledge/ConceptFormation.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ConceptFormation - Automatic concept creation and refinement.
 * Part of the AGENT-ZERO-GENESIS project - Phase 3 Knowledge Module
 */

#ifndef _OPENCOG_AGENTZERO_KNOWLEDGE_CONCEPT_FORMATION_H
#define _OPENCOG_AGENTZERO_KNOWLEDGE_CONCEPT_FORMATION_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <optional>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace knowledge {

/**
 * ConceptCandidate - A concept candidate being evaluated for promotion
 */
struct ConceptCandidate {
    std::string name;
    std::vector<Handle> exemplars;     ///< Positive examples
    std::vector<Handle> counter_exemplars; ///< Negative examples
    double coherence{0.0};             ///< Internal coherence score [0,1]
    double coverage{0.0};              ///< Fraction of domain exemplars covered
    double novelty{0.0};               ///< Dissimilarity from existing concepts
    bool promoted{false};              ///< True once added to the AtomSpace
    Handle concept_handle;             ///< Set after promotion
};

/**
 * RefinementRecord - History of a concept's evolution
 */
struct RefinementRecord {
    Handle concept_handle;
    std::string concept_name;
    std::string change_type;           ///< "merge", "split", "extend", "restrict"
    double old_strength{0.0};
    double new_strength{0.0};
    std::chrono::system_clock::time_point refined_at;
    std::string rationale;
};

/**
 * ConceptFormationConfig - Tuning parameters for the concept formation loop
 */
struct ConceptFormationConfig {
    double min_coherence{0.5};         ///< Minimum coherence to promote a concept
    double min_coverage{0.1};          ///< Minimum exemplar coverage fraction
    double novelty_threshold{0.3};     ///< Must be this different from existing concepts
    size_t min_exemplars{2};           ///< Minimum positive examples before formation
    size_t max_concepts{500};          ///< Upper bound on stored concepts
    bool auto_merge{true};             ///< Automatically merge highly similar concepts
    double merge_similarity{0.85};     ///< Similarity threshold for auto-merge
    bool auto_split{true};             ///< Automatically split bimodal concepts
    double split_incoherence{0.3};     ///< Coherence below which a concept is split
};

/**
 * ConceptFormation - Automatic concept creation and refinement
 *
 * This class implements a simplified version of the COBWEB / CLASSIT paradigm
 * adapted to an AtomSpace-based representation.  It:
 * - Observes exemplar atoms fed in from perception or episode replay
 * - Groups similar exemplars into concept candidates
 * - Promotes candidates that pass coherence/coverage/novelty thresholds
 * - Represents promoted concepts as ConceptNodes + InheritanceLinks in
 *   the AtomSpace
 * - Refines existing concepts by merge/split/extend/restrict operations
 *   as new evidence arrives
 *
 * Typical usage:
 * @code
 *   ConceptFormation cf(atomspace);
 *   cf.initialize();
 *   cf.observeExemplar(my_handle, "cat");
 *   cf.observeExemplar(another_handle, "cat");
 *   cf.formConcepts();
 *   auto cats = cf.getConceptHandle("cat");
 * @endcode
 */
class ConceptFormation
{
public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the backing AtomSpace
     */
    explicit ConceptFormation(AtomSpacePtr atomspace);

    /**
     * Destructor
     */
    ~ConceptFormation() = default;

    // =========================================================
    // Lifecycle
    // =========================================================

    /**
     * Initialize and verify AtomSpace connectivity.
     * Must be called once before any other method.
     */
    bool initialize();

    bool shutdown();

    bool isInitialized() const { return _initialized; }

    // =========================================================
    // Exemplar Observation
    // =========================================================

    /**
     * Register an atom as a positive exemplar for the given concept label.
     *
     * If the concept label does not yet exist as a candidate it is created.
     *
     * @param exemplar Handle to the exemplar atom
     * @param label    Human-readable concept label (e.g. "cat", "fast-action")
     * @return True if the exemplar was accepted
     */
    bool observeExemplar(const Handle& exemplar, const std::string& label);

    /**
     * Register an atom as a negative exemplar (counter-example) for the
     * given concept label.
     *
     * @param counter Handle to the counter-exemplar atom
     * @param label   Concept label to refine against
     * @return True if the counter-exemplar was accepted
     */
    bool observeCounterExemplar(const Handle& counter, const std::string& label);

    /**
     * Return the current number of positive exemplars for a concept label.
     */
    size_t exemplarCount(const std::string& label) const;

    // =========================================================
    // Concept Formation
    // =========================================================

    /**
     * Trigger a full concept-formation pass over all candidates.
     *
     * For each candidate with enough exemplars:
     *  1. Compute coherence, coverage, novelty.
     *  2. If thresholds pass and the concept is not yet promoted,
     *     create a ConceptNode + InheritanceLinks in the AtomSpace.
     *  3. If already promoted, check for refinement opportunities.
     *
     * @param config  Optional configuration override
     * @return Number of concepts newly promoted or refined
     */
    size_t formConcepts(const ConceptFormationConfig& config = ConceptFormationConfig{});

    /**
     * Force formation/update for a single concept label.
     *
     * @param label  Concept label to process
     * @return True if the concept was promoted or refined
     */
    bool formConcept(const std::string& label);

    // =========================================================
    // Concept Retrieval
    // =========================================================

    /**
     * Return the AtomSpace Handle for a promoted concept.
     * Returns Handle::UNDEFINED if the concept has not been promoted.
     */
    Handle getConceptHandle(const std::string& label) const;

    /**
     * Return all promoted concept handles in the AtomSpace.
     */
    std::vector<Handle> getAllConcepts() const;

    /**
     * Return all candidate names (promoted or not).
     */
    std::vector<std::string> getCandidateLabels() const;

    /**
     * Return the candidate record for a given label.
     */
    std::optional<ConceptCandidate> getCandidate(const std::string& label) const;

    // =========================================================
    // Refinement
    // =========================================================

    /**
     * Merge two concepts whose exemplar sets are highly similar.
     *
     * @param label_a  First concept label
     * @param label_b  Second concept label
     * @param merged_label  Name of the merged concept (defaults to label_a)
     * @return Handle of the merged concept, or Handle::UNDEFINED on failure
     */
    Handle mergeConcepts(const std::string& label_a,
                         const std::string& label_b,
                         const std::string& merged_label = "");

    /**
     * Split a concept whose exemplar set appears bimodal.
     *
     * @param label  Concept label to split
     * @param label_a  Name of the first sub-concept
     * @param label_b  Name of the second sub-concept
     * @return True if the split succeeded
     */
    bool splitConcept(const std::string& label,
                      const std::string& label_a,
                      const std::string& label_b);

    /**
     * Return the refinement history for all concepts.
     */
    std::vector<RefinementRecord> getRefinementHistory() const;

    // =========================================================
    // Hierarchy Building
    // =========================================================

    /**
     * Compute a taxonomy / subsumption hierarchy among all promoted
     * concepts based on exemplar subset relationships and add
     * InheritanceLinks to the AtomSpace.
     *
     * @return Number of InheritanceLinks added
     */
    size_t buildConceptHierarchy();

    // =========================================================
    // Statistics and Monitoring
    // =========================================================

    /**
     * Return a human-readable statistics summary.
     */
    std::string getStatsSummary() const;

    bool isHealthy() const;

private:
    // -------------------------
    // Internal helpers
    // -------------------------

    /**
     * Promote a candidate to a ConceptNode + InheritanceLinks.
     */
    bool promoteConcept(ConceptCandidate& candidate);

    /**
     * Compute coherence as average pairwise structural similarity of exemplars.
     * Returns a value in [0, 1].
     */
    double computeCoherence(const ConceptCandidate& candidate) const;

    /**
     * Compute novelty relative to all existing promoted concepts.
     * Returns a value in [0, 1] (higher = more novel).
     */
    double computeNovelty(const ConceptCandidate& candidate) const;

    /**
     * Structural similarity between two handles:
     * 1.0  → identical type and name
     * 0.5  → same type, different name
     * 0.25 → different type
     */
    double structuralSimilarity(const Handle& a, const Handle& b) const;

    /**
     * Compute Jaccard similarity between two sets of Handle-vectors.
     */
    double jaccardSimilarity(const std::vector<Handle>& a,
                             const std::vector<Handle>& b) const;

    // -------------------------
    // State
    // -------------------------

    AtomSpacePtr _atomspace;
    bool _initialized{false};
    mutable std::mutex _mutex;

    std::map<std::string, ConceptCandidate> _candidates;
    std::vector<RefinementRecord> _refinement_history;
};

} // namespace knowledge
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_KNOWLEDGE_CONCEPT_FORMATION_H
