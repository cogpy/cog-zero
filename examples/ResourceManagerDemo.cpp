/*
 * ResourceManagerDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of ResourceManager functionality
 * Part of AZ-RESOURCE-001: Create ResourceManager for optimization
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/tools/ResourceManager.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

void printSeparator()
{
    std::cout << "\n" << std::string(80, '=') << "\n" << std::endl;
}

void demonstrateBasicUsage()
{
    std::cout << "DEMONSTRATION 1: Basic Resource Management" << std::endl;
    printSeparator();
    
    // Create ResourceManager with AtomSpace
    auto atomspace = std::make_shared<AtomSpace>();
    auto manager = std::make_unique<ResourceManager>(atomspace);
    
    // Create resource pools for different resource types
    std::cout << "Creating resource pools..." << std::endl;
    manager->createResourcePool(ResourceType::CPU, "CPU_Main", 100.0);
    manager->createResourcePool(ResourceType::MEMORY, "RAM_Main", 32768.0); // MB
    manager->createResourcePool(ResourceType::DISK, "Storage_Main", 1000000.0); // MB
    manager->createResourcePool(ResourceType::NETWORK, "Network_Main", 1000.0); // Mbps
    
    std::cout << "\n✓ Created 4 resource pools" << std::endl;
    
    // Allocate resources for a task
    std::cout << "\nAllocating resources for 'DataProcessingTask'..." << std::endl;
    
    auto cpu_alloc = manager->allocateResource("DataProcessingTask", ResourceType::CPU, 40.0);
    auto mem_alloc = manager->allocateResource("DataProcessingTask", ResourceType::MEMORY, 8192.0);
    auto disk_alloc = manager->allocateResource("DataProcessingTask", ResourceType::DISK, 50000.0);
    
    if (cpu_alloc && mem_alloc && disk_alloc) {
        std::cout << "✓ Resources allocated successfully:" << std::endl;
        std::cout << "  - CPU: " << cpu_alloc->getAmount() << " units" << std::endl;
        std::cout << "  - Memory: " << mem_alloc->getAmount() << " MB" << std::endl;
        std::cout << "  - Disk: " << disk_alloc->getAmount() << " MB" << std::endl;
    }
    
    // Show current resource usage
    std::cout << "\nCurrent Resource Usage:" << std::endl;
    std::cout << "  CPU: " << manager->getResourceUsage(ResourceType::CPU) << "%" << std::endl;
    std::cout << "  Memory: " << manager->getResourceUsage(ResourceType::MEMORY) << "%" << std::endl;
    std::cout << "  Disk: " << manager->getResourceUsage(ResourceType::DISK) << "%" << std::endl;
    
    // Deallocate resources
    std::cout << "\nDeallocating resources..." << std::endl;
    int deallocated = manager->deallocateResourcesForRequester("DataProcessingTask");
    std::cout << "✓ Deallocated " << deallocated << " allocations" << std::endl;
}

void demonstrateOptimizationStrategies()
{
    std::cout << "\n\nDEMONSTRATION 2: Optimization Strategies" << std::endl;
    printSeparator();
    
    auto atomspace = std::make_shared<AtomSpace>();
    auto manager = std::make_unique<ResourceManager>(atomspace);
    
    // Create multiple pools for the same resource type
    std::cout << "Creating multiple CPU pools..." << std::endl;
    manager->createResourcePool(ResourceType::CPU, "CPU_Pool_1", 100.0);
    manager->createResourcePool(ResourceType::CPU, "CPU_Pool_2", 150.0);
    manager->createResourcePool(ResourceType::CPU, "CPU_Pool_3", 200.0);
    
    std::cout << "✓ Created 3 CPU pools with different capacities" << std::endl;
    
    // Demonstrate different optimization strategies
    std::vector<OptimizationStrategy> strategies = {
        OptimizationStrategy::FIRST_FIT,
        OptimizationStrategy::BEST_FIT,
        OptimizationStrategy::WORST_FIT,
        OptimizationStrategy::BALANCED
    };
    
    for (auto strategy : strategies) {
        manager->setOptimizationStrategy(strategy);
        
        std::cout << "\n--- Using " << ResourceManager::optimizationStrategyToString(strategy) 
                  << " strategy ---" << std::endl;
        
        // Allocate resources
        auto alloc = manager->allocateResource("Task_" + ResourceManager::optimizationStrategyToString(strategy), 
                                              ResourceType::CPU, 50.0);
        
        if (alloc) {
            std::cout << "  Allocated 50 CPU units" << std::endl;
            std::cout << "  Allocation ID: " << alloc->getAllocationId() << std::endl;
        }
        
        // Show pool usage
        auto pools = manager->getPoolsForType(ResourceType::CPU);
        for (size_t i = 0; i < pools.size(); ++i) {
            std::cout << "  Pool " << (i + 1) << " usage: " 
                     << pools[i]->getUsagePercentage() << "%" << std::endl;
        }
    }
}

void demonstrateTimeBasedAllocation()
{
    std::cout << "\n\nDEMONSTRATION 3: Time-Based Resource Allocation" << std::endl;
    printSeparator();
    
    auto atomspace = std::make_shared<AtomSpace>();
    auto manager = std::make_unique<ResourceManager>(atomspace);
    
    manager->createResourcePool(ResourceType::MEMORY, "Temp_Memory", 10240.0);
    
    std::cout << "Creating temporary allocations with expiration..." << std::endl;
    
    // Create short-lived allocations
    auto alloc1 = manager->allocateResource("ShortTask1", ResourceType::MEMORY, 2048.0, 2.0); // 2 seconds
    auto alloc2 = manager->allocateResource("ShortTask2", ResourceType::MEMORY, 3072.0, 3.0); // 3 seconds
    auto alloc3 = manager->allocateResource("LongTask", ResourceType::MEMORY, 1024.0, 10.0);  // 10 seconds
    
    std::cout << "✓ Created 3 allocations with different expiration times" << std::endl;
    std::cout << "  - ShortTask1: 2048 MB for 2 seconds" << std::endl;
    std::cout << "  - ShortTask2: 3072 MB for 3 seconds" << std::endl;
    std::cout << "  - LongTask: 1024 MB for 10 seconds" << std::endl;
    
    // Monitor allocation status over time
    for (int i = 0; i < 5; ++i) {
        std::cout << "\nTime: " << i << " seconds" << std::endl;
        std::cout << "  Available memory: " 
                 << manager->getTotalAvailableCapacity(ResourceType::MEMORY) << " MB" << std::endl;
        
        int cleaned = manager->cleanupExpiredAllocations();
        if (cleaned > 0) {
            std::cout << "  ✓ Cleaned up " << cleaned << " expired allocations" << std::endl;
        }
        
        if (i < 4) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void demonstrateAtomSpaceIntegration()
{
    std::cout << "\n\nDEMONSTRATION 4: AtomSpace Integration" << std::endl;
    printSeparator();
    
    auto atomspace = std::make_shared<AtomSpace>();
    auto manager = std::make_unique<ResourceManager>(atomspace);
    
    manager->createResourcePool(ResourceType::ATOMSPACE, "AtomSpace_Pool", 10000.0);
    
    std::cout << "ResourceManager integrated with AtomSpace" << std::endl;
    
    // Get the manager's AtomSpace representation
    Handle manager_atom = manager->getManagerAtom();
    std::cout << "\nManager atom in AtomSpace:" << std::endl;
    std::cout << "  " << atomspace->atom_as_string(manager_atom) << std::endl;
    
    // Allocate some resources
    manager->allocateResource("AtomSpaceTask1", ResourceType::ATOMSPACE, 1000.0);
    manager->allocateResource("AtomSpaceTask2", ResourceType::ATOMSPACE, 2000.0);
    
    std::cout << "\n✓ Allocations recorded in AtomSpace" << std::endl;
    std::cout << "  Total atoms in AtomSpace: " << atomspace->get_size() << std::endl;
}

void demonstrateComplexScenario()
{
    std::cout << "\n\nDEMONSTRATION 5: Complex Multi-Task Scenario" << std::endl;
    printSeparator();
    
    auto atomspace = std::make_shared<AtomSpace>();
    auto manager = std::make_unique<ResourceManager>(atomspace);
    
    // Set up comprehensive resource pools
    manager->createResourcePool(ResourceType::CPU, "CPU_Primary", 200.0);
    manager->createResourcePool(ResourceType::MEMORY, "RAM_Primary", 65536.0);
    manager->createResourcePool(ResourceType::GPU, "GPU_Primary", 100.0);
    manager->createResourcePool(ResourceType::NETWORK, "Network_Primary", 1000.0);
    
    // Use balanced optimization
    manager->setOptimizationStrategy(OptimizationStrategy::BALANCED);
    
    std::cout << "Simulating multiple concurrent tasks..." << std::endl;
    
    // Simulate different types of tasks
    struct Task {
        std::string name;
        double cpu;
        double memory;
        double gpu;
        double network;
    };
    
    std::vector<Task> tasks = {
        {"VideoProcessing", 50.0, 8192.0, 30.0, 100.0},
        {"MachineLearning", 80.0, 16384.0, 60.0, 50.0},
        {"WebServer", 20.0, 4096.0, 0.0, 300.0},
        {"Database", 30.0, 12288.0, 0.0, 200.0},
        {"Analytics", 40.0, 8192.0, 20.0, 100.0}
    };
    
    std::vector<std::vector<std::shared_ptr<ResourceAllocation>>> task_allocations;
    
    for (const auto& task : tasks) {
        std::cout << "\n--- Allocating resources for " << task.name << " ---" << std::endl;
        
        std::vector<std::shared_ptr<ResourceAllocation>> allocations;
        
        auto cpu_alloc = manager->allocateResource(task.name, ResourceType::CPU, task.cpu);
        if (cpu_alloc) {
            allocations.push_back(cpu_alloc);
            std::cout << "  ✓ CPU: " << task.cpu << " units" << std::endl;
        }
        
        auto mem_alloc = manager->allocateResource(task.name, ResourceType::MEMORY, task.memory);
        if (mem_alloc) {
            allocations.push_back(mem_alloc);
            std::cout << "  ✓ Memory: " << task.memory << " MB" << std::endl;
        }
        
        if (task.gpu > 0) {
            auto gpu_alloc = manager->allocateResource(task.name, ResourceType::GPU, task.gpu);
            if (gpu_alloc) {
                allocations.push_back(gpu_alloc);
                std::cout << "  ✓ GPU: " << task.gpu << " units" << std::endl;
            }
        }
        
        auto net_alloc = manager->allocateResource(task.name, ResourceType::NETWORK, task.network);
        if (net_alloc) {
            allocations.push_back(net_alloc);
            std::cout << "  ✓ Network: " << task.network << " Mbps" << std::endl;
        }
        
        task_allocations.push_back(allocations);
    }
    
    // Show overall resource usage
    std::cout << "\n\n--- Overall Resource Usage ---" << std::endl;
    std::cout << "CPU Usage: " << manager->getResourceUsage(ResourceType::CPU) << "%" << std::endl;
    std::cout << "Memory Usage: " << manager->getResourceUsage(ResourceType::MEMORY) << "%" << std::endl;
    std::cout << "GPU Usage: " << manager->getResourceUsage(ResourceType::GPU) << "%" << std::endl;
    std::cout << "Network Usage: " << manager->getResourceUsage(ResourceType::NETWORK) << "%" << std::endl;
    
    // Show detailed statistics
    std::cout << "\n\n--- Detailed Statistics ---" << std::endl;
    std::cout << manager->getStatistics() << std::endl;
}

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ResourceManager Demonstration - Agent-Zero Tools             ║\n";
    std::cout << "║                    AZ-RESOURCE-001 Implementation                     ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝\n";
    
    try {
        demonstrateBasicUsage();
        demonstrateOptimizationStrategies();
        demonstrateTimeBasedAllocation();
        demonstrateAtomSpaceIntegration();
        demonstrateComplexScenario();
        
        printSeparator();
        std::cout << "\n✓ All demonstrations completed successfully!" << std::endl;
        std::cout << "\nResourceManager provides comprehensive resource management for" << std::endl;
        std::cout << "Agent-Zero, including CPU, memory, disk, network, GPU, and AtomSpace" << std::endl;
        std::cout << "resources with multiple optimization strategies and AtomSpace integration." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Demonstration failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
