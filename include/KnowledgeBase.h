/*
 * opencog/agentzero/knowledge/KnowledgeBase.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * KnowledgeBase - Extended AtomSpace operations with bulk load and
 * SPARQL-like query support.
 * Part of the AGENT-ZERO-GENESIS project - Phase 3 Knowledge Module
 */

#ifndef _OPENCOG_AGENTZERO_KNOWLEDGE_KNOWLEDGE_BASE_H
#define _OPENCOG_AGENTZERO_KNOWLEDGE_KNOWLEDGE_BASE_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace knowledge {

/**
 * QueryResult - Holds results of a SPARQL-like query
 */
struct QueryResult {
    std::vector<std::map<std::string, Handle>> bindings; ///< Variable bindings per row
    size_t total_matches{0};
    bool success{false};
    std::string error_message;
};

/**
 * QueryVariable - A named variable in a SPARQL-like query pattern
 */
struct QueryVariable {
    std::string name;
    Type type_constraint{NOTYPE}; ///< 0 means unconstrained
};

/**
 * QueryTriple - A subject-predicate-object triple pattern (atom-space-style)
 *
 * Each field is either a fixed Handle or the name of a query variable
 * (prefixed with '?', e.g. "?subject").
 */
struct QueryTriple {
    std::string subject;   ///< Handle key or "?varname"
    std::string predicate; ///< Handle key or "?varname"
    std::string object;    ///< Handle key or "?varname"
};

/**
 * BulkLoadResult - Summary of a bulk load operation
 */
struct BulkLoadResult {
    size_t atoms_loaded{0};
    size_t atoms_failed{0};
    size_t atoms_skipped{0};
    bool success{false};
    std::string error_message;
    std::chrono::milliseconds elapsed{0};
};

/**
 * KnowledgeBase - Extended AtomSpace operations
 *
 * Provides a higher-level knowledge management interface on top of
 * OpenCog's AtomSpace, including:
 * - Bulk loading of knowledge from structured data or Scheme/Atomese files
 * - SPARQL-inspired triple-pattern queries over the hypergraph
 * - Namespace management for scoped knowledge domains
 * - Truth-value filtering during retrieval
 * - Statistics and health monitoring
 */
class KnowledgeBase
{
public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the backing AtomSpace
     */
    explicit KnowledgeBase(AtomSpacePtr atomspace);

    /**
     * Destructor
     */
    ~KnowledgeBase() = default;

    // =========================================================
    // Lifecycle
    // =========================================================

    /**
     * Initialize internal structures and verify AtomSpace connectivity.
     * Must be called once before any other method.
     * @return True on success
     */
    bool initialize();

    /**
     * Gracefully shut down the KnowledgeBase.
     * @return True on success
     */
    bool shutdown();

    /**
     * Return true if the KnowledgeBase has been successfully initialized.
     */
    bool isInitialized() const { return _initialized; }

    // =========================================================
    // Bulk Load
    // =========================================================

    /**
     * Load knowledge from an Atomese/Scheme file.
     *
     * The file is expected to contain s-expressions that can be evaluated
     * against the AtomSpace (e.g., produced by `cog-prt-atomspace`).
     *
     * @param file_path Absolute or relative path to the .scm or .atomese file
     * @param namespace_prefix Optional namespace to tag loaded atoms
     * @return BulkLoadResult describing how many atoms were loaded/failed
     */
    BulkLoadResult loadFromFile(const std::string& file_path,
                                const std::string& namespace_prefix = "");

    /**
     * Load knowledge from a vector of (subject, predicate, object) string
     * triples.  Each string is used as the name of a ConceptNode.
     *
     * @param triples  Vector of <subject, predicate, object> tuples
     * @param namespace_prefix Optional namespace prefix
     * @return BulkLoadResult describing how many atoms were created
     */
    BulkLoadResult loadFromTriples(
        const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
        const std::string& namespace_prefix = "");

    /**
     * Load knowledge from a map of concept-name → list-of-related-concepts.
     * Creates ConceptNodes connected via EvaluationLinks.
     *
     * @param adjacency  Map: concept name → set of related concept names
     * @param relation   Name of the relationship predicate (default "related-to")
     * @param namespace_prefix Optional namespace prefix
     * @return BulkLoadResult
     */
    BulkLoadResult loadFromAdjacency(
        const std::map<std::string, std::vector<std::string>>& adjacency,
        const std::string& relation = "related-to",
        const std::string& namespace_prefix = "");

    // =========================================================
    // SPARQL-like Queries
    // =========================================================

    /**
     * Execute a triple-pattern query against the AtomSpace.
     *
     * Patterns use "?varname" strings for variables, e.g.:
     * @code
     *   QueryTriple p{"?x", "isa", "Animal"};
     *   auto result = kb.query({p});
     * @endcode
     *
     * @param patterns  One or more triple patterns (joined with AND)
     * @param variables Named variables to project into the result set
     * @param limit     Maximum number of result rows (0 = unlimited)
     * @return QueryResult with variable bindings
     */
    QueryResult query(const std::vector<QueryTriple>& patterns,
                      const std::vector<QueryVariable>& variables = {},
                      size_t limit = 0);

    /**
     * Simple convenience query: find all handles with a given type.
     *
     * @param atom_type AtomSpace type constant
     * @param min_confidence Minimum truth-value strength to include
     * @return Vector of matching handles
     */
    std::vector<Handle> queryByType(Type atom_type,
                                    double min_confidence = 0.0);

    /**
     * Find all atoms whose name contains the given substring.
     *
     * @param name_fragment  Substring to search for (case-sensitive)
     * @param atom_type      Optional type filter (NOTYPE = any type)
     * @return Vector of matching handles
     */
    std::vector<Handle> queryByName(const std::string& name_fragment,
                                    Type atom_type = NOTYPE);

    /**
     * Find all atoms connected to a given handle via a specific link type.
     *
     * @param source    Handle to start from
     * @param link_type Type of link to follow (e.g. INHERITANCE_LINK)
     * @param incoming  If true, search incoming links; otherwise outgoing
     * @return Vector of connected handles
     */
    std::vector<Handle> queryNeighbors(const Handle& source,
                                       Type link_type,
                                       bool incoming = false);

    /**
     * Return all atoms whose truth-value strength is above a threshold.
     *
     * @param min_strength Minimum strength (0.0–1.0)
     * @param min_confidence Minimum confidence (0.0–1.0)
     * @return Vector of handles meeting criteria
     */
    std::vector<Handle> queryByTruthValue(double min_strength,
                                          double min_confidence = 0.0);

    // =========================================================
    // Direct AtomSpace Helpers
    // =========================================================

    /**
     * Add or update an atom in the knowledge base.
     *
     * @param atom   Handle to add (must already be in _atomspace or be a
     *               newly constructed atom)
     * @param tv     Optional truth value to set
     * @return The handle as stored in the AtomSpace
     */
    Handle addAtom(const Handle& atom,
                   TruthValuePtr tv = nullptr);

    /**
     * Remove an atom from the knowledge base.
     *
     * @param handle  Handle to remove
     * @param recursive If true, also remove incoming atoms
     * @return True if removed successfully
     */
    bool removeAtom(const Handle& handle, bool recursive = false);

    /**
     * Check if a handle is currently present in the knowledge base.
     */
    bool contains(const Handle& handle) const;

    /**
     * Return the number of atoms currently in the AtomSpace.
     */
    size_t size() const;

    /**
     * Clear all atoms from the AtomSpace.
     */
    void clear();

    // =========================================================
    // Statistics and Monitoring
    // =========================================================

    /**
     * Return a human-readable summary of knowledge-base statistics.
     */
    std::string getStatsSummary() const;

    /**
     * Return per-type atom counts.
     */
    std::map<std::string, size_t> getTypeCounts() const;

    /**
     * Return true if the knowledge base is in a healthy state.
     */
    bool isHealthy() const;

private:
    // -------------------------
    // Internal helpers
    // -------------------------

    /**
     * Resolve a triple-pattern field.
     *
     * @param field  The string from a QueryTriple (either "?var" or a concept name)
     * @param bindings  Current variable bindings for this row
     * @param[out] handle  Resolved handle if the field is concrete
     * @param[out] var_name  Variable name if the field is a variable ("?var")
     * @return True if field is a variable, false if it is concrete
     */
    bool resolveField(const std::string& field,
                      const std::map<std::string, Handle>& bindings,
                      Handle& handle,
                      std::string& var_name) const;

    /**
     * Recursively enumerate all partial bindings that satisfy all patterns.
     */
    void matchPatterns(const std::vector<QueryTriple>& patterns,
                       size_t pattern_idx,
                       std::map<std::string, Handle>& current_bindings,
                       std::vector<std::map<std::string, Handle>>& results,
                       size_t limit) const;

    /**
     * Find all EvaluationLinks whose subject is `subj_handle` and
     * whose predicate is `pred_name`.  Returns the object handles.
     */
    std::vector<Handle> findObjects(const Handle& subj_handle,
                                    const std::string& pred_name) const;

    // -------------------------
    // State
    // -------------------------

    AtomSpacePtr _atomspace;
    bool _initialized{false};
    mutable std::mutex _mutex;
};

} // namespace knowledge
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_KNOWLEDGE_KNOWLEDGE_BASE_H
