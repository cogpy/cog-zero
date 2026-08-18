/*
 * opencog/agentzero/memory/ContextManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ContextManager - Context-aware atom retrieval and relevance scoring
 * Part of Agent-Zero Memory & Context Management module (Phase 7)
 * Part of the AGENT-ZERO-GENESIS project - AZ-CONTEXT-001
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_CONTEXT_MANAGER_H
#define _OPENCOG_AGENTZERO_MEMORY_CONTEXT_MANAGER_H

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

#include <opencog/agentzero/memory/MemoryTypes.h>

namespace opencog {
namespace agentzero {
namespace memory {

/**
 * A managed context with associated atoms and metadata.
 */
struct ContextEntry {
    std::string context_id;
    ContextType context_type;
    std::set<Handle> active_atoms;
    std::map<std::string, std::string> metadata;
    double importance;
    size_t access_count;
    TimePoint created_time;
    TimePoint last_accessed_time;

    ContextEntry(const std::string& id = "",
                 ContextType type = ContextType::COGNITIVE)
        : context_id(id)
        , context_type(type)
        , importance(0.5)
        , access_count(0)
        , created_time(std::chrono::system_clock::now())
        , last_accessed_time(std::chrono::system_clock::now())
    {}
};

/**
 * Aggregate statistics for the context manager.
 */
struct ContextStatistics {
    size_t total_contexts;
    size_t active_contexts;
    size_t total_context_switches;
    size_t total_atoms_in_contexts;
    double average_context_importance;

    ContextStatistics()
        : total_contexts(0)
        , active_contexts(0)
        , total_context_switches(0)
        , total_atoms_in_contexts(0)
        , average_context_importance(0.0)
    {}
};

/**
 * ContextManager — organises atoms into typed contexts and scores
 * relevance of atoms relative to an active or queried context.
 */
class ContextManager
{
public:
    explicit ContextManager(
        AtomSpacePtr atomspace,
        size_t max_contexts = 1000,
        double min_importance = 0.05,
        Duration decay_time = std::chrono::hours(24));
    ~ContextManager();

    bool initialize();
    bool shutdown();

    // Context lifecycle
    bool createContext(
        const std::string& context_id,
        ContextType context_type = ContextType::COGNITIVE,
        const std::map<std::string, std::string>& metadata = {},
        double initial_importance = 0.5);
    bool deleteContext(const std::string& context_id);
    bool hasContext(const std::string& context_id) const;
    std::shared_ptr<ContextEntry> getContext(const std::string& context_id) const;

    // Active context
    bool setActiveContext(const std::string& context_id);
    std::string getActiveContext() const;
    bool clearActiveContext();

    // Atom membership
    bool addAtomToContext(const std::string& context_id, const Handle& atom);
    bool removeAtomFromContext(const std::string& context_id, const Handle& atom);
    std::set<Handle> getAtomsInContext(const std::string& context_id) const;
    std::set<std::string> getContextsForAtom(const Handle& atom) const;
    bool clearContextAtoms(const std::string& context_id);

    // Listing / filtering
    std::vector<std::string> getAllContexts() const;
    std::vector<std::string> getContextsByType(ContextType context_type) const;
    std::vector<std::string> getContextsByImportance(double min_importance) const;
    std::vector<std::string> getMostImportantContexts(size_t count = 10) const;
    std::vector<std::string> getContextHistory(size_t count = 10) const;

    // Metadata & importance
    bool setContextMetadata(const std::string& context_id,
                            const std::string& key,
                            const std::string& value);
    std::string getContextMetadata(const std::string& context_id,
                                   const std::string& key) const;
    std::map<std::string, std::string>
        getAllContextMetadata(const std::string& context_id) const;
    bool setContextImportance(const std::string& context_id, double importance);
    double getContextImportance(const std::string& context_id) const;
    bool boostContextImportance(const std::string& context_id,
                                double boost_factor = 1.1);
    size_t decayContextImportances();

    // Context-aware retrieval & relevance scoring
    double scoreAtomRelevance(const std::string& context_id,
                              const Handle& atom) const;
    std::vector<std::pair<Handle, double>> retrieveRelevantAtoms(
        const std::string& context_id,
        size_t max_results = 20) const;
    std::vector<std::pair<Handle, double>> retrieveRelevantAtoms(
        const std::string& context_id,
        const std::vector<Handle>& candidates,
        size_t max_results = 20) const;
    std::vector<std::string> findRelevantContexts(const Handle& atom,
                                                  size_t max_results = 10) const;

    // Bulk / utility
    ContextStatistics getStatistics() const;
    size_t getContextCount() const;
    size_t getTotalAtomCount() const;
    std::map<std::string, std::string>
        getContextInfo(const std::string& context_id) const;
    bool clearAllContexts();
    bool mergeContexts(const std::string& source_id,
                       const std::string& target_id,
                       bool delete_source = true);
    Handle createContextSnapshot();

private:
    AtomSpacePtr _atomspace;
    std::map<std::string, std::shared_ptr<ContextEntry>> _contexts;
    std::map<Handle, std::set<std::string>> _atom_to_contexts;
    std::map<ContextType, std::set<std::string>> _type_index;
    std::multimap<double, std::string> _importance_index;
    std::deque<std::string> _context_history;

    std::string _active_context_id;
    size_t _max_history_size;
    size_t _context_switch_count;
    size_t _max_contexts;
    double _min_context_importance;
    Duration _context_decay_time;
    ContextStatistics _statistics;

    mutable std::recursive_mutex _context_mutex;

    void updateContextImportance(const std::string& context_id);
    void addToIndices(const std::string& context_id,
                      std::shared_ptr<ContextEntry> entry);
    void removeFromIndices(const std::string& context_id);
    void rebuildImportanceIndex();
    void cleanupLowImportanceContexts();
    void recordContextAccess(const std::string& context_id);
    std::string generateContextKey(ContextType type,
                                   const std::string& identifier);
    void updateStatistics();
};

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_CONTEXT_MANAGER_H
