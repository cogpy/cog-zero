/*
 * opencog/agentzero/KnowledgeIntegrator.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AtomSpace bridge for semantic search and lightweight pattern mining.
 * Part of AGENT-ZERO-GENESIS Phase 1.
 */
#ifndef _OPENCOG_AGENTZERO_KNOWLEDGE_INTEGRATOR_H
#define _OPENCOG_AGENTZERO_KNOWLEDGE_INTEGRATOR_H

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;

class KnowledgeIntegrator {
public:
    enum class KnowledgeType {
        FACTUAL,
        PROCEDURAL,
        EPISODIC,
        SEMANTIC,
        CONDITIONAL,
        TEMPORAL
    };

    enum class ConfidenceLevel {
        VERY_LOW = 0,
        LOW = 25,
        MEDIUM = 50,
        HIGH = 75,
        VERY_HIGH = 100
    };

    struct KnowledgeItem {
        std::string key;
        std::string content;
        KnowledgeType type = KnowledgeType::FACTUAL;
        double strength = 1.0;
        double confidence = 0.8;
        Handle atom = Handle::UNDEFINED;
    };

    KnowledgeIntegrator(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    ~KnowledgeIntegrator();

    Handle addKnowledge(const std::string& key, const std::string& content,
                        KnowledgeType type = KnowledgeType::FACTUAL,
                        double strength = 1.0, double confidence = 0.8);

    Handle addFact(const std::string& subject, const std::string& predicate,
                   const std::string& object, double strength = 1.0,
                   double confidence = 0.8);

    std::vector<Handle> semanticSearch(const std::string& query,
                                       size_t limit = 10) const;

    std::vector<Handle> findByType(KnowledgeType type) const;
    std::vector<Handle> minePatterns(size_t min_support = 2) const;

    bool removeKnowledge(const std::string& key);
    size_t knowledgeCount() const;

    bool processKnowledgeIntegration();
    std::string getStatusInfo() const;

    Handle getKnowledgeBaseAtom() const { return _knowledge_base; }

private:
    Handle ensureConcept(const std::string& name);
    Handle encodeItem(const KnowledgeItem& item);

    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;

    mutable std::mutex _mu;
    std::map<std::string, KnowledgeItem> _registry;
    std::set<Handle> _active;

    Handle _knowledge_base = Handle::UNDEFINED;
    Handle _semantic_network = Handle::UNDEFINED;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_KNOWLEDGE_INTEGRATOR_H
