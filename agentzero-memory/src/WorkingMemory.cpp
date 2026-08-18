/*
 * src/WorkingMemory.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * WorkingMemory - Active context and short-term memory management
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-002
 */

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/WorkingMemory.h"

using namespace opencog;
using namespace opencog::agentzero;

// ===================================================================
// Constructor implementations
// ===================================================================

WorkingMemory::WorkingMemory(AtomSpacePtr atomspace)
    : WorkingMemory(atomspace, 1000, 0.1, std::chrono::seconds(3600))
{
}

WorkingMemory::WorkingMemory(AtomSpacePtr atomspace, 
                            size_t max_capacity,
                            double importance_threshold,
                            std::chrono::seconds max_retention_time)
    : _atomspace(atomspace)
    , _max_capacity(max_capacity)
    , _importance_threshold(importance_threshold)
    , _max_retention_time(max_retention_time)
    , _decay_interval(std::chrono::seconds(60))  // Run decay every minute
    , _cleanup_running(false)
    , _access_count(0)
    , _hit_count(0)
    , _miss_count(0)
    , _total_operations(0)
    , _active_context("default")
{
    auto now = std::chrono::steady_clock::now();
    _creation_time = now;
    _last_cleanup = now;
    _last_performance_reset = now;
    
    logger().info() << "[WorkingMemory] Initializing with capacity=" << _max_capacity 
                    << ", threshold=" << _importance_threshold 
                    << ", retention=" << _max_retention_time.count() << "s";
    
    // Initialize AtomSpace structures
    initializeAtomSpaceStructures();
    
#ifdef HAVE_ATTENTION_BANK
    // Try to get attention bank from atomspace
    if (_atomspace) {
        _attention_bank = _atomspace->get_attention_bank();
        if (_attention_bank) {
            logger().info() << "[WorkingMemory] ECAN attention bank integration enabled";
        }
    }
#endif
    
    logger().info() << "[WorkingMemory] Initialization complete";
}

WorkingMemory::~WorkingMemory()
{
    logger().info() << "[WorkingMemory] Destructor: Cleaning up working memory";
    
    // Run final cleanup
    if (!_cleanup_running.load()) {
        runCleanup(true);
    }
    
    // Clear all memory
    clear();
    
    logger().info() << "[WorkingMemory] Cleanup complete";
}

// ===================================================================
// Core memory operations
// ===================================================================

bool WorkingMemory::addItem(Handle atom, double importance, const std::string& context)
{
    if (!atom) {
        logger().warn() << "[WorkingMemory] Cannot add invalid atom to working memory";
        return false;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    
    // Check if item already exists
    auto it = _memory_index.find(atom);
    if (it != _memory_index.end()) {
        // Update existing item
        it->second->importance = importance;
        it->second->last_access = std::chrono::steady_clock::now();
        it->second->access_count++;
        
        // Update context if provided
        if (!context.empty() && context != it->second->context) {
            removeFromIndices(it->second);
            it->second->context = context;
            addToIndices(it->second);
        }
        
        logger().debug() << "[WorkingMemory] Updated existing item: " 
                        << atom->to_string() << " importance=" << importance;
        return true;
    }
    
    // Ensure there is room for the new item (evict until size < capacity).
    // Operate on already-locked structures — do not call public locking APIs.
    while (_memory_buffer.size() >= _max_capacity) {
        if (_importance_index.empty()) {
            break;
        }
        auto imp_it = _importance_index.begin();  // lowest importance
        auto victim = imp_it->second;
        if (!victim || !victim->atom) {
            _importance_index.erase(imp_it);
            continue;
        }
        Handle victim_atom = victim->atom;
        removeFromIndices(victim);
        _memory_index.erase(victim_atom);
        auto buffer_it = std::find(_memory_buffer.begin(), _memory_buffer.end(), victim);
        if (buffer_it != _memory_buffer.end()) {
            _memory_buffer.erase(buffer_it);
        }
    }
    
    // Create new memory item
    std::string item_context = context.empty() ? _active_context : context;
    auto memory_item = std::make_shared<MemoryItem>(atom, importance, item_context);
    
    // Add to all data structures
    _memory_buffer.push_back(memory_item);
    _memory_index[atom] = memory_item;
    addToIndices(memory_item);
    
    logger().debug() << "[WorkingMemory] Added new item: " << atom->to_string() 
                    << " importance=" << importance << " context=" << item_context;
    
    // Update AtomSpace representation
    updateAtomSpaceRepresentation();
    
    return true;
}

std::shared_ptr<MemoryItem> WorkingMemory::getItem(Handle atom)
{
    if (!atom) {
        return nullptr;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    _access_count++;
    
    auto it = _memory_index.find(atom);
    if (it != _memory_index.end()) {
        _hit_count++;
        return it->second;
    }
    
    _miss_count++;
    return nullptr;
}

bool WorkingMemory::hasItem(Handle atom) const
{
    if (!atom) {
        return false;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _memory_index.find(atom) != _memory_index.end();
}

bool WorkingMemory::removeItem(Handle atom)
{
    if (!atom) {
        return false;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    
    auto it = _memory_index.find(atom);
    if (it == _memory_index.end()) {
        return false;
    }
    
    auto memory_item = it->second;
    
    // Remove from indices
    removeFromIndices(memory_item);
    
    // Remove from main index
    _memory_index.erase(it);
    
    // Remove from buffer (find and erase)
    auto buffer_it = std::find(_memory_buffer.begin(), _memory_buffer.end(), memory_item);
    if (buffer_it != _memory_buffer.end()) {
        _memory_buffer.erase(buffer_it);
    }
    
    logger().debug() << "[WorkingMemory] Removed item: " << atom->to_string();
    
    // Update AtomSpace representation
    updateAtomSpaceRepresentation();
    
    return true;
}

bool WorkingMemory::updateImportance(Handle atom, double importance)
{
    if (!atom) {
        return false;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    
    auto it = _memory_index.find(atom);
    if (it == _memory_index.end()) {
        return false;
    }
    
    double old_importance = it->second->importance;
    it->second->importance = importance;
    it->second->last_access = std::chrono::steady_clock::now();
    
    // Update indices
    updateIndicesForImportanceChange(it->second, old_importance);
    
    logger().debug() << "[WorkingMemory] Updated importance: " << atom->to_string() 
                    << " from " << old_importance << " to " << importance;
    
    return true;
}

std::shared_ptr<MemoryItem> WorkingMemory::accessItem(Handle atom)
{
    if (!atom) {
        return nullptr;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    _access_count++;
    
    auto it = _memory_index.find(atom);
    if (it != _memory_index.end()) {
        _hit_count++;
        
        // Update access information
        it->second->last_access = std::chrono::steady_clock::now();
        it->second->access_count++;
        
        logger().debug() << "[WorkingMemory] Accessed item: " << atom->to_string() 
                        << " (access_count=" << it->second->access_count << ")";
        
        return it->second;
    }
    
    _miss_count++;
    return nullptr;
}

// ===================================================================
// Context-based operations
// ===================================================================

std::vector<std::shared_ptr<MemoryItem>> WorkingMemory::getItemsByContext(const std::string& context) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    std::vector<std::shared_ptr<MemoryItem>> result;
    auto range = _context_index.equal_range(context);
    
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    
    logger().debug() << "[WorkingMemory] Retrieved " << result.size() 
                    << " items from context: " << context;
    
    return result;
}

void WorkingMemory::setActiveContext(const std::string& context)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _active_context = context;
    
    logger().debug() << "[WorkingMemory] Set active context: " << context;
}

std::string WorkingMemory::getActiveContext() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _active_context;
}

size_t WorkingMemory::clearContext(const std::string& context)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    
    std::vector<Handle> to_remove;
    auto range = _context_index.equal_range(context);
    
    for (auto it = range.first; it != range.second; ++it) {
        to_remove.push_back(it->second->atom);
    }
    
    size_t removed_count = 0;
    for (Handle atom : to_remove) {
        if (removeItem(atom)) {
            removed_count++;
        }
    }
    
    logger().info() << "[WorkingMemory] Cleared " << removed_count 
                   << " items from context: " << context;
    
    return removed_count;
}

// ===================================================================
// Importance and attention-based operations
// ===================================================================

std::vector<std::shared_ptr<MemoryItem>> WorkingMemory::getImportantItems(double min_importance) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    std::vector<std::shared_ptr<MemoryItem>> result;
    
    // Use importance index for efficient lookup
    auto it = _importance_index.lower_bound(min_importance);
    for (; it != _importance_index.end(); ++it) {
        result.push_back(it->second);
    }
    
    logger().debug() << "[WorkingMemory] Retrieved " << result.size() 
                    << " items above importance " << min_importance;
    
    return result;
}

std::vector<std::shared_ptr<MemoryItem>> WorkingMemory::getMostImportantItems(size_t max_items) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    std::vector<std::shared_ptr<MemoryItem>> result;
    
    // Iterate from highest importance (reverse iterator)
    auto rit = _importance_index.rbegin();
    for (size_t count = 0; count < max_items && rit != _importance_index.rend(); ++rit, ++count) {
        result.push_back(rit->second);
    }
    
    logger().debug() << "[WorkingMemory] Retrieved " << result.size() 
                    << " most important items";
    
    return result;
}

std::vector<std::shared_ptr<MemoryItem>> WorkingMemory::getLeastImportantItems(size_t max_items) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    std::vector<std::shared_ptr<MemoryItem>> result;
    
    // Iterate from lowest importance
    auto it = _importance_index.begin();
    for (size_t count = 0; count < max_items && it != _importance_index.end(); ++it, ++count) {
        result.push_back(it->second);
    }
    
    logger().debug() << "[WorkingMemory] Retrieved " << result.size() 
                    << " least important items";
    
    return result;
}

#ifdef HAVE_ATTENTION_BANK
void WorkingMemory::synchronizeWithAttentionBank()
{
    if (!_attention_bank) {
        logger().warn() << "[WorkingMemory] No attention bank available for synchronization";
        return;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _total_operations++;
    
    size_t updated_count = 0;
    
    for (auto& pair : _memory_index) {
        Handle atom = pair.first;
        auto memory_item = pair.second;
        
        // Get attention value from ECAN
        AttentionValuePtr av = _attention_bank->get_attention_value(atom);
        if (av) {
            double sti = av->getSTI();
            double old_importance = memory_item->importance;
            
            // Update importance based on STI
            memory_item->importance = std::max(0.0, sti / 100.0);  // Normalize STI
            memory_item->last_access = std::chrono::steady_clock::now();
            
            // Update indices if importance changed significantly
            if (std::abs(old_importance - memory_item->importance) > 0.01) {
                updateIndicesForImportanceChange(memory_item, old_importance);
                updated_count++;
            }
        }
    }
    
    logger().info() << "[WorkingMemory] Synchronized " << updated_count 
                   << " items with attention bank";
}

void WorkingMemory::setAttentionBank(AttentionBankPtr attention_bank)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    _attention_bank = attention_bank;
    
    logger().info() << "[WorkingMemory] Attention bank set for ECAN integration";
}
#endif

// ===================================================================
// Memory management and cleanup
// ===================================================================

size_t WorkingMemory::runCleanup(bool force_cleanup)
{
    // Prevent concurrent cleanup operations
    if (_cleanup_running.exchange(true)) {
        return 0;  // Another cleanup is already running
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    // Check if cleanup is needed (unless forced)
    if (!force_cleanup) {
        auto time_since_last_cleanup = now - _last_cleanup;
        if (time_since_last_cleanup < _decay_interval) {
            _cleanup_running.store(false);
            return 0;
        }
    }
    
    logger().debug() << "[WorkingMemory] Running memory cleanup (force=" << force_cleanup << ")";
    
    size_t removed_count = 0;
    std::vector<Handle> to_remove;
    
    // Apply temporal decay first
    applyTemporalDecay();
    
    // Identify items for removal
    for (const auto& memory_item : _memory_buffer) {
        bool should_remove = false;
        
        // Check importance threshold
        if (memory_item->importance < _importance_threshold) {
            should_remove = true;
        }
        
        // Check retention time
        auto age = now - memory_item->timestamp;
        if (age > _max_retention_time) {
            should_remove = true;
        }
        
        if (should_remove) {
            to_remove.push_back(memory_item->atom);
        }
    }
    
    // Remove identified items
    for (Handle atom : to_remove) {
        if (removeItem(atom)) {
            removed_count++;
        }
    }
    
    // Enforce capacity limits
    removed_count += enforceCapacityLimits();
    
    // Compact memory if significant items were removed
    if (removed_count > _max_capacity / 10) {
        compactMemory();
    }
    
    _last_cleanup = now;
    _cleanup_running.store(false);
    
    logger().info() << "[WorkingMemory] Cleanup complete: removed " << removed_count 
                   << " items, current size=" << _memory_buffer.size();
    
    return removed_count;
}

void WorkingMemory::applyTemporalDecay()
{
    auto now = std::chrono::steady_clock::now();
    
    for (auto& memory_item : _memory_buffer) {
        double decay_factor = calculateDecayFactor(memory_item);
        double old_importance = memory_item->importance;
        
        memory_item->importance *= decay_factor;
        
        // Update indices if importance changed significantly
        if (std::abs(old_importance - memory_item->importance) > 0.01) {
            updateIndicesForImportanceChange(memory_item, old_importance);
        }
    }
    
    logger().debug() << "[WorkingMemory] Applied temporal decay to " << _memory_buffer.size() << " items";
}

void WorkingMemory::clear()
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    size_t initial_size = _memory_buffer.size();
    
    _memory_buffer.clear();
    _memory_index.clear();
    _context_index.clear();
    _importance_index.clear();
    
    logger().info() << "[WorkingMemory] Cleared all " << initial_size << " items from memory";
    
    // Update AtomSpace representation
    updateAtomSpaceRepresentation();
}

void WorkingMemory::compactMemory()
{
    // Remove gaps in memory buffer and rebuild indices
    std::vector<std::shared_ptr<MemoryItem>> valid_items;
    
    for (const auto& item : _memory_buffer) {
        if (item && item->atom) {
            valid_items.push_back(item);
        }
    }
    
    _memory_buffer = std::deque<std::shared_ptr<MemoryItem>>(valid_items.begin(), valid_items.end());
    
    logger().debug() << "[WorkingMemory] Compacted memory, retained " << valid_items.size() << " valid items";
}

// ===================================================================
// Configuration and capacity management
// ===================================================================

void WorkingMemory::setMaxCapacity(size_t capacity)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    size_t old_capacity = _max_capacity;
    _max_capacity = capacity;
    
    logger().info() << "[WorkingMemory] Changed max capacity from " << old_capacity 
                   << " to " << capacity;
    
    // Enforce new capacity immediately if needed
    if (_memory_buffer.size() > _max_capacity) {
        size_t removed = enforceCapacityLimits();
        logger().info() << "[WorkingMemory] Removed " << removed 
                       << " items to fit new capacity";
    }
}

size_t WorkingMemory::getCurrentSize() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _memory_buffer.size();
}

bool WorkingMemory::isAtCapacity() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _memory_buffer.size() >= _max_capacity;
}

void WorkingMemory::setImportanceThreshold(double threshold)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    double old_threshold = _importance_threshold;
    _importance_threshold = threshold;
    
    logger().info() << "[WorkingMemory] Changed importance threshold from " 
                   << old_threshold << " to " << threshold;
}

// ===================================================================
// AtomSpace integration
// ===================================================================

void WorkingMemory::createAtomSpaceRepresentation()
{
    if (!_atomspace) {
        return;
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    // Create structured representation in AtomSpace
    HandleSeq working_memory_contents;
    
    for (const auto& memory_item : _memory_buffer) {
        if (memory_item && memory_item->atom) {
            working_memory_contents.push_back(memory_item->atom);
        }
    }
    
    // Update working memory root with current contents
    if (!working_memory_contents.empty()) {
        Handle memory_set = _atomspace->add_link(SET_LINK, std::move(working_memory_contents));
        
        // Link to working memory root
        HandleSeq evaluation_seq;
        evaluation_seq.push_back(_working_memory_root);
        evaluation_seq.push_back(memory_set);
        
        _atomspace->add_link(EVALUATION_LINK, std::move(evaluation_seq));
    }
    
    logger().debug() << "[WorkingMemory] Created AtomSpace representation with " 
                    << working_memory_contents.size() << " items";
}

void WorkingMemory::updateAtomSpaceRepresentation()
{
    // For now, recreate the representation
    // Could be optimized to do incremental updates
    createAtomSpaceRepresentation();
}

// ===================================================================
// Performance monitoring and statistics
// ===================================================================

std::map<std::string, double> WorkingMemory::getPerformanceStats() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - _creation_time);
    
    std::map<std::string, double> stats;
    
    stats["hit_rate"] = getHitRate();
    stats["current_size"] = static_cast<double>(_memory_buffer.size());
    stats["max_capacity"] = static_cast<double>(_max_capacity);
    stats["capacity_utilization"] = static_cast<double>(_memory_buffer.size()) / _max_capacity;
    stats["total_accesses"] = static_cast<double>(_access_count);
    stats["total_hits"] = static_cast<double>(_hit_count);
    stats["total_misses"] = static_cast<double>(_miss_count);
    stats["total_operations"] = static_cast<double>(_total_operations);
    stats["uptime_seconds"] = static_cast<double>(uptime.count());
    
    if (uptime.count() > 0) {
        stats["operations_per_second"] = static_cast<double>(_total_operations) / uptime.count();
    }
    
    return stats;
}

double WorkingMemory::getHitRate() const
{
    size_t total_accesses = _access_count;
    if (total_accesses == 0) {
        return 0.0;
    }
    
    return static_cast<double>(_hit_count) / total_accesses;
}

void WorkingMemory::resetPerformanceCounters()
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    _access_count = 0;
    _hit_count = 0;
    _miss_count = 0;
    _total_operations = 0;
    _last_performance_reset = std::chrono::steady_clock::now();
    
    logger().info() << "[WorkingMemory] Performance counters reset";
}

std::map<std::string, size_t> WorkingMemory::getMemoryUsage() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    std::map<std::string, size_t> usage;
    
    usage["memory_buffer_size"] = _memory_buffer.size();
    usage["memory_index_size"] = _memory_index.size();
    usage["context_index_size"] = _context_index.size();
    usage["importance_index_size"] = _importance_index.size();
    
    // Estimate memory usage
    size_t estimated_bytes = 0;
    estimated_bytes += _memory_buffer.size() * sizeof(std::shared_ptr<MemoryItem>);
    estimated_bytes += _memory_index.size() * (sizeof(Handle) + sizeof(std::shared_ptr<MemoryItem>));
    estimated_bytes += _context_index.size() * (sizeof(std::string) + sizeof(std::shared_ptr<MemoryItem>));
    estimated_bytes += _importance_index.size() * (sizeof(double) + sizeof(std::shared_ptr<MemoryItem>));
    
    usage["estimated_memory_bytes"] = estimated_bytes;
    
    return usage;
}

// ===================================================================
// Debug and diagnostic operations
// ===================================================================

void WorkingMemory::printMemoryContents(const std::string& log_level) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    std::ostringstream ss;
    ss << "[WorkingMemory] Current contents (" << _memory_buffer.size() << " items):\n";
    
    for (const auto& memory_item : _memory_buffer) {
        if (memory_item && memory_item->atom) {
            auto age = std::chrono::steady_clock::now() - memory_item->timestamp;
            auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(age);
            
            ss << "  " << memory_item->atom->to_string() 
               << " [importance=" << std::fixed << std::setprecision(3) << memory_item->importance
               << ", context=" << memory_item->context
               << ", age=" << age_seconds.count() << "s"
               << ", accesses=" << memory_item->access_count << "]\n";
        }
    }
    
    if (log_level == "info") {
        logger().info() << ss.str();
    } else if (log_level == "warn") {
        logger().warn() << ss.str();
    } else if (log_level == "error") {
        logger().error() << ss.str();
    } else {
        logger().debug() << ss.str();
    }
}

bool WorkingMemory::validateMemoryConsistency() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    bool is_consistent = true;
    
    // Check that all items in buffer are in index
    for (const auto& memory_item : _memory_buffer) {
        if (memory_item && memory_item->atom) {
            auto it = _memory_index.find(memory_item->atom);
            if (it == _memory_index.end() || it->second != memory_item) {
                logger().error() << "[WorkingMemory] Inconsistency: item in buffer not in index: " 
                                << memory_item->atom->to_string();
                is_consistent = false;
            }
        }
    }
    
    // Check that all items in index are in buffer
    for (const auto& pair : _memory_index) {
        auto memory_item = pair.second;
        auto it = std::find(_memory_buffer.begin(), _memory_buffer.end(), memory_item);
        if (it == _memory_buffer.end()) {
            logger().error() << "[WorkingMemory] Inconsistency: item in index not in buffer: " 
                            << pair.first->to_string();
            is_consistent = false;
        }
    }
    
    if (is_consistent) {
        logger().debug() << "[WorkingMemory] Memory consistency check passed";
    }
    
    return is_consistent;
}

std::string WorkingMemory::getItemInfo(Handle atom) const
{
    if (!atom) {
        return "Invalid atom";
    }
    
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    
    auto it = _memory_index.find(atom);
    if (it == _memory_index.end()) {
        return "Item not found in working memory";
    }
    
    auto memory_item = it->second;
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - memory_item->timestamp);
    auto last_access_age = std::chrono::duration_cast<std::chrono::seconds>(now - memory_item->last_access);
    
    std::ostringstream ss;
    ss << "WorkingMemory Item Information:\n"
       << "  Atom: " << atom->to_string() << "\n"
       << "  Importance: " << std::fixed << std::setprecision(3) << memory_item->importance << "\n"
       << "  Context: " << memory_item->context << "\n"
       << "  Age: " << age.count() << " seconds\n"
       << "  Last Access: " << last_access_age.count() << " seconds ago\n"
       << "  Access Count: " << memory_item->access_count << "\n"
       << "  Decay Rate: " << memory_item->decay_rate;
    
    return ss.str();
}

// ===================================================================
// Private helper methods
// ===================================================================

void WorkingMemory::initializeAtomSpaceStructures()
{
    if (!_atomspace) {
        logger().warn() << "[WorkingMemory] No AtomSpace available for initialization";
        return;
    }
    
    // Create root nodes for working memory organization
    _working_memory_root = _atomspace->add_node(CONCEPT_NODE, "WorkingMemoryRoot");
    _context_space = _atomspace->add_node(CONCEPT_NODE, "ContextSpace");
    _active_goals = _atomspace->add_node(CONCEPT_NODE, "ActiveGoals");
    _recent_percepts = _atomspace->add_node(CONCEPT_NODE, "RecentPercepts");
    _temporary_conclusions = _atomspace->add_node(CONCEPT_NODE, "TemporaryConclusions");
    
    logger().info() << "[WorkingMemory] Initialized AtomSpace structures";
}

void WorkingMemory::removeFromIndices(std::shared_ptr<MemoryItem> item)
{
    // Remove from context index
    auto context_range = _context_index.equal_range(item->context);
    for (auto it = context_range.first; it != context_range.second; ++it) {
        if (it->second == item) {
            _context_index.erase(it);
            break;
        }
    }
    
    // Remove from importance index
    auto importance_range = _importance_index.equal_range(item->importance);
    for (auto it = importance_range.first; it != importance_range.second; ++it) {
        if (it->second == item) {
            _importance_index.erase(it);
            break;
        }
    }
}

void WorkingMemory::addToIndices(std::shared_ptr<MemoryItem> item)
{
    _context_index.emplace(item->context, item);
    _importance_index.emplace(item->importance, item);
}

size_t WorkingMemory::enforceCapacityLimits()
{
    if (_memory_buffer.size() < _max_capacity) {
        return 0;
    }

    // Remove enough items to get strictly below capacity (room for a new insert).
    // Caller must already hold _memory_mutex; mutate structures directly.
    size_t target = (_max_capacity == 0) ? 0 : _max_capacity - 1;
    if (_memory_buffer.size() <= target) {
        return 0;
    }

    size_t removed_count = 0;
    while (_memory_buffer.size() > target && !_importance_index.empty()) {
        auto imp_it = _importance_index.begin();
        auto victim = imp_it->second;
        if (!victim || !victim->atom) {
            _importance_index.erase(imp_it);
            continue;
        }
        Handle victim_atom = victim->atom;
        removeFromIndices(victim);
        _memory_index.erase(victim_atom);
        auto buffer_it = std::find(_memory_buffer.begin(), _memory_buffer.end(), victim);
        if (buffer_it != _memory_buffer.end()) {
            _memory_buffer.erase(buffer_it);
        }
        ++removed_count;
    }

    logger().debug() << "[WorkingMemory] Enforced capacity limits: removed "
                    << removed_count << " items";

    return removed_count;
}

double WorkingMemory::calculateDecayFactor(const std::shared_ptr<MemoryItem>& item) const
{
    auto now = std::chrono::steady_clock::now();
    auto time_since_access = std::chrono::duration_cast<std::chrono::seconds>(now - item->last_access);
    
    // Exponential decay: decay_factor = e^(-decay_rate * time)
    double time_hours = time_since_access.count() / 3600.0;
    double decay_factor = std::exp(-item->decay_rate * time_hours);
    
    return std::max(0.01, decay_factor);  // Minimum decay factor to prevent complete loss
}

void WorkingMemory::updateIndicesForImportanceChange(std::shared_ptr<MemoryItem> item, double old_importance)
{
    // Remove old importance entry
    auto old_range = _importance_index.equal_range(old_importance);
    for (auto it = old_range.first; it != old_range.second; ++it) {
        if (it->second == item) {
            _importance_index.erase(it);
            break;
        }
    }
    
    // Add new importance entry
    _importance_index.emplace(item->importance, item);
}
