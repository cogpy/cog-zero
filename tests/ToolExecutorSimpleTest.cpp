/*
 * tests/ToolExecutorSimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple standalone test for ToolExecutor - sandboxed tool invocation
 * with result normalisation
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/tools/ToolExecutor.h"
#include "../include/opencog/agentzero/tools/ToolWrapper.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

int main()
{
    logger().set_level(Logger::WARN);
    logger().set_print_to_stdout_flag(false);

    std::cout << "=== ToolExecutor Simple Test ===" << std::endl;

    try {
        // Test 1: Initialisation
        std::cout << "\n1. Testing initialisation..." << std::endl;

        auto atomspace = std::make_shared<AtomSpace>();
        auto executor  = std::make_unique<ToolExecutor>(atomspace);

        assert(executor != nullptr);
        assert(executor->getExecutionCount() == 0);
        assert(executor->getSuccessCount()   == 0);
        assert(executor->getFailureCount()   == 0);
        assert(executor->getTimeoutCount()   == 0);

        std::cout << "  ✓ Initialisation successful" << std::endl;

        // Test 2: normalise() with a successful ToolResult
        std::cout << "\n2. Testing normalise() with COMPLETED result..." << std::endl;

        ToolResult raw_ok(ToolStatus::COMPLETED);
        raw_ok.setOutput("hello world");
        raw_ok.setMetadata("key", "value");

        NormalisedResult n_ok = executor->normalise(raw_ok);
        assert(n_ok.success);
        assert(n_ok.output == "hello world");
        assert(n_ok.metadata.at("key") == "value");
        assert(n_ok.metadata.at("raw_status") == "COMPLETED");

        std::cout << "  ✓ COMPLETED result normalised correctly" << std::endl;

        // Test 3: normalise() with a failed ToolResult
        std::cout << "\n3. Testing normalise() with FAILED result..." << std::endl;

        ToolResult raw_fail(ToolStatus::FAILED);
        raw_fail.setErrorMessage("something went wrong");

        NormalisedResult n_fail = executor->normalise(raw_fail);
        assert(!n_fail.success);
        assert(n_fail.error == "something went wrong");
        assert(n_fail.metadata.at("raw_status") == "FAILED");

        std::cout << "  ✓ FAILED result normalised correctly" << std::endl;

        // Test 4: normalise() with TIMEOUT status
        std::cout << "\n4. Testing normalise() with TIMEOUT result..." << std::endl;

        ToolResult raw_to(ToolStatus::TIMEOUT);
        NormalisedResult n_to = executor->normalise(raw_to);
        assert(!n_to.success);
        assert(n_to.metadata.at("raw_status") == "TIMEOUT");
        assert(!n_to.error.empty());

        std::cout << "  ✓ TIMEOUT result normalised correctly" << std::endl;

        // Test 5: Execute a custom tool via ToolWrapper
        std::cout << "\n5. Testing execute() with a custom ToolWrapper..." << std::endl;

        auto tool = std::make_shared<ToolWrapper>("test_tool", ToolType::CUSTOM, atomspace);
        tool->setCustomExecutor([&](const ToolExecutionContext& ctx) -> ToolResult {
            ToolResult r(ToolStatus::COMPLETED);
            r.setOutput("custom result for: " + ctx.getParameter("input"));
            return r;
        });

        ToolExecutionContext ctx(atomspace);
        ctx.setParameter("input", "test_value");
        ctx.setTimeout(5000.0);

        SandboxPolicy policy;
        policy.timeout_ms = 5000.0;

        NormalisedResult result = executor->execute(*tool, ctx, policy);
        assert(result.success);
        assert(result.output.find("test_value") != std::string::npos);
        assert(result.metadata.at("tool_name") == "test_tool");

        std::cout << "  ✓ Custom tool executed and normalised correctly" << std::endl;

        // Test 6: Statistics updated after execution
        std::cout << "\n6. Testing statistics..." << std::endl;

        assert(executor->getExecutionCount() == 1);
        assert(executor->getSuccessCount()   == 1);
        assert(executor->getFailureCount()   == 0);

        std::string stats = executor->getStatistics();
        assert(!stats.empty());
        assert(stats.find("total_executions") != std::string::npos);
        assert(stats.find("success_rate") != std::string::npos);

        std::cout << "  Statistics JSON: " << stats << std::endl;
        std::cout << "  ✓ Statistics updated correctly" << std::endl;

        // Test 7: Execute a failing tool
        std::cout << "\n7. Testing execute() with a failing tool..." << std::endl;

        auto fail_tool = std::make_shared<ToolWrapper>("fail_tool", ToolType::CUSTOM, atomspace);
        fail_tool->setCustomExecutor([](const ToolExecutionContext&) -> ToolResult {
            ToolResult r(ToolStatus::FAILED);
            r.setErrorMessage("deliberate failure");
            return r;
        });

        NormalisedResult fail_result = executor->execute(*fail_tool, ctx, policy);
        assert(!fail_result.success);
        assert(fail_result.error == "deliberate failure");
        assert(executor->getFailureCount() == 1);

        std::cout << "  ✓ Failed tool handled correctly" << std::endl;

        // Test 8: NormalisedResult serialisation
        std::cout << "\n8. Testing NormalisedResult serialisation..." << std::endl;

        std::string json_str = result.toJSON();
        assert(!json_str.empty());
        assert(json_str.find("\"success\":true") != std::string::npos);
        assert(json_str.find("execution_time_ms") != std::string::npos);

        std::string str_repr = result.toString();
        assert(!str_repr.empty());
        assert(str_repr.find("NormalisedResult") != std::string::npos);

        std::cout << "  JSON: " << json_str << std::endl;
        std::cout << "  ✓ Serialisation working correctly" << std::endl;

        // Test 9: SandboxPolicy defaults
        std::cout << "\n9. Testing SandboxPolicy defaults..." << std::endl;

        SandboxPolicy default_policy;
        assert(default_policy.timeout_ms          == 30000.0);
        assert(default_policy.max_memory_bytes    == 0);
        assert(!default_policy.allow_network_access);
        assert(!default_policy.allow_filesystem_write);
        assert(default_policy.allowed_paths.empty());

        std::cout << "  ✓ SandboxPolicy defaults correct" << std::endl;

        // Test 10: AtomSpace set/get
        std::cout << "\n10. Testing AtomSpace set/get..." << std::endl;

        auto executor2 = std::make_unique<ToolExecutor>();
        assert(executor2->getAtomSpace() == nullptr);

        executor2->setAtomSpace(atomspace);
        assert(executor2->getAtomSpace() == atomspace);

        std::cout << "  ✓ AtomSpace set/get working" << std::endl;

        std::cout << "\n=== All ToolExecutor tests passed! ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
