/*
 * opencog/agentzero/tools/ResourceManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ResourceManager for Resource Optimization
 * Manages computational and physical resources for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 * Task ID: AZ-RESOURCE-001
 */

#ifndef _OPENCOG_AGENTZERO_RESOURCEMANAGER_H
#define _OPENCOG_AGENTZERO_RESOURCEMANAGER_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <chrono>
#include <mutex>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace tools {

/**
 * ResourceType - Types of resources that can be managed
 */
enum class ResourceType {
    CPU,                    // CPU processing resources
    MEMORY,                 // Memory/RAM resources
    DISK,                   // Disk storage resources
    NETWORK,                // Network bandwidth resources
    GPU,                    // GPU computing resources
    ATOMSPACE,              // AtomSpace node/link capacity
    TOOL_INSTANCE,          // Tool execution instances
    CUSTOM                  // Custom resource types
};

/**
 * ResourceStatus - Status of a resource
 */
enum class ResourceStatus {
    AVAILABLE,              // Resource is available for allocation
    ALLOCATED,              // Resource is allocated to a task
    EXHAUSTED,              // Resource is fully utilized
    OVERLOADED,             // Resource is over capacity
    ERROR,                  // Resource is in error state
    OFFLINE                 // Resource is offline/unavailable
};

/**
 * ResourceAllocation - Represents an allocation of resources
 */
class ResourceAllocation {
private:
    std::string _allocation_id;
    std::string _requester_id;
    ResourceType _resource_type;
    double _amount;
    std::chrono::steady_clock::time_point _allocation_time;
    std::chrono::steady_clock::time_point _expiration_time;
    bool _is_active;
    std::map<std::string, std::string> _metadata;

public:
    ResourceAllocation(const std::string& allocation_id,
                      const std::string& requester_id,
                      ResourceType resource_type,
                      double amount);
    
    // Getters
    const std::string& getAllocationId() const { return _allocation_id; }
    const std::string& getRequesterId() const { return _requester_id; }
    ResourceType getResourceType() const { return _resource_type; }
    double getAmount() const { return _amount; }
    bool isActive() const { return _is_active; }
    std::chrono::steady_clock::time_point getAllocationTime() const { return _allocation_time; }
    std::chrono::steady_clock::time_point getExpirationTime() const { return _expiration_time; }
    
    // Setters
    void setExpirationTime(std::chrono::steady_clock::time_point exp_time) { _expiration_time = exp_time; }
    void setActive(bool active) { _is_active = active; }
    void setMetadata(const std::string& key, const std::string& value) { _metadata[key] = value; }
    std::string getMetadata(const std::string& key) const;
    
    // Utility
    bool isExpired() const;
    double getDurationSeconds() const;
    std::string toString() const;
};

/**
 * ResourcePool - Manages a pool of specific resource type
 */
class ResourcePool {
private:
    ResourceType _resource_type;
    std::string _resource_name;
    double _total_capacity;
    double _available_capacity;
    double _allocated_capacity;
    double _peak_usage;
    ResourceStatus _status;
    std::vector<std::shared_ptr<ResourceAllocation>> _allocations;
    std::map<std::string, double> _usage_history;
    std::mutex _pool_mutex;
    
    // Resource limits
    double _warning_threshold;      // Percentage for warning
    double _critical_threshold;     // Percentage for critical state
    
    // Statistics
    int _total_allocations;
    int _failed_allocations;
    int _successful_allocations;

public:
    ResourcePool(ResourceType type, const std::string& name, double capacity);
    
    // Core operations
    std::shared_ptr<ResourceAllocation> allocate(const std::string& requester_id, 
                                                  double amount, 
                                                  double duration_seconds = 0.0);
    bool deallocate(const std::string& allocation_id);
    bool deallocateForRequester(const std::string& requester_id);
    
    // Resource management
    void setCapacity(double capacity);
    double getTotalCapacity() const { return _total_capacity; }
    double getAvailableCapacity() const { return _available_capacity; }
    double getAllocatedCapacity() const { return _allocated_capacity; }
    double getUsagePercentage() const;
    double getPeakUsage() const { return _peak_usage; }
    
    // Resource type
    ResourceType getResourceType() const { return _resource_type; }
    const std::string& getResourceName() const { return _resource_name; }
    
    // Status management
    ResourceStatus getStatus() const { return _status; }
    void updateStatus();
    bool isAvailable() const { return _status == ResourceStatus::AVAILABLE; }
    bool isOverloaded() const { return _status == ResourceStatus::OVERLOADED; }
    
    // Allocation tracking
    std::vector<std::shared_ptr<ResourceAllocation>> getAllocations() const;
    int getAllocationCount() const { return _allocations.size(); }
    
    // Cleanup
    int cleanupExpiredAllocations();
    void releaseAll();
    
    // Statistics
    std::string getStatistics() const;
    int getTotalAllocationCount() const { return _total_allocations; }
    int getSuccessfulAllocationCount() const { return _successful_allocations; }
    int getFailedAllocationCount() const { return _failed_allocations; }
    double getSuccessRate() const;
    
    // Configuration
    void setThresholds(double warning_threshold, double critical_threshold);
};

/**
 * ResourceOptimizationStrategy - Strategy for resource optimization
 */
enum class OptimizationStrategy {
    FIRST_FIT,              // Allocate from first available resource
    BEST_FIT,               // Allocate from resource with least waste
    WORST_FIT,              // Allocate from resource with most space
    BALANCED,               // Balance allocation across all resources
    PRIORITY_BASED,         // Allocate based on requester priority
    ADAPTIVE                // Adaptive strategy based on usage patterns
};

/**
 * ResourceManager - Main class for managing computational and physical resources
 * 
 * Provides comprehensive resource management for Agent-Zero, including:
 * - CPU, memory, disk, network, GPU resource tracking
 * - AtomSpace capacity management
 * - Tool instance pooling
 * - Allocation and deallocation strategies
 * - Resource optimization algorithms
 * - Usage monitoring and statistics
 * - Integration with OpenCog AtomSpace
 * 
 * Usage Example:
 * ```
 * auto atomspace = std::make_shared<AtomSpace>();
 * auto manager = std::make_unique<ResourceManager>(atomspace);
 * 
 * // Create resource pools
 * manager->createResourcePool(ResourceType::CPU, "CPU_Pool", 100.0);
 * manager->createResourcePool(ResourceType::MEMORY, "Memory_Pool", 16384.0);
 * 
 * // Allocate resources
 * auto cpu_alloc = manager->allocateResource("task_1", ResourceType::CPU, 25.0, 60.0);
 * auto mem_alloc = manager->allocateResource("task_1", ResourceType::MEMORY, 2048.0);
 * 
 * // ... use resources ...
 * 
 * // Deallocate when done
 * manager->deallocateResource(cpu_alloc->getAllocationId());
 * manager->deallocateResourcesForRequester("task_1");
 * ```
 */
class ResourceManager {
private:
    // AtomSpace integration
    AtomSpacePtr _atomspace;
    Handle _manager_atom;
    
    // Resource pools
    std::map<ResourceType, std::vector<std::shared_ptr<ResourcePool>>> _resource_pools;
    std::map<std::string, std::shared_ptr<ResourcePool>> _named_pools;
    
    // Optimization
    OptimizationStrategy _optimization_strategy;
    bool _auto_cleanup_enabled;
    double _cleanup_interval_seconds;
    std::chrono::steady_clock::time_point _last_cleanup_time;
    
    // Statistics
    int _total_allocations;
    int _total_deallocations;
    int _failed_allocations;
    std::map<ResourceType, double> _total_usage_by_type;
    
    // Thread safety
    mutable std::mutex _manager_mutex;
    
    // Internal methods
    void initializeManagerAtom();
    std::shared_ptr<ResourcePool> selectPoolForAllocation(ResourceType type, 
                                                          double amount,
                                                          const std::string& requester_id);
    void updateAtomSpaceRepresentation();
    void recordAllocationInAtomSpace(const ResourceAllocation& allocation);
    void recordDeallocationInAtomSpace(const std::string& allocation_id);
    void performAutoCleanup();
    
public:
    /**
     * Constructor - Creates a ResourceManager instance
     * @param atomspace Optional AtomSpace for knowledge integration
     */
    explicit ResourceManager(AtomSpacePtr atomspace = nullptr);
    
    /**
     * Destructor - Cleans up all resources
     */
    virtual ~ResourceManager();
    
    // Resource pool management
    /**
     * Create a new resource pool
     * @param type Type of resource
     * @param name Name of the pool
     * @param capacity Total capacity of the pool
     * @return True if pool was created successfully
     */
    bool createResourcePool(ResourceType type, 
                           const std::string& name, 
                           double capacity);
    
    /**
     * Remove a resource pool
     * @param name Name of the pool to remove
     * @return True if pool was removed successfully
     */
    bool removeResourcePool(const std::string& name);
    
    /**
     * Get a resource pool by name
     * @param name Name of the pool
     * @return Shared pointer to the pool, or nullptr if not found
     */
    std::shared_ptr<ResourcePool> getResourcePool(const std::string& name) const;
    
    /**
     * Get all pools for a specific resource type
     * @param type Resource type
     * @return Vector of pools for that type
     */
    std::vector<std::shared_ptr<ResourcePool>> getPoolsForType(ResourceType type) const;
    
    // Resource allocation
    /**
     * Allocate resources for a requester
     * @param requester_id Unique identifier of the requester
     * @param type Type of resource to allocate
     * @param amount Amount of resource to allocate
     * @param duration_seconds Duration of allocation (0 = unlimited)
     * @return Allocation object if successful, nullptr otherwise
     */
    std::shared_ptr<ResourceAllocation> allocateResource(const std::string& requester_id,
                                                         ResourceType type,
                                                         double amount,
                                                         double duration_seconds = 0.0);
    
    /**
     * Allocate from specific pool
     * @param requester_id Unique identifier of the requester
     * @param pool_name Name of specific pool to allocate from
     * @param amount Amount to allocate
     * @param duration_seconds Duration of allocation
     * @return Allocation object if successful, nullptr otherwise
     */
    std::shared_ptr<ResourceAllocation> allocateFromPool(const std::string& requester_id,
                                                         const std::string& pool_name,
                                                         double amount,
                                                         double duration_seconds = 0.0);
    
    /**
     * Deallocate a specific allocation
     * @param allocation_id Unique allocation identifier
     * @return True if deallocation was successful
     */
    bool deallocateResource(const std::string& allocation_id);
    
    /**
     * Deallocate all resources for a requester
     * @param requester_id Unique identifier of the requester
     * @return Number of allocations deallocated
     */
    int deallocateResourcesForRequester(const std::string& requester_id);
    
    // Resource queries
    /**
     * Check if sufficient resources are available
     * @param type Resource type
     * @param amount Amount needed
     * @return True if resources are available
     */
    bool hasAvailableResources(ResourceType type, double amount) const;
    
    /**
     * Get total available capacity for a resource type
     * @param type Resource type
     * @return Total available capacity across all pools
     */
    double getTotalAvailableCapacity(ResourceType type) const;
    
    /**
     * Get total allocated capacity for a resource type
     * @param type Resource type
     * @return Total allocated capacity across all pools
     */
    double getTotalAllocatedCapacity(ResourceType type) const;
    
    /**
     * Get overall resource usage percentage
     * @param type Resource type
     * @return Usage percentage (0.0 to 100.0)
     */
    double getResourceUsage(ResourceType type) const;
    
    // Optimization
    /**
     * Set the optimization strategy
     * @param strategy Optimization strategy to use
     */
    void setOptimizationStrategy(OptimizationStrategy strategy);
    
    /**
     * Get current optimization strategy
     * @return Current strategy
     */
    OptimizationStrategy getOptimizationStrategy() const { return _optimization_strategy; }
    
    /**
     * Enable or disable automatic cleanup of expired allocations
     * @param enabled True to enable auto cleanup
     */
    void setAutoCleanupEnabled(bool enabled) { _auto_cleanup_enabled = enabled; }
    
    /**
     * Set cleanup interval
     * @param interval_seconds Interval in seconds
     */
    void setCleanupInterval(double interval_seconds) { _cleanup_interval_seconds = interval_seconds; }
    
    /**
     * Manually trigger cleanup of expired allocations
     * @return Number of allocations cleaned up
     */
    int cleanupExpiredAllocations();
    
    /**
     * Optimize resource allocation based on current strategy
     * @return True if optimization was successful
     */
    bool optimizeResourceAllocation();
    
    // AtomSpace integration
    /**
     * Set the AtomSpace for this manager
     * @param atomspace Shared pointer to AtomSpace
     */
    void setAtomSpace(AtomSpacePtr atomspace);
    
    /**
     * Get the AtomSpace for this manager
     * @return Shared pointer to AtomSpace
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    
    /**
     * Get the AtomSpace representation of this manager
     * @return Handle to manager atom
     */
    Handle getManagerAtom() const { return _manager_atom; }
    
    // Statistics and monitoring
    /**
     * Get comprehensive statistics
     * @return JSON string with all statistics
     */
    std::string getStatistics() const;
    
    /**
     * Get statistics for a specific resource type
     * @param type Resource type
     * @return JSON string with type-specific statistics
     */
    std::string getResourceTypeStatistics(ResourceType type) const;
    
    /**
     * Get total number of allocations
     * @return Total allocation count
     */
    int getTotalAllocationCount() const { return _total_allocations; }
    
    /**
     * Get total number of deallocations
     * @return Total deallocation count
     */
    int getTotalDeallocationCount() const { return _total_deallocations; }
    
    /**
     * Get number of failed allocations
     * @return Failed allocation count
     */
    int getFailedAllocationCount() const { return _failed_allocations; }
    
    /**
     * Get overall allocation success rate
     * @return Success rate as decimal (0.0 to 1.0)
     */
    double getSuccessRate() const;
    
    /**
     * Reset all statistics
     */
    void resetStatistics();
    
    /**
     * Get string representation of resource type
     * @param type Resource type enum
     * @return String representation
     */
    static std::string resourceTypeToString(ResourceType type);
    
    /**
     * Get string representation of resource status
     * @param status Resource status enum
     * @return String representation
     */
    static std::string resourceStatusToString(ResourceStatus status);
    
    /**
     * Get string representation of optimization strategy
     * @param strategy Optimization strategy enum
     * @return String representation
     */
    static std::string optimizationStrategyToString(OptimizationStrategy strategy);
};

} // namespace tools
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_RESOURCEMANAGER_H
