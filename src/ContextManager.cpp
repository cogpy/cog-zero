/*
 * opencog/agentzero/memory/ContextManager.cpp
 * src/ContextManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ContextManager Implementation
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-CONTEXT-001
 */

#include "opencog/agentzero/memory/ContextManager.h"
#include <opencog/util/Logger.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <algorithm>
#include <sstream>
#include <cmath>

using namespace opencog;
using namespace opencog::agentzero::memory;

// Constructor
ContextManager::ContextManager(
    AtomSpacePtr atomspace,
    size_t max_contexts,
    double min_importance,
    Duration decay_time
)
    : _atomspace(atomspace)
    , _active_context_id("")
    , _max_history_size(100)
    , _context_switch_count(0)
    , _max_contexts(max_contexts)
    , _min_context_importance(min_importance)
    , _context_decay_time(decay_time)
{
    if (!_atomspace) {
        throw std::runtime_error("ContextManager requires valid AtomSpace");
    }
    
    logger().info() << "[ContextManager] Initializing with max_contexts=" << max_contexts
                    << ", min_importance=" << min_importance;
}

// Destructor
ContextManager::~ContextManager()
{
    logger().info() << "[ContextManager] Shutting down...";
    shutdown();
}

// Initialize the context manager
bool ContextManager::initialize()
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    try {
        logger().info() << "[ContextManager] Starting initialization...";
        
        // Initialize statistics
        updateStatistics();
        
        logger().info() << "[ContextManager] Initialization complete";
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[ContextManager] Initialization failed: " << e.what();
        return false;
    }
}

// Shutdown the context manager
bool ContextManager::shutdown()
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    try {
        logger().info() << "[ContextManager] Shutting down...";
        
        // Clear all contexts
        _contexts.clear();
        _type_index.clear();
        _importance_index.clear();
        _atom_to_contexts.clear();
        _context_history.clear();
        _active_context_id = "";
        
        logger().info() << "[ContextManager] Shutdown complete";
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[ContextManager] Shutdown error: " << e.what();
        return false;
    }
}

// Create a new context
bool ContextManager::createContext(
    const std::string& context_id,
    ContextType context_type,
    const std::map<std::string, std::string>& metadata,
    double initial_importance
)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    if (context_id.empty()) {
        logger().warn() << "[ContextManager] Cannot create context with empty ID";
        return false;
    }
    
    if (_contexts.find(context_id) != _contexts.end()) {
        logger().warn() << "[ContextManager] Context already exists: " << context_id;
        return false;
    }
    
    // Check if we need to cleanup old contexts
    if (_contexts.size() >= _max_contexts) {
        cleanupLowImportanceContexts();
    }
    
    // Create new context entry
    auto entry = std::make_shared<ContextEntry>(context_id, context_type);
    entry->metadata = metadata;
    entry->importance = std::max(0.0, std::min(1.0, initial_importance));
    
    // Store context
    _contexts[context_id] = entry;
    
    // Update indices
    addToIndices(context_id, entry);
    
    // Update statistics
    updateStatistics();
    
    logger().debug() << "[ContextManager] Created context: " << context_id
                     << " (type=" << static_cast<int>(context_type) << ")";
    
    return true;
}

// Delete a context
bool ContextManager::deleteContext(const std::string& context_id)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        logger().warn() << "[ContextManager] Context not found: " << context_id;
        return false;
    }
    
    // Remove from indices
    removeFromIndices(context_id);
    
    // Remove from atom-to-contexts mapping
    for (const auto& atom : it->second->active_atoms) {
        auto atom_it = _atom_to_contexts.find(atom);
        if (atom_it != _atom_to_contexts.end()) {
            atom_it->second.erase(context_id);
            if (atom_it->second.empty()) {
                _atom_to_contexts.erase(atom_it);
            }
        }
    }
    
    // Clear active context if this was it
    if (_active_context_id == context_id) {
        _active_context_id = "";
    }
    
    // Remove context
    _contexts.erase(it);
    
    // Update statistics
    updateStatistics();
    
    logger().debug() << "[ContextManager] Deleted context: " << context_id;
    
    return true;
}

// Check if a context exists
bool ContextManager::hasContext(const std::string& context_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    return _contexts.find(context_id) != _contexts.end();
}

// Get a context entry
std::shared_ptr<ContextEntry> ContextManager::getContext(const std::string& context_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it != _contexts.end()) {
        return it->second;
    }
    
    return nullptr;
}

// Set the active context
bool ContextManager::setActiveContext(const std::string& context_id)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    if (context_id.empty()) {
        return clearActiveContext();
    }
    
    if (!hasContext(context_id)) {
        logger().warn() << "[ContextManager] Cannot activate non-existent context: " << context_id;
        return false;
    }
    
    // Record context switch
    if (_active_context_id != context_id) {
        _context_switch_count++;
        
        // Add to history
        _context_history.push_front(context_id);
        if (_context_history.size() > _max_history_size) {
            _context_history.pop_back();
        }
        
        logger().debug() << "[ContextManager] Context switch: " << _active_context_id
                         << " -> " << context_id;
    }
    
    _active_context_id = context_id;
    
    // Record access
    recordContextAccess(context_id);
    
    return true;
}

// Get the current active context ID
std::string ContextManager::getActiveContext() const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    return _active_context_id;
}

// Clear the active context
bool ContextManager::clearActiveContext()
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    _active_context_id = "";
    return true;
}

// Add an atom to a context
bool ContextManager::addAtomToContext(const std::string& context_id, const Handle& atom)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    if (atom == Handle::UNDEFINED) {
        return false;
    }
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        logger().warn() << "[ContextManager] Cannot add atom to non-existent context: " << context_id;
        return false;
    }
    
    // Add to context
    it->second->active_atoms.insert(atom);
    
    // Update atom-to-contexts mapping
    _atom_to_contexts[atom].insert(context_id);
    
    // Boost context importance slightly
    updateContextImportance(context_id);
    
    return true;
}

// Remove an atom from a context
bool ContextManager::removeAtomFromContext(const std::string& context_id, const Handle& atom)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return false;
    }
    
    // Remove from context
    it->second->active_atoms.erase(atom);
    
    // Update atom-to-contexts mapping
    auto atom_it = _atom_to_contexts.find(atom);
    if (atom_it != _atom_to_contexts.end()) {
        atom_it->second.erase(context_id);
        if (atom_it->second.empty()) {
            _atom_to_contexts.erase(atom_it);
        }
    }
    
    return true;
}

// Get all atoms in a context
std::set<Handle> ContextManager::getAtomsInContext(const std::string& context_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it != _contexts.end()) {
        return it->second->active_atoms;
    }
    
    return std::set<Handle>();
}

// Get all contexts containing an atom
std::set<std::string> ContextManager::getContextsForAtom(const Handle& atom) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _atom_to_contexts.find(atom);
    if (it != _atom_to_contexts.end()) {
        return it->second;
    }
    
    return std::set<std::string>();
}

// Clear all atoms from a context
bool ContextManager::clearContextAtoms(const std::string& context_id)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return false;
    }
    
    // Remove from atom-to-contexts mapping
    for (const auto& atom : it->second->active_atoms) {
        auto atom_it = _atom_to_contexts.find(atom);
        if (atom_it != _atom_to_contexts.end()) {
            atom_it->second.erase(context_id);
            if (atom_it->second.empty()) {
                _atom_to_contexts.erase(atom_it);
            }
        }
    }
    
    // Clear atoms
    it->second->active_atoms.clear();
    
    return true;
}

// Get all context IDs
std::vector<std::string> ContextManager::getAllContexts() const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    std::vector<std::string> result;
    result.reserve(_contexts.size());
    
    for (const auto& pair : _contexts) {
        result.push_back(pair.first);
    }
    
    return result;
}

// Get contexts by type
std::vector<std::string> ContextManager::getContextsByType(ContextType context_type) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _type_index.find(context_type);
    if (it != _type_index.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    
    return std::vector<std::string>();
}

// Get contexts by importance threshold
std::vector<std::string> ContextManager::getContextsByImportance(double min_importance) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    std::vector<std::string> result;
    
    auto lower = _importance_index.lower_bound(min_importance);
    for (auto it = lower; it != _importance_index.end(); ++it) {
        result.push_back(it->second);
    }
    
    return result;
}

// Get the most important contexts
std::vector<std::string> ContextManager::getMostImportantContexts(size_t count) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    std::vector<std::string> result;
    result.reserve(std::min(count, _importance_index.size()));
    
    // Iterate from highest to lowest importance
    auto it = _importance_index.rbegin();
    size_t added = 0;
    
    while (it != _importance_index.rend() && added < count) {
        result.push_back(it->second);
        ++it;
        ++added;
    }
    
    return result;
}

// Get context history
std::vector<std::string> ContextManager::getContextHistory(size_t count) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    std::vector<std::string> result;
    size_t to_copy = std::min(count, _context_history.size());
    
    auto begin = _context_history.begin();
    auto end = begin + to_copy;
    
    result.assign(begin, end);
    
    return result;
}

// Set metadata for a context
bool ContextManager::setContextMetadata(
    const std::string& context_id,
    const std::string& key,
    const std::string& value
)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return false;
    }
    
    it->second->metadata[key] = value;
    return true;
}

// Get metadata from a context
std::string ContextManager::getContextMetadata(
    const std::string& context_id,
    const std::string& key
) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return "";
    }
    
    auto meta_it = it->second->metadata.find(key);
    if (meta_it != it->second->metadata.end()) {
        return meta_it->second;
    }
    
    return "";
}

// Get all metadata for a context
std::map<std::string, std::string> ContextManager::getAllContextMetadata(
    const std::string& context_id
) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it != _contexts.end()) {
        return it->second->metadata;
    }
    
    return std::map<std::string, std::string>();
}

// Update the importance of a context
bool ContextManager::setContextImportance(const std::string& context_id, double importance)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return false;
    }
    
    // Remove old importance index entry
    for (auto imp_it = _importance_index.begin(); imp_it != _importance_index.end(); ) {
        if (imp_it->second == context_id) {
            imp_it = _importance_index.erase(imp_it);
        } else {
            ++imp_it;
        }
    }
    
    // Update importance
    importance = std::max(0.0, std::min(1.0, importance));
    it->second->importance = importance;
    
    // Add new importance index entry
    _importance_index.insert({importance, context_id});
    
    return true;
}

// Get the importance of a context
double ContextManager::getContextImportance(const std::string& context_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it != _contexts.end()) {
        return it->second->importance;
    }
    
    return -1.0;
}

// Boost the importance of a context
bool ContextManager::boostContextImportance(const std::string& context_id, double boost_factor)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return false;
    }
    
    double new_importance = it->second->importance * boost_factor;
    return setContextImportance(context_id, new_importance);
}

// Decay importance of all contexts
size_t ContextManager::decayContextImportances()
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    size_t decayed_count = 0;
    auto now = std::chrono::system_clock::now();
    
    for (auto& pair : _contexts) {
        auto& entry = pair.second;
        
        // Calculate time since last access
        auto time_diff = std::chrono::duration_cast<Duration>(
            now - entry->last_accessed_time
        );
        
        // Apply decay if enough time has passed
        if (time_diff >= _context_decay_time) {
            double decay_factor = 0.9;  // 10% decay
            entry->importance *= decay_factor;
            
            // Clamp to minimum
            if (entry->importance < _min_context_importance) {
                entry->importance = _min_context_importance;
            }
            
            decayed_count++;
        }
    }
    
    // Rebuild importance index after decay
    if (decayed_count > 0) {
        rebuildImportanceIndex();
    }
    
    return decayed_count;
}

// Get context statistics
ContextStatistics ContextManager::getStatistics() const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    return _statistics;
}

// Get the number of contexts
size_t ContextManager::getContextCount() const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    return _contexts.size();
}

// Get the total number of atoms across all contexts
size_t ContextManager::getTotalAtomCount() const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    return _atom_to_contexts.size();
}

// Get detailed information about a context
std::map<std::string, std::string> ContextManager::getContextInfo(const std::string& context_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    std::map<std::string, std::string> info;
    
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return info;
    }
    
    auto& entry = it->second;
    
    info["context_id"] = entry->context_id;
    info["context_type"] = std::to_string(static_cast<int>(entry->context_type));
    info["importance"] = std::to_string(entry->importance);
    info["atom_count"] = std::to_string(entry->active_atoms.size());
    info["access_count"] = std::to_string(entry->access_count);
    
    return info;
}

// Clear all contexts
bool ContextManager::clearAllContexts()
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    _contexts.clear();
    _type_index.clear();
    _importance_index.clear();
    _atom_to_contexts.clear();
    _context_history.clear();
    _active_context_id = "";
    
    updateStatistics();
    
    logger().info() << "[ContextManager] Cleared all contexts";
    
    return true;
}

// Merge two contexts
bool ContextManager::mergeContexts(
    const std::string& source_id,
    const std::string& target_id,
    bool delete_source
)
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    auto source_it = _contexts.find(source_id);
    auto target_it = _contexts.find(target_id);
    
    if (source_it == _contexts.end() || target_it == _contexts.end()) {
        logger().warn() << "[ContextManager] Cannot merge: context not found";
        return false;
    }
    
    auto& source = source_it->second;
    auto& target = target_it->second;
    
    // Merge atoms
    for (const auto& atom : source->active_atoms) {
        target->active_atoms.insert(atom);
        _atom_to_contexts[atom].insert(target_id);
    }
    
    // Merge metadata
    for (const auto& meta : source->metadata) {
        if (target->metadata.find(meta.first) == target->metadata.end()) {
            target->metadata[meta.first] = meta.second;
        }
    }
    
    // Update importance (average of both)
    double combined_importance = (source->importance + target->importance) / 2.0;
    setContextImportance(target_id, combined_importance);
    
    // Merge access counts
    target->access_count += source->access_count;
    
    // Delete source if requested
    if (delete_source) {
        deleteContext(source_id);
    }
    
    logger().debug() << "[ContextManager] Merged contexts: " << source_id
                     << " -> " << target_id;
    
    return true;
}

// Create a snapshot of the current context state
Handle ContextManager::createContextSnapshot()
{
    std::lock_guard<std::recursive_mutex> lock(_context_mutex);
    
    // Create a concept node representing the context snapshot
    std::stringstream ss;
    ss << "ContextSnapshot_" << std::chrono::system_clock::now().time_since_epoch().count();
    
    Handle snapshot = _atomspace->add_node(CONCEPT_NODE, ss.str());
    
    // Link active context
    if (!_active_context_id.empty()) {
        Handle active_ctx = _atomspace->add_node(CONCEPT_NODE, "ActiveContext_" + _active_context_id);
        _atomspace->add_link(MEMBER_LINK, HandleSeq{active_ctx, snapshot});
    }
    
    // Link all contexts
    for (const auto& pair : _contexts) {
        Handle ctx_node = _atomspace->add_node(CONCEPT_NODE, "Context_" + pair.first);
        _atomspace->add_link(MEMBER_LINK, HandleSeq{ctx_node, snapshot});
    }
    
    return snapshot;
}

// === Private Helper Methods ===

// Update context importance based on activity
void ContextManager::updateContextImportance(const std::string& context_id)
{
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return;
    }
    
    auto& entry = it->second;
    
    // Calculate importance based on:
    // - Number of atoms (more atoms = higher importance)
    // - Access frequency (more accesses = higher importance)
    // - Recency (more recent = higher importance)
    
    double atom_factor = std::min(1.0, entry->active_atoms.size() / 100.0);
    double access_factor = std::min(1.0, entry->access_count / 50.0);
    
    auto now = std::chrono::system_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::hours>(
        now - entry->last_accessed_time
    ).count();
    double recency_factor = 1.0 / (1.0 + time_diff / 24.0);
    
    double new_importance = (atom_factor * 0.4 + access_factor * 0.3 + recency_factor * 0.3);
    
    // Apply with some damping to avoid drastic changes
    entry->importance = entry->importance * 0.7 + new_importance * 0.3;
    entry->importance = std::max(_min_context_importance, std::min(1.0, entry->importance));
}

// Add context to indices
void ContextManager::addToIndices(const std::string& context_id, std::shared_ptr<ContextEntry> entry)
{
    _type_index[entry->context_type].insert(context_id);
    _importance_index.insert({entry->importance, context_id});
}

// Remove context from indices
void ContextManager::removeFromIndices(const std::string& context_id)
{
    auto it = _contexts.find(context_id);
    if (it == _contexts.end()) {
        return;
    }
    
    // Remove from type index
    auto type_it = _type_index.find(it->second->context_type);
    if (type_it != _type_index.end()) {
        type_it->second.erase(context_id);
        if (type_it->second.empty()) {
            _type_index.erase(type_it);
        }
    }
    
    // Remove from importance index
    for (auto imp_it = _importance_index.begin(); imp_it != _importance_index.end(); ) {
        if (imp_it->second == context_id) {
            imp_it = _importance_index.erase(imp_it);
        } else {
            ++imp_it;
        }
    }
}

// Rebuild importance index
void ContextManager::rebuildImportanceIndex()
{
    _importance_index.clear();
    
    for (const auto& pair : _contexts) {
        _importance_index.insert({pair.second->importance, pair.first});
    }
}

// Cleanup low importance contexts
void ContextManager::cleanupLowImportanceContexts()
{
    // Remove contexts with lowest importance until we're under limit
    size_t to_remove = _contexts.size() - _max_contexts + 1;
    
    auto it = _importance_index.begin();
    std::vector<std::string> to_delete;
    
    for (size_t i = 0; i < to_remove && it != _importance_index.end(); ++i, ++it) {
        if (it->first < _min_context_importance && it->second != _active_context_id) {
            to_delete.push_back(it->second);
        }
    }
    
    for (const auto& context_id : to_delete) {
        deleteContext(context_id);
    }
}

// Record context access
void ContextManager::recordContextAccess(const std::string& context_id)
{
    auto it = _contexts.find(context_id);
    if (it != _contexts.end()) {
        it->second->last_accessed_time = std::chrono::system_clock::now();
        it->second->access_count++;
        updateContextImportance(context_id);
    }
}

// Generate context key
std::string ContextManager::generateContextKey(ContextType type, const std::string& identifier)
{
    std::stringstream ss;
    ss << "ctx_" << static_cast<int>(type) << "_" << identifier;
    return ss.str();
}

// Update statistics
void ContextManager::updateStatistics()
{
    _statistics.total_contexts = _contexts.size();
    _statistics.active_contexts = _active_context_id.empty() ? 0 : 1;
    _statistics.total_context_switches = _context_switch_count;
    _statistics.total_atoms_in_contexts = _atom_to_contexts.size();
    
    // Calculate average importance
    if (!_contexts.empty()) {
        double total_importance = 0.0;
        for (const auto& pair : _contexts) {
            total_importance += pair.second->importance;
        }
        _statistics.average_context_importance = total_importance / _contexts.size();
    } else {
        _statistics.average_context_importance = 0.0;
    }
}
