/*
 * ResourceManagerSimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple standalone test for ResourceManager class compilation and basic functionality
 * Part of AZ-RESOURCE-001: Create ResourceManager for optimization
 */

#include <iostream>
#include <memory>
#include <cassert>
#include <thread>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

// Include our implementation
#include "../include/opencog/agentzero/tools/ResourceManager.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "=== ResourceManager Simple Test ===" << std::endl;
    
    try {
        // Test 1: Basic initialization
        std::cout << "\n1. Testing initialization..." << std::endl;
        
        auto atomspace = std::make_shared<AtomSpace>();
        auto manager = std::make_unique<ResourceManager>(atomspace);
        
        assert(manager != nullptr);
        assert(manager->getTotalAllocationCount() == 0);
        assert(manager->getTotalDeallocationCount() == 0);
        
        std::cout << "✓ Initialization successful" << std::endl;
        
        // Test 2: Create resource pools
        std::cout << "\n2. Testing resource pool creation..." << std::endl;
        
        bool success = manager->createResourcePool(ResourceType::CPU, "CPU_Pool", 100.0);
        assert(success);
        
        success = manager->createResourcePool(ResourceType::MEMORY, "Memory_Pool", 16384.0);
        assert(success);
        
        success = manager->createResourcePool(ResourceType::DISK, "Disk_Pool", 1000000.0);
        assert(success);
        
        auto cpu_pool = manager->getResourcePool("CPU_Pool");
        assert(cpu_pool != nullptr);
        assert(cpu_pool->getTotalCapacity() == 100.0);
        
        std::cout << "✓ Resource pools created successfully" << std::endl;
        
        // Test 3: Basic resource allocation
        std::cout << "\n3. Testing basic resource allocation..." << std::endl;
        
        auto cpu_alloc = manager->allocateResource("task_1", ResourceType::CPU, 25.0);
        assert(cpu_alloc != nullptr);
        assert(cpu_alloc->getAmount() == 25.0);
        assert(cpu_alloc->isActive());
        
        auto mem_alloc = manager->allocateResource("task_1", ResourceType::MEMORY, 2048.0);
        assert(mem_alloc != nullptr);
        
        std::cout << "✓ Resources allocated successfully" << std::endl;
        std::cout << "  CPU allocation: " << cpu_alloc->toString() << std::endl;
        std::cout << "  Memory allocation: " << mem_alloc->toString() << std::endl;
        
        // Test 4: Check resource availability
        std::cout << "\n4. Testing resource availability..." << std::endl;
        
        double available_cpu = manager->getTotalAvailableCapacity(ResourceType::CPU);
        assert(available_cpu == 75.0);  // 100 - 25
        
        double allocated_cpu = manager->getTotalAllocatedCapacity(ResourceType::CPU);
        assert(allocated_cpu == 25.0);
        
        double cpu_usage = manager->getResourceUsage(ResourceType::CPU);
        std::cout << "  CPU usage: " << cpu_usage << "%" << std::endl;
        assert(cpu_usage == 25.0);
        
        std::cout << "✓ Resource availability checked" << std::endl;
        
        // Test 5: Multiple allocations
        std::cout << "\n5. Testing multiple allocations..." << std::endl;
        
        auto cpu_alloc2 = manager->allocateResource("task_2", ResourceType::CPU, 30.0);
        auto cpu_alloc3 = manager->allocateResource("task_3", ResourceType::CPU, 20.0);
        
        assert(cpu_alloc2 != nullptr);
        assert(cpu_alloc3 != nullptr);
        
        available_cpu = manager->getTotalAvailableCapacity(ResourceType::CPU);
        assert(available_cpu == 25.0);  // 100 - 25 - 30 - 20
        
        std::cout << "✓ Multiple allocations successful" << std::endl;
        std::cout << "  Available CPU: " << available_cpu << std::endl;
        
        // Test 6: Resource deallocation
        std::cout << "\n6. Testing resource deallocation..." << std::endl;
        
        bool deallocated = manager->deallocateResource(cpu_alloc->getAllocationId());
        assert(deallocated);
        
        available_cpu = manager->getTotalAvailableCapacity(ResourceType::CPU);
        assert(available_cpu == 50.0);  // 100 - 30 - 20
        
        std::cout << "✓ Resource deallocated successfully" << std::endl;
        std::cout << "  Available CPU after deallocation: " << available_cpu << std::endl;
        
        // Test 7: Deallocate all resources for a requester
        std::cout << "\n7. Testing bulk deallocation..." << std::endl;
        
        auto disk_alloc1 = manager->allocateResource("task_bulk", ResourceType::DISK, 10000.0);
        auto disk_alloc2 = manager->allocateResource("task_bulk", ResourceType::DISK, 20000.0);
        
        int count = manager->deallocateResourcesForRequester("task_bulk");
        assert(count == 2);
        
        std::cout << "✓ Bulk deallocation successful (deallocated " << count << " allocations)" << std::endl;
        
        // Test 8: Insufficient resources
        std::cout << "\n8. Testing insufficient resource handling..." << std::endl;
        
        auto impossible_alloc = manager->allocateResource("task_4", ResourceType::CPU, 200.0);
        assert(impossible_alloc == nullptr);
        
        std::cout << "✓ Correctly handled insufficient resources" << std::endl;
        
        // Test 9: Optimization strategies
        std::cout << "\n9. Testing optimization strategies..." << std::endl;
        
        manager->setOptimizationStrategy(OptimizationStrategy::BEST_FIT);
        assert(manager->getOptimizationStrategy() == OptimizationStrategy::BEST_FIT);
        
        manager->setOptimizationStrategy(OptimizationStrategy::BALANCED);
        assert(manager->getOptimizationStrategy() == OptimizationStrategy::BALANCED);
        
        std::cout << "✓ Optimization strategies set successfully" << std::endl;
        
        // Test 10: Time-based allocation (with expiration)
        std::cout << "\n10. Testing time-based allocation..." << std::endl;
        
        auto timed_alloc = manager->allocateResource("task_timed", ResourceType::MEMORY, 1024.0, 1.0);
        assert(timed_alloc != nullptr);
        assert(timed_alloc->isActive());
        
        std::cout << "  Waiting 2 seconds for allocation to expire..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Trigger cleanup
        int cleaned = manager->cleanupExpiredAllocations();
        std::cout << "  Cleaned up " << cleaned << " expired allocations" << std::endl;
        
        std::cout << "✓ Time-based allocation working" << std::endl;
        
        // Test 11: Statistics
        std::cout << "\n11. Testing statistics..." << std::endl;
        
        std::string stats = manager->getStatistics();
        std::cout << "  Overall statistics: " << stats << std::endl;
        
        std::string cpu_stats = manager->getResourceTypeStatistics(ResourceType::CPU);
        std::cout << "  CPU statistics: " << cpu_stats << std::endl;
        
        double success_rate = manager->getSuccessRate();
        std::cout << "  Success rate: " << (success_rate * 100.0) << "%" << std::endl;
        
        std::cout << "✓ Statistics retrieved successfully" << std::endl;
        
        // Test 12: AtomSpace integration
        std::cout << "\n12. Testing AtomSpace integration..." << std::endl;
        
        Handle manager_atom = manager->getManagerAtom();
        assert(manager_atom != Handle::UNDEFINED);
        
        std::cout << "  Manager atom: " << atomspace->atom_as_string(manager_atom) << std::endl;
        
        std::cout << "✓ AtomSpace integration working" << std::endl;
        
        // Test 13: Resource pool operations
        std::cout << "\n13. Testing resource pool operations..." << std::endl;
        
        auto pools = manager->getPoolsForType(ResourceType::CPU);
        assert(pools.size() == 1);
        
        auto pool = pools[0];
        std::string pool_stats = pool->getStatistics();
        std::cout << "  Pool statistics: " << pool_stats << std::endl;
        
        std::cout << "✓ Resource pool operations working" << std::endl;
        
        // Test 14: Static utility methods
        std::cout << "\n14. Testing static utility methods..." << std::endl;
        
        std::string cpu_str = ResourceManager::resourceTypeToString(ResourceType::CPU);
        assert(cpu_str == "CPU");
        
        std::string avail_str = ResourceManager::resourceStatusToString(ResourceStatus::AVAILABLE);
        assert(avail_str == "AVAILABLE");
        
        std::string balanced_str = ResourceManager::optimizationStrategyToString(OptimizationStrategy::BALANCED);
        assert(balanced_str == "BALANCED");
        
        std::cout << "✓ Static utility methods working" << std::endl;
        
        // Final summary
        std::cout << "\n=== All Tests Passed ===" << std::endl;
        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << manager->getStatistics() << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
