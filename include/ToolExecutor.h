/*
 * opencog/agentzero/tools/ToolExecutor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ToolExecutor - Sandboxed tool invocation with result normalisation
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#ifndef _OPENCOG_AGENTZERO_TOOLEXECUTOR_H
#define _OPENCOG_AGENTZERO_TOOLEXECUTOR_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/ToolWrapper.h>

namespace opencog {
namespace agentzero {
namespace tools {

/**
 * SandboxPolicy - Security and resource constraints for sandboxed execution
 */
struct SandboxPolicy {
    double timeout_ms{30000.0};          ///< Maximum execution time (ms)
    size_t max_memory_bytes{0};          ///< Memory limit; 0 = no limit
    bool allow_network_access{false};    ///< Permit outbound network calls
    bool allow_filesystem_write{false};  ///< Permit writes to the filesystem
    std::vector<std::string> allowed_paths; ///< Whitelisted filesystem paths

    SandboxPolicy() = default;
};

/**
 * NormalisedResult - Unified result format produced by ToolExecutor
 *
 * All tool outputs are normalised into this structure regardless of the
 * original tool type or raw output format, making results uniformly
 * processable by the rest of the Agent-Zero cognitive pipeline.
 */
struct NormalisedResult {
    bool success{false};
    std::string output;
    std::string error;
    double execution_time_ms{0.0};
    std::map<std::string, std::string> metadata;
    HandleSeq atomspace_results;

    /** Serialise to a compact JSON string */
    std::string toJSON() const;

    /** Human-readable one-line summary */
    std::string toString() const;
};

/**
 * ToolExecutor - Sandboxed tool invocation with result normalisation
 *
 * Provides a secure, isolated execution environment for external tools
 * and normalises heterogeneous tool outputs into a unified NormalisedResult
 * suitable for direct consumption by AtomSpace-aware components.
 *
 * Security features:
 *  - Configurable per-invocation timeouts enforced via std::future
 *  - Input sanitisation before execution
 *  - Allowlist-based filesystem path validation
 *
 * Result normalisation:
 *  - Converts ToolResult (from ToolWrapper) to NormalisedResult
 *  - Extracts structured metadata from the raw output
 *  - Records execution provenance in AtomSpace when available
 */
class ToolExecutor
{
public:
    /**
     * Constructor
     * @param atomspace  Optional AtomSpace for provenance recording
     */
    explicit ToolExecutor(AtomSpacePtr atomspace = nullptr);

    /** Destructor */
    ~ToolExecutor();

    // ----------- Core execution -----------

    /**
     * Execute a ToolWrapper in a sandboxed environment
     *
     * @param tool     Tool to execute (mutated only via its internal stats)
     * @param context  Execution context supplying parameters and input atoms
     * @param policy   Sandbox security / resource policy
     * @return         Normalised result
     */
    NormalisedResult execute(ToolWrapper& tool,
                             const ToolExecutionContext& context,
                             const SandboxPolicy& policy = SandboxPolicy{});

    /**
     * Execute a shell command in a sandboxed environment
     *
     * The command is validated against the policy before execution.
     * stdout is captured and returned in NormalisedResult::output.
     *
     * @param command  Executable name or absolute path
     * @param args     Command-line arguments (not shell-expanded)
     * @param policy   Sandbox security / resource policy
     * @return         Normalised result
     */
    NormalisedResult executeCommand(const std::string& command,
                                    const std::vector<std::string>& args = {},
                                    const SandboxPolicy& policy = SandboxPolicy{});

    /**
     * Normalise a raw ToolResult into a NormalisedResult
     *
     * This can also be called independently to post-process results
     * obtained outside ToolExecutor.
     *
     * @param raw  Raw ToolResult from ToolWrapper::execute()
     * @return     Normalised result
     */
    NormalisedResult normalise(const ToolResult& raw) const;

    // ----------- Configuration -----------

    /**
     * Set the AtomSpace used for provenance recording
     * @param atomspace AtomSpace instance
     */
    void setAtomSpace(AtomSpacePtr atomspace) { _atomspace = atomspace; }

    /**
     * Get the current AtomSpace
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }

    // ----------- Statistics -----------

    /** Total executions performed (successful + failed + timed-out) */
    int getExecutionCount() const { return _execution_count.load(); }

    /** Number of executions that completed successfully */
    int getSuccessCount()   const { return _success_count.load();   }

    /** Number of executions that completed with an error */
    int getFailureCount()   const { return _failure_count.load();   }

    /** Number of executions that were cancelled due to timeout */
    int getTimeoutCount()   const { return _timeout_count.load();   }

    /**
     * Return aggregated statistics as a JSON string
     */
    std::string getStatistics() const;

private:
    AtomSpacePtr _atomspace;

    std::atomic<int> _execution_count{0};
    std::atomic<int> _success_count{0};
    std::atomic<int> _failure_count{0};
    std::atomic<int> _timeout_count{0};

    // Internal helpers
    NormalisedResult runWithTimeout(std::function<NormalisedResult()> fn,
                                    double timeout_ms);
    std::string sanitiseInput(const std::string& input) const;
    bool validateCommand(const std::string& command,
                         const SandboxPolicy& policy) const;
    void recordInAtomSpace(const NormalisedResult& result,
                           const std::string& tool_name);
    void updateStats(const NormalisedResult& result);
};

} // namespace tools
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TOOLEXECUTOR_H
