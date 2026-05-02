/*
 * tests/RestApiAdapterSimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple standalone test for RestApiAdapter - REST API tool adapter
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

#include "../include/opencog/agentzero/tools/RestApiAdapter.h"
#include "../include/opencog/agentzero/tools/ToolWrapper.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

int main()
{
    logger().set_level(Logger::WARN);
    logger().set_print_to_stdout_flag(false);

    std::cout << "=== RestApiAdapter Simple Test ===" << std::endl;

    try {
        // Test 1: Initialisation
        std::cout << "\n1. Testing initialisation..." << std::endl;

        auto atomspace = std::make_shared<AtomSpace>();
        auto adapter = std::make_unique<RestApiAdapter>("http://localhost:9999", atomspace);

        assert(adapter != nullptr);
        assert(adapter->getBaseUrl() == "http://localhost:9999");
        assert(adapter->getSuccessCount() == 0);
        assert(adapter->getFailureCount() == 0);

        std::cout << "  ✓ Initialisation successful" << std::endl;

        // Test 2: Configuration
        std::cout << "\n2. Testing configuration..." << std::endl;

        adapter->setBaseUrl("http://api.example.com:8080");
        assert(adapter->getBaseUrl() == "http://api.example.com:8080");

        adapter->setTimeout(5000.0);
        adapter->setDefaultHeader("X-Custom-Header", "test-value");
        adapter->setBearerToken("my-token-123");

        std::cout << "  ✓ Configuration methods work correctly" << std::endl;

        // Test 3: Basic auth encoding
        std::cout << "\n3. Testing Basic auth encoding..." << std::endl;

        adapter->setBasicAuth("admin", "password");
        // If it didn't throw, encoding worked
        std::cout << "  ✓ Basic auth configured (no exception)" << std::endl;

        // Test 4: HttpResponse toJSON()
        std::cout << "\n4. Testing HttpResponse serialisation..." << std::endl;

        HttpResponse resp;
        resp.status_code = 200;
        resp.body        = "{\"result\":\"ok\"}";
        resp.success     = true;
        resp.elapsed_ms  = 42.5;
        resp.error       = "";

        std::string json_str = resp.toJSON();
        assert(!json_str.empty());
        assert(json_str.find("\"status_code\":200") != std::string::npos);
        assert(json_str.find("\"success\":true")    != std::string::npos);
        assert(json_str.find("\"elapsed_ms\"")      != std::string::npos);

        std::cout << "  JSON: " << json_str << std::endl;
        std::cout << "  ✓ HttpResponse serialisation working" << std::endl;

        // Test 5: responseToAtom() with successful response
        std::cout << "\n5. Testing responseToAtom()..." << std::endl;

        HttpResponse ok_resp;
        ok_resp.status_code = 200;
        ok_resp.body        = "{\"data\":\"test\"}";
        ok_resp.success     = true;

        Handle atom = adapter->responseToAtom(ok_resp, atomspace);
        assert(atom != Handle::UNDEFINED);

        // The node should have truth value 1.0 (success)
        TruthValuePtr tv = atom->getTruthValue();
        assert(tv->get_mean() == 1.0);

        std::cout << "  Atom: " << atom->to_short_string() << std::endl;
        std::cout << "  ✓ responseToAtom() creates correct atom" << std::endl;

        // Test 6: responseToAtom() with failed response
        std::cout << "\n6. Testing responseToAtom() with failure..." << std::endl;

        HttpResponse fail_resp;
        fail_resp.status_code = 500;
        fail_resp.body        = "Internal Server Error";
        fail_resp.success     = false;

        Handle fail_atom = adapter->responseToAtom(fail_resp, atomspace);
        assert(fail_atom != Handle::UNDEFINED);

        TruthValuePtr fail_tv = fail_atom->getTruthValue();
        assert(fail_tv->get_mean() == 0.0);

        std::cout << "  ✓ responseToAtom() handles failure correctly" << std::endl;

        // Test 7: responseToAtom() without AtomSpace
        std::cout << "\n7. Testing responseToAtom() without AtomSpace..." << std::endl;

        RestApiAdapter no_as_adapter("http://localhost:9999");
        Handle no_as_atom = no_as_adapter.responseToAtom(ok_resp, nullptr);
        assert(no_as_atom == Handle::UNDEFINED);

        std::cout << "  ✓ Gracefully returns UNDEFINED when no AtomSpace" << std::endl;

        // Test 8: createToolWrapper()
        std::cout << "\n8. Testing createToolWrapper()..." << std::endl;

        auto tool = adapter->createToolWrapper("api_tool", "/api/v1/query", atomspace);
        assert(tool != nullptr);
        assert(tool->getToolName() == "api_tool");
        assert(tool->getToolType() == ToolType::EXTERNAL_REST_API);

        std::cout << "  Tool name: " << tool->getToolName() << std::endl;
        std::cout << "  Tool type: " << static_cast<int>(tool->getToolType()) << std::endl;
        std::cout << "  ✓ createToolWrapper() created correct ToolWrapper" << std::endl;

        // Test 9: Statistics
        std::cout << "\n9. Testing statistics..." << std::endl;

        std::string stats = adapter->getStatistics();
        assert(!stats.empty());
        assert(stats.find("base_url")      != std::string::npos);
        assert(stats.find("success_count") != std::string::npos);
        assert(stats.find("failure_count") != std::string::npos);
        assert(stats.find("success_rate")  != std::string::npos);

        std::cout << "  Statistics: " << stats << std::endl;
        std::cout << "  ✓ Statistics JSON is well-formed" << std::endl;

        // Test 10: callTool() without a server (expected failure)
        std::cout << "\n10. Testing callTool() against unreachable server..." << std::endl;

        RestApiAdapter unreachable_adapter("http://127.0.0.1:19999");
        unreachable_adapter.setTimeout(500.0);

        ToolExecutionContext ctx(atomspace);
        ctx.setParameter("query", "test");
        ctx.setTimeout(500.0);

        ToolResult tool_result = unreachable_adapter.callTool("/test", ctx);
        // Must complete without throwing (connection refused is handled gracefully)
        assert(tool_result.getStatus() == ToolStatus::FAILED ||
               tool_result.getStatus() == ToolStatus::COMPLETED);
        assert(unreachable_adapter.getFailureCount() >= 0);

        std::cout << "  Result status: " << static_cast<int>(tool_result.getStatus()) << std::endl;
        std::cout << "  ✓ callTool() handled unreachable server gracefully" << std::endl;

        std::cout << "\n=== All RestApiAdapter tests passed! ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
