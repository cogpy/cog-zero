/*
 * examples/ToolWrapperDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demonstration of ToolWrapper unified interface
 * Shows integration with external-tools and ros-behavior-scripting
 * Part of the AGENT-ZERO-GENESIS project - AZ-TOOL-002
 */

#include <iostream>
#include <memory>
#include <string>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/ToolWrapper.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

/**
 * Example 1: Custom Tool Wrapper
 * Demonstrates creating a custom tool with a lambda executor
 */
void example_custom_tool()
{
    std::cout << "\n=== Example 1: Custom Tool Wrapper ===" << std::endl;
    
    // Create AtomSpace for knowledge representation
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create a custom tool
    auto tool = std::make_shared<ToolWrapper>("sentiment_analyzer", ToolType::CUSTOM, atomspace);
    tool->setDescription("Analyzes sentiment of text input");
    
    // Set custom executor
    tool->setCustomExecutor([](const ToolExecutionContext& context) {
        ToolResult result(ToolStatus::COMPLETED);
        
        // Get input parameter
        std::string text = context.getParameter("text");
        
        // Simple sentiment analysis (placeholder)
        double sentiment = 0.5; // Neutral
        if (text.find("happy") != std::string::npos || text.find("great") != std::string::npos) {
            sentiment = 0.8; // Positive
        } else if (text.find("sad") != std::string::npos || text.find("bad") != std::string::npos) {
            sentiment = 0.2; // Negative
        }
        
        result.setOutput("Sentiment score: " + std::to_string(sentiment));
        result.setMetadata("sentiment_score", std::to_string(sentiment));
        result.setMetadata("text_length", std::to_string(text.length()));
        
        return result;
    });
    
    // Execute the tool
    ToolExecutionContext context(atomspace);
    context.setParameter("text", "This is a great day!");
    context.setTimeout(5000.0);
    
    ToolResult result = tool->execute(context);
    
    std::cout << "Tool Name: " << tool->getToolName() << std::endl;
    std::cout << "Status: " << (result.isSuccess() ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Output: " << result.getOutput() << std::endl;
    std::cout << "Execution Time: " << result.getExecutionTime() << "ms" << std::endl;
    std::cout << "Statistics: " << tool->getStatistics() << std::endl;
}

/**
 * Example 2: REST API Tool Wrapper
 * Demonstrates wrapping an external REST API tool
 */
void example_rest_api_tool()
{
    std::cout << "\n=== Example 2: REST API Tool Wrapper ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create a REST API tool wrapper
    auto tool = std::make_shared<ToolWrapper>("face_detector", ToolType::EXTERNAL_REST_API, atomspace);
    tool->setDescription("Detects faces in images via REST API");
    tool->setToolEndpoint("http://localhost:5000/api/detect_faces");
    tool->setToolConfig("api_key", "test_key_12345");
    tool->setToolConfig("timeout", "10000");
    
    // Add required parameters
    tool->addRequiredParameter("image_url");
    tool->addRequiredParameter("confidence_threshold");
    
    // Create execution context
    ToolExecutionContext context(atomspace);
    context.setParameter("image_url", "http://example.com/image.jpg");
    context.setParameter("confidence_threshold", "0.8");
    context.setTimeout(10000.0);
    
    // Execute (will use placeholder implementation)
    ToolResult result = tool->execute(context);
    
    std::cout << "Tool Name: " << tool->getToolName() << std::endl;
    std::cout << "Endpoint: " << tool->getToolEndpoint() << std::endl;
    std::cout << "Status: " << (result.isSuccess() ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Output: " << result.getOutput() << std::endl;
    std::cout << "Result JSON: " << result.toJSON() << std::endl;
}

/**
 * Example 3: ROS Behavior Tool Wrapper
 * Demonstrates wrapping ROS behavior scripting
 */
void example_ros_behavior_tool()
{
    std::cout << "\n=== Example 3: ROS Behavior Tool Wrapper ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create a ROS behavior tool wrapper
    auto tool = std::make_shared<ToolWrapper>("robot_movement", ToolType::ROS_BEHAVIOR, atomspace);
    tool->setDescription("Controls robot movement via ROS");
    tool->setToolEndpoint("/robot/move_to");
    tool->setToolConfig("ros_master", "http://localhost:11311");
    
    // Create execution context
    ToolExecutionContext context(atomspace);
    context.setParameter("x", "1.5");
    context.setParameter("y", "2.0");
    context.setParameter("theta", "0.0");
    
    // Execute (will use placeholder implementation)
    ToolResult result = tool->execute(context);
    
    std::cout << "Tool Name: " << tool->getToolName() << std::endl;
    std::cout << "ROS Topic: " << tool->getToolEndpoint() << std::endl;
    std::cout << "Status: " << (result.isSuccess() ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Output: " << result.getOutput() << std::endl;
}

/**
 * Example 4: AtomSpace Query Tool
 * Demonstrates querying AtomSpace with tool wrapper
 */
void example_atomspace_query_tool()
{
    std::cout << "\n=== Example 4: AtomSpace Query Tool ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create some test data in AtomSpace
    Handle person1 = atomspace->add_node(CONCEPT_NODE, "Person_Alice");
    Handle person2 = atomspace->add_node(CONCEPT_NODE, "Person_Bob");
    Handle location = atomspace->add_node(CONCEPT_NODE, "Location_Office");
    
    HandleSeq alice_at_office;
    alice_at_office.push_back(person1);
    alice_at_office.push_back(location);
    atomspace->add_link(EVALUATION_LINK, std::move(alice_at_office));
    
    // Create query tool
    auto tool = std::make_shared<ToolWrapper>("location_query", ToolType::ATOMSPACE_QUERY, atomspace);
    tool->setDescription("Queries person locations from AtomSpace");
    
    // Create execution context with input atoms
    ToolExecutionContext context(atomspace);
    context.addInputAtom(person1);
    context.addInputAtom(person2);
    
    // Execute query
    ToolResult result = tool->execute(context);
    
    std::cout << "Tool Name: " << tool->getToolName() << std::endl;
    std::cout << "Status: " << (result.isSuccess() ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Output: " << result.getOutput() << std::endl;
    std::cout << "AtomSpace Results: " << result.getAtomSpaceResults().size() << " atoms" << std::endl;
}

/**
 * Example 5: Multiple Tool Execution with Statistics
 * Demonstrates executing a tool multiple times and tracking statistics
 */
void example_tool_statistics()
{
    std::cout << "\n=== Example 5: Tool Statistics ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create a tool with variable success rate
    auto tool = std::make_shared<ToolWrapper>("random_processor", ToolType::CUSTOM, atomspace);
    
    int execution_number = 0;
    tool->setCustomExecutor([&execution_number](const ToolExecutionContext& context) {
        execution_number++;
        
        // Succeed 80% of the time
        if (execution_number % 5 != 0) {
            ToolResult result(ToolStatus::COMPLETED);
            result.setOutput("Processing successful");
            return result;
        } else {
            ToolResult result(ToolStatus::FAILED);
            result.setErrorMessage("Simulated failure");
            return result;
        }
    });
    
    // Execute multiple times
    ToolExecutionContext context(atomspace);
    for (int i = 0; i < 10; i++) {
        tool->execute(context);
    }
    
    std::cout << "Tool Name: " << tool->getToolName() << std::endl;
    std::cout << "Total Executions: " << tool->getExecutionCount() << std::endl;
    std::cout << "Success Rate: " << (tool->getSuccessRate() * 100) << "%" << std::endl;
    std::cout << "Average Execution Time: " << tool->getAverageExecutionTime() << "ms" << std::endl;
    std::cout << "Full Statistics: " << tool->getStatistics() << std::endl;
}

/**
 * Example 6: Tool Integration with Agent-Zero
 * Demonstrates how ToolWrapper integrates with Agent-Zero cognitive architecture
 */
void example_agentzero_integration()
{
    std::cout << "\n=== Example 6: Agent-Zero Integration ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Create agent representation
    Handle agent_self = atomspace->add_node(CONCEPT_NODE, "Agent_Zero");
    
    // Create multiple tools
    auto vision_tool = std::make_shared<ToolWrapper>("vision_processor", ToolType::EXTERNAL_REST_API, atomspace);
    auto planning_tool = std::make_shared<ToolWrapper>("path_planner", ToolType::CUSTOM, atomspace);
    auto action_tool = std::make_shared<ToolWrapper>("robot_action", ToolType::ROS_BEHAVIOR, atomspace);
    
    // Configure tools
    vision_tool->setDescription("Processes visual input");
    planning_tool->setDescription("Plans robot paths");
    action_tool->setDescription("Executes robot actions");
    
    planning_tool->setCustomExecutor([](const ToolExecutionContext& context) {
        ToolResult result(ToolStatus::COMPLETED);
        result.setOutput("Path planned: waypoint1 -> waypoint2 -> goal");
        
        // Return result as atoms in AtomSpace
        if (context.getAtomSpace()) {
            Handle plan = context.getAtomSpace()->add_node(CONCEPT_NODE, "Plan_123");
            result.addAtomSpaceResult(plan);
        }
        
        return result;
    });
    
    std::cout << "Agent Representation: " << agent_self->to_short_string() << std::endl;
    std::cout << "Available Tools:" << std::endl;
    std::cout << "  - " << vision_tool->getToolName() << " (Tool Atom: " 
              << vision_tool->getToolAtom()->to_short_string() << ")" << std::endl;
    std::cout << "  - " << planning_tool->getToolName() << " (Tool Atom: " 
              << planning_tool->getToolAtom()->to_short_string() << ")" << std::endl;
    std::cout << "  - " << action_tool->getToolName() << " (Tool Atom: " 
              << action_tool->getToolAtom()->to_short_string() << ")" << std::endl;
    
    // Execute planning tool
    ToolExecutionContext context(atomspace);
    context.setParameter("start", "current_position");
    context.setParameter("goal", "target_position");
    
    ToolResult result = planning_tool->execute(context);
    
    std::cout << "\nPlanning Tool Execution:" << std::endl;
    std::cout << "  Status: " << (result.isSuccess() ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "  Output: " << result.getOutput() << std::endl;
    std::cout << "  AtomSpace Size: " << atomspace->get_size() << " atoms" << std::endl;
}

/**
 * Main function - runs all examples
 */
int main(int argc, char* argv[])
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_timestamp_flag(false);
    logger().set_print_level_flag(true);
    
    std::cout << "========================================" << std::endl;
    std::cout << "ToolWrapper Unified Interface Demo" << std::endl;
    std::cout << "AZ-TOOL-002 - Phase 8: Tool Integration" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        example_custom_tool();
        example_rest_api_tool();
        example_ros_behavior_tool();
        example_atomspace_query_tool();
        example_tool_statistics();
        example_agentzero_integration();
        
        std::cout << "========================================" << std::endl;
        std::cout << "All examples completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
