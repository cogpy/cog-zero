/*
 * src/ResourceManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ResourceManager Implementation
 * Manages computational and physical resources for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 * Task ID: AZ-RESOURCE-001
 */

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include <opencog/agentzero/tools/ResourceManager.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

// ========================================================================================
// ResourceAllocation Implementation
// ========================================================================================

ResourceAllocation::ResourceAllocation(const std::string& allocation_id,
                                     const std::string& requester_id,
                                     ResourceType resource_type,
                                     double amount)
    : _allocation_id(allocation_id)
    , _requester_id(requester_id)
    , _resource_type(resource_type)
    , _amount(amount)
    , _allocation_time(std::chrono::steady_clock::now())
    , _expiration_time(std::chrono::steady_clock::time_point::max())
    , _is_active(true)
{
}

std::string ResourceAllocation::getMetadata(const std::string& key) const
{
    auto it = _metadata.find(key);
    return (it != _metadata.end()) ? it->second : "";
}

bool ResourceAllocation::isExpired() const
{
    if (!_is_active) return true;
    if (_expiration_time == std::chrono::steady_clock::time_point::max()) return false;
    return std::chrono::steady_clock::now() >= _expiration_time;
}

double ResourceAllocation::getDurationSeconds() const
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _allocation_time);
    return duration.count() / 1000.0;
}

std::string ResourceAllocation::toString() const
{
    std::ostringstream ss;
    ss << "ResourceAllocation[id=" << _allocation_id 
       << ", requester=" << _requester_id
       << ", amount=" << _amount
       << ", active=" << (_is_active ? "true" : "false")
       << ", duration=" << getDurationSeconds() << "s]";
    return ss.str();
}

// ========================================================================================
// ResourcePool Implementation
// ========================================================================================

ResourcePool::ResourcePool(ResourceType type, const std::string& name, double capacity)
    : _resource_type(type)
    , _resource_name(name)
    , _total_capacity(capacity)
    , _available_capacity(capacity)
    , _allocated_capacity(0.0)
    , _peak_usage(0.0)
    , _status(ResourceStatus::AVAILABLE)
    , _warning_threshold(0.75)  // 75% warning
    , _critical_threshold(0.90) // 90% critical
    , _total_allocations(0)
    , _failed_allocations(0)
    , _successful_allocations(0)
{
}

std::shared_ptr<ResourceAllocation> ResourcePool::allocate(const std::string& requester_id, 
                                                           double amount,
                                                           double duration_seconds)
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    _total_allocations++;
    
    // Check if we have enough capacity
    if (amount > _available_capacity) {
        _failed_allocations++;
        logger().warn("ResourcePool: Insufficient capacity in pool '%s'. "
                     "Requested: %.2f, Available: %.2f",
                     _resource_name.c_str(), amount, _available_capacity);
        return nullptr;
    }
    
    // Create allocation
    std::string alloc_id = _resource_name + "_" + std::to_string(_total_allocations);
    auto allocation = std::make_shared<ResourceAllocation>(alloc_id, requester_id, 
                                                           _resource_type, amount);
    
    // Set expiration if duration specified
    if (duration_seconds > 0.0) {
        auto exp_time = std::chrono::steady_clock::now() + 
                       std::chrono::milliseconds(static_cast<int64_t>(duration_seconds * 1000));
        allocation->setExpirationTime(exp_time);
    }
    
    // Update capacity
    _available_capacity -= amount;
    _allocated_capacity += amount;
    
    // Track peak usage
    if (_allocated_capacity > _peak_usage) {
        _peak_usage = _allocated_capacity;
    }
    
    // Add to allocations
    _allocations.push_back(allocation);
    _successful_allocations++;
    
    // Update status
    updateStatus();
    
    logger().debug("ResourcePool: Allocated %.2f from pool '%s' for requester '%s'",
                  amount, _resource_name.c_str(), requester_id.c_str());
    
    return allocation;
}

bool ResourcePool::deallocate(const std::string& allocation_id)
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    // Find allocation
    auto it = std::find_if(_allocations.begin(), _allocations.end(),
        [&allocation_id](const std::shared_ptr<ResourceAllocation>& alloc) {
            return alloc->getAllocationId() == allocation_id;
        });
    
    if (it == _allocations.end()) {
        logger().warn("ResourcePool: Allocation '%s' not found in pool '%s'",
                     allocation_id.c_str(), _resource_name.c_str());
        return false;
    }
    
    // Release resources
    double amount = (*it)->getAmount();
    _available_capacity += amount;
    _allocated_capacity -= amount;
    
    // Remove allocation
    _allocations.erase(it);
    
    // Update status
    updateStatus();
    
    logger().debug("ResourcePool: Deallocated %.2f from pool '%s' (allocation '%s')",
                  amount, _resource_name.c_str(), allocation_id.c_str());
    
    return true;
}

bool ResourcePool::deallocateForRequester(const std::string& requester_id)
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    bool any_deallocated = false;
    
    // Remove all allocations for this requester
    _allocations.erase(
        std::remove_if(_allocations.begin(), _allocations.end(),
            [&](const std::shared_ptr<ResourceAllocation>& alloc) {
                if (alloc->getRequesterId() == requester_id) {
                    double amount = alloc->getAmount();
                    _available_capacity += amount;
                    _allocated_capacity -= amount;
                    any_deallocated = true;
                    return true;
                }
                return false;
            }),
        _allocations.end()
    );
    
    if (any_deallocated) {
        updateStatus();
        logger().debug("ResourcePool: Deallocated all resources for requester '%s' in pool '%s'",
                      requester_id.c_str(), _resource_name.c_str());
    }
    
    return any_deallocated;
}

void ResourcePool::setCapacity(double capacity)
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    double delta = capacity - _total_capacity;
    _total_capacity = capacity;
    _available_capacity += delta;
    
    // Make sure available doesn't go negative
    if (_available_capacity < 0.0) {
        _available_capacity = 0.0;
    }
    
    updateStatus();
    
    logger().info("ResourcePool: Updated capacity for pool '%s' to %.2f",
                 _resource_name.c_str(), capacity);
}

double ResourcePool::getUsagePercentage() const
{
    if (_total_capacity <= 0.0) return 0.0;
    return (_allocated_capacity / _total_capacity) * 100.0;
}

void ResourcePool::updateStatus()
{
    double usage_ratio = (_total_capacity > 0.0) ? (_allocated_capacity / _total_capacity) : 0.0;
    
    if (_available_capacity <= 0.0) {
        _status = ResourceStatus::EXHAUSTED;
    } else if (usage_ratio >= _critical_threshold) {
        _status = ResourceStatus::OVERLOADED;
    } else if (usage_ratio >= _warning_threshold) {
        _status = ResourceStatus::ALLOCATED;
    } else {
        _status = ResourceStatus::AVAILABLE;
    }
}

std::vector<std::shared_ptr<ResourceAllocation>> ResourcePool::getAllocations() const
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    return _allocations;
}

int ResourcePool::cleanupExpiredAllocations()
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    int cleaned = 0;
    
    _allocations.erase(
        std::remove_if(_allocations.begin(), _allocations.end(),
            [&](const std::shared_ptr<ResourceAllocation>& alloc) {
                if (alloc->isExpired()) {
                    double amount = alloc->getAmount();
                    _available_capacity += amount;
                    _allocated_capacity -= amount;
                    cleaned++;
                    return true;
                }
                return false;
            }),
        _allocations.end()
    );
    
    if (cleaned > 0) {
        updateStatus();
        logger().debug("ResourcePool: Cleaned up %d expired allocations from pool '%s'",
                      cleaned, _resource_name.c_str());
    }
    
    return cleaned;
}

void ResourcePool::releaseAll()
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    _allocations.clear();
    _available_capacity = _total_capacity;
    _allocated_capacity = 0.0;
    updateStatus();
    
    logger().info("ResourcePool: Released all allocations from pool '%s'", _resource_name.c_str());
}

std::string ResourcePool::getStatistics() const
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    std::ostringstream json;
    json << "{";
    json << "\"pool_name\":\"" << _resource_name << "\",";
    json << "\"total_capacity\":" << _total_capacity << ",";
    json << "\"available_capacity\":" << _available_capacity << ",";
    json << "\"allocated_capacity\":" << _allocated_capacity << ",";
    json << "\"usage_percentage\":" << std::fixed << std::setprecision(2) << getUsagePercentage() << ",";
    json << "\"peak_usage\":" << _peak_usage << ",";
    json << "\"active_allocations\":" << _allocations.size() << ",";
    json << "\"total_allocations\":" << _total_allocations << ",";
    json << "\"successful_allocations\":" << _successful_allocations << ",";
    json << "\"failed_allocations\":" << _failed_allocations << ",";
    json << "\"success_rate\":" << std::fixed << std::setprecision(2) << getSuccessRate();
    json << "}";
    
    return json.str();
}

double ResourcePool::getSuccessRate() const
{
    if (_total_allocations == 0) return 1.0;
    return static_cast<double>(_successful_allocations) / _total_allocations;
}

void ResourcePool::setThresholds(double warning_threshold, double critical_threshold)
{
    std::lock_guard<std::mutex> lock(_pool_mutex);
    
    _warning_threshold = warning_threshold;
    _critical_threshold = critical_threshold;
    updateStatus();
}

// ========================================================================================
// ResourceManager Implementation
// ========================================================================================

ResourceManager::ResourceManager(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _optimization_strategy(OptimizationStrategy::BALANCED)
    , _auto_cleanup_enabled(true)
    , _cleanup_interval_seconds(60.0)
    , _last_cleanup_time(std::chrono::steady_clock::now())
    , _total_allocations(0)
    , _total_deallocations(0)
    , _failed_allocations(0)
{
    if (_atomspace) {
        initializeManagerAtom();
    }
    
    logger().info("ResourceManager: Initialized with optimization strategy: %s",
                 optimizationStrategyToString(_optimization_strategy).c_str());
}

ResourceManager::~ResourceManager()
{
    // Release all resources
    for (auto& [type, pools] : _resource_pools) {
        for (auto& pool : pools) {
            pool->releaseAll();
        }
    }
    
    logger().info("ResourceManager: Destroyed, released all resources");
}

void ResourceManager::initializeManagerAtom()
{
    if (!_atomspace) return;
    
    // Create ConceptNode for this resource manager
    _manager_atom = _atomspace->add_node(CONCEPT_NODE, "ResourceManager");
    
    // Add inheritance link to indicate it's a resource manager
    Handle manager_type = _atomspace->add_node(CONCEPT_NODE, "ResourceManagerType");
    _atomspace->add_link(INHERITANCE_LINK, _manager_atom, manager_type);
    
    logger().debug("ResourceManager: Initialized AtomSpace representation");
}

bool ResourceManager::createResourcePool(ResourceType type, 
                                        const std::string& name, 
                                        double capacity)
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    // Check if pool already exists
    if (_named_pools.find(name) != _named_pools.end()) {
        logger().warn("ResourceManager: Pool '%s' already exists", name.c_str());
        return false;
    }
    
    // Create new pool
    auto pool = std::make_shared<ResourcePool>(type, name, capacity);
    
    // Add to pools
    _resource_pools[type].push_back(pool);
    _named_pools[name] = pool;
    
    logger().info("ResourceManager: Created resource pool '%s' of type %s with capacity %.2f",
                 name.c_str(), resourceTypeToString(type).c_str(), capacity);
    
    // Update AtomSpace if available
    if (_atomspace) {
        updateAtomSpaceRepresentation();
    }
    
    return true;
}

bool ResourceManager::removeResourcePool(const std::string& name)
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    auto it = _named_pools.find(name);
    if (it == _named_pools.end()) {
        logger().warn("ResourceManager: Pool '%s' not found", name.c_str());
        return false;
    }
    
    auto pool = it->second;
    ResourceType type = pool->getResourceType();
    
    // Release all allocations first
    pool->releaseAll();
    
    // Remove from named pools
    _named_pools.erase(it);
    
    // Remove from type pools
    auto& type_pools = _resource_pools[type];
    type_pools.erase(
        std::remove(type_pools.begin(), type_pools.end(), pool),
        type_pools.end()
    );
    
    logger().info("ResourceManager: Removed resource pool '%s'", name.c_str());
    
    return true;
}

std::shared_ptr<ResourcePool> ResourceManager::getResourcePool(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    auto it = _named_pools.find(name);
    if (it != _named_pools.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<ResourcePool>> ResourceManager::getPoolsForType(ResourceType type) const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    auto it = _resource_pools.find(type);
    if (it != _resource_pools.end()) {
        return it->second;
    }
    
    return std::vector<std::shared_ptr<ResourcePool>>();
}

std::shared_ptr<ResourcePool> ResourceManager::selectPoolForAllocation(ResourceType type,
                                                                       double amount,
                                                                       const std::string& requester_id)
{
    auto pools = getPoolsForType(type);
    
    if (pools.empty()) {
        logger().warn("ResourceManager: No pools found for resource type %s",
                     resourceTypeToString(type).c_str());
        return nullptr;
    }
    
    std::shared_ptr<ResourcePool> selected_pool = nullptr;
    
    switch (_optimization_strategy) {
        case OptimizationStrategy::FIRST_FIT:
            // Select first pool with sufficient capacity
            for (auto& pool : pools) {
                if (pool->getAvailableCapacity() >= amount) {
                    selected_pool = pool;
                    break;
                }
            }
            break;
            
        case OptimizationStrategy::BEST_FIT: {
            // Select pool with least waste (smallest sufficient capacity)
            double min_waste = std::numeric_limits<double>::max();
            for (auto& pool : pools) {
                double available = pool->getAvailableCapacity();
                if (available >= amount) {
                    double waste = available - amount;
                    if (waste < min_waste) {
                        min_waste = waste;
                        selected_pool = pool;
                    }
                }
            }
            break;
        }
        
        case OptimizationStrategy::WORST_FIT: {
            // Select pool with most available space
            double max_available = 0.0;
            for (auto& pool : pools) {
                double available = pool->getAvailableCapacity();
                if (available >= amount && available > max_available) {
                    max_available = available;
                    selected_pool = pool;
                }
            }
            break;
        }
        
        case OptimizationStrategy::BALANCED: {
            // Select pool with lowest usage percentage
            double min_usage = std::numeric_limits<double>::max();
            for (auto& pool : pools) {
                if (pool->getAvailableCapacity() >= amount) {
                    double usage = pool->getUsagePercentage();
                    if (usage < min_usage) {
                        min_usage = usage;
                        selected_pool = pool;
                    }
                }
            }
            break;
        }
        
        case OptimizationStrategy::PRIORITY_BASED:
        case OptimizationStrategy::ADAPTIVE:
            // For now, fall back to balanced strategy
            // These can be extended with priority metadata or adaptive learning
            double min_usage = std::numeric_limits<double>::max();
            for (auto& pool : pools) {
                if (pool->getAvailableCapacity() >= amount) {
                    double usage = pool->getUsagePercentage();
                    if (usage < min_usage) {
                        min_usage = usage;
                        selected_pool = pool;
                    }
                }
            }
            break;
    }
    
    return selected_pool;
}

std::shared_ptr<ResourceAllocation> ResourceManager::allocateResource(const std::string& requester_id,
                                                                      ResourceType type,
                                                                      double amount,
                                                                      double duration_seconds)
{
    // Perform auto cleanup if enabled
    if (_auto_cleanup_enabled) {
        performAutoCleanup();
    }
    
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    _total_allocations++;
    
    // Select appropriate pool
    auto pool = selectPoolForAllocation(type, amount, requester_id);
    
    if (!pool) {
        _failed_allocations++;
        logger().warn("ResourceManager: No suitable pool found for allocation (type=%s, amount=%.2f)",
                     resourceTypeToString(type).c_str(), amount);
        return nullptr;
    }
    
    // Allocate from selected pool
    auto allocation = pool->allocate(requester_id, amount, duration_seconds);
    
    if (!allocation) {
        _failed_allocations++;
        return nullptr;
    }
    
    // Track usage
    _total_usage_by_type[type] += amount;
    
    // Record in AtomSpace if available
    if (_atomspace) {
        recordAllocationInAtomSpace(*allocation);
    }
    
    logger().info("ResourceManager: Allocated %.2f of %s for requester '%s'",
                 amount, resourceTypeToString(type).c_str(), requester_id.c_str());
    
    return allocation;
}

std::shared_ptr<ResourceAllocation> ResourceManager::allocateFromPool(const std::string& requester_id,
                                                                      const std::string& pool_name,
                                                                      double amount,
                                                                      double duration_seconds)
{
    if (_auto_cleanup_enabled) {
        performAutoCleanup();
    }
    
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    _total_allocations++;
    
    auto pool = getResourcePool(pool_name);
    if (!pool) {
        _failed_allocations++;
        logger().warn("ResourceManager: Pool '%s' not found", pool_name.c_str());
        return nullptr;
    }
    
    auto allocation = pool->allocate(requester_id, amount, duration_seconds);
    
    if (!allocation) {
        _failed_allocations++;
        return nullptr;
    }
    
    // Track usage
    _total_usage_by_type[pool->getResourceType()] += amount;
    
    // Record in AtomSpace
    if (_atomspace) {
        recordAllocationInAtomSpace(*allocation);
    }
    
    return allocation;
}

bool ResourceManager::deallocateResource(const std::string& allocation_id)
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    // Search through all pools
    for (auto& [type, pools] : _resource_pools) {
        for (auto& pool : pools) {
            if (pool->deallocate(allocation_id)) {
                _total_deallocations++;
                
                // Record in AtomSpace
                if (_atomspace) {
                    recordDeallocationInAtomSpace(allocation_id);
                }
                
                return true;
            }
        }
    }
    
    logger().warn("ResourceManager: Allocation '%s' not found", allocation_id.c_str());
    return false;
}

int ResourceManager::deallocateResourcesForRequester(const std::string& requester_id)
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    int count = 0;
    
    // Deallocate from all pools
    for (auto& [type, pools] : _resource_pools) {
        for (auto& pool : pools) {
            if (pool->deallocateForRequester(requester_id)) {
                count++;
            }
        }
    }
    
    _total_deallocations += count;
    
    logger().info("ResourceManager: Deallocated %d allocations for requester '%s'",
                 count, requester_id.c_str());
    
    return count;
}

bool ResourceManager::hasAvailableResources(ResourceType type, double amount) const
{
    return getTotalAvailableCapacity(type) >= amount;
}

double ResourceManager::getTotalAvailableCapacity(ResourceType type) const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    double total = 0.0;
    auto it = _resource_pools.find(type);
    if (it != _resource_pools.end()) {
        for (const auto& pool : it->second) {
            total += pool->getAvailableCapacity();
        }
    }
    
    return total;
}

double ResourceManager::getTotalAllocatedCapacity(ResourceType type) const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    double total = 0.0;
    auto it = _resource_pools.find(type);
    if (it != _resource_pools.end()) {
        for (const auto& pool : it->second) {
            total += pool->getAllocatedCapacity();
        }
    }
    
    return total;
}

double ResourceManager::getResourceUsage(ResourceType type) const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    double total_capacity = 0.0;
    double allocated_capacity = 0.0;
    
    auto it = _resource_pools.find(type);
    if (it != _resource_pools.end()) {
        for (const auto& pool : it->second) {
            total_capacity += pool->getTotalCapacity();
            allocated_capacity += pool->getAllocatedCapacity();
        }
    }
    
    if (total_capacity <= 0.0) return 0.0;
    return (allocated_capacity / total_capacity) * 100.0;
}

void ResourceManager::setOptimizationStrategy(OptimizationStrategy strategy)
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    _optimization_strategy = strategy;
    logger().info("ResourceManager: Set optimization strategy to %s",
                 optimizationStrategyToString(strategy).c_str());
}

int ResourceManager::cleanupExpiredAllocations()
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    int total_cleaned = 0;
    
    for (auto& [type, pools] : _resource_pools) {
        for (auto& pool : pools) {
            total_cleaned += pool->cleanupExpiredAllocations();
        }
    }
    
    _last_cleanup_time = std::chrono::steady_clock::now();
    
    if (total_cleaned > 0) {
        logger().info("ResourceManager: Cleaned up %d expired allocations", total_cleaned);
    }
    
    return total_cleaned;
}

void ResourceManager::performAutoCleanup()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - _last_cleanup_time);
    
    if (elapsed.count() >= _cleanup_interval_seconds) {
        cleanupExpiredAllocations();
    }
}

bool ResourceManager::optimizeResourceAllocation()
{
    // This is a placeholder for advanced optimization
    // Could implement load balancing, defragmentation, etc.
    logger().info("ResourceManager: Optimization triggered (strategy: %s)",
                 optimizationStrategyToString(_optimization_strategy).c_str());
    
    cleanupExpiredAllocations();
    
    return true;
}

void ResourceManager::setAtomSpace(AtomSpacePtr atomspace)
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    _atomspace = atomspace;
    
    if (_atomspace) {
        initializeManagerAtom();
        updateAtomSpaceRepresentation();
    }
}

void ResourceManager::updateAtomSpaceRepresentation()
{
    if (!_atomspace || !_manager_atom) return;
    
    // Create nodes for each resource pool
    for (const auto& [name, pool] : _named_pools) {
        Handle pool_node = _atomspace->add_node(CONCEPT_NODE, "ResourcePool_" + name);
        _atomspace->add_link(MEMBER_LINK, pool_node, _manager_atom);
    }
}

void ResourceManager::recordAllocationInAtomSpace(const ResourceAllocation& allocation)
{
    if (!_atomspace) return;
    
    Handle alloc_node = _atomspace->add_node(CONCEPT_NODE, 
                                             "Allocation_" + allocation.getAllocationId());
    Handle requester_node = _atomspace->add_node(CONCEPT_NODE,
                                                 "Requester_" + allocation.getRequesterId());
    
    _atomspace->add_link(EVALUATION_LINK, alloc_node, requester_node);
}

void ResourceManager::recordDeallocationInAtomSpace(const std::string& allocation_id)
{
    if (!_atomspace) return;
    
    // Mark allocation as deallocated in AtomSpace
    Handle alloc_node = _atomspace->add_node(CONCEPT_NODE, "Allocation_" + allocation_id);
    // Could update truth value or add deallocation timestamp
}

std::string ResourceManager::getStatistics() const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    std::ostringstream json;
    json << "{";
    json << "\"total_allocations\":" << _total_allocations << ",";
    json << "\"total_deallocations\":" << _total_deallocations << ",";
    json << "\"failed_allocations\":" << _failed_allocations << ",";
    json << "\"success_rate\":" << std::fixed << std::setprecision(2) << getSuccessRate() << ",";
    json << "\"optimization_strategy\":\"" << optimizationStrategyToString(_optimization_strategy) << "\",";
    json << "\"total_pools\":" << _named_pools.size() << ",";
    json << "\"resource_types\":[";
    
    bool first = true;
    for (const auto& [type, pools] : _resource_pools) {
        if (!first) json << ",";
        json << "{";
        json << "\"type\":\"" << resourceTypeToString(type) << "\",";
        json << "\"pool_count\":" << pools.size() << ",";
        json << "\"total_capacity\":" << getTotalAvailableCapacity(type) + getTotalAllocatedCapacity(type) << ",";
        json << "\"available_capacity\":" << getTotalAvailableCapacity(type) << ",";
        json << "\"allocated_capacity\":" << getTotalAllocatedCapacity(type) << ",";
        json << "\"usage_percentage\":" << std::fixed << std::setprecision(2) << getResourceUsage(type);
        json << "}";
        first = false;
    }
    
    json << "]";
    json << "}";
    
    return json.str();
}

std::string ResourceManager::getResourceTypeStatistics(ResourceType type) const
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    std::ostringstream json;
    json << "{";
    json << "\"resource_type\":\"" << resourceTypeToString(type) << "\",";
    json << "\"pools\":[";
    
    auto it = _resource_pools.find(type);
    if (it != _resource_pools.end()) {
        bool first = true;
        for (const auto& pool : it->second) {
            if (!first) json << ",";
            json << pool->getStatistics();
            first = false;
        }
    }
    
    json << "]";
    json << "}";
    
    return json.str();
}

double ResourceManager::getSuccessRate() const
{
    if (_total_allocations == 0) return 1.0;
    return static_cast<double>(_total_allocations - _failed_allocations) / _total_allocations;
}

void ResourceManager::resetStatistics()
{
    std::lock_guard<std::mutex> lock(_manager_mutex);
    
    _total_allocations = 0;
    _total_deallocations = 0;
    _failed_allocations = 0;
    _total_usage_by_type.clear();
    
    logger().info("ResourceManager: Statistics reset");
}

// Static utility methods

std::string ResourceManager::resourceTypeToString(ResourceType type)
{
    switch (type) {
        case ResourceType::CPU: return "CPU";
        case ResourceType::MEMORY: return "MEMORY";
        case ResourceType::DISK: return "DISK";
        case ResourceType::NETWORK: return "NETWORK";
        case ResourceType::GPU: return "GPU";
        case ResourceType::ATOMSPACE: return "ATOMSPACE";
        case ResourceType::TOOL_INSTANCE: return "TOOL_INSTANCE";
        case ResourceType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

std::string ResourceManager::resourceStatusToString(ResourceStatus status)
{
    switch (status) {
        case ResourceStatus::AVAILABLE: return "AVAILABLE";
        case ResourceStatus::ALLOCATED: return "ALLOCATED";
        case ResourceStatus::EXHAUSTED: return "EXHAUSTED";
        case ResourceStatus::OVERLOADED: return "OVERLOADED";
        case ResourceStatus::ERROR: return "ERROR";
        case ResourceStatus::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

std::string ResourceManager::optimizationStrategyToString(OptimizationStrategy strategy)
{
    switch (strategy) {
        case OptimizationStrategy::FIRST_FIT: return "FIRST_FIT";
        case OptimizationStrategy::BEST_FIT: return "BEST_FIT";
        case OptimizationStrategy::WORST_FIT: return "WORST_FIT";
        case OptimizationStrategy::BALANCED: return "BALANCED";
        case OptimizationStrategy::PRIORITY_BASED: return "PRIORITY_BASED";
        case OptimizationStrategy::ADAPTIVE: return "ADAPTIVE";
        default: return "UNKNOWN";
    }
}
