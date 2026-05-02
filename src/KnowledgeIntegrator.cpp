/*
 * standalone/src/KnowledgeIntegrator.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <set>
#include <sstream>

#include "cog0/KnowledgeIntegrator.h"
#include "cog0/Logger.h"

namespace cog0 {

std::string knowledgeTypeName(KnowledgeType t) {
    switch (t) {
        case KnowledgeType::FACTUAL:     return "FACTUAL";
        case KnowledgeType::PROCEDURAL:  return "PROCEDURAL";
        case KnowledgeType::EPISODIC:    return "EPISODIC";
        case KnowledgeType::SEMANTIC:    return "SEMANTIC";
        case KnowledgeType::CONDITIONAL: return "CONDITIONAL";
        case KnowledgeType::TEMPORAL:    return "TEMPORAL";
    }
    return "UNKNOWN";
}

KnowledgeIntegrator::KnowledgeIntegrator(std::shared_ptr<AtomStore> store)
    : _store(std::move(store))
{
    logger().debug("[KnowledgeIntegrator] Initialized");
}

KnowledgeIntegrator::~KnowledgeIntegrator() = default;

size_t KnowledgeIntegrator::addKnowledge(const std::string& content,
                                          KnowledgeType type,
                                          double strength,
                                          double confidence) {
    // Check for duplicates
    auto it = _contentIndex.find(content);
    if (it != _contentIndex.end()) {
        // Update confidence/strength if duplicate
        auto& e = _entries[it->second];
        e.strength   = std::max(e.strength, strength);
        e.confidence = std::max(e.confidence, confidence);
        if (e.atom) e.atom->setTV(TruthValue{e.strength, e.confidence});
        return it->second;
    }

    size_t id = _nextId++;
    KnowledgeEntry entry(id, content, type, strength, confidence);
    entry.atom = toAtom(entry);
    _entries[id]          = entry;
    _contentIndex[content] = id;

    logger().debug("[KnowledgeIntegrator] Added " + knowledgeTypeName(type) +
                   " knowledge: " + content);
    return id;
}

bool KnowledgeIntegrator::removeKnowledge(size_t id) {
    auto it = _entries.find(id);
    if (it == _entries.end()) return false;
    _contentIndex.erase(it->second.content);
    if (it->second.atom) _store->remove(it->second.atom);
    _entries.erase(it);
    return true;
}

std::vector<KnowledgeEntry> KnowledgeIntegrator::getByType(KnowledgeType type) const {
    std::vector<KnowledgeEntry> result;
    for (const auto& [id, entry] : _entries)
        if (entry.type == type) result.push_back(entry);
    return result;
}

std::vector<KnowledgeEntry> KnowledgeIntegrator::getAll() const {
    std::vector<KnowledgeEntry> result;
    result.reserve(_entries.size());
    for (const auto& [id, entry] : _entries) result.push_back(entry);
    return result;
}

bool KnowledgeIntegrator::hasFact(const std::string& content) const {
    return _contentIndex.count(content) > 0;
}

Handle KnowledgeIntegrator::formConcept(const std::string& conceptName,
                                          const HandleVec& exemplars,
                                          double strength) {
    auto conceptNode = _store->addNode(AtomType::CONCEPT, conceptName);
    conceptNode->setTV(TruthValue{strength, 0.9});
    conceptNode->setSTI(strength);

    // Create inheritance links from exemplars to the concept
    for (const auto& exemplar : exemplars) {
        if (!exemplar) continue;
        auto link = _store->addLink(AtomType::INHERITANCE, {exemplar, conceptNode});
        link->setTV(TruthValue{strength, 0.8});
    }

    logger().debug("[KnowledgeIntegrator] Formed concept: " + conceptName +
                   " with " + std::to_string(exemplars.size()) + " exemplars");
    return conceptNode;
}

Handle KnowledgeIntegrator::assertRelation(const Handle& subject,
                                             const Handle& object,
                                             AtomType linkType,
                                             double strength) {
    if (!subject || !object) return nullptr;
    auto link = _store->addLink(linkType, {subject, object});
    link->setTV(TruthValue{strength, 0.9});
    return link;
}

HandleVec KnowledgeIntegrator::findRelated(const Handle& atom,
                                             size_t maxResults) const {
    if (!atom) return {};
    HandleVec incoming = _store->getIncoming(atom);
    HandleVec result;
    for (const auto& link : incoming) {
        if (!link || link->out().size() < 2) continue;
        for (const auto& out : link->out()) {
            if (out != atom && result.size() < maxResults)
                result.push_back(out);
        }
    }
    return result;
}

size_t KnowledgeIntegrator::consolidate(double confidenceThreshold) {
    size_t removed = 0;
    std::vector<size_t> toRemove;
    for (const auto& [id, entry] : _entries) {
        if (entry.confidence < confidenceThreshold)
            toRemove.push_back(id);
    }
    for (size_t id : toRemove) {
        removeKnowledge(id);
        ++removed;
    }
    if (removed > 0)
        logger().info("[KnowledgeIntegrator] Consolidated: removed " +
                      std::to_string(removed) + " low-confidence entries");
    return removed;
}

void KnowledgeIntegrator::clear() {
    _entries.clear();
    _contentIndex.clear();
    _nextId = 1;
}

std::string KnowledgeIntegrator::statusReport() const {
    std::ostringstream oss;
    oss << "KnowledgeIntegrator: total=" << _entries.size() << "\n";
    std::map<KnowledgeType, size_t> counts;
    for (const auto& [id, e] : _entries) counts[e.type]++;
    for (const auto& [type, count] : counts)
        oss << "  " << knowledgeTypeName(type) << ": " << count << "\n";
    return oss.str();
}

Handle KnowledgeIntegrator::toAtom(const KnowledgeEntry& entry) {
    AtomType atype = AtomType::CONCEPT;
    switch (entry.type) {
        case KnowledgeType::FACTUAL:     atype = AtomType::EVALUATION; break;
        case KnowledgeType::PROCEDURAL:  atype = AtomType::EXECUTION;  break;
        case KnowledgeType::SEMANTIC:    atype = AtomType::CONCEPT;    break;
        case KnowledgeType::CONDITIONAL: atype = AtomType::IMPLICATION; break;
        case KnowledgeType::EPISODIC:    atype = AtomType::STATE;      break;
        case KnowledgeType::TEMPORAL:    atype = AtomType::STATE;      break;
    }

    // For node-type atoms, use content as name
    if (atype == AtomType::CONCEPT || atype == AtomType::EVALUATION ||
        atype == AtomType::STATE) {
        auto node = _store->addNode(atype, "Knowledge:" + entry.content);
        node->setTV(TruthValue{entry.strength, entry.confidence});
        return node;
    }

    // For others, create a named concept and wrap in the link type later
    auto node = _store->addNode(AtomType::CONCEPT, "Knowledge:" + entry.content);
    node->setTV(TruthValue{entry.strength, entry.confidence});
    return node;
}

double KnowledgeIntegrator::computeSimilarity(const Handle& a,
                                               const Handle& b) const {
    if (!a || !b) return 0.0;
    if (a == b) return 1.0;
    // Jaccard character-set similarity: |A ∩ B| / |A ∪ B|
    const std::string& sa = a->name();
    const std::string& sb = b->name();
    if (sa.empty() || sb.empty()) return 0.0;
    std::set<char> setA(sa.begin(), sa.end());
    std::set<char> setB(sb.begin(), sb.end());
    std::set<char> intersection;
    std::set<char> unionSet;
    std::set_intersection(setA.begin(), setA.end(),
                          setB.begin(), setB.end(),
                          std::inserter(intersection, intersection.begin()));
    std::set_union(setA.begin(), setA.end(),
                   setB.begin(), setB.end(),
                   std::inserter(unionSet, unionSet.begin()));
    if (unionSet.empty()) return 0.0;
    return static_cast<double>(intersection.size()) /
           static_cast<double>(unionSet.size());
}

} // namespace cog0
