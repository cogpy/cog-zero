/*
 * src/ToolExecutor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ToolExecutor Implementation
 * Sandboxed tool invocation with result normalisation
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#include <array>
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/ToolExecutor.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

// ========================================================================================
// NormalisedResult
// ========================================================================================

std::string NormalisedResult::toJSON() const
{
    std::ostringstream j;
    j << "{";
    j << "\"success\":" << (success ? "true" : "false") << ",";
    j << "\"output\":\"" << output << "\",";
    j << "\"error\":\"" << error << "\",";
    j << "\"execution_time_ms\":" << std::fixed << std::setprecision(2) << execution_time_ms << ",";
    j << "\"atomspace_result_count\":" << atomspace_results.size() << ",";
    j << "\"metadata\":{";

    bool first = true;
    for (const auto& kv : metadata) {
        if (!first) j << ",";
        j << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    j << "}}";
    return j.str();
}

std::string NormalisedResult::toString() const
{
    std::ostringstream s;
    s << "NormalisedResult["
      << "success=" << (success ? "true" : "false")
      << ", time=" << std::fixed << std::setprecision(2) << execution_time_ms << "ms"
      << ", atoms=" << atomspace_results.size();
    if (!error.empty()) {
        s << ", error=" << error;
    }
    s << "]";
    return s.str();
}

// ========================================================================================
// ToolExecutor
// ========================================================================================

ToolExecutor::ToolExecutor(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    logger().info() << "[ToolExecutor] Initialised";
}

ToolExecutor::~ToolExecutor()
{
    logger().info() << "[ToolExecutor] Destroyed (total executions: "
                    << _execution_count.load() << ")";
}

// ---------------------------------------------------------------------------
// execute()
// ---------------------------------------------------------------------------
NormalisedResult ToolExecutor::execute(ToolWrapper& tool,
                                       const ToolExecutionContext& context,
                                       const SandboxPolicy& policy)
{
    logger().info() << "[ToolExecutor] Executing tool: " << tool.getToolName();

    auto fn = [&]() -> NormalisedResult {
        ToolResult raw = tool.execute(context);
        return normalise(raw);
    };

    NormalisedResult result = runWithTimeout(fn, policy.timeout_ms);
    result.metadata["tool_name"] = tool.getToolName();
    result.metadata["tool_type"] = std::to_string(static_cast<int>(tool.getToolType()));

    if (_atomspace) {
        recordInAtomSpace(result, tool.getToolName());
    }

    updateStats(result);
    logger().info() << "[ToolExecutor] " << result.toString();
    return result;
}

// ---------------------------------------------------------------------------
// executeCommand()
// ---------------------------------------------------------------------------
NormalisedResult ToolExecutor::executeCommand(const std::string& command,
                                               const std::vector<std::string>& args,
                                               const SandboxPolicy& policy)
{
    if (!validateCommand(command, policy)) {
        NormalisedResult r;
        r.success = false;
        r.error = "Command validation failed: " + command;
        logger().error() << "[ToolExecutor] " << r.error;
        updateStats(r);
        return r;
    }

    auto fn = [&]() -> NormalisedResult {
        NormalisedResult result;
        auto start = std::chrono::high_resolution_clock::now();

        // Build the command string with sanitised arguments
        std::string cmd = command;
        for (const auto& arg : args) {
            cmd += " " + sanitiseInput(arg);
        }
        // Redirect stderr to stdout so we capture everything
        cmd += " 2>&1";

        std::array<char, 256> buffer{};
        std::string captured;

        // popen is used for portability; POSIX platforms guarantee it
        FILE* pipe = popen(cmd.c_str(), "r"); // NOLINT(cert-env33-c)
        if (!pipe) {
            result.success = false;
            result.error = "Failed to open subprocess";
            return result;
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            captured += buffer.data();
        }

        int exit_code = pclose(pipe);

        auto end = std::chrono::high_resolution_clock::now();
        result.execution_time_ms =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        result.success = (exit_code == 0);
        result.output  = captured;
        if (!result.success) {
            result.error = "Process exited with code " + std::to_string(exit_code);
        }
        result.metadata["exit_code"] = std::to_string(exit_code);
        result.metadata["command"]   = command;
        return result;
    };

    NormalisedResult result = runWithTimeout(fn, policy.timeout_ms);
    updateStats(result);

    if (_atomspace) {
        recordInAtomSpace(result, command);
    }

    return result;
}

// ---------------------------------------------------------------------------
// normalise()
// ---------------------------------------------------------------------------
NormalisedResult ToolExecutor::normalise(const ToolResult& raw) const
{
    NormalisedResult n;
    n.success               = raw.isSuccess();
    n.output                = raw.getOutput();
    n.error                 = raw.getErrorMessage();
    n.execution_time_ms     = raw.getExecutionTime();
    n.atomspace_results     = raw.getAtomSpaceResults();

    // Copy metadata from raw result
    for (const auto& kv : raw.getAllMetadata()) {
        n.metadata[kv.first] = kv.second;
    }

    // Encode ToolStatus as a metadata field
    switch (raw.getStatus()) {
        case ToolStatus::NOT_STARTED: n.metadata["raw_status"] = "NOT_STARTED"; break;
        case ToolStatus::RUNNING:     n.metadata["raw_status"] = "RUNNING";     break;
        case ToolStatus::COMPLETED:   n.metadata["raw_status"] = "COMPLETED";   break;
        case ToolStatus::FAILED:      n.metadata["raw_status"] = "FAILED";      break;
        case ToolStatus::TIMEOUT:
            n.metadata["raw_status"] = "TIMEOUT";
            n.error = "Tool execution timed out";
            break;
        case ToolStatus::CANCELLED:   n.metadata["raw_status"] = "CANCELLED";   break;
    }

    return n;
}

// ---------------------------------------------------------------------------
// getStatistics()
// ---------------------------------------------------------------------------
std::string ToolExecutor::getStatistics() const
{
    int total   = _execution_count.load();
    int success = _success_count.load();
    int fail    = _failure_count.load();
    int timeout = _timeout_count.load();

    double rate = (total > 0)
                ? (static_cast<double>(success) / static_cast<double>(total))
                : 0.0;

    std::ostringstream j;
    j << "{";
    j << "\"total_executions\":" << total << ",";
    j << "\"success_count\":" << success << ",";
    j << "\"failure_count\":" << fail << ",";
    j << "\"timeout_count\":" << timeout << ",";
    j << "\"success_rate\":" << std::fixed << std::setprecision(3) << rate;
    j << "}";
    return j.str();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

NormalisedResult ToolExecutor::runWithTimeout(std::function<NormalisedResult()> fn,
                                               double timeout_ms)
{
    auto future = std::async(std::launch::async, fn);

    auto status = future.wait_for(
        std::chrono::milliseconds(static_cast<int>(timeout_ms)));

    if (status == std::future_status::timeout) {
        _timeout_count.fetch_add(1);
        NormalisedResult r;
        r.success = false;
        r.error   = "Execution timed out after " + std::to_string(timeout_ms) + "ms";
        r.execution_time_ms = timeout_ms;
        r.metadata["timed_out"] = "true";
        logger().warn() << "[ToolExecutor] " << r.error;
        return r;
    }

    try {
        return future.get();
    } catch (const std::exception& e) {
        NormalisedResult r;
        r.success = false;
        r.error   = std::string("Exception in executor thread: ") + e.what();
        logger().error() << "[ToolExecutor] " << r.error;
        return r;
    }
}

std::string ToolExecutor::sanitiseInput(const std::string& input) const
{
    // Simple shell-escape: wrap in single quotes and escape embedded single quotes
    std::string safe;
    safe.reserve(input.size() + 2);
    safe += '\'';
    for (char c : input) {
        if (c == '\'') {
            safe += "'\\''";  // end quote, escaped quote, restart quote
        } else {
            safe += c;
        }
    }
    safe += '\'';
    return safe;
}

bool ToolExecutor::validateCommand(const std::string& command,
                                   const SandboxPolicy& policy) const
{
    if (command.empty()) {
        return false;
    }

    // If allowed_paths is configured, require the command to live there
    if (!policy.allowed_paths.empty()) {
        bool found = false;
        for (const auto& p : policy.allowed_paths) {
            if (command.find(p) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            logger().warn() << "[ToolExecutor] Command not in allowed paths: " << command;
            return false;
        }
    }

    return true;
}

void ToolExecutor::recordInAtomSpace(const NormalisedResult& result,
                                     const std::string& tool_name)
{
    if (!_atomspace) {
        return;
    }

    try {
        Handle tool_node = _atomspace->add_node(CONCEPT_NODE, "Tool_" + tool_name);
        Handle exec_node = _atomspace->add_node(
            CONCEPT_NODE,
            "Execution_" + tool_name + "_" + std::to_string(_execution_count.load()));

        TruthValuePtr tv = SimpleTruthValue::createTV(result.success ? 1.0 : 0.0, 1.0);
        exec_node->setTruthValue(tv);

        HandleSeq link_atoms;
        link_atoms.push_back(tool_node);
        link_atoms.push_back(exec_node);
        _atomspace->add_link(EVALUATION_LINK, std::move(link_atoms));

    } catch (const std::exception& e) {
        logger().error() << "[ToolExecutor] AtomSpace record failed: " << e.what();
    }
}

void ToolExecutor::updateStats(const NormalisedResult& result)
{
    _execution_count.fetch_add(1);
    if (result.success) {
        _success_count.fetch_add(1);
    } else {
        auto it = result.metadata.find("timed_out");
        if (it != result.metadata.end() && it->second == "true") {
            // timeout already counted in runWithTimeout
        } else {
            _failure_count.fetch_add(1);
        }
    }
}
