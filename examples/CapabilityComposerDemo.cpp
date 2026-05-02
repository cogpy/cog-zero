/*
 * CapabilityComposerDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of CapabilityComposer for complex task execution
 * Part of AZ-TOOL-003: Implement CapabilityComposer
 */

#include <iostream>
#include <memory>
#include <iomanip>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/tools/CapabilityComposer.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

// Simulated capabilities for a robot manipulation task

bool sensorReadCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [SENSOR] Reading environment sensors..." << std::endl;
    std::cout << "  [SENSOR] Detected object at position (1.5, 2.0, 0.8)" << std::endl;
    return true;
}

bool visionAnalysisCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [VISION] Analyzing visual input..." << std::endl;
    std::cout << "  [VISION] Object identified: Red cube, 5cm x 5cm x 5cm" << std::endl;
    return true;
}

bool pathPlanningCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [PLANNING] Computing optimal path to object..." << std::endl;
    std::cout << "  [PLANNING] Path computed with 8 waypoints" << std::endl;
    return true;
}

bool armMovementCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [MOTION] Moving robotic arm along planned path..." << std::endl;
    std::cout << "  [MOTION] Arm positioned at target location" << std::endl;
    return true;
}

bool gripperControlCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [GRIPPER] Opening gripper..." << std::endl;
    std::cout << "  [GRIPPER] Closing gripper on object" << std::endl;
    std::cout << "  [GRIPPER] Object grasped successfully" << std::endl;
    return true;
}

bool objectPlacementCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [PLACEMENT] Moving object to target location..." << std::endl;
    std::cout << "  [PLACEMENT] Releasing object" << std::endl;
    std::cout << "  [PLACEMENT] Object placed at target" << std::endl;
    return true;
}

bool safetyCheckCapability(const CapabilityComposer::ExecutionContext& ctx)
{
    std::cout << "  [SAFETY] Performing safety validation..." << std::endl;
    std::cout << "  [SAFETY] No obstacles detected in path" << std::endl;
    std::cout << "  [SAFETY] Operation within safe limits" << std::endl;
    return true;
}

void printSectionHeader(const std::string& title)
{
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     CapabilityComposer Demonstration                      ║" << std::endl;
    std::cout << "║     Complex Task: Robot Object Manipulation               ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        // Initialize
        printSectionHeader("1. INITIALIZATION");
        
        auto atomspace = std::make_shared<AtomSpace>();
        auto composer = std::make_unique<CapabilityComposer>(atomspace);
        
        std::cout << "\n✓ AtomSpace created" << std::endl;
        std::cout << "✓ CapabilityComposer initialized" << std::endl;
        
        // Register capabilities
        printSectionHeader("2. CAPABILITY REGISTRATION");
        
        std::cout << "\nRegistering robotic capabilities:" << std::endl;
        
        // Basic perception capabilities
        composer->registerCapability(
            "sensor_read",
            "Sensor Reading",
            "Read data from environmental sensors",
            sensorReadCapability,
            {}
        );
        std::cout << "  ✓ Sensor Reading" << std::endl;
        
        composer->registerCapability(
            "vision_analysis",
            "Vision Analysis",
            "Analyze visual input and identify objects",
            visionAnalysisCapability,
            {"sensor_read"}
        );
        std::cout << "  ✓ Vision Analysis (depends on: sensor_read)" << std::endl;
        
        composer->registerCapability(
            "safety_check",
            "Safety Validation",
            "Validate operation safety constraints",
            safetyCheckCapability,
            {"sensor_read"}
        );
        std::cout << "  ✓ Safety Validation (depends on: sensor_read)" << std::endl;
        
        // Planning capability
        composer->registerCapability(
            "path_planning",
            "Path Planning",
            "Compute optimal path for robot movement",
            pathPlanningCapability,
            {"vision_analysis", "safety_check"}
        );
        std::cout << "  ✓ Path Planning (depends on: vision_analysis, safety_check)" << std::endl;
        
        // Execution capabilities
        composer->registerCapability(
            "arm_movement",
            "Arm Movement Control",
            "Control robotic arm movement",
            armMovementCapability,
            {"path_planning"}
        );
        std::cout << "  ✓ Arm Movement (depends on: path_planning)" << std::endl;
        
        composer->registerCapability(
            "gripper_control",
            "Gripper Control",
            "Control gripper open/close operations",
            gripperControlCapability,
            {"arm_movement"}
        );
        std::cout << "  ✓ Gripper Control (depends on: arm_movement)" << std::endl;
        
        composer->registerCapability(
            "object_placement",
            "Object Placement",
            "Place object at target location",
            objectPlacementCapability,
            {"gripper_control"}
        );
        std::cout << "  ✓ Object Placement (depends on: gripper_control)" << std::endl;
        
        std::cout << "\nTotal registered capabilities: " << composer->getCapabilityCount() << std::endl;
        
        // Show dependency structure
        printSectionHeader("3. DEPENDENCY ANALYSIS");
        
        std::cout << "\nDependency tree for 'object_placement':" << std::endl;
        auto dep_tree = composer->getCapabilityDependencyTree("object_placement", 10);
        
        for (const auto& [cap, deps] : dep_tree) {
            std::cout << "  " << cap << " requires:" << std::endl;
            for (const auto& dep : deps) {
                std::cout << "    - " << dep << std::endl;
            }
        }
        
        // Compose task plan
        printSectionHeader("4. TASK COMPOSITION");
        
        std::cout << "\nTask: Pick and place object" << std::endl;
        std::cout << "Goal: Complete object manipulation sequence" << std::endl;
        
        CapabilityComposer::TaskRequirements requirements;
        requirements.task_description = "Pick and place object manipulation";
        requirements.required_outputs = {"object_placement"};
        requirements.min_success_probability = 0.8;
        
        std::cout << "\nComposing execution plan..." << std::endl;
        auto plan = composer->composeForTask(requirements);
        
        if (plan.is_valid) {
            std::cout << "\n✓ Plan composition successful!" << std::endl;
            std::cout << "\nPlan details:" << std::endl;
            std::cout << "  Plan ID: " << plan.plan_id << std::endl;
            std::cout << "  Steps: " << plan.capability_sequence.size() << std::endl;
            std::cout << "  Estimated success: " << std::fixed << std::setprecision(2) 
                     << (plan.estimated_success_probability * 100) << "%" << std::endl;
            
            std::cout << "\nExecution sequence:" << std::endl;
            for (size_t i = 0; i < plan.capability_sequence.size(); ++i) {
                const auto& cap_id = plan.capability_sequence[i];
                const auto* cap = composer->getCapability(cap_id);
                if (cap) {
                    std::cout << "  " << (i + 1) << ". " << cap->name 
                             << " (" << cap_id << ")" << std::endl;
                }
            }
        } else {
            std::cerr << "\n✗ Plan composition failed!" << std::endl;
            return 1;
        }
        
        // Execute the plan
        printSectionHeader("5. PLAN EXECUTION");
        
        std::cout << "\nExecuting composed plan..." << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        CapabilityComposer::ExecutionContext context;
        context.context_id = "manipulation_context_001";
        context.atomspace = atomspace;
        context.resource_allocation = 1.0;
        
        auto result = composer->executePlan(plan, context);
        
        std::cout << std::string(60, '-') << std::endl;
        
        if (result == CapabilityComposer::CapabilityResult::SUCCESS) {
            std::cout << "\n✓ Plan execution completed successfully!" << std::endl;
        } else {
            std::cout << "\n✗ Plan execution failed with result: " 
                     << static_cast<int>(result) << std::endl;
        }
        
        // Show statistics
        printSectionHeader("6. EXECUTION STATISTICS");
        
        auto stats = composer->getCapabilityStatistics();
        
        std::cout << "\nCapability System Statistics:" << std::endl;
        std::cout << "  Total capabilities: " << stats["total_capabilities"] << std::endl;
        std::cout << "  Total executions: " << stats["total_executions"] << std::endl;
        std::cout << "  Average success rate: " << std::fixed << std::setprecision(2)
                 << (stats["average_success_rate"] * 100) << "%" << std::endl;
        std::cout << "  Cached plans: " << stats["total_plans"] << std::endl;
        
        // Export capabilities
        printSectionHeader("7. CAPABILITY EXPORT");
        
        std::string exported = composer->exportCapabilities("json");
        std::cout << "\nExported capability definitions (JSON):" << std::endl;
        std::cout << exported << std::endl;
        
        printSectionHeader("DEMONSTRATION COMPLETE");
        
        std::cout << "\n✓ All operations completed successfully" << std::endl;
        std::cout << "✓ CapabilityComposer demonstrated complex task composition" << std::endl;
        std::cout << "✓ Automatic dependency resolution working" << std::endl;
        std::cout << "✓ AtomSpace integration functional" << std::endl;
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              Demonstration Successful!                    ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Demonstration failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
