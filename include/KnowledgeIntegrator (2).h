/*
 * standalone/include/cog0/KnowledgeIntegrator.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Knowledge management and integration for the standalone cog0 agent.
 * Adapted from agentzero-core/KnowledgeIntegrator without OpenCog dependencies.
 */
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "AtomStore.h"

namespace cog0 {

// -----------------------------------------------------------------------
// KnowledgeType — categories of knowledge

enum class KnowledgeType {
    FACTUAL,      // Facts about the world
    PROCEDURAL,   // How to perform actions
    EPISODIC,     // Experience memories
    SEMANTIC,     // Concept relationships
    CONDITIONAL,  // If-then rules
    TEMPORAL      // Time-based knowledge
};

std::string knowledgeTypeName(KnowledgeType t);

// -----------------------------------------------------------------------
// KnowledgeEntry — a single piece of integrated knowledge

struct KnowledgeEntry {
    size_t      id       = 0;
    std::string content;
    KnowledgeType type   = KnowledgeType::FACTUAL;
    double      strength = 1.0;   // truth value strength
    double      confidence = 1.0; // truth value confidence
    Handle      atom;             // representation in AtomStore

    KnowledgeEntry() = default;
    KnowledgeEntry(size_t i, std::string c, KnowledgeType t,
                   double s = 1.0, double cf = 1.0)
        : id(i), content(std::move(c)), type(t), strength(s), confidence(cf) {}
};

// -----------------------------------------------------------------------
// KnowledgeIntegrator — unified knowledge management

class KnowledgeIntegrator {
public:
    explicit KnowledgeIntegrator(std::shared_ptr<AtomStore> store);
    ~KnowledgeIntegrator();

    // Add a piece of knowledge; returns the entry id
    size_t addKnowledge(const std::string& content,
                        KnowledgeType type = KnowledgeType::FACTUAL,
                        double strength = 1.0,
                        double confidence = 1.0);

    // Remove knowledge by id
    bool removeKnowledge(size_t id);

    // Retrieve knowledge by type
    std::vector<KnowledgeEntry> getByType(KnowledgeType type) const;

    // Retrieve all knowledge
    std::vector<KnowledgeEntry> getAll() const;

    // Query: does a fact with this content exist?
    bool hasFact(const std::string& content) const;

    // Form a concept (ConceptNode) from a list of exemplar atoms
    Handle formConcept(const std::string& conceptName,
                       const HandleVec& exemplars,
                       double strength = 0.8);

    // Integrate concept relations (Inheritance/Similarity links)
    Handle assertRelation(const Handle& subject,
                          const Handle& object,
                          AtomType linkType,
                          double strength = 1.0);

    // Semantic query: find concepts related to an atom
    HandleVec findRelated(const Handle& atom, size_t maxResults = 10) const;

    // Consolidate memory: remove low-confidence entries below threshold
    size_t consolidate(double confidenceThreshold = 0.3);

    // Clear all integrated knowledge (keeps AtomStore intact)
    void clear();

    // Stats
    size_t size() const { return _entries.size(); }
    std::string statusReport() const;

private:
    Handle toAtom(const KnowledgeEntry& entry);
    double computeSimilarity(const Handle& a, const Handle& b) const;

    std::shared_ptr<AtomStore> _store;
    std::map<size_t, KnowledgeEntry> _entries;
    std::map<std::string, size_t>    _contentIndex; // content → id
    size_t _nextId = 1;
};

} // namespace cog0
