/*
 * opencog/agentzero/knowledge/KnowledgeBase.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * KnowledgeBase Implementation
 * Part of Agent-Zero Knowledge Representation & Reasoning module
 * Part of the AGENT-ZERO-GENESIS project - Phase 3
 */

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <regex>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/knowledge/KnowledgeBase.h"

using namespace opencog;
using namespace opencog::agentzero::knowledge;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

KnowledgeBase::KnowledgeBase(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("KnowledgeBase requires a valid AtomSpace");
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool KnowledgeBase::initialize()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_initialized) return true;

    logger().info() << "[KnowledgeBase] Initializing";
    _initialized = true;
    logger().info() << "[KnowledgeBase] Initialization complete";
    return true;
}

bool KnowledgeBase::shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);
    logger().info() << "[KnowledgeBase] Shutting down";
    _initialized = false;
    return true;
}

// ---------------------------------------------------------------------------
// Bulk Load — from file
// ---------------------------------------------------------------------------

BulkLoadResult KnowledgeBase::loadFromFile(const std::string& file_path,
                                            const std::string& namespace_prefix)
{
    auto start = std::chrono::steady_clock::now();
    BulkLoadResult result;

    std::ifstream f(file_path);
    if (!f.is_open()) {
        result.error_message = "Cannot open file: " + file_path;
        logger().warn() << "[KnowledgeBase] " << result.error_message;
        return result;
    }

    // Simple parser: each non-empty, non-comment line is treated as a
    // whitespace-separated triple  subject predicate object .
    // Lines starting with ';' or '#' are treated as comments.
    std::string line;
    while (std::getline(f, line)) {
        // Strip comments and whitespace
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string subj, pred, obj;
        if (!(ss >> subj >> pred >> obj)) {
            ++result.atoms_skipped;
            continue;
        }

        try {
            std::string pfx = namespace_prefix.empty() ? "" : namespace_prefix + ":";
            Handle s = _atomspace->add_node(CONCEPT_NODE, pfx + subj);
            Handle p = _atomspace->add_node(PREDICATE_NODE, pfx + pred);
            Handle o = _atomspace->add_node(CONCEPT_NODE, pfx + obj);
            Handle list = _atomspace->add_link(LIST_LINK, {s, o});
            _atomspace->add_link(EVALUATION_LINK, {p, list});
            ++result.atoms_loaded;
        } catch (const std::exception& e) {
            ++result.atoms_failed;
            logger().warn() << "[KnowledgeBase] Failed to load triple: " << e.what();
        }
    }

    auto end = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.success = (result.atoms_failed == 0);
    logger().info() << "[KnowledgeBase] loadFromFile: loaded=" << result.atoms_loaded
                    << " failed=" << result.atoms_failed
                    << " skipped=" << result.atoms_skipped
                    << " ms=" << result.elapsed.count();
    return result;
}

// ---------------------------------------------------------------------------
// Bulk Load — from triples
// ---------------------------------------------------------------------------

BulkLoadResult KnowledgeBase::loadFromTriples(
    const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
    const std::string& namespace_prefix)
{
    auto start = std::chrono::steady_clock::now();
    BulkLoadResult result;
    std::string pfx = namespace_prefix.empty() ? "" : namespace_prefix + ":";

    for (auto& [subj, pred, obj] : triples) {
        try {
            Handle s = _atomspace->add_node(CONCEPT_NODE, pfx + subj);
            Handle p = _atomspace->add_node(PREDICATE_NODE, pfx + pred);
            Handle o = _atomspace->add_node(CONCEPT_NODE, pfx + obj);
            Handle list = _atomspace->add_link(LIST_LINK, {s, o});
            _atomspace->add_link(EVALUATION_LINK, {p, list});
            ++result.atoms_loaded;
        } catch (const std::exception& e) {
            ++result.atoms_failed;
            logger().warn() << "[KnowledgeBase] Triple load failed: " << e.what();
        }
    }

    auto end = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.success = (result.atoms_failed == 0);
    return result;
}

// ---------------------------------------------------------------------------
// Bulk Load — from adjacency map
// ---------------------------------------------------------------------------

BulkLoadResult KnowledgeBase::loadFromAdjacency(
    const std::map<std::string, std::vector<std::string>>& adjacency,
    const std::string& relation,
    const std::string& namespace_prefix)
{
    std::vector<std::tuple<std::string, std::string, std::string>> triples;
    triples.reserve(adjacency.size() * 4);
    for (auto& [concept, neighbors] : adjacency) {
        for (auto& neighbor : neighbors)
            triples.emplace_back(concept, relation, neighbor);
    }
    return loadFromTriples(triples, namespace_prefix);
}

// ---------------------------------------------------------------------------
// SPARQL-like Query helpers
// ---------------------------------------------------------------------------

bool KnowledgeBase::resolveField(const std::string& field,
                                  const std::map<std::string, Handle>& bindings,
                                  Handle& handle,
                                  std::string& var_name) const
{
    if (!field.empty() && field[0] == '?') {
        var_name = field;
        auto it = bindings.find(field);
        if (it != bindings.end())
            handle = it->second;
        else
            handle = Handle::UNDEFINED;
        return true; // is a variable
    }
    var_name.clear();
    // Resolve as a concrete concept name
    handle = _atomspace->get_node(CONCEPT_NODE, field);
    if (!handle) handle = _atomspace->get_node(PREDICATE_NODE, field);
    return false; // is concrete
}

std::vector<Handle> KnowledgeBase::findObjects(const Handle& subj_handle,
                                                const std::string& pred_name) const
{
    std::vector<Handle> results;
    if (!subj_handle) return results;

    Handle pred = _atomspace->get_node(PREDICATE_NODE, pred_name);
    if (!pred) return results;

    // Walk incoming EvaluationLinks of the predicate
    IncomingSet evals = pred->getIncomingSetByType(EVALUATION_LINK);
    for (auto& eval : evals) {
        if (eval->getArity() < 2) continue;
        Handle list_link = eval->getOutgoingAtom(1);
        if (list_link->getArity() < 2) continue;
        Handle s = list_link->getOutgoingAtom(0);
        Handle o = list_link->getOutgoingAtom(1);
        if (s == subj_handle)
            results.push_back(o);
    }
    return results;
}

void KnowledgeBase::matchPatterns(
    const std::vector<QueryTriple>& patterns,
    size_t pattern_idx,
    std::map<std::string, Handle>& current_bindings,
    std::vector<std::map<std::string, Handle>>& results,
    size_t limit) const
{
    if (limit > 0 && results.size() >= limit) return;
    if (pattern_idx >= patterns.size()) {
        results.push_back(current_bindings);
        return;
    }

    const QueryTriple& pat = patterns[pattern_idx];
    Handle subj_handle, pred_handle, obj_handle;
    std::string subj_var, pred_var, obj_var;

    bool subj_is_var = resolveField(pat.subject, current_bindings, subj_handle, subj_var);
    bool pred_is_var = resolveField(pat.predicate, current_bindings, pred_handle, pred_var);
    bool obj_is_var  = resolveField(pat.object,  current_bindings, obj_handle,  obj_var);

    // If subject and predicate are concrete, enumerate matching objects
    if (!subj_is_var && subj_handle && !pred_is_var && pred_handle) {
        std::string pred_name = pred_handle->get_name();
        auto objects = findObjects(subj_handle, pred_name);
        for (auto& obj : objects) {
            if (!obj_is_var && obj_handle && obj != obj_handle) continue;
            auto new_bindings = current_bindings;
            if (obj_is_var && !obj_var.empty())
                new_bindings[obj_var] = obj;
            matchPatterns(patterns, pattern_idx + 1, new_bindings, results, limit);
        }
        return;
    }

    // Fallback: enumerate all EvaluationLinks in the atomspace
    HandleSeq all_evals;
    _atomspace->get_handles_by_type(all_evals, EVALUATION_LINK);
    for (auto& eval : all_evals) {
        if (eval->getArity() < 2) continue;
        Handle pred_atom = eval->getOutgoingAtom(0);
        Handle list_atom = eval->getOutgoingAtom(1);
        if (list_atom->getArity() < 2) continue;
        Handle s = list_atom->getOutgoingAtom(0);
        Handle o = list_atom->getOutgoingAtom(1);

        // Check constraints
        if (!subj_is_var && subj_handle && s != subj_handle) continue;
        if (!pred_is_var && pred_handle && pred_atom != pred_handle) continue;
        if (!obj_is_var  && obj_handle  && o != obj_handle)   continue;

        auto new_bindings = current_bindings;
        if (subj_is_var && !subj_var.empty()) new_bindings[subj_var] = s;
        if (pred_is_var && !pred_var.empty()) new_bindings[pred_var] = pred_atom;
        if (obj_is_var  && !obj_var.empty())  new_bindings[obj_var]  = o;

        matchPatterns(patterns, pattern_idx + 1, new_bindings, results, limit);
        if (limit > 0 && results.size() >= limit) return;
    }
}

// ---------------------------------------------------------------------------
// SPARQL-like query (public)
// ---------------------------------------------------------------------------

QueryResult KnowledgeBase::query(const std::vector<QueryTriple>& patterns,
                                  const std::vector<QueryVariable>& /*variables*/,
                                  size_t limit)
{
    QueryResult result;
    if (!_initialized) {
        result.error_message = "KnowledgeBase not initialized";
        return result;
    }
    if (patterns.empty()) {
        result.success = true;
        return result;
    }

    std::map<std::string, Handle> initial_bindings;
    matchPatterns(patterns, 0, initial_bindings, result.bindings, limit);
    result.total_matches = result.bindings.size();
    result.success = true;
    return result;
}

// ---------------------------------------------------------------------------
// Convenience queries
// ---------------------------------------------------------------------------

std::vector<Handle> KnowledgeBase::queryByType(Type atom_type,
                                                double min_confidence)
{
    HandleSeq handles;
    _atomspace->get_handles_by_type(handles, atom_type);
    if (min_confidence <= 0.0) return handles;

    std::vector<Handle> filtered;
    for (auto& h : handles) {
        TruthValuePtr tv = h->getTruthValue();
        if (tv && tv->get_confidence() >= min_confidence)
            filtered.push_back(h);
    }
    return filtered;
}

std::vector<Handle> KnowledgeBase::queryByName(const std::string& name_fragment,
                                                Type atom_type)
{
    HandleSeq all;
    if (atom_type == NOTYPE)
        _atomspace->get_handles_by_type(all, ATOM, true);
    else
        _atomspace->get_handles_by_type(all, atom_type);

    std::vector<Handle> result;
    for (auto& h : all) {
        if (h->is_node() && h->get_name().find(name_fragment) != std::string::npos)
            result.push_back(h);
    }
    return result;
}

std::vector<Handle> KnowledgeBase::queryNeighbors(const Handle& source,
                                                    Type link_type,
                                                    bool incoming)
{
    std::vector<Handle> result;
    if (!source) return result;

    IncomingSet links = incoming
        ? source->getIncomingSetByType(link_type)
        : source->getIncomingSetByType(link_type);

    if (!incoming) {
        // outgoing: find all links of this type and check if source is in outgoing
        HandleSeq all_links;
        _atomspace->get_handles_by_type(all_links, link_type);
        for (auto& lnk : all_links) {
            auto& oset = lnk->getOutgoingSet();
            if (std::find(oset.begin(), oset.end(), source) != oset.end()) {
                for (auto& h : oset)
                    if (h != source) result.push_back(h);
            }
        }
        return result;
    }

    for (auto& lnk : links) {
        for (auto& h : lnk->getOutgoingSet())
            if (h != source) result.push_back(h);
    }
    return result;
}

std::vector<Handle> KnowledgeBase::queryByTruthValue(double min_strength,
                                                       double min_confidence)
{
    HandleSeq all;
    _atomspace->get_handles_by_type(all, ATOM, true);
    std::vector<Handle> result;
    for (auto& h : all) {
        TruthValuePtr tv = h->getTruthValue();
        if (tv && tv->get_mean() >= min_strength
               && tv->get_confidence() >= min_confidence)
            result.push_back(h);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Direct operations
// ---------------------------------------------------------------------------

Handle KnowledgeBase::addAtom(const Handle& atom, TruthValuePtr tv)
{
    if (!atom) return Handle::UNDEFINED;
    Handle result = _atomspace->add_atom(atom);
    if (tv && result) result->setTruthValue(tv);
    return result;
}

bool KnowledgeBase::removeAtom(const Handle& handle, bool /*recursive*/)
{
    if (!handle) return false;
    return _atomspace->remove_atom(handle);
}

bool KnowledgeBase::contains(const Handle& handle) const
{
    if (!handle) return false;
    return _atomspace->is_valid_handle(handle);
}

size_t KnowledgeBase::size() const
{
    return _atomspace->get_size();
}

void KnowledgeBase::clear()
{
    _atomspace->clear();
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::map<std::string, size_t> KnowledgeBase::getTypeCounts() const
{
    std::map<std::string, size_t> counts;
    HandleSeq all;
    _atomspace->get_handles_by_type(all, ATOM, true);
    for (auto& h : all) {
        std::string type_name = nameserver().getTypeName(h->get_type());
        counts[type_name]++;
    }
    return counts;
}

std::string KnowledgeBase::getStatsSummary() const
{
    std::ostringstream ss;
    ss << "[KnowledgeBase] size=" << _atomspace->get_size()
       << " initialized=" << _initialized;
    return ss.str();
}

bool KnowledgeBase::isHealthy() const
{
    return _initialized && (_atomspace != nullptr);
}
