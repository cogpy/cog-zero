/*
 * opencog/agentzero/memory/ContextManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ContextManager - Maintains relevant contextual information for situational awareness
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-CONTEXT-001
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_CONTEXT_MANAGER_H
#define _OPENCOG_AGENTZERO_MEMORY_CONTEXT_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <deque>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "MemoryTypes.h"

namespace opencog {
namespace agentzero {
namespace memory {

/**
 * Context entry representing a specific situational context
 */
struct ContextEntry {
    std::string context_id;              // Unique context identifier
    ContextType context_type;            // Type of context
    std::set<Handle> active_atoms;       // Atoms relevant to this context
    std::map<std::string, std::string> metadata;  // Context metadata
    TimePoint created_time;              // When context was created
    TimePoint last_accessed_time;        // Last access time
    size_t access_count;                 // Number of times accessed
    double importance;                   // Context importance (0.0-1.0)
    
    ContextEntry()
        : context_id("")
        , context_type(ContextType::COGNITIVE)
        , created_time(std::chrono::system_clock::now())
        , last_accessed_time(std::chrono::system_clock::now())
        , access_count(0)
        , importance(0.5) {}
    
    ContextEntry(const std::string& id, ContextType type)
        : context_id(id)
        , context_type(type)
        , created_time(std::chrono::system_clock::now())
        , last_accessed_time(std::chrono::system_clock::now())
        , access_count(0)
        , importance(0.5) {}
};

/**
 * Context statistics for monitoring
 */
struct ContextStatistics {
    size_t total_contexts;
    size_t active_contexts;
    size_t total_context_switches;
    double average_context_importance;
    size_t total_atoms_in_contexts;
    
    ContextStatistics()
        : total_contexts(0)
        , active_contexts(0)
        , total_context_switches(0)
        , average_context_importance(0.0)
        , total_atoms_in_contexts(0) {}
};

/**
 * ContextManager - Maintains relevant contextual information for situational awareness
 *
 * This class provides comprehensive context management for Agent-Zero's cognitive operations.
 * It tracks multiple contexts simultaneously, manages context switching, and integrates
 * with the AtomSpace for semantic context representation.
 *
 * Key Features:
 * - Multi-context tracking with importance-based prioritization
 * - Context switching and activation management
 * - AtomSpace integration for semantic representation
 * - Context-based memory retrieval and filtering
 * - Automatic context importance calculation
 * - Context history and statistics tracking
 * - Thread-safe operations
 */
class ContextManager
{
private:
    // Core components
    AtomSpacePtr _atomspace;
    
    // Context management
    std::map<std::string, std::shared_ptr<ContextEntry>> _contexts;
    std::string _active_context_id;
    std::deque<std::string> _context_history;
    size_t _max_history_size;
    
    // Context indexing
    std::map<ContextType, std::set<std::string>> _type_index;
    std::multimap<double, std::string> _importance_index;
    std::map<Handle, std::set<std::string>> _atom_to_contexts;
    
    // Statistics and monitoring
    ContextStatistics _statistics;
    size_t _context_switch_count;
    
    // Configuration
    size_t _max_contexts;
    double _min_context_importance;
    Duration _context_decay_time;
    
    // Threading and synchronization
    mutable std::recursive_mutex _context_mutex;
    
    // Internal methods
    void updateContextImportance(const std::string& context_id);
    void addToIndices(const std::string& context_id, std::shared_ptr<ContextEntry> entry);
    void removeFromIndices(const std::string& context_id);
    void rebuildImportanceIndex();
    void cleanupLowImportanceContexts();
    void recordContextAccess(const std::string& context_id);
    std::string generateContextKey(ContextType type, const std::string& identifier);
    void updateStatistics();
    
public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     * @param max_contexts Maximum number of concurrent contexts (default: 100)
     * @param min_importance Minimum importance threshold for context retention (default: 0.1)
     * @param decay_time Time period for context importance decay (default: 1 hour)
     */
    explicit ContextManager(
        AtomSpacePtr atomspace,
        size_t max_contexts = 100,
        double min_importance = 0.1,
        Duration decay_time = std::chrono::hours(1)
    );
    
    /**
     * Destructor - ensures clean shutdown
     */
    ~ContextManager();
    
    /**
     * Initialize the context manager
     * @return True if initialization succeeded
     */
    bool initialize();
    
    /**
     * Shutdown the context manager gracefully
     * @return True if shutdown completed successfully
     */
    bool shutdown();
    
    // === Core Context Operations ===
    
    /**
     * Create a new context
     * @param context_id Unique identifier for the context
     * @param context_type Type of context
     * @param metadata Optional metadata map
     * @param initial_importance Initial importance value (0.0-1.0)
     * @return True if context was created successfully
     */
    bool createContext(
        const std::string& context_id,
        ContextType context_type,
        const std::map<std::string, std::string>& metadata = {},
        double initial_importance = 0.5
    );
    
    /**
     * Delete a context
     * @param context_id Context identifier to delete
     * @return True if context was deleted successfully
     */
    bool deleteContext(const std::string& context_id);
    
    /**
     * Check if a context exists
     * @param context_id Context identifier to check
     * @return True if context exists
     */
    bool hasContext(const std::string& context_id) const;
    
    /**
     * Get a context entry
     * @param context_id Context identifier
     * @return Shared pointer to context entry, or nullptr if not found
     */
    std::shared_ptr<ContextEntry> getContext(const std::string& context_id) const;
    
    /**
     * Set the active context
     * @param context_id Context identifier to activate
     * @return True if context was activated successfully
     */
    bool setActiveContext(const std::string& context_id);
    
    /**
     * Get the current active context ID
     * @return Active context identifier, or empty string if none
     */
    std::string getActiveContext() const;
    
    /**
     * Clear the active context (no context active)
     * @return True if cleared successfully
     */
    bool clearActiveContext();
    
    // === Context Content Management ===
    
    /**
     * Add an atom to a context
     * @param context_id Context identifier
     * @param atom Handle to add
     * @return True if atom was added successfully
     */
    bool addAtomToContext(const std::string& context_id, const Handle& atom);
    
    /**
     * Remove an atom from a context
     * @param context_id Context identifier
     * @param atom Handle to remove
     * @return True if atom was removed successfully
     */
    bool removeAtomFromContext(const std::string& context_id, const Handle& atom);
    
    /**
     * Get all atoms in a context
     * @param context_id Context identifier
     * @return Set of handles in the context
     */
    std::set<Handle> getAtomsInContext(const std::string& context_id) const;
    
    /**
     * Get all contexts containing an atom
     * @param atom Handle to search for
     * @return Set of context IDs containing the atom
     */
    std::set<std::string> getContextsForAtom(const Handle& atom) const;
    
    /**
     * Clear all atoms from a context
     * @param context_id Context identifier
     * @return True if cleared successfully
     */
    bool clearContextAtoms(const std::string& context_id);
    
    // === Context Queries and Filtering ===
    
    /**
     * Get all context IDs
     * @return Vector of all context identifiers
     */
    std::vector<std::string> getAllContexts() const;
    
    /**
     * Get contexts by type
     * @param context_type Type of context to retrieve
     * @return Vector of context identifiers matching the type
     */
    std::vector<std::string> getContextsByType(ContextType context_type) const;
    
    /**
     * Get contexts by importance threshold
     * @param min_importance Minimum importance value
     * @return Vector of context identifiers with importance >= threshold
     */
    std::vector<std::string> getContextsByImportance(double min_importance) const;
    
    /**
     * Get the most important contexts
     * @param count Number of contexts to retrieve
     * @return Vector of context identifiers ordered by importance (highest first)
     */
    std::vector<std::string> getMostImportantContexts(size_t count) const;
    
    /**
     * Get context history (ordered from most recent to oldest)
     * @param count Maximum number of history entries to return
     * @return Vector of context identifiers from history
     */
    std::vector<std::string> getContextHistory(size_t count = 10) const;
    
    // === Context Metadata Management ===
    
    /**
     * Set metadata for a context
     * @param context_id Context identifier
     * @param key Metadata key
     * @param value Metadata value
     * @return True if metadata was set successfully
     */
    bool setContextMetadata(
        const std::string& context_id,
        const std::string& key,
        const std::string& value
    );
    
    /**
     * Get metadata from a context
     * @param context_id Context identifier
     * @param key Metadata key
     * @return Metadata value, or empty string if not found
     */
    std::string getContextMetadata(
        const std::string& context_id,
        const std::string& key
    ) const;
    
    /**
     * Get all metadata for a context
     * @param context_id Context identifier
     * @return Map of metadata key-value pairs
     */
    std::map<std::string, std::string> getAllContextMetadata(
        const std::string& context_id
    ) const;
    
    // === Context Importance Management ===
    
    /**
     * Update the importance of a context
     * @param context_id Context identifier
     * @param importance New importance value (0.0-1.0)
     * @return True if importance was updated successfully
     */
    bool setContextImportance(const std::string& context_id, double importance);
    
    /**
     * Get the importance of a context
     * @param context_id Context identifier
     * @return Importance value (0.0-1.0), or -1.0 if context not found
     */
    double getContextImportance(const std::string& context_id) const;
    
    /**
     * Boost the importance of a context (increases by a factor)
     * @param context_id Context identifier
     * @param boost_factor Factor to multiply importance by (default: 1.5)
     * @return True if boost was applied successfully
     */
    bool boostContextImportance(const std::string& context_id, double boost_factor = 1.5);
    
    /**
     * Decay importance of all contexts based on time
     * @return Number of contexts that were decayed
     */
    size_t decayContextImportances();
    
    // === Statistics and Monitoring ===
    
    /**
     * Get context statistics
     * @return ContextStatistics structure
     */
    ContextStatistics getStatistics() const;
    
    /**
     * Get the number of contexts
     * @return Total number of contexts
     */
    size_t getContextCount() const;
    
    /**
     * Get the number of atoms across all contexts
     * @return Total number of unique atoms in all contexts
     */
    size_t getTotalAtomCount() const;
    
    /**
     * Get detailed information about a context
     * @param context_id Context identifier
     * @return Map with context details
     */
    std::map<std::string, std::string> getContextInfo(const std::string& context_id) const;
    
    // === Utility Methods ===
    
    /**
     * Clear all contexts
     * @return True if all contexts were cleared successfully
     */
    bool clearAllContexts();
    
    /**
     * Merge two contexts
     * @param source_id Source context to merge from
     * @param target_id Target context to merge into
     * @param delete_source Whether to delete source after merge
     * @return True if merge was successful
     */
    bool mergeContexts(
        const std::string& source_id,
        const std::string& target_id,
        bool delete_source = true
    );
    
    /**
     * Create a snapshot of the current context state in AtomSpace
     * @return Handle to the context snapshot representation
     */
    Handle createContextSnapshot();
};

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_CONTEXT_MANAGER_H