/*
 * opencog/agentzero/memory/LongTermMemory.cpp
 * src/LongTermMemory.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LongTermMemory Implementation
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-003
 */

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_ATTENTION_BANK
#include <opencog/attentionbank/avalue/AttentionValue.h>
#endif

#include "opencog/agentzero/memory/LongTermMemory.h"

using namespace opencog;
using namespace opencog::agentzero::memory;

// Constructor with AtomSpace and configuration
LongTermMemory::LongTermMemory(AtomSpacePtr atomspace, const MemoryConfig& config)
    : _atomspace(atomspace)
    , _config(config)
    , _consolidation_running(false)
    , _shutdown_requested(false)
{
    if (!_atomspace) {
        throw std::runtime_error("LongTermMemory requires valid AtomSpace");
    }
    
    logger().info() << "[LongTermMemory] Initializing with default storage path";
}

// Constructor with AtomSpace, storage path and configuration
LongTermMemory::LongTermMemory(AtomSpacePtr atomspace,
                              const std::string& storage_path,
                              const MemoryConfig& config)
    : _atomspace(atomspace)
    , _config(config)
    , _consolidation_running(false)
    , _shutdown_requested(false)
{
    if (!_atomspace) {
        throw std::runtime_error("LongTermMemory requires valid AtomSpace");
    }
    
    _config.persistence_directory = storage_path;
    logger().info() << "[LongTermMemory] Initializing with storage path: " << storage_path;
}

// Destructor
LongTermMemory::~LongTermMemory()
{
    logger().info() << "[LongTermMemory] Shutting down...";
    shutdown();
}

// Initialize the long-term memory system
bool LongTermMemory::initialize()
{
    try {
        logger().info() << "[LongTermMemory] Starting initialization...";

        // Create storage directory if it doesn't exist
        std::filesystem::create_directories(_config.persistence_directory);

        // Initialize persistent storage (no-op when atomspace-rocks unavailable)
        initializePersistentStorage();

        // Initialize memory structures
        initializeMemoryStructures();

        // Start background tasks
        startBackgroundTasks();

        logger().info() << "[LongTermMemory] Initialization complete";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Initialization failed: " << e.what();
        return false;
    }
}

// Initialize persistent storage using RocksDB
void LongTermMemory::initializePersistentStorage()
{
#ifdef HAVE_ATOMSPACE_ROCKS
    try {
        std::string rocks_uri = "rocks://" + _config.persistence_directory + "/atomspace.db";
        logger().info() << "[LongTermMemory] Initializing RocksDB storage: " << rocks_uri;

        _persistent_storage = std::make_unique<RocksStorage>(rocks_uri);

        if (!_persistent_storage) {
            throw std::runtime_error("Failed to create RocksStorage instance");
        }

        // Open the storage
        _persistent_storage->open();

        logger().info() << "[LongTermMemory] RocksDB storage initialized successfully";

    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to initialize persistent storage: " << e.what();
        throw;
    }
#else
    logger().info() << "[LongTermMemory] atomspace-rocks not available; persistence disabled";
#endif
}

// Initialize memory data structures
void LongTermMemory::initializeMemoryStructures()
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    
    logger().info() << "[LongTermMemory] Initializing memory structures...";
    
    // Clear existing data structures
    _memory_records.clear();
#ifdef HAVE_ATTENTION_BANK
    _importance_cache.clear();
#endif
    _persistent_handles.clear();
    _context_index.clear();
    _temporal_index.clear();
    _importance_index.clear();
    _keyword_index.clear();
    _access_cache.clear();
    _dirty_handles.clear();
    
    // Initialize statistics
    _statistics = MemoryStatistics();
    _consolidation_status = ConsolidationStatus();
    
    logger().info() << "[LongTermMemory] Memory structures initialized";
}

// Start background tasks (consolidation, backup)
void LongTermMemory::startBackgroundTasks()
{
    logger().info() << "[LongTermMemory] Starting background tasks...";
    
    _shutdown_requested = false;
    
    // Start consolidation thread
    _consolidation_thread = std::thread(&LongTermMemory::consolidationWorker, this);
    
    // Start backup thread
    _backup_thread = std::thread(&LongTermMemory::backupWorker, this);
    
    logger().info() << "[LongTermMemory] Background tasks started";
}

// Stop background tasks
void LongTermMemory::stopBackgroundTasks()
{
    logger().info() << "[LongTermMemory] Stopping background tasks...";
    
    _shutdown_requested = true;
    _consolidation_cv.notify_all();

    // Wait for consolidation thread to finish
    if (_consolidation_thread.joinable()) {
        _consolidation_thread.join();
    }

    // Wait for backup thread to finish
    if (_backup_thread.joinable()) {
        _backup_thread.join();
    }
    
    logger().info() << "[LongTermMemory] Background tasks stopped";
}

// Graceful shutdown
bool LongTermMemory::shutdown()
{
    try {
        logger().info() << "[LongTermMemory] Starting graceful shutdown...";

        // Stop background tasks
        stopBackgroundTasks();

        // Flush any remaining dirty handles
        flushDirtyHandles();

#ifdef HAVE_ATOMSPACE_ROCKS
        // Close persistent storage
        if (_persistent_storage) {
            _persistent_storage->close();
        }
#endif

        logger().info() << "[LongTermMemory] Shutdown complete";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Shutdown error: " << e.what();
        return false;
    }
}

// Store a handle in long-term memory
bool LongTermMemory::store(const Handle& handle,
                          MemoryImportance importance,
                          PersistenceLevel persistence_level,
                          const std::vector<ContextType>& contexts)
{
    if (handle == Handle::UNDEFINED) {
        logger().warn() << "[LongTermMemory] Attempted to store undefined handle";
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(_memory_mutex);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create or update memory record
        MemoryRecord& record = _memory_records[handle];
        record.handle = handle;
        record.importance = importance;
        record.persistence_level = persistence_level;
        record.contexts = contexts;
        record.last_modified_time = std::chrono::system_clock::now();
        record.modification_count++;
        
        // Update indices
        addToIndices(handle);

#ifdef HAVE_ATTENTION_BANK
        // Cache attention value for performance
        AttentionValuePtr av = getAttentionValue(handle);
        if (av) {
            _importance_cache[handle] = av;
        }
#endif
        
        // Mark for persistence if required
        if (persistence_level >= PersistenceLevel::MEDIUM_TERM) {
            _dirty_handles.insert(handle);
            _persistent_handles.insert(handle);
        }
        
        // Update statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        updateStatistics("store", duration);
        
        logger().debug() << "[LongTermMemory] Stored handle " << handle << " with importance " 
                        << static_cast<int>(importance);
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to store handle: " << e.what();
        return false;
    }
}

// Retrieve a handle from long-term memory
Handle LongTermMemory::retrieve(const Handle& handle)
{
    if (handle == Handle::UNDEFINED) {
        return Handle::UNDEFINED;
    }
    
    try {
        std::lock_guard<std::mutex> lock(_memory_mutex);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Check if handle is in active memory
        auto record_it = _memory_records.find(handle);
        if (record_it != _memory_records.end()) {
            // Update access information
            record_it->second.last_accessed_time = std::chrono::system_clock::now();
            record_it->second.access_count++;
            updateAccessCache(handle);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
            updateStatistics("retrieve_cached", duration);
            
            return handle;
        }
        
        // Try to load from persistence
        if (loadHandle(handle)) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
            updateStatistics("retrieve_loaded", duration);
            
            return handle;
        }
        
        logger().debug() << "[LongTermMemory] Handle " << handle << " not found";
        return Handle::UNDEFINED;
        
    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to retrieve handle: " << e.what();
        return Handle::UNDEFINED;
    }
}

// Check if a handle exists in memory
bool LongTermMemory::contains(const Handle& handle) const
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    
    // Check active memory
    if (_memory_records.find(handle) != _memory_records.end()) {
        return true;
    }
    
    // Check persistent handles set
    return _persistent_handles.find(handle) != _persistent_handles.end();
}

// Remove a handle from memory
bool LongTermMemory::remove(const Handle& handle, bool remove_from_persistence)
{
    if (handle == Handle::UNDEFINED) {
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(_memory_mutex);
        
        // Remove from active memory
        _memory_records.erase(handle);
#ifdef HAVE_ATTENTION_BANK
        _importance_cache.erase(handle);
#endif
        _dirty_handles.erase(handle);
        
        // Remove from indices
        removeFromIndices(handle);
        
        // Remove from persistence if requested
        if (remove_from_persistence) {
            _persistent_handles.erase(handle);
            removeFromPersistence(handle);
        }
        
        logger().debug() << "[LongTermMemory] Removed handle " << handle;
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to remove handle: " << e.what();
        return false;
    }
}

// Find memories by importance level
std::vector<Handle> LongTermMemory::findByImportance(MemoryImportance min_importance, size_t max_results)
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::vector<Handle> results;
    
    for (const auto& pair : _memory_records) {
        if (pair.second.importance >= min_importance) {
            results.push_back(pair.first);
            if (results.size() >= max_results) {
                break;
            }
        }
    }
    
    return results;
}

// Find memories by context type
std::vector<Handle> LongTermMemory::findByContext(ContextType context_type, size_t max_results)
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::vector<Handle> results;
    
    auto context_it = _context_index.find(context_type);
    if (context_it != _context_index.end()) {
        size_t count = 0;
        for (const Handle& handle : context_it->second) {
            results.push_back(handle);
            if (++count >= max_results) {
                break;
            }
        }
    }
    
    return results;
}

// Trigger memory consolidation
ConsolidationStatus LongTermMemory::consolidate(bool force)
{
    if (!force && _consolidation_running.load()) {
        logger().debug() << "[LongTermMemory] Consolidation already running";
        return _consolidation_status;
    }
    
    performMemoryConsolidation();
    return _consolidation_status;
}

// Perform memory consolidation
void LongTermMemory::performMemoryConsolidation()
{
    logger().info() << "[LongTermMemory] Starting memory consolidation...";
    
    _consolidation_running = true;
    auto start_time = std::chrono::system_clock::now();
    
    try {
        std::lock_guard<std::mutex> lock(_memory_mutex);
        
        // Identify consolidation candidates
        auto candidates = identifyConsolidationCandidates();
        
        size_t removed_count = 0;
        size_t consolidated_count = 0;
        
        // Process candidates
        for (const Handle& handle : candidates) {
            if (shouldRetainMemory(handle)) {
                // Consolidate but keep in memory
                persistHandle(handle, true);
                consolidated_count++;
            } else {
                // Remove from active memory but keep in persistence if important
                auto record_it = _memory_records.find(handle);
                if (record_it != _memory_records.end()) {
                    if (record_it->second.importance >= MemoryImportance::MEDIUM) {
                        persistHandle(handle, true);
                    }
                    _memory_records.erase(record_it);
                    removed_count++;
                }
            }
        }
        
        // Update consolidation status
        _consolidation_status.total_memories = _memory_records.size();
        _consolidation_status.consolidated_memories = consolidated_count;
        _consolidation_status.removed_memories = removed_count;
        _consolidation_status.last_consolidation = start_time;
        
        auto end_time = std::chrono::system_clock::now();
        _consolidation_status.consolidation_time = 
            std::chrono::duration_cast<Duration>(end_time - start_time);
        
        logger().info() << "[LongTermMemory] Consolidation complete. Consolidated: " 
                       << consolidated_count << ", Removed: " << removed_count;
        
    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Consolidation failed: " << e.what();
    }
    
    _consolidation_running = false;
}

// Get memory statistics
MemoryStatistics LongTermMemory::getStatistics() const
{
    std::lock_guard<std::mutex> lock(_statistics_mutex);
    return _statistics;
}

// Get current configuration
MemoryConfig LongTermMemory::getConfig() const
{
    return _config;
}

// Get system status
std::string LongTermMemory::getSystemStatus() const
{
    std::ostringstream oss;
    
    std::lock_guard<std::mutex> lock(_memory_mutex);
    
    oss << "=== LongTermMemory System Status ===" << std::endl;
    oss << "Active memories: " << _memory_records.size() << std::endl;
    oss << "Persistent handles: " << _persistent_handles.size() << std::endl;
    oss << "Dirty handles: " << _dirty_handles.size() << std::endl;
    oss << "Cache size: " << _access_cache.size() << std::endl;
    oss << "Storage directory: " << _config.persistence_directory << std::endl;
    oss << "Consolidation running: " << (_consolidation_running.load() ? "Yes" : "No") << std::endl;
    oss << "Last consolidation: " << std::chrono::duration_cast<std::chrono::hours>(
        std::chrono::system_clock::now() - _consolidation_status.last_consolidation).count() 
        << " hours ago" << std::endl;
    
    return oss.str();
}

// Check system health
bool LongTermMemory::isHealthy() const
{
#ifdef HAVE_ATOMSPACE_ROCKS
    // Check if persistent storage is available
    if (!_persistent_storage) {
        return false;
    }
#endif

    // Check if shutdown was requested
    if (_shutdown_requested.load()) {
        return false;
    }

    // Check memory usage limits
    std::lock_guard<std::mutex> lock(_memory_mutex);
    if (_memory_records.size() > _config.max_memory_count * 1.1) {  // 10% tolerance
        return false;
    }

    return true;
}

// === Private Implementation Methods ===

// Persist a handle to storage
bool LongTermMemory::persistHandle(const Handle& handle, bool force)
{
#ifdef HAVE_ATOMSPACE_ROCKS
    if (!_persistent_storage || handle == Handle::UNDEFINED) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(_persistence_mutex);
        _persistent_storage->storeAtom(handle);
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to persist handle: " << e.what();
        return false;
    }
#else
    (void)handle; (void)force;
    return false;
#endif
}

// Load a handle from storage
bool LongTermMemory::loadHandle(const Handle& handle)
{
#ifdef HAVE_ATOMSPACE_ROCKS
    if (!_persistent_storage || handle == Handle::UNDEFINED) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(_persistence_mutex);
        Handle loaded = _persistent_storage->getAtom(handle);

        if (loaded != Handle::UNDEFINED) {
            // Create memory record for loaded handle
            MemoryRecord record(loaded);
            record.importance = calculateMemoryImportance(loaded);
            record.last_accessed_time = std::chrono::system_clock::now();

            _memory_records[loaded] = record;
            addToIndices(loaded);

            return true;
        }

        return false;

    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to load handle: " << e.what();
        return false;
    }
#else
    (void)handle;
    return false;
#endif
}

// Add handle to indices
void LongTermMemory::addToIndices(const Handle& handle)
{
    auto record_it = _memory_records.find(handle);
    if (record_it == _memory_records.end()) {
        return;
    }
    
    const MemoryRecord& record = record_it->second;
    
    // Add to importance index
    _importance_index[record.importance].insert(handle);
    
    // Add to temporal index
    _temporal_index[record.created_time].insert(handle);
    
    // Add to context indices
    for (ContextType context : record.contexts) {
        _context_index[context].insert(handle);
    }

    // Index node name for keyword search
    if (handle && handle->is_node()) {
        _keyword_index.emplace(handle->get_name(), handle);
    }
}

// Remove handle from indices
void LongTermMemory::removeFromIndices(const Handle& handle)
{
    // Remove from all indices
    for (auto& importance_pair : _importance_index) {
        importance_pair.second.erase(handle);
    }
    
    for (auto& temporal_pair : _temporal_index) {
        temporal_pair.second.erase(handle);
    }
    
    for (auto& context_pair : _context_index) {
        context_pair.second.erase(handle);
    }
    
    // Remove from keyword index
    std::string atom_name = (handle && handle->is_node()) ? handle->get_name() : generateHandleKey(handle);
    auto keyword_range = _keyword_index.equal_range(atom_name);
    for (auto it = keyword_range.first; it != keyword_range.second; ++it) {
        if (it->second == handle) {
            _keyword_index.erase(it);
            break;
        }
    }
}

// Update access cache
void LongTermMemory::updateAccessCache(const Handle& handle)
{
    _access_cache[handle] = std::make_pair(std::chrono::system_clock::now(), static_cast<size_t>(1));
}

// Calculate memory importance based on attention values
MemoryImportance LongTermMemory::calculateMemoryImportance(const Handle& handle)
{
#ifdef HAVE_ATTENTION_BANK
    AttentionValuePtr av = getAttentionValue(handle);
    if (!av) {
        return MemoryImportance::LOW;
    }

    // Map attention values to importance levels
    double sti = av->getSTI();
    double lti = av->getLTI();
    double vlti = av->getVLTI();

    // Use a weighted combination of attention values
    double importance_score = (sti * 0.3) + (lti * 0.5) + (vlti * 0.2);

    if (importance_score >= 80) return MemoryImportance::CRITICAL;
    else if (importance_score >= 60) return MemoryImportance::HIGH;
    else if (importance_score >= 40) return MemoryImportance::MEDIUM;
    else if (importance_score >= 20) return MemoryImportance::LOW;
    else return MemoryImportance::MINIMAL;
#else
    (void)handle;
    return MemoryImportance::MEDIUM;
#endif
}

#ifdef HAVE_ATTENTION_BANK
// Get attention value for handle
AttentionValuePtr LongTermMemory::getAttentionValue(const Handle& handle)
{
    if (handle == Handle::UNDEFINED) {
        return nullptr;
    }

    // Check cache first
    auto cache_it = _importance_cache.find(handle);
    if (cache_it != _importance_cache.end()) {
        return cache_it->second;
    }

    // Get from AtomSpace
    AttentionValuePtr av = AttentionValueCast(handle->getValue(AttentionValue::key()));
    if (av) {
        _importance_cache[handle] = av;
    }

    return av;
}
#endif

// Identify consolidation candidates
std::vector<Handle> LongTermMemory::identifyConsolidationCandidates()
{
    std::vector<Handle> candidates;
    auto now = std::chrono::system_clock::now();
    
    for (const auto& pair : _memory_records) {
        const Handle& handle = pair.first;
        const MemoryRecord& record = pair.second;
        
        // Consider for consolidation based on various criteria
        bool is_candidate = false;
        
        // Age-based criteria
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - record.created_time);
        if (age > std::chrono::hours(24) && record.importance <= MemoryImportance::LOW) {
            is_candidate = true;
        }
        
        // Access pattern criteria
        if (record.access_count < 2 && record.importance <= MemoryImportance::MEDIUM) {
            is_candidate = true;
        }
        
        if (is_candidate) {
            candidates.push_back(handle);
        }
    }
    
    return candidates;
}

// Determine if memory should be retained
bool LongTermMemory::shouldRetainMemory(const Handle& handle)
{
    auto record_it = _memory_records.find(handle);
    if (record_it == _memory_records.end()) {
        return false;
    }
    
    const MemoryRecord& record = record_it->second;
    
    // Always retain critical and high importance memories
    if (record.importance >= MemoryImportance::HIGH) {
        return true;
    }
    
    // Retain frequently accessed memories
    if (record.access_count > 10) {
        return true;
    }
    
    // Retain recent memories
    auto age = std::chrono::duration_cast<std::chrono::hours>(
        std::chrono::system_clock::now() - record.created_time);
    if (age < std::chrono::hours(1)) {
        return true;
    }
    
    return false;
}

// Flush dirty handles to persistence
void LongTermMemory::flushDirtyHandles()
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    
    logger().debug() << "[LongTermMemory] Flushing " << _dirty_handles.size() << " dirty handles";
    
    for (const Handle& handle : _dirty_handles) {
        persistHandle(handle, true);
    }
    
    _dirty_handles.clear();
}

// Update statistics
void LongTermMemory::updateStatistics(const std::string& operation, Duration duration)
{
    std::lock_guard<std::mutex> lock(_statistics_mutex);
    
    if (operation == "store" || operation.find("retrieve") != std::string::npos) {
        if (operation == "store") {
            _statistics.write_operations++;
            // Update average write time (simple moving average)
            _statistics.average_write_time = 
                (_statistics.average_write_time + duration) / 2;
        } else {
            _statistics.read_operations++;
            if (operation == "retrieve_cached") {
                _statistics.cache_hits++;
            } else {
                _statistics.cache_misses++;
            }
            // Update average read time
            _statistics.average_read_time = 
                (_statistics.average_read_time + duration) / 2;
        }
    }
}

// Consolidation worker thread
void LongTermMemory::consolidationWorker()
{
    logger().info() << "[LongTermMemory] Consolidation worker started";

    while (!_shutdown_requested.load()) {
        try {
            // Interruptible wait — wake at least every second so shutdown is responsive
            auto deadline = std::chrono::steady_clock::now() + _config.consolidation_interval;
            while (!_shutdown_requested.load() &&
                   std::chrono::steady_clock::now() < deadline) {
                std::unique_lock<std::mutex> lk(_memory_mutex);
                _consolidation_cv.wait_for(lk, std::chrono::seconds(1));
            }

            if (!_shutdown_requested.load()) {
                performMemoryConsolidation();
            }

        } catch (const std::exception& e) {
            logger().error() << "[LongTermMemory] Consolidation worker error: " << e.what();
        }
    }

    logger().info() << "[LongTermMemory] Consolidation worker stopped";
}

// Backup worker thread
void LongTermMemory::backupWorker()
{
    logger().info() << "[LongTermMemory] Backup worker started";

    while (!_shutdown_requested.load()) {
        try {
            auto deadline = std::chrono::steady_clock::now() + _config.backup_interval;
            while (!_shutdown_requested.load() &&
                   std::chrono::steady_clock::now() < deadline) {
                std::unique_lock<std::mutex> lk(_memory_mutex);
                _consolidation_cv.wait_for(lk, std::chrono::seconds(1));
            }

            if (!_shutdown_requested.load() && _config.enable_incremental_backup) {
                std::string backup_path = _config.persistence_directory + "/backup_" +
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                    ".db";
                createBackup(backup_path);
            }

        } catch (const std::exception& e) {
            logger().error() << "[LongTermMemory] Backup worker error: " << e.what();
        }
    }

    logger().info() << "[LongTermMemory] Backup worker stopped";
}

// Create backup
bool LongTermMemory::createBackup(const std::string& backup_path)
{
    try {
        logger().info() << "[LongTermMemory] Creating backup: " << backup_path;

        // Flush any pending data
        flushDirtyHandles();

        std::filesystem::create_directories(
            std::filesystem::path(backup_path).parent_path());

        // Prefer copying rocks DB when present; otherwise write a metadata snapshot.
        const std::string db_path = _config.persistence_directory + "/atomspace.db";
        if (std::filesystem::exists(db_path)) {
            std::filesystem::copy_file(
                db_path,
                backup_path,
                std::filesystem::copy_options::overwrite_existing
            );
        } else {
            std::ofstream out(backup_path);
            if (!out) {
                throw std::runtime_error("unable to open backup path");
            }
            out << "agentzero-memory-backup\n";
            out << "records=" << _memory_records.size() << "\n";
            out << "persistent=" << _persistent_handles.size() << "\n";
            for (const auto& pair : _memory_records) {
                out << generateHandleKey(pair.first) << "\n";
            }
        }

        logger().info() << "[LongTermMemory] Backup created successfully";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Backup creation failed: " << e.what();
        return false;
    }
}

bool LongTermMemory::restoreFromBackup(const std::string& backup_path)
{
    try {
        logger().info() << "[LongTermMemory] Restoring from backup: " << backup_path;
        std::filesystem::copy_file(
            backup_path,
            _config.persistence_directory + "/atomspace.db",
            std::filesystem::copy_options::overwrite_existing
        );
        logger().info() << "[LongTermMemory] Restore complete";
        return true;
    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Restore failed: " << e.what();
        return false;
    }
}

void LongTermMemory::removeFromPersistence(const Handle& handle)
{
#ifdef HAVE_ATOMSPACE_ROCKS
    if (!_persistent_storage || handle == Handle::UNDEFINED) {
        return;
    }
    try {
        std::lock_guard<std::mutex> lock(_persistence_mutex);
        _persistent_storage->removeAtom(handle);
    } catch (const std::exception& e) {
        logger().error() << "[LongTermMemory] Failed to remove from persistence: " << e.what();
    }
#else
    (void)handle;
#endif
}

bool LongTermMemory::updateImportance(const Handle& handle, MemoryImportance new_importance)
{
    if (handle == Handle::UNDEFINED) {
        return false;
    }
    std::lock_guard<std::mutex> lock(_memory_mutex);
    auto it = _memory_records.find(handle);
    if (it == _memory_records.end()) {
        return false;
    }
    _importance_index[it->second.importance].erase(handle);
    it->second.importance = new_importance;
    it->second.last_modified_time = std::chrono::system_clock::now();
    _importance_index[new_importance].insert(handle);
    return true;
}

std::vector<Handle> LongTermMemory::findByTimeRange(
    const TimePoint& start_time,
    const TimePoint& end_time,
    size_t max_results)
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::vector<Handle> results;
    auto it_begin = _temporal_index.lower_bound(start_time);
    auto it_end = _temporal_index.upper_bound(end_time);
    for (auto it = it_begin; it != it_end && results.size() < max_results; ++it) {
        for (const Handle& h : it->second) {
            results.push_back(h);
            if (results.size() >= max_results) break;
        }
    }
    return results;
}

std::vector<std::pair<Handle, double>> LongTermMemory::findSimilar(
    [[maybe_unused]] const Handle& reference_handle,
    size_t max_results,
    [[maybe_unused]] double similarity_threshold)
{
    // Basic implementation: return stored memories (similarity scoring requires PLN)
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::vector<std::pair<Handle, double>> results;
    size_t count = 0;
    for (const auto& pair : _memory_records) {
        if (count++ >= max_results) break;
        results.emplace_back(pair.first, 0.5);
    }
    return results;
}

std::vector<Handle> LongTermMemory::search(
    const std::vector<std::string>& keywords,
    [[maybe_unused]] RetrievalMode mode,
    size_t max_results)
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::vector<Handle> results;
    for (const auto& kw : keywords) {
        auto range = _keyword_index.equal_range(kw);
        for (auto it = range.first; it != range.second && results.size() < max_results; ++it) {
            results.push_back(it->second);
        }
    }
    return results;
}

void LongTermMemory::setConsolidationStrategy(ConsolidationStrategy strategy)
{
    _config.consolidation_strategy = strategy;
}

ConsolidationStatus LongTermMemory::getConsolidationStatus() const
{
    return _consolidation_status;
}

size_t LongTermMemory::flushToPersistence()
{
    flushDirtyHandles();
    return _persistent_handles.size();
}

size_t LongTermMemory::loadFromPersistence()
{
    // Without RocksDB, nothing to load
    return 0;
}

std::map<std::string, size_t> LongTermMemory::getMemoryUsage() const
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::map<std::string, size_t> usage;
    usage["active_records"] = _memory_records.size();
    usage["persistent_handles"] = _persistent_handles.size();
    usage["dirty_handles"] = _dirty_handles.size();
    usage["cache_entries"] = _access_cache.size();
    return usage;
}

std::map<std::string, Duration> LongTermMemory::getPerformanceMetrics() const
{
    std::lock_guard<std::mutex> lock(_statistics_mutex);
    std::map<std::string, Duration> metrics;
    metrics["avg_read_time"] = _statistics.average_read_time;
    metrics["avg_write_time"] = _statistics.average_write_time;
    metrics["avg_consolidation_time"] = _statistics.average_consolidation_time;
    return metrics;
}

void LongTermMemory::resetStatistics()
{
    std::lock_guard<std::mutex> lock(_statistics_mutex);
    _statistics = MemoryStatistics();
}

bool LongTermMemory::updateConfig(const MemoryConfig& new_config)
{
    _config = new_config;
    return true;
}

bool LongTermMemory::backup(const std::string& backup_path)
{
    std::string path = backup_path.empty()
        ? _config.persistence_directory + "/backup.db"
        : backup_path;
    return createBackup(path);
}

bool LongTermMemory::restore(const std::string& backup_path)
{
    return restoreFromBackup(backup_path);
}

void LongTermMemory::updateMemoryImportance(const Handle& handle)
{
    if (handle == Handle::UNDEFINED) return;
    std::lock_guard<std::mutex> lock(_memory_mutex);
    auto it = _memory_records.find(handle);
    if (it != _memory_records.end()) {
        MemoryImportance new_imp = calculateMemoryImportance(handle);
        _importance_index[it->second.importance].erase(handle);
        it->second.importance = new_imp;
        _importance_index[new_imp].insert(handle);
    }
}

void LongTermMemory::updateIndices(const Handle& handle)
{
    removeFromIndices(handle);
    addToIndices(handle);
}

void LongTermMemory::rebuildIndices()
{
    _importance_index.clear();
    _temporal_index.clear();
    _context_index.clear();
    _keyword_index.clear();
    for (const auto& pair : _memory_records) {
        addToIndices(pair.first);
    }
}

void LongTermMemory::cleanupAccessCache()
{
    auto now = std::chrono::system_clock::now();
    std::vector<Handle> to_remove;
    for (const auto& entry : _access_cache) {
        auto age = now - entry.second.first;
        if (age > _config.cache_expiry_time) {
            to_remove.push_back(entry.first);
        }
    }
    for (const Handle& h : to_remove) {
        _access_cache.erase(h);
    }
}

bool LongTermMemory::isInCache(const Handle& handle) const
{
    return _access_cache.find(handle) != _access_cache.end();
}

void LongTermMemory::evictFromCache(const Handle& handle)
{
    _access_cache.erase(handle);
}

void LongTermMemory::logMemoryStatus()
{
    logger().info() << getSystemStatus();
}

std::string LongTermMemory::generateHandleKey(const Handle& handle)
{
    if (handle == Handle::UNDEFINED) return "";
    std::ostringstream oss;
    oss << static_cast<const void*>(handle.get());
    return oss.str();
}

std::vector<Handle> LongTermMemory::getHandlesByImportance(MemoryImportance min_importance)
{
    std::lock_guard<std::mutex> lock(_memory_mutex);
    std::vector<Handle> results;
    for (const auto& pair : _memory_records) {
        if (pair.second.importance >= min_importance) {
            results.push_back(pair.first);
        }
    }
    return results;
}
