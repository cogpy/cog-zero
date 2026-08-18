/*
 * src/ToolRegistry.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Tool Registry Implementation
 * Catalog of available tools and capabilities with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ToolRegistry.h"

using namespace opencog;
using namespace opencog::agentzero;

ToolRegistry::ToolRegistry(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _tool_registry_root(Handle::UNDEFINED)
    , _external_tools_context(Handle::UNDEFINED)
    , _ros_tools_context(Handle::UNDEFINED)
    , _available_tools_context(Handle::UNDEFINED)
    , _enable_tool_composition(true)
    , _enable_capability_matching(true)
    , _track_tool_usage(true)
    , _minimum_reliability_threshold(0.5)
{
    logger().info() << "[ToolRegistry] Initializing tool catalog system";
    initializeToolRegistry();
    discoverExternalTools();
    discoverROSTools();
}

ToolRegistry::~ToolRegistry()
{
    logger().info() << "[ToolRegistry] Shutting down tool catalog system";
}

void ToolRegistry::initializeToolRegistry()
{
    logger().debug() << "[ToolRegistry] Creating AtomSpace structures for tool management";
    
    // Create root context for tool registry
    _tool_registry_root = _atomspace->add_node(CONCEPT_NODE, "ToolRegistry");
    
    // Create contexts for different tool sources
    _external_tools_context = _atomspace->add_node(CONCEPT_NODE, "ExternalTools");
    _ros_tools_context = _atomspace->add_node(CONCEPT_NODE, "ROSTools");
    _available_tools_context = _atomspace->add_node(CONCEPT_NODE, "AvailableTools");
    
    // Link contexts to registry root
    HandleSeq external_links;
    external_links.push_back(_tool_registry_root);
    external_links.push_back(_external_tools_context);
    _atomspace->add_link(MEMBER_LINK, std::move(external_links));
    
    HandleSeq ros_links;
    ros_links.push_back(_tool_registry_root);
    ros_links.push_back(_ros_tools_context);
    _atomspace->add_link(MEMBER_LINK, std::move(ros_links));
    
    logger().info() << "[ToolRegistry] Tool registry structures initialized";
}

void ToolRegistry::discoverExternalTools()
{
    logger().debug() << "[ToolRegistry] Discovering external-tools components";
    
    // Register visualization tools from external-tools
    ToolMetadata atomspace_subscriber;
    atomspace_subscriber.name = "AtomSpaceSubscriber";
    atomspace_subscriber.description = "Subscribe to AtomSpace changes and visualize data";
    atomspace_subscriber.category = ToolCategory::VISUALIZATION;
    atomspace_subscriber.capabilities = {ToolCapability::READ_ONLY, ToolCapability::ASYNC_EXECUTION};
    atomspace_subscriber.version = "1.0.0";
    atomspace_subscriber.reliability_score = 0.95;
    
    // Create executor for AtomSpaceSubscriber
    ToolExecutor subscriber_executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        // Placeholder implementation - would integrate with actual AtomSpaceSubscriber
        return as->add_node(CONCEPT_NODE, "AtomSpaceSubscriberResult");
    };
    
    registerTool(atomspace_subscriber, subscriber_executor);
    
    // Register import/export tools
    ToolMetadata cycl_importer;
    cycl_importer.name = "CyclImporter";
    cycl_importer.description = "Import knowledge from Cyc/OpenCyc format";
    cycl_importer.category = ToolCategory::IMPORT_EXPORT;
    cycl_importer.capabilities = {ToolCapability::READ_WRITE, ToolCapability::BATCH_PROCESSING};
    cycl_importer.version = "1.0.0";
    cycl_importer.reliability_score = 0.85;
    
    ToolExecutor cycl_executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        // Placeholder for Cyc import functionality
        return as->add_node(CONCEPT_NODE, "CyclImportResult");
    };
    
    registerTool(cycl_importer, cycl_executor);
    
    logger().info() << "[ToolRegistry] Discovered " << _tool_catalog.size() << " external tools";
}

void ToolRegistry::discoverROSTools()
{
    logger().debug() << "[ToolRegistry] Discovering ROS behavior scripting tools";
    
    // Register perception tools from ros-behavior-scripting
    ToolMetadata vision_sensor;
    vision_sensor.name = "VisionSensor";
    vision_sensor.description = "Process visual sensory input from ROS vision topics";
    vision_sensor.category = ToolCategory::PERCEPTION;
    vision_sensor.capabilities = {
        ToolCapability::READ_ONLY, 
        ToolCapability::REAL_TIME, 
        ToolCapability::REQUIRES_ROS
    };
    vision_sensor.dependencies = {"ROS", "spacetime"};
    vision_sensor.version = "1.0.0";
    vision_sensor.reliability_score = 0.90;
    
    ToolExecutor vision_executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        // Placeholder for vision sensor integration
        return as->add_node(CONCEPT_NODE, "VisionSensorData");
    };
    
    registerTool(vision_sensor, vision_executor);
    
    // Register motor control tools
    ToolMetadata motor_control;
    motor_control.name = "MotorController";
    motor_control.description = "Control robot motor movements via ROS";
    motor_control.category = ToolCategory::MOTOR_CONTROL;
    motor_control.capabilities = {
        ToolCapability::READ_WRITE,
        ToolCapability::REAL_TIME,
        ToolCapability::REQUIRES_ROS
    };
    motor_control.dependencies = {"ROS", "motor"};
    motor_control.version = "1.0.0";
    motor_control.reliability_score = 0.88;
    
    ToolExecutor motor_executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        // Placeholder for motor control
        return as->add_node(CONCEPT_NODE, "MotorControlResult");
    };
    
    registerTool(motor_control, motor_executor);
    
    logger().info() << "[ToolRegistry] Discovered ROS tools, total catalog size: " 
                    << _tool_catalog.size();
}

Handle ToolRegistry::createToolAtom(const ToolMetadata& metadata)
{
    // Create tool node
    Handle tool_node = _atomspace->add_node(CONCEPT_NODE, "Tool:" + metadata.name);
    
    // Add description (need to copy since metadata.description is const)
    std::string desc_copy = metadata.description;
    Handle desc_node = _atomspace->add_node(CONCEPT_NODE, std::move(desc_copy));
    HandleSeq desc_link;
    desc_link.push_back(tool_node);
    desc_link.push_back(desc_node);
    _atomspace->add_link(INHERITANCE_LINK, std::move(desc_link));
    
    // Add category
    std::string category_name;
    switch (metadata.category) {
        case ToolCategory::VISUALIZATION: category_name = "Visualization"; break;
        case ToolCategory::ANALYSIS: category_name = "Analysis"; break;
        case ToolCategory::IMPORT_EXPORT: category_name = "ImportExport"; break;
        case ToolCategory::ROBOTICS: category_name = "Robotics"; break;
        case ToolCategory::PERCEPTION: category_name = "Perception"; break;
        case ToolCategory::MOTOR_CONTROL: category_name = "MotorControl"; break;
        case ToolCategory::COMMUNICATION: category_name = "Communication"; break;
        case ToolCategory::UTILITY: category_name = "Utility"; break;
        case ToolCategory::CUSTOM: category_name = "Custom"; break;
    }
    
    Handle category_node = _atomspace->add_node(CONCEPT_NODE, "ToolCategory:" + category_name);
    HandleSeq cat_link;
    cat_link.push_back(tool_node);
    cat_link.push_back(category_node);
    _atomspace->add_link(MEMBER_LINK, std::move(cat_link));
    
    // Set truth value based on reliability
    TruthValuePtr tv = SimpleTruthValue::createTV(metadata.reliability_score, 0.9);
    tool_node->setTruthValue(tv);
    
    // Link to available tools context
    HandleSeq avail_link;
    avail_link.push_back(_available_tools_context);
    avail_link.push_back(tool_node);
    _atomspace->add_link(MEMBER_LINK, std::move(avail_link));
    
    return tool_node;
}

Handle ToolRegistry::registerTool(const ToolMetadata& metadata, ToolExecutor executor)
{
    logger().debug() << "[ToolRegistry] Registering tool: " << metadata.name;
    
    // Store metadata
    _tool_catalog[metadata.name] = metadata;
    
    // Store executor
    _tool_executors[metadata.name] = executor;
    
    // Set initial status
    _tool_status[metadata.name] = ToolStatus::AVAILABLE;
    
    // Add to category index
    _tools_by_category[metadata.category].insert(metadata.name);
    
    // Create AtomSpace representation
    Handle tool_atom = createToolAtom(metadata);
    _tool_atoms[metadata.name] = tool_atom;
    
    logger().info() << "[ToolRegistry] Registered tool: " << metadata.name;
    
    return tool_atom;
}

bool ToolRegistry::unregisterTool(const std::string& tool_name)
{
    logger().debug() << "[ToolRegistry] Unregistering tool: " << tool_name;
    
    auto it = _tool_catalog.find(tool_name);
    if (it == _tool_catalog.end()) {
        logger().warn() << "[ToolRegistry] Tool not found: " << tool_name;
        return false;
    }
    
    // Remove from category index
    ToolCategory category = it->second.category;
    _tools_by_category[category].erase(tool_name);
    
    // Remove from all maps
    _tool_catalog.erase(tool_name);
    _tool_executors.erase(tool_name);
    _tool_status.erase(tool_name);
    _tool_atoms.erase(tool_name);
    
    logger().info() << "[ToolRegistry] Unregistered tool: " << tool_name;
    
    return true;
}

bool ToolRegistry::isToolRegistered(const std::string& tool_name) const
{
    return _tool_catalog.find(tool_name) != _tool_catalog.end();
}

std::vector<std::string> ToolRegistry::getAllTools() const
{
    std::vector<std::string> tools;
    tools.reserve(_tool_catalog.size());
    
    for (const auto& pair : _tool_catalog) {
        tools.push_back(pair.first);
    }
    
    return tools;
}

std::vector<std::string> ToolRegistry::getToolsByCategory(ToolCategory category) const
{
    std::vector<std::string> tools;
    
    auto it = _tools_by_category.find(category);
    if (it != _tools_by_category.end()) {
        tools.assign(it->second.begin(), it->second.end());
    }
    
    return tools;
}

std::vector<std::string> ToolRegistry::getToolsByCapabilities(
    const std::vector<ToolCapability>& capabilities) const
{
    if (!_enable_capability_matching) {
        return getAllTools();
    }
    
    return findToolsByCapability(capabilities);
}

std::vector<std::string> ToolRegistry::findToolsByCapability(
    const std::vector<ToolCapability>& required_capabilities) const
{
    std::vector<std::string> matching_tools;
    
    for (const auto& pair : _tool_catalog) {
        const ToolMetadata& metadata = pair.second;
        bool has_all_capabilities = true;
        
        for (const auto& required_cap : required_capabilities) {
            bool has_capability = std::find(
                metadata.capabilities.begin(),
                metadata.capabilities.end(),
                required_cap
            ) != metadata.capabilities.end();
            
            if (!has_capability) {
                has_all_capabilities = false;
                break;
            }
        }
        
        if (has_all_capabilities) {
            matching_tools.push_back(pair.first);
        }
    }
    
    return matching_tools;
}

std::vector<std::string> ToolRegistry::searchTools(const std::string& keywords) const
{
    std::vector<std::string> matching_tools;
    std::string lower_keywords = keywords;
    std::transform(lower_keywords.begin(), lower_keywords.end(), 
                   lower_keywords.begin(), ::tolower);
    
    for (const auto& pair : _tool_catalog) {
        const ToolMetadata& metadata = pair.second;
        
        // Search in name
        std::string lower_name = metadata.name;
        std::transform(lower_name.begin(), lower_name.end(), 
                      lower_name.begin(), ::tolower);
        
        // Search in description
        std::string lower_desc = metadata.description;
        std::transform(lower_desc.begin(), lower_desc.end(), 
                      lower_desc.begin(), ::tolower);
        
        if (lower_name.find(lower_keywords) != std::string::npos ||
            lower_desc.find(lower_keywords) != std::string::npos) {
            matching_tools.push_back(pair.first);
        }
    }
    
    return matching_tools;
}

ToolRegistry::ToolMetadata ToolRegistry::getToolMetadata(const std::string& tool_name) const
{
    auto it = _tool_catalog.find(tool_name);
    if (it != _tool_catalog.end()) {
        return it->second;
    }
    return ToolMetadata();
}

ToolRegistry::ToolStatus ToolRegistry::getToolStatus(const std::string& tool_name) const
{
    auto it = _tool_status.find(tool_name);
    if (it != _tool_status.end()) {
        return it->second;
    }
    return ToolStatus::UNAVAILABLE;
}

Handle ToolRegistry::getToolAtom(const std::string& tool_name) const
{
    auto it = _tool_atoms.find(tool_name);
    if (it != _tool_atoms.end()) {
        return it->second;
    }
    return Handle::UNDEFINED;
}

void ToolRegistry::updateToolStatus(const std::string& tool_name, ToolStatus status)
{
    auto it = _tool_status.find(tool_name);
    if (it != _tool_status.end()) {
        it->second = status;
        logger().debug() << "[ToolRegistry] Updated status for " << tool_name;
    }
}

bool ToolRegistry::checkToolDependencies(const std::string& tool_name)
{
    auto it = _tool_catalog.find(tool_name);
    if (it == _tool_catalog.end()) {
        return false;
    }
    
    // For now, we assume all dependencies are met
    // In a real implementation, this would check if ROS is running,
    // if required packages are installed, etc.
    return true;
}

Handle ToolRegistry::executeTool(const std::string& tool_name, const HandleSeq& arguments)
{
    logger().debug() << "[ToolRegistry] Executing tool: " << tool_name;
    
    // Check if tool exists
    auto executor_it = _tool_executors.find(tool_name);
    if (executor_it == _tool_executors.end()) {
        logger().error() << "[ToolRegistry] Tool not found: " << tool_name;
        return Handle::UNDEFINED;
    }
    
    // Check if tool is available
    ToolStatus status = getToolStatus(tool_name);
    if (status != ToolStatus::AVAILABLE) {
        logger().warn() << "[ToolRegistry] Tool not available: " << tool_name;
        return Handle::UNDEFINED;
    }
    
    // Check dependencies
    if (!checkToolDependencies(tool_name)) {
        logger().error() << "[ToolRegistry] Tool dependencies not met: " << tool_name;
        return Handle::UNDEFINED;
    }
    
    // Update status to busy
    updateToolStatus(tool_name, ToolStatus::BUSY);
    
    // Execute tool
    Handle result;
    try {
        result = executor_it->second(arguments, _atomspace);
        
        // Update statistics
        if (_track_tool_usage) {
            updateToolUsageStatistics(tool_name);
            updateToolReliability(tool_name, true);
        }
        
        // Update status back to available
        updateToolStatus(tool_name, ToolStatus::AVAILABLE);
        
        logger().info() << "[ToolRegistry] Tool execution successful: " << tool_name;
        
    } catch (const std::exception& e) {
        logger().error() << "[ToolRegistry] Tool execution failed: " << tool_name 
                        << " - " << e.what();
        updateToolStatus(tool_name, ToolStatus::ERROR);
        updateToolReliability(tool_name, false);
        result = Handle::UNDEFINED;
    }
    
    return result;
}

Handle ToolRegistry::executeToolChain(const std::vector<std::string>& tool_chain, 
                                     const HandleSeq& initial_input)
{
    logger().debug() << "[ToolRegistry] Executing tool chain with " 
                    << tool_chain.size() << " tools";
    
    if (!validateToolComposition(tool_chain)) {
        logger().error() << "[ToolRegistry] Tool chain validation failed";
        return Handle::UNDEFINED;
    }
    
    HandleSeq current_input = initial_input;
    
    for (const std::string& tool_name : tool_chain) {
        Handle result = executeTool(tool_name, current_input);
        
        if (result == Handle::UNDEFINED) {
            logger().error() << "[ToolRegistry] Tool chain execution failed at: " 
                           << tool_name;
            return Handle::UNDEFINED;
        }
        
        // Use result as input for next tool
        current_input.clear();
        current_input.push_back(result);
    }
    
    logger().info() << "[ToolRegistry] Tool chain execution completed successfully";
    
    return current_input.empty() ? Handle::UNDEFINED : current_input[0];
}

std::vector<std::string> ToolRegistry::composeToolChain(const std::string& task_description)
{
    if (!_enable_tool_composition) {
        return {};
    }
    
    return composeTool(task_description);
}

std::vector<std::string> ToolRegistry::composeTool(const std::string& task_description)
{
    logger().debug() << "[ToolRegistry] Composing tools for task: " << task_description;
    
    // Simple heuristic-based composition
    // In a real implementation, this would use PLN reasoning or pattern matching
    std::vector<std::string> composition;
    
    // Example: if task involves visualization, use visualization tools
    if (task_description.find("visualize") != std::string::npos ||
        task_description.find("display") != std::string::npos) {
        auto viz_tools = getToolsByCategory(ToolCategory::VISUALIZATION);
        if (!viz_tools.empty()) {
            composition.push_back(viz_tools[0]);
        }
    }
    
    // Example: if task involves robot control, use robotics tools
    if (task_description.find("robot") != std::string::npos ||
        task_description.find("move") != std::string::npos) {
        auto robot_tools = getToolsByCategory(ToolCategory::MOTOR_CONTROL);
        if (!robot_tools.empty()) {
            composition.push_back(robot_tools[0]);
        }
    }
    
    return composition;
}

bool ToolRegistry::validateToolComposition(const std::vector<std::string>& tool_chain)
{
    if (tool_chain.empty()) {
        return false;
    }
    
    // Check if all tools exist
    for (const std::string& tool_name : tool_chain) {
        if (!isToolRegistered(tool_name)) {
            logger().warn() << "[ToolRegistry] Unknown tool in chain: " << tool_name;
            return false;
        }
    }
    
    // Check if consecutive tools can be composed
    for (size_t i = 0; i < tool_chain.size() - 1; ++i) {
        if (!canCompose(tool_chain[i], tool_chain[i + 1])) {
            logger().warn() << "[ToolRegistry] Tools cannot be composed: " 
                           << tool_chain[i] << " -> " << tool_chain[i + 1];
            return false;
        }
    }
    
    return true;
}

bool ToolRegistry::canCompose(const std::string& tool1, const std::string& tool2) const
{
    // Simple compatibility check
    // In a real implementation, this would check output/input type compatibility
    return isToolRegistered(tool1) && isToolRegistered(tool2);
}

void ToolRegistry::updateToolUsageStatistics(const std::string& tool_name)
{
    auto it = _tool_catalog.find(tool_name);
    if (it != _tool_catalog.end()) {
        it->second.usage_count++;
    }
}

double ToolRegistry::calculateToolReliability(const std::string& tool_name)
{
    auto it = _tool_catalog.find(tool_name);
    if (it != _tool_catalog.end()) {
        return it->second.reliability_score;
    }
    return 0.0;
}

void ToolRegistry::updateToolReliability(const std::string& tool_name, bool success)
{
    auto it = _tool_catalog.find(tool_name);
    if (it == _tool_catalog.end()) {
        return;
    }
    
    // Update reliability using exponential moving average
    double current_reliability = it->second.reliability_score;
    double alpha = 0.1; // Learning rate
    double new_value = success ? 1.0 : 0.0;
    
    it->second.reliability_score = (1 - alpha) * current_reliability + alpha * new_value;
    
    // Update AtomSpace representation
    Handle tool_atom = getToolAtom(tool_name);
    if (tool_atom != Handle::UNDEFINED) {
        TruthValuePtr tv = SimpleTruthValue::createTV(it->second.reliability_score, 0.9);
        tool_atom->setTruthValue(tv);
    }
    
    logger().debug() << "[ToolRegistry] Updated reliability for " << tool_name 
                    << ": " << it->second.reliability_score;
}

std::map<std::string, std::pair<int, double>> ToolRegistry::getToolStatistics() const
{
    std::map<std::string, std::pair<int, double>> stats;
    
    for (const auto& pair : _tool_catalog) {
        const std::string& name = pair.first;
        const ToolMetadata& metadata = pair.second;
        stats[name] = std::make_pair(metadata.usage_count, metadata.reliability_score);
    }
    
    return stats;
}

void ToolRegistry::clearStatistics()
{
    logger().info() << "[ToolRegistry] Clearing usage statistics";
    
    for (auto& pair : _tool_catalog) {
        pair.second.usage_count = 0;
    }
}

void ToolRegistry::refreshToolAvailability()
{
    logger().debug() << "[ToolRegistry] Refreshing tool availability";
    
    for (auto& pair : _tool_status) {
        const std::string& tool_name = pair.first;
        
        if (checkToolDependencies(tool_name)) {
            if (pair.second == ToolStatus::UNAVAILABLE) {
                pair.second = ToolStatus::AVAILABLE;
                logger().info() << "[ToolRegistry] Tool now available: " << tool_name;
            }
        } else {
            pair.second = ToolStatus::UNAVAILABLE;
            logger().warn() << "[ToolRegistry] Tool unavailable: " << tool_name;
        }
    }
}

void ToolRegistry::setEnableToolComposition(bool enable)
{
    _enable_tool_composition = enable;
    logger().info() << "[ToolRegistry] Tool composition " 
                   << (enable ? "enabled" : "disabled");
}

void ToolRegistry::setEnableCapabilityMatching(bool enable)
{
    _enable_capability_matching = enable;
    logger().info() << "[ToolRegistry] Capability matching " 
                   << (enable ? "enabled" : "disabled");
}

void ToolRegistry::setMinimumReliabilityThreshold(double threshold)
{
    if (threshold >= 0.0 && threshold <= 1.0) {
        _minimum_reliability_threshold = threshold;
        logger().info() << "[ToolRegistry] Minimum reliability threshold set to: " 
                       << threshold;
    } else {
        logger().warn() << "[ToolRegistry] Invalid reliability threshold: " << threshold;
    }
}

std::map<std::string, std::string> ToolRegistry::getConfiguration() const
{
    std::map<std::string, std::string> config;
    
    config["enable_tool_composition"] = _enable_tool_composition ? "true" : "false";
    config["enable_capability_matching"] = _enable_capability_matching ? "true" : "false";
    config["track_tool_usage"] = _track_tool_usage ? "true" : "false";
    config["minimum_reliability_threshold"] = std::to_string(_minimum_reliability_threshold);
    config["total_registered_tools"] = std::to_string(_tool_catalog.size());
    
    return config;
}
