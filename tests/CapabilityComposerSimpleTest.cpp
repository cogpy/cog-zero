/*
 * CapabilityComposerSimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple standalone test for CapabilityComposer class compilation and basic functionality
 * Part of AZ-TOOL-003: Implement CapabilityComposer
 */

#include <iostream>
#include <memory>
#include <vector>
#include <cassert>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

// Include our implementation
#include "../include/opencog/agentzero/tools/CapabilityComposer.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

// Simple test capability functions
bool testCapability1(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  Executing test capability 1" << std::endl;
    return true;
}

bool testCapability2(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  Executing test capability 2" << std::endl;
    return true;
}

bool testCapability3(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  Executing test capability 3 (depends on 1 and 2)" << std::endl;
    return true;
}

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "=== CapabilityComposer Simple Test ===" << std::endl;
    
    try {
        // Test 1: Basic initialization
        std::cout << "\n1. Testing initialization..." << std::endl;
        
        auto atomspace = std::make_shared<AtomSpace>();
        auto composer = std::make_unique<CapabilityComposer>(atomspace);
        
        assert(composer != nullptr);
        assert(composer->getCapabilityCount() == 0);
        assert(composer->getPlanCount() == 0);
        
        std::cout << "✓ Initialization successful" << std::endl;
        
        // Test 2: Capability registration
        std::cout << "\n2. Testing capability registration..." << std::endl;
        
        bool success = composer->registerCapability(
            "test_cap_1",
            "Test Capability 1",
            "A simple test capability",
            testCapability1,
            {}  // No dependencies
        );
        
        assert(success);
        assert(composer->getCapabilityCount() == 1);
        assert(composer->isCapabilityRegistered("test_cap_1"));
        
        std::cout << "✓ Capability 1 registered successfully" << std::endl;
        
        // Test 3: Register capabilities with dependencies
        std::cout << "\n3. Testing capability registration with dependencies..." << std::endl;
        
        success = composer->registerCapability(
            "test_cap_2",
            "Test Capability 2",
            "Another test capability",
            testCapability2,
            {}
        );
        assert(success);
        
        success = composer->registerCapability(
            "test_cap_3",
            "Test Capability 3",
            "Capability that depends on 1 and 2",
            testCapability3,
            {"test_cap_1", "test_cap_2"}
        );
        assert(success);
        assert(composer->getCapabilityCount() == 3);
        
        std::cout << "✓ All capabilities registered (3 total)" << std::endl;
        
        // Test 4: Dependency validation
        std::cout << "\n4. Testing dependency validation..." << std::endl;
        
        bool deps_valid = composer->validateCapabilityDependencies("test_cap_3");
        assert(deps_valid);
        
        std::cout << "✓ Dependencies validated successfully" << std::endl;
        
        // Test 5: Get dependency tree
        std::cout << "\n5. Testing dependency tree retrieval..." << std::endl;
        
        auto dep_tree = composer->getCapabilityDependencyTree("test_cap_3", 10);
        assert(!dep_tree.empty());
        
        std::cout << "✓ Dependency tree retrieved: " << dep_tree.size() << " nodes" << std::endl;
        
        // Test 6: Get registered capabilities
        std::cout << "\n6. Testing capability listing..." << std::endl;
        
        auto registered_caps = composer->getRegisteredCapabilities();
        assert(registered_caps.size() == 3);
        
        std::cout << "✓ Retrieved " << registered_caps.size() << " registered capabilities" << std::endl;
        for (const auto& cap_id : registered_caps) {
            std::cout << "  - " << cap_id << std::endl;
        }
        
        // Test 7: Find capabilities providing output
        std::cout << "\n7. Testing capability provider search..." << std::endl;
        
        auto providers = composer->findCapabilitiesProviding("test_cap_1");
        assert(providers.size() == 1);
        
        std::cout << "✓ Found " << providers.size() << " provider(s) for test_cap_1" << std::endl;
        
        // Test 8: Compose a plan
        std::cout << "\n8. Testing plan composition..." << std::endl;
        
        CapabilityComposer::TaskRequirements requirements;
        requirements.task_description = "Execute test capabilities in order";
        requirements.required_outputs = {"test_cap_3"};  // Need output from cap 3
        
        auto plan = composer->composeForTask(requirements);
        
        // Plan should be valid and contain all three capabilities in dependency order
        assert(plan.is_valid);
        assert(!plan.capability_sequence.empty());
        
        std::cout << "✓ Plan composed with " << plan.capability_sequence.size() << " steps" << std::endl;
        std::cout << "  Execution sequence:" << std::endl;
        for (const auto& cap_id : plan.capability_sequence) {
            std::cout << "    - " << cap_id << std::endl;
        }
        
        // Test 9: Execute the plan
        std::cout << "\n9. Testing plan execution..." << std::endl;
        
        CapabilityComposer::ExecutionContext context;
        context.context_id = "test_context";
        context.atomspace = atomspace;
        
        auto result = composer->executePlan(plan, context);
        assert(result == CapabilityComposer::CapabilityResult::SUCCESS);
        
        std::cout << "✓ Plan executed successfully" << std::endl;
        
        // Test 10: Statistics
        std::cout << "\n10. Testing statistics..." << std::endl;
        
        auto stats = composer->getCapabilityStatistics();
        assert(stats["total_capabilities"] == 3.0);
        assert(stats["total_executions"] == 3.0);
        
        std::cout << "✓ Statistics retrieved:" << std::endl;
        std::cout << "  - Total capabilities: " << stats["total_capabilities"] << std::endl;
        std::cout << "  - Total executions: " << stats["total_executions"] << std::endl;
        std::cout << "  - Average success rate: " << stats["average_success_rate"] << std::endl;
        
        // Test 11: Export capabilities
        std::cout << "\n11. Testing capability export..." << std::endl;
        
        std::string exported = composer->exportCapabilities("json");
        assert(!exported.empty());
        assert(exported.find("test_cap_1") != std::string::npos);
        
        std::cout << "✓ Capabilities exported (" << exported.length() << " characters)" << std::endl;
        
        // Test 12: Compose and execute in one step
        std::cout << "\n12. Testing compose and execute..." << std::endl;
        
        CapabilityComposer::ExecutionContext context2;
        context2.context_id = "test_context_2";
        context2.atomspace = atomspace;
        
        result = composer->composeAndExecute(requirements, context2);
        assert(result == CapabilityComposer::CapabilityResult::SUCCESS);
        
        std::cout << "✓ Compose and execute completed successfully" << std::endl;
        
        // Test 13: Unregister capability
        std::cout << "\n13. Testing capability unregistration..." << std::endl;
        
        success = composer->unregisterCapability("test_cap_1");
        assert(success);
        assert(composer->getCapabilityCount() == 2);
        assert(!composer->isCapabilityRegistered("test_cap_1"));
        
        std::cout << "✓ Capability unregistered successfully" << std::endl;
        
        // Test 14: Clear cached plans
        std::cout << "\n14. Testing plan cache clearing..." << std::endl;
        
        size_t cleared = composer->clearCachedPlans();
        assert(cleared > 0);
        assert(composer->getPlanCount() == 0);
        
        std::cout << "✓ Cleared " << cleared << " cached plan(s)" << std::endl;
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
