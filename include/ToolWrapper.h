/*
 * opencog/agentzero/tools/ToolWrapper.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ToolWrapper Unified Interface
 * Provides unified interface for external tool integration
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 * Task ID: AZ-TOOL-002
 */

#ifndef _OPENCOG_AGENTZERO_TOOLWRAPPER_H
#define _OPENCOG_AGENTZERO_TOOLWRAPPER_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace tools {

// Forward declarations
class ToolExecutionContext;
class ToolResult;

/**
 * ToolType - Enumeration of supported tool types
 */
enum class ToolType {
    EXTERNAL_REST_API,      // External tools via REST API
    ROS_BEHAVIOR,           // ROS behavior scripting tools
    PYTHON_SCRIPT,          // Python script execution
    SHELL_COMMAND,          // Shell command execution
    ATOMSPACE_QUERY,        // AtomSpace query tools
    CUSTOM                  // Custom tool implementations
};

/**
 * ToolStatus - Status of tool execution
 */
enum class ToolStatus {
    NOT_STARTED,
    RUNNING,
    COMPLETED,
    FAILED,
    TIMEOUT,
    CANCELLED
};

/**
 * ToolResult - Result of tool execution
 * 
 * Encapsulates the result of executing a tool, including:
 * - Status of execution
 * - Output data (as string, AtomSpace handles, or structured data)
 * - Error information if execution failed
 * - Execution metadata (timing, resource usage, etc.)
 */
class ToolResult {
private:
    ToolStatus _status;
    std::string _output;
    std::string _error_message;
    HandleSeq _atomspace_results;
    std::map<std::string, std::string> _metadata;
    double _execution_time_ms;

public:
    ToolResult(ToolStatus status = ToolStatus::NOT_STARTED);
    
    // Status management
    ToolStatus getStatus() const { return _status; }
    void setStatus(ToolStatus status) { _status = status; }
    bool isSuccess() const { return _status == ToolStatus::COMPLETED; }
    bool isFailure() const { return _status == ToolStatus::FAILED; }
    
    // Output management
    const std::string& getOutput() const { return _output; }
    void setOutput(const std::string& output) { _output = output; }
    
    // Error handling
    const std::string& getErrorMessage() const { return _error_message; }
    void setErrorMessage(const std::string& error) { _error_message = error; }
    
    // AtomSpace integration
    const HandleSeq& getAtomSpaceResults() const { return _atomspace_results; }
    void setAtomSpaceResults(const HandleSeq& results) { _atomspace_results = results; }
    void addAtomSpaceResult(const Handle& h) { _atomspace_results.push_back(h); }
    
    // Metadata management
    void setMetadata(const std::string& key, const std::string& value);
    std::string getMetadata(const std::string& key) const;
    const std::map<std::string, std::string>& getAllMetadata() const { return _metadata; }
    
    // Execution timing
    double getExecutionTime() const { return _execution_time_ms; }
    void setExecutionTime(double time_ms) { _execution_time_ms = time_ms; }
    
    // Serialization
    std::string toJSON() const;
    std::string toString() const;
};

/**
 * ToolExecutionContext - Context for tool execution
 * 
 * Provides the execution context for a tool, including:
 * - AtomSpace for knowledge representation
 * - Input parameters
 * - Execution configuration
 * - Resource constraints
 */
class ToolExecutionContext {
private:
    AtomSpacePtr _atomspace;
    std::map<std::string, std::string> _parameters;
    std::map<std::string, std::string> _config;
    HandleSeq _input_atoms;
    double _timeout_ms;
    bool _async_execution;

public:
    ToolExecutionContext(AtomSpacePtr atomspace = nullptr);
    
    // AtomSpace access
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    void setAtomSpace(AtomSpacePtr atomspace) { _atomspace = atomspace; }
    
    // Parameter management
    void setParameter(const std::string& key, const std::string& value);
    std::string getParameter(const std::string& key) const;
    bool hasParameter(const std::string& key) const;
    const std::map<std::string, std::string>& getAllParameters() const { return _parameters; }
    
    // Configuration management
    void setConfig(const std::string& key, const std::string& value);
    std::string getConfig(const std::string& key) const;
    const std::map<std::string, std::string>& getAllConfig() const { return _config; }
    
    // Input atoms
    void addInputAtom(const Handle& h) { _input_atoms.push_back(h); }
    void setInputAtoms(const HandleSeq& atoms) { _input_atoms = atoms; }
    const HandleSeq& getInputAtoms() const { return _input_atoms; }
    
    // Execution control
    void setTimeout(double timeout_ms) { _timeout_ms = timeout_ms; }
    double getTimeout() const { return _timeout_ms; }
    void setAsyncExecution(bool async) { _async_execution = async; }
    bool isAsyncExecution() const { return _async_execution; }
};

/**
 * ToolWrapper - Unified interface for external tool integration
 * 
 * This class provides a unified interface for integrating external tools
 * with the Agent-Zero cognitive architecture and OpenCog AtomSpace.
 * 
 * Key Features:
 * - Unified interface for multiple tool types (REST API, ROS, Python, etc.)
 * - AtomSpace integration for input/output
 * - Asynchronous and synchronous execution modes
 * - Error handling and timeout management
 * - Resource tracking and optimization
 * - Extensible design for custom tool types
 * 
 * Usage Example:
 * ```
 * auto tool = std::make_shared<ToolWrapper>("face_detector", ToolType::ROS_BEHAVIOR);
 * tool->setAtomSpace(atomspace);
 * tool->setToolEndpoint("http://localhost:5000/detect_face");
 * 
 * ToolExecutionContext context(atomspace);
 * context.setParameter("confidence_threshold", "0.8");
 * 
 * ToolResult result = tool->execute(context);
 * if (result.isSuccess()) {
 *     // Process results
 * }
 * ```
 */
class ToolWrapper {
private:
    // Tool identification
    std::string _tool_name;
    std::string _tool_description;
    ToolType _tool_type;
    std::string _tool_id;
    
    // Tool configuration
    std::string _tool_endpoint;  // URL for REST API, ROS topic, etc.
    std::map<std::string, std::string> _tool_config;
    std::vector<std::string> _required_parameters;
    
    // AtomSpace integration
    AtomSpacePtr _atomspace;
    Handle _tool_atom;  // AtomSpace representation of this tool
    
    // Execution state
    ToolStatus _current_status;
    std::shared_ptr<ToolResult> _last_result;
    
    // Statistics
    int _execution_count;
    int _success_count;
    int _failure_count;
    double _total_execution_time_ms;
    
    // Callbacks for custom tool types
    using ExecutionCallback = std::function<ToolResult(const ToolExecutionContext&)>;
    ExecutionCallback _custom_executor;
    
    // Internal methods
    void initializeToolAtom();
    ToolResult executeRESTTool(const ToolExecutionContext& context);
    ToolResult executeROSBehavior(const ToolExecutionContext& context);
    ToolResult executePythonScript(const ToolExecutionContext& context);
    ToolResult executeShellCommand(const ToolExecutionContext& context);
    ToolResult executeAtomSpaceQuery(const ToolExecutionContext& context);
    ToolResult executeCustomTool(const ToolExecutionContext& context);
    bool validateContext(const ToolExecutionContext& context) const;
    void updateStatistics(const ToolResult& result);
    void recordExecutionInAtomSpace(const ToolExecutionContext& context, const ToolResult& result);

public:
    /**
     * Constructor - Creates a ToolWrapper instance
     * @param tool_name Name identifier for this tool
     * @param tool_type Type of tool being wrapped
     * @param atomspace Optional AtomSpace for knowledge integration
     */
    ToolWrapper(const std::string& tool_name, 
                ToolType tool_type = ToolType::CUSTOM,
                AtomSpacePtr atomspace = nullptr);
    
    /**
     * Destructor - Cleans up resources
     */
    virtual ~ToolWrapper();
    
    // Core functionality
    /**
     * Execute the tool with given context
     * @param context Execution context with parameters and input
     * @return ToolResult containing execution results
     */
    ToolResult execute(const ToolExecutionContext& context);
    
    /**
     * Execute the tool asynchronously
     * @param context Execution context
     * @param callback Callback function called when execution completes
     * @return Tool execution ID for tracking
     * @note This method is not yet fully implemented and will be completed in a future update
     */
    std::string executeAsync(const ToolExecutionContext& context,
                            std::function<void(const ToolResult&)> callback = nullptr);
    
    // Configuration
    /**
     * Set the tool endpoint (URL, ROS topic, script path, etc.)
     * @param endpoint Tool endpoint identifier
     */
    void setToolEndpoint(const std::string& endpoint) { _tool_endpoint = endpoint; }
    
    /**
     * Get the tool endpoint
     * @return Tool endpoint string
     */
    const std::string& getToolEndpoint() const { return _tool_endpoint; }
    
    /**
     * Set tool configuration parameter
     * @param key Configuration key
     * @param value Configuration value
     */
    void setToolConfig(const std::string& key, const std::string& value);
    
    /**
     * Get tool configuration parameter
     * @param key Configuration key
     * @return Configuration value
     */
    std::string getToolConfig(const std::string& key) const;
    
    /**
     * Set description for this tool
     * @param description Tool description
     */
    void setDescription(const std::string& description) { _tool_description = description; }
    
    /**
     * Get tool description
     * @return Tool description string
     */
    const std::string& getDescription() const { return _tool_description; }
    
    /**
     * Add required parameter that must be present in execution context
     * @param param_name Parameter name
     */
    void addRequiredParameter(const std::string& param_name);
    
    // AtomSpace integration
    /**
     * Set the AtomSpace for this tool
     * @param atomspace Shared pointer to AtomSpace
     */
    void setAtomSpace(AtomSpacePtr atomspace);
    
    /**
     * Get the AtomSpace for this tool
     * @return Shared pointer to AtomSpace
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    
    /**
     * Get the AtomSpace representation of this tool
     * @return Handle to tool atom
     */
    Handle getToolAtom() const { return _tool_atom; }
    
    // Status and information
    /**
     * Get the tool name
     * @return Tool name string
     */
    const std::string& getToolName() const { return _tool_name; }
    
    /**
     * Get the tool type
     * @return ToolType enumeration
     */
    ToolType getToolType() const { return _tool_type; }
    
    /**
     * Get the unique tool ID
     * @return Tool ID string
     */
    const std::string& getToolId() const { return _tool_id; }
    
    /**
     * Get current execution status
     * @return ToolStatus enumeration
     */
    ToolStatus getCurrentStatus() const { return _current_status; }
    
    /**
     * Get the last execution result
     * @return Shared pointer to last ToolResult
     */
    std::shared_ptr<ToolResult> getLastResult() const { return _last_result; }
    
    // Statistics
    /**
     * Get execution statistics
     * @return JSON string with statistics
     */
    std::string getStatistics() const;
    
    /**
     * Get total number of executions
     * @return Execution count
     */
    int getExecutionCount() const { return _execution_count; }
    
    /**
     * Get success rate (0.0 to 1.0)
     * @return Success rate as decimal
     */
    double getSuccessRate() const;
    
    /**
     * Get average execution time in milliseconds
     * @return Average execution time
     */
    double getAverageExecutionTime() const;
    
    // Custom tool integration
    /**
     * Set a custom executor function for CUSTOM tool type
     * @param executor Function that executes the tool
     */
    void setCustomExecutor(ExecutionCallback executor) { _custom_executor = executor; }
    
    /**
     * Reset statistics
     */
    void resetStatistics();
};

} // namespace tools
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TOOLWRAPPER_H
