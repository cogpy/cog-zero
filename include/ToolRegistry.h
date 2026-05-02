/*
 * opencog/agentzero/ToolRegistry.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Tool Registry Implementation
 * Catalog of available tools and capabilities with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_TOOL_REGISTRY_H
#define _OPENCOG_AGENTZERO_TOOL_REGISTRY_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <set>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

/**
 * ToolRegistry - Catalog of available tools and capabilities
 *
 * This class provides a comprehensive catalog system for managing
 * external tools and capabilities that Agent-Zero can utilize.
 * It integrates with OpenCog's AtomSpace for tool representation
 * and supports dynamic tool discovery and composition.
 *
 * Key features:
 * - Tool registration and discovery
 * - Capability-based tool matching
 * - Tool composition for complex tasks
 * - Integration with external-tools and ros-behavior-scripting
 * - AtomSpace representation of tool metadata
 * - Dynamic tool availability tracking
 */
class ToolRegistry
{
public:
    // Tool categories for classification
    enum class ToolCategory {
        VISUALIZATION,      // AtomSpace visualization tools
        ANALYSIS,          // Data analysis and statistics
        IMPORT_EXPORT,     // Data import/export utilities
        ROBOTICS,          // Robotics and ROS integration
        PERCEPTION,        // Sensory and perception tools
        MOTOR_CONTROL,     // Motor and movement control
        COMMUNICATION,     // Communication interfaces
        UTILITY,           // General utility tools
        CUSTOM             // Custom tool implementations
    };
    
    // Tool execution status
    enum class ToolStatus {
        AVAILABLE,         // Tool is ready to use
        BUSY,             // Tool is currently executing
        UNAVAILABLE,      // Tool is not available
        ERROR,            // Tool encountered an error
        DEPRECATED        // Tool is deprecated
    };
    
    // Tool capability flags
    enum class ToolCapability {
        READ_ONLY,        // Tool only reads data
        READ_WRITE,       // Tool can modify data
        ASYNC_EXECUTION,  // Tool supports asynchronous execution
        BATCH_PROCESSING, // Tool can process multiple items
        REAL_TIME,        // Tool operates in real-time
        REQUIRES_ROS,     // Tool requires ROS environment
        NETWORK_ACCESS    // Tool requires network access
    };

    // Tool metadata structure
    struct ToolMetadata {
        std::string name;
        std::string description;
        ToolCategory category;
        std::vector<ToolCapability> capabilities;
        std::vector<std::string> dependencies;
        std::string version;
        double reliability_score;
        int usage_count;
        
        ToolMetadata() 
            : category(ToolCategory::UTILITY)
            , reliability_score(1.0)
            , usage_count(0) {}
    };
    
    // Tool execution callback type
    using ToolExecutor = std::function<Handle(const HandleSeq&, AtomSpacePtr)>;

private:
    // Core references
    AtomSpacePtr _atomspace;
    
    // Tool management structures
    std::map<std::string, ToolMetadata> _tool_catalog;
    std::map<std::string, ToolExecutor> _tool_executors;
    std::map<std::string, ToolStatus> _tool_status;
    std::map<ToolCategory, std::set<std::string>> _tools_by_category;
    std::map<std::string, Handle> _tool_atoms;
    
    // AtomSpace handles for tool management
    Handle _tool_registry_root;
    Handle _external_tools_context;
    Handle _ros_tools_context;
    Handle _available_tools_context;
    
    // Configuration
    bool _enable_tool_composition;
    bool _enable_capability_matching;
    bool _track_tool_usage;
    double _minimum_reliability_threshold;
    
    // Internal methods
    void initializeToolRegistry();
    void discoverExternalTools();
    void discoverROSTools();
    Handle createToolAtom(const ToolMetadata& metadata);
    void updateToolStatus(const std::string& tool_name, ToolStatus status);
    bool checkToolDependencies(const std::string& tool_name);
    std::vector<std::string> findToolsByCapability(const std::vector<ToolCapability>& required_capabilities) const;
    double calculateToolReliability(const std::string& tool_name);
    void updateToolUsageStatistics(const std::string& tool_name);
    
    // Tool composition methods
    std::vector<std::string> composeToolChain(const std::string& task_description);
    bool validateToolComposition(const std::vector<std::string>& tool_chain);
    
public:
    /**
     * Constructor
     * 
     * @param atomspace Shared pointer to the AtomSpace for tool representation
     */
    explicit ToolRegistry(AtomSpacePtr atomspace);
    
    /**
     * Destructor
     */
    ~ToolRegistry();
    
    // Tool registration methods
    
    /**
     * Register a new tool in the catalog
     * 
     * @param metadata Tool metadata including name, description, category
     * @param executor Function to execute the tool
     * @return Handle to the tool atom in AtomSpace
     */
    Handle registerTool(const ToolMetadata& metadata, ToolExecutor executor);
    
    /**
     * Unregister a tool from the catalog
     * 
     * @param tool_name Name of the tool to unregister
     * @return true if successful, false otherwise
     */
    bool unregisterTool(const std::string& tool_name);
    
    /**
     * Check if a tool is registered
     * 
     * @param tool_name Name of the tool
     * @return true if tool is registered, false otherwise
     */
    bool isToolRegistered(const std::string& tool_name) const;
    
    // Tool discovery methods
    
    /**
     * Get all registered tools
     * 
     * @return Vector of tool names
     */
    std::vector<std::string> getAllTools() const;
    
    /**
     * Get tools by category
     * 
     * @param category Tool category to filter by
     * @return Vector of tool names in the category
     */
    std::vector<std::string> getToolsByCategory(ToolCategory category) const;
    
    /**
     * Get tools with specific capabilities
     * 
     * @param capabilities Required capabilities
     * @return Vector of tool names matching capabilities
     */
    std::vector<std::string> getToolsByCapabilities(const std::vector<ToolCapability>& capabilities) const;
    
    /**
     * Search tools by description keywords
     * 
     * @param keywords Search keywords
     * @return Vector of matching tool names
     */
    std::vector<std::string> searchTools(const std::string& keywords) const;
    
    // Tool metadata access
    
    /**
     * Get metadata for a specific tool
     * 
     * @param tool_name Name of the tool
     * @return Tool metadata (empty if not found)
     */
    ToolMetadata getToolMetadata(const std::string& tool_name) const;
    
    /**
     * Get current status of a tool
     * 
     * @param tool_name Name of the tool
     * @return Current tool status
     */
    ToolStatus getToolStatus(const std::string& tool_name) const;
    
    /**
     * Get AtomSpace handle for a tool
     * 
     * @param tool_name Name of the tool
     * @return Handle to tool atom
     */
    Handle getToolAtom(const std::string& tool_name) const;
    
    // Tool execution methods
    
    /**
     * Execute a registered tool
     * 
     * @param tool_name Name of the tool to execute
     * @param arguments Tool arguments as HandleSeq
     * @return Result handle from tool execution
     */
    Handle executeTool(const std::string& tool_name, const HandleSeq& arguments);
    
    /**
     * Execute a sequence of tools
     * 
     * @param tool_chain Vector of tool names to execute in sequence
     * @param initial_input Initial input for the tool chain
     * @return Final result handle
     */
    Handle executeToolChain(const std::vector<std::string>& tool_chain, const HandleSeq& initial_input);
    
    // Tool composition methods
    
    /**
     * Compose tools for a complex task
     * 
     * @param task_description Description of the task
     * @return Vector of tool names forming the composition
     */
    std::vector<std::string> composeTool(const std::string& task_description);
    
    /**
     * Validate if tools can be composed together
     * 
     * @param tool1 First tool name
     * @param tool2 Second tool name
     * @return true if composition is valid, false otherwise
     */
    bool canCompose(const std::string& tool1, const std::string& tool2) const;
    
    // Tool management methods
    
    /**
     * Update tool reliability score based on execution results
     * 
     * @param tool_name Name of the tool
     * @param success Whether the last execution was successful
     */
    void updateToolReliability(const std::string& tool_name, bool success);
    
    /**
     * Get statistics for all tools
     * 
     * @return Map of tool names to usage statistics
     */
    std::map<std::string, std::pair<int, double>> getToolStatistics() const;
    
    /**
     * Clear usage statistics for all tools
     */
    void clearStatistics();
    
    /**
     * Refresh tool availability by re-checking dependencies
     */
    void refreshToolAvailability();
    
    // Configuration methods
    
    /**
     * Enable or disable tool composition
     * 
     * @param enable true to enable, false to disable
     */
    void setEnableToolComposition(bool enable);
    
    /**
     * Enable or disable capability matching
     * 
     * @param enable true to enable, false to disable
     */
    void setEnableCapabilityMatching(bool enable);
    
    /**
     * Set minimum reliability threshold for tool selection
     * 
     * @param threshold Reliability threshold (0.0 to 1.0)
     */
    void setMinimumReliabilityThreshold(double threshold);
    
    /**
     * Get current configuration status
     * 
     * @return Map of configuration keys to values
     */
    std::map<std::string, std::string> getConfiguration() const;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TOOL_REGISTRY_H
