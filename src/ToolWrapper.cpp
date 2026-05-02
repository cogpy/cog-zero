/*
 * src/ToolWrapper.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ToolWrapper Implementation
 * Provides unified interface for external tool integration
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 * Task ID: AZ-TOOL-002
 */

#include <sstream>
#include <chrono>
#include <stdexcept>
#include <iomanip>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include <opencog/agentzero/tools/ToolWrapper.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

// ========================================================================================
// ToolResult Implementation
// ========================================================================================

ToolResult::ToolResult(ToolStatus status)
    : _status(status)
    , _execution_time_ms(0.0)
{
}

void ToolResult::setMetadata(const std::string& key, const std::string& value)
{
    _metadata[key] = value;
}

std::string ToolResult::getMetadata(const std::string& key) const
{
    auto it = _metadata.find(key);
    return (it != _metadata.end()) ? it->second : "";
}

std::string ToolResult::toJSON() const
{
    std::ostringstream json;
    json << "{";
    json << "\"status\":\"";
    
    switch (_status) {
        case ToolStatus::NOT_STARTED: json << "NOT_STARTED"; break;
        case ToolStatus::RUNNING: json << "RUNNING"; break;
        case ToolStatus::COMPLETED: json << "COMPLETED"; break;
        case ToolStatus::FAILED: json << "FAILED"; break;
        case ToolStatus::TIMEOUT: json << "TIMEOUT"; break;
        case ToolStatus::CANCELLED: json << "CANCELLED"; break;
    }
    
    json << "\",";
    json << "\"output\":\"" << _output << "\",";
    json << "\"error_message\":\"" << _error_message << "\",";
    json << "\"execution_time_ms\":" << _execution_time_ms << ",";
    json << "\"atomspace_result_count\":" << _atomspace_results.size() << ",";
    json << "\"metadata\":{";
    
    bool first = true;
    for (const auto& kv : _metadata) {
        if (!first) json << ",";
        json << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    
    json << "}";
    json << "}";
    
    return json.str();
}

std::string ToolResult::toString() const
{
    std::ostringstream str;
    str << "ToolResult[";
    str << "status=";
    
    switch (_status) {
        case ToolStatus::NOT_STARTED: str << "NOT_STARTED"; break;
        case ToolStatus::RUNNING: str << "RUNNING"; break;
        case ToolStatus::COMPLETED: str << "COMPLETED"; break;
        case ToolStatus::FAILED: str << "FAILED"; break;
        case ToolStatus::TIMEOUT: str << "TIMEOUT"; break;
        case ToolStatus::CANCELLED: str << "CANCELLED"; break;
    }
    
    str << ", execution_time=" << _execution_time_ms << "ms";
    str << ", atoms=" << _atomspace_results.size();
    
    if (!_error_message.empty()) {
        str << ", error=" << _error_message;
    }
    
    str << "]";
    return str.str();
}

// ========================================================================================
// ToolExecutionContext Implementation
// ========================================================================================

ToolExecutionContext::ToolExecutionContext(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _timeout_ms(30000.0)  // 30 seconds default timeout
    , _async_execution(false)
{
}

void ToolExecutionContext::setParameter(const std::string& key, const std::string& value)
{
    _parameters[key] = value;
}

std::string ToolExecutionContext::getParameter(const std::string& key) const
{
    auto it = _parameters.find(key);
    return (it != _parameters.end()) ? it->second : "";
}

bool ToolExecutionContext::hasParameter(const std::string& key) const
{
    return _parameters.find(key) != _parameters.end();
}

void ToolExecutionContext::setConfig(const std::string& key, const std::string& value)
{
    _config[key] = value;
}

std::string ToolExecutionContext::getConfig(const std::string& key) const
{
    auto it = _config.find(key);
    return (it != _config.end()) ? it->second : "";
}

// ========================================================================================
// ToolWrapper Implementation
// ========================================================================================

ToolWrapper::ToolWrapper(const std::string& tool_name, 
                         ToolType tool_type,
                         AtomSpacePtr atomspace)
    : _tool_name(tool_name)
    , _tool_type(tool_type)
    , _atomspace(atomspace)
    , _tool_atom(Handle::UNDEFINED)
    , _current_status(ToolStatus::NOT_STARTED)
    , _execution_count(0)
    , _success_count(0)
    , _failure_count(0)
    , _total_execution_time_ms(0.0)
{
    // Generate unique tool ID
    _tool_id = _tool_name + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Initialize AtomSpace representation if atomspace is provided
    if (_atomspace) {
        initializeToolAtom();
    }
    
    logger().info() << "[ToolWrapper] Created tool: " << _tool_name 
                    << " (ID: " << _tool_id << ", Type: " << static_cast<int>(_tool_type) << ")";
}

ToolWrapper::~ToolWrapper()
{
    logger().info() << "[ToolWrapper] Destroying tool: " << _tool_name 
                    << " (Executions: " << _execution_count << ")";
}

void ToolWrapper::initializeToolAtom()
{
    if (!_atomspace) {
        logger().warn() << "[ToolWrapper] Cannot initialize tool atom: No AtomSpace";
        return;
    }
    
    // Create tool atom in AtomSpace
    _tool_atom = _atomspace->add_node(CONCEPT_NODE, "Tool_" + _tool_name);
    
    // Set truth value to indicate tool is available
    TruthValuePtr tv = SimpleTruthValue::createTV(1.0, 1.0);
    _tool_atom->setTruthValue(tv);
    
    // Create type annotation
    Handle tool_type_atom = _atomspace->add_node(CONCEPT_NODE, "ToolType");
    HandleSeq type_link_seq;
    type_link_seq.push_back(_tool_atom);
    type_link_seq.push_back(tool_type_atom);
    _atomspace->add_link(INHERITANCE_LINK, std::move(type_link_seq));
    
    logger().debug() << "[ToolWrapper] Tool atom initialized: " << _tool_atom->to_short_string();
}

ToolResult ToolWrapper::execute(const ToolExecutionContext& context)
{
    logger().info() << "[ToolWrapper] Executing tool: " << _tool_name;
    
    // Validate context
    if (!validateContext(context)) {
        ToolResult result(ToolStatus::FAILED);
        result.setErrorMessage("Context validation failed: missing required parameters");
        logger().error() << "[ToolWrapper] " << result.getErrorMessage();
        return result;
    }
    
    // Record start time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    _current_status = ToolStatus::RUNNING;
    ToolResult result;
    
    try {
        // Execute based on tool type
        switch (_tool_type) {
            case ToolType::EXTERNAL_REST_API:
                result = executeRESTTool(context);
                break;
                
            case ToolType::ROS_BEHAVIOR:
                result = executeROSBehavior(context);
                break;
                
            case ToolType::PYTHON_SCRIPT:
                result = executePythonScript(context);
                break;
                
            case ToolType::SHELL_COMMAND:
                result = executeShellCommand(context);
                break;
                
            case ToolType::ATOMSPACE_QUERY:
                result = executeAtomSpaceQuery(context);
                break;
                
            case ToolType::CUSTOM:
                result = executeCustomTool(context);
                break;
                
            default:
                result = ToolResult(ToolStatus::FAILED);
                result.setErrorMessage("Unknown tool type");
                break;
        }
        
    } catch (const std::exception& e) {
        result = ToolResult(ToolStatus::FAILED);
        result.setErrorMessage(std::string("Exception during execution: ") + e.what());
        logger().error() << "[ToolWrapper] Exception: " << e.what();
    }
    
    // Calculate execution time
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double execution_time_ms = duration.count() / 1000.0;
    result.setExecutionTime(execution_time_ms);
    
    // Update status
    _current_status = result.getStatus();
    _last_result = std::make_shared<ToolResult>(result);
    
    // Update statistics
    updateStatistics(result);
    
    // Record execution in AtomSpace if available
    if (_atomspace) {
        recordExecutionInAtomSpace(context, result);
    }
    
    logger().info() << "[ToolWrapper] Execution complete: " << result.toString();
    
    return result;
}

std::string ToolWrapper::executeAsync(const ToolExecutionContext& context,
                                     std::function<void(const ToolResult&)> callback)
{
    logger().info() << "[ToolWrapper] Starting async execution: " << _tool_name;
    
    // Generate execution ID
    std::string exec_id = _tool_id + "_" + 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // TODO: Implement actual async execution using thread pool or async framework
    // For now, this is a placeholder that would be implemented based on specific needs
    
    logger().warn() << "[ToolWrapper] Async execution not yet fully implemented";
    
    return exec_id;
}

ToolResult ToolWrapper::executeRESTTool(const ToolExecutionContext& context)
{
    logger().debug() << "[ToolWrapper] Executing REST API tool";
    
    ToolResult result(ToolStatus::COMPLETED);
    
    // TODO: Implement actual REST API call
    // This would use a library like libcurl or Boost.Beast to make HTTP requests
    // For now, this is a placeholder implementation
    
    if (_tool_endpoint.empty()) {
        result.setStatus(ToolStatus::FAILED);
        result.setErrorMessage("Tool endpoint not configured");
        return result;
    }
    
    result.setOutput("REST API call to " + _tool_endpoint + " (placeholder implementation)");
    result.setMetadata("endpoint", _tool_endpoint);
    result.setMetadata("method", "POST");
    
    logger().debug() << "[ToolWrapper] REST tool execution completed";
    
    return result;
}

ToolResult ToolWrapper::executeROSBehavior(const ToolExecutionContext& context)
{
    logger().debug() << "[ToolWrapper] Executing ROS behavior";
    
    ToolResult result(ToolStatus::COMPLETED);
    
    // TODO: Implement actual ROS topic/service interaction
    // This would use ROS client libraries to publish/subscribe or call services
    // For now, this is a placeholder implementation
    
    if (_tool_endpoint.empty()) {
        result.setStatus(ToolStatus::FAILED);
        result.setErrorMessage("ROS topic/service not configured");
        return result;
    }
    
    result.setOutput("ROS behavior on " + _tool_endpoint + " (placeholder implementation)");
    result.setMetadata("ros_topic", _tool_endpoint);
    
    logger().debug() << "[ToolWrapper] ROS behavior execution completed";
    
    return result;
}

ToolResult ToolWrapper::executePythonScript(const ToolExecutionContext& context)
{
    logger().debug() << "[ToolWrapper] Executing Python script";
    
    ToolResult result(ToolStatus::COMPLETED);
    
    // TODO: Implement actual Python script execution
    // This would use system() call or better process management
    // For now, this is a placeholder implementation
    
    if (_tool_endpoint.empty()) {
        result.setStatus(ToolStatus::FAILED);
        result.setErrorMessage("Script path not configured");
        return result;
    }
    
    result.setOutput("Python script " + _tool_endpoint + " executed (placeholder implementation)");
    result.setMetadata("script_path", _tool_endpoint);
    
    logger().debug() << "[ToolWrapper] Python script execution completed";
    
    return result;
}

ToolResult ToolWrapper::executeShellCommand(const ToolExecutionContext& context)
{
    logger().debug() << "[ToolWrapper] Executing shell command";
    
    ToolResult result(ToolStatus::COMPLETED);
    
    // TODO: Implement actual shell command execution with proper security
    // This would use popen() or better process management with timeout
    // For now, this is a placeholder implementation
    
    if (_tool_endpoint.empty()) {
        result.setStatus(ToolStatus::FAILED);
        result.setErrorMessage("Shell command not configured");
        return result;
    }
    
    result.setOutput("Shell command executed (placeholder implementation)");
    result.setMetadata("command", _tool_endpoint);
    
    logger().debug() << "[ToolWrapper] Shell command execution completed";
    
    return result;
}

ToolResult ToolWrapper::executeAtomSpaceQuery(const ToolExecutionContext& context)
{
    logger().debug() << "[ToolWrapper] Executing AtomSpace query";
    
    ToolResult result(ToolStatus::COMPLETED);
    
    if (!context.getAtomSpace()) {
        result.setStatus(ToolStatus::FAILED);
        result.setErrorMessage("No AtomSpace provided in context");
        return result;
    }
    
    // Example: Query atoms based on input atoms
    HandleSeq query_results;
    const HandleSeq& input_atoms = context.getInputAtoms();
    
    for (const Handle& h : input_atoms) {
        // Placeholder: In real implementation, would perform pattern matching or queries
        query_results.push_back(h);
    }
    
    result.setAtomSpaceResults(query_results);
    result.setOutput("AtomSpace query executed, " + std::to_string(query_results.size()) + " atoms found");
    result.setMetadata("query_type", "pattern_match");
    result.setMetadata("result_count", std::to_string(query_results.size()));
    
    logger().debug() << "[ToolWrapper] AtomSpace query execution completed";
    
    return result;
}

ToolResult ToolWrapper::executeCustomTool(const ToolExecutionContext& context)
{
    logger().debug() << "[ToolWrapper] Executing custom tool";
    
    if (!_custom_executor) {
        ToolResult result(ToolStatus::FAILED);
        result.setErrorMessage("No custom executor function set");
        return result;
    }
    
    try {
        ToolResult result = _custom_executor(context);
        logger().debug() << "[ToolWrapper] Custom tool execution completed";
        return result;
    } catch (const std::exception& e) {
        ToolResult result(ToolStatus::FAILED);
        result.setErrorMessage(std::string("Custom executor exception: ") + e.what());
        return result;
    }
}

bool ToolWrapper::validateContext(const ToolExecutionContext& context) const
{
    // Check required parameters
    for (const auto& param : _required_parameters) {
        if (!context.hasParameter(param)) {
            logger().error() << "[ToolWrapper] Missing required parameter: " << param;
            return false;
        }
    }
    
    // Check timeout is reasonable
    if (context.getTimeout() <= 0) {
        logger().error() << "[ToolWrapper] Invalid timeout value";
        return false;
    }
    
    return true;
}

void ToolWrapper::updateStatistics(const ToolResult& result)
{
    _execution_count++;
    _total_execution_time_ms += result.getExecutionTime();
    
    if (result.isSuccess()) {
        _success_count++;
    } else if (result.isFailure()) {
        _failure_count++;
    }
}

void ToolWrapper::recordExecutionInAtomSpace(const ToolExecutionContext& context, 
                                            const ToolResult& result)
{
    if (!_atomspace || _tool_atom == Handle::UNDEFINED) {
        return;
    }
    
    try {
        // Create execution record atom
        std::string exec_id = "Execution_" + _tool_id + "_" + std::to_string(_execution_count);
        Handle exec_atom = _atomspace->add_node(CONCEPT_NODE, exec_id);
        
        // Link execution to tool
        HandleSeq exec_link_seq;
        exec_link_seq.push_back(_tool_atom);
        exec_link_seq.push_back(exec_atom);
        _atomspace->add_link(EVALUATION_LINK, std::move(exec_link_seq));
        
        // Store result status as truth value
        double strength = result.isSuccess() ? 1.0 : 0.0;
        TruthValuePtr exec_tv = SimpleTruthValue::createTV(strength, 1.0);
        exec_atom->setTruthValue(exec_tv);
        
        logger().debug() << "[ToolWrapper] Execution recorded in AtomSpace: " << exec_atom->to_short_string();
        
    } catch (const std::exception& e) {
        logger().error() << "[ToolWrapper] Failed to record execution in AtomSpace: " << e.what();
    }
}

void ToolWrapper::setToolConfig(const std::string& key, const std::string& value)
{
    _tool_config[key] = value;
    logger().debug() << "[ToolWrapper] Config set: " << key << " = " << value;
}

std::string ToolWrapper::getToolConfig(const std::string& key) const
{
    auto it = _tool_config.find(key);
    return (it != _tool_config.end()) ? it->second : "";
}

void ToolWrapper::addRequiredParameter(const std::string& param_name)
{
    _required_parameters.push_back(param_name);
    logger().debug() << "[ToolWrapper] Required parameter added: " << param_name;
}

void ToolWrapper::setAtomSpace(AtomSpacePtr atomspace)
{
    _atomspace = atomspace;
    
    if (_atomspace && _tool_atom == Handle::UNDEFINED) {
        initializeToolAtom();
    }
    
    logger().debug() << "[ToolWrapper] AtomSpace set for tool: " << _tool_name;
}

std::string ToolWrapper::getStatistics() const
{
    std::ostringstream stats;
    stats << "{";
    stats << "\"tool_name\":\"" << _tool_name << "\",";
    stats << "\"tool_id\":\"" << _tool_id << "\",";
    stats << "\"execution_count\":" << _execution_count << ",";
    stats << "\"success_count\":" << _success_count << ",";
    stats << "\"failure_count\":" << _failure_count << ",";
    stats << "\"success_rate\":" << std::fixed << std::setprecision(3) << getSuccessRate() << ",";
    stats << "\"average_execution_time_ms\":" << std::fixed << std::setprecision(2) << getAverageExecutionTime();
    stats << "}";
    
    return stats.str();
}

double ToolWrapper::getSuccessRate() const
{
    if (_execution_count == 0) {
        return 0.0;
    }
    return static_cast<double>(_success_count) / static_cast<double>(_execution_count);
}

double ToolWrapper::getAverageExecutionTime() const
{
    if (_execution_count == 0) {
        return 0.0;
    }
    return _total_execution_time_ms / static_cast<double>(_execution_count);
}

void ToolWrapper::resetStatistics()
{
    _execution_count = 0;
    _success_count = 0;
    _failure_count = 0;
    _total_execution_time_ms = 0.0;
    
    logger().info() << "[ToolWrapper] Statistics reset for tool: " << _tool_name;
}
