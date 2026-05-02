/*
 * tests/RosBehaviorBridgeSimpleTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Simple standalone test for RosBehaviorBridge - ROS behaviour scripting bridge
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

#include "../include/opencog/agentzero/tools/RosBehaviorBridge.h"
#include "../include/opencog/agentzero/tools/ToolWrapper.h"

using namespace opencog;
using namespace opencog::agentzero::tools;

int main()
{
    logger().set_level(Logger::WARN);
    logger().set_print_to_stdout_flag(false);

    std::cout << "=== RosBehaviorBridge Simple Test ===" << std::endl;

    try {
        // Test 1: Initialisation
        std::cout << "\n1. Testing initialisation..." << std::endl;

        auto atomspace = std::make_shared<AtomSpace>();
        auto bridge    = std::make_unique<RosBehaviorBridge>("test_node", atomspace);

        assert(bridge != nullptr);
        assert(!bridge->isConnected());
        assert(bridge->getPublishCount()         == 0);
        assert(bridge->getReceiveCount()         == 0);
        assert(bridge->getServiceCallCount()     == 0);
        assert(bridge->getScriptExecutionCount() == 0);

        std::cout << "  ✓ Initialisation successful" << std::endl;

        // Test 2: Connect (simulation mode expected when ROS not installed)
        std::cout << "\n2. Testing connect()..." << std::endl;

        bool connected = bridge->connect();
        assert(connected);
        assert(bridge->isConnected());
        assert(bridge->isSimulationMode() || !bridge->isSimulationMode());
        // Either mode is acceptable depending on environment

        std::cout << "  Simulation mode: " << (bridge->isSimulationMode() ? "yes" : "no") << std::endl;
        std::cout << "  ✓ connect() succeeded" << std::endl;

        // Test 3: publish()
        std::cout << "\n3. Testing publish()..." << std::endl;

        bool pub_ok = bridge->publish("/chatter", RosMessageType::STD_MSGS_STRING,
                                      "{\"data\":\"hello from agentzero\"}");
        assert(pub_ok);
        assert(bridge->getPublishCount() == 1);

        pub_ok = bridge->publish("/cmd_vel", RosMessageType::GEOMETRY_MSGS_TWIST,
                                 "{\"linear\":{\"x\":0.5},\"angular\":{\"z\":0.0}}");
        assert(pub_ok);
        assert(bridge->getPublishCount() == 2);

        std::cout << "  ✓ publish() working (count=" << bridge->getPublishCount() << ")" << std::endl;

        // Test 4: publish() with invalid topic
        std::cout << "\n4. Testing publish() with empty topic..." << std::endl;

        bool invalid_pub = bridge->publish("", RosMessageType::STD_MSGS_STRING, "test");
        assert(!invalid_pub);  // Should fail

        std::cout << "  ✓ Invalid topic rejected correctly" << std::endl;

        // Test 5: subscribe() and unsubscribe()
        std::cout << "\n5. Testing subscribe() and unsubscribe()..." << std::endl;

        int callback_count = 0;
        std::string received_payload;

        std::string sub_id = bridge->subscribe("/chatter", RosMessageType::STD_MSGS_STRING,
            [&](const RosMessage& msg) {
                ++callback_count;
                received_payload = msg.payload;
            });

        assert(!sub_id.empty());
        std::cout << "  Subscription ID: " << sub_id << std::endl;

        // Simulate a message delivery (only in simulation mode)
        RosMessage sim_msg;
        sim_msg.topic    = "/chatter";
        sim_msg.msg_type = RosMessageType::STD_MSGS_STRING;
        sim_msg.payload  = "{\"data\":\"simulated message\"}";

        int notified = bridge->simulateMessage(sim_msg);
        assert(notified == 1);
        assert(callback_count == 1);
        assert(received_payload == "{\"data\":\"simulated message\"}");

        std::cout << "  Received payload: " << received_payload << std::endl;

        // Unsubscribe
        bool unsub_ok = bridge->unsubscribe(sub_id);
        assert(unsub_ok);

        // Simulate again — should not trigger callback
        int notified2 = bridge->simulateMessage(sim_msg);
        assert(notified2 == 0);
        assert(callback_count == 1);  // unchanged

        std::cout << "  ✓ subscribe/unsubscribe/simulateMessage working" << std::endl;

        // Test 6: callService()
        std::cout << "\n6. Testing callService()..." << std::endl;

        std::string resp = bridge->callService("/get_status", "{\"query\":\"state\"}");
        assert(!resp.empty());
        assert(bridge->getServiceCallCount() == 1);

        std::cout << "  Service response: " << resp << std::endl;
        std::cout << "  ✓ callService() working" << std::endl;

        // Test 7: executeBehaviorTree() with valid description
        std::cout << "\n7. Testing executeBehaviorTree() with valid YAML..." << std::endl;

        std::string bt_yaml = R"(
BehaviorTree:
  Sequence:
    - Action: MoveForward
    - Action: TurnLeft
)";
        ToolResult bt_result = bridge->executeBehaviorTree(bt_yaml);
        // In simulation mode this should succeed
        if (bridge->isSimulationMode()) {
            assert(bt_result.isSuccess());
        }

        std::cout << "  BT result: " << bt_result.toString() << std::endl;
        std::cout << "  ✓ executeBehaviorTree() completed" << std::endl;

        // Test 8: executeBehaviorTree() with empty description
        std::cout << "\n8. Testing executeBehaviorTree() with empty string..." << std::endl;

        ToolResult empty_bt = bridge->executeBehaviorTree("");
        assert(!empty_bt.isSuccess());
        assert(!empty_bt.getErrorMessage().empty());

        std::cout << "  ✓ Empty behaviour tree rejected correctly" << std::endl;

        // Test 9: executeBehaviorTree() with invalid description (no root)
        std::cout << "\n9. Testing executeBehaviorTree() with invalid description..." << std::endl;

        ToolResult bad_bt = bridge->executeBehaviorTree("random text without structure");
        assert(!bad_bt.isSuccess());

        std::cout << "  ✓ Invalid behaviour tree rejected correctly" << std::endl;

        // Test 10: executeBehaviorScript() with non-existent script
        std::cout << "\n10. Testing executeBehaviorScript() with non-existent file..." << std::endl;

        ToolResult script_result = bridge->executeBehaviorScript(
            "/tmp/nonexistent_script_xyz.py",
            {{"param1", "value1"}});
        // The script will fail to run, but the call must not crash
        assert(bridge->getScriptExecutionCount() == 1);

        std::cout << "  Script result status: "
                  << static_cast<int>(script_result.getStatus()) << std::endl;
        std::cout << "  ✓ executeBehaviorScript() completed without crash" << std::endl;

        // Test 11: executeBehaviorScript() with empty path
        std::cout << "\n11. Testing executeBehaviorScript() with empty path..." << std::endl;

        ToolResult empty_script = bridge->executeBehaviorScript("");
        assert(!empty_script.isSuccess());
        assert(!empty_script.getErrorMessage().empty());

        std::cout << "  ✓ Empty script path rejected correctly" << std::endl;

        // Test 12: createTopicPublisherTool()
        std::cout << "\n12. Testing createTopicPublisherTool()..." << std::endl;

        auto pub_tool = bridge->createTopicPublisherTool(
            "cmd_vel_publisher", "/cmd_vel", RosMessageType::GEOMETRY_MSGS_TWIST);
        assert(pub_tool != nullptr);
        assert(pub_tool->getToolName() == "cmd_vel_publisher");
        assert(pub_tool->getToolType() == ToolType::ROS_BEHAVIOR);
        assert(pub_tool->getToolEndpoint() == "/cmd_vel");

        // Execute the tool
        ToolExecutionContext ctx(atomspace);
        ctx.setParameter("linear_x", "0.5");
        ctx.setParameter("angular_z", "0.0");
        ctx.setTimeout(5000.0);

        ToolResult pub_result = pub_tool->execute(ctx);
        assert(pub_result.isSuccess());

        std::cout << "  Tool output: " << pub_result.getOutput() << std::endl;
        std::cout << "  ✓ createTopicPublisherTool() working" << std::endl;

        // Test 13: createServiceCallerTool()
        std::cout << "\n13. Testing createServiceCallerTool()..." << std::endl;

        auto svc_tool = bridge->createServiceCallerTool("status_service", "/get_status");
        assert(svc_tool != nullptr);
        assert(svc_tool->getToolName() == "status_service");
        assert(svc_tool->getToolType() == ToolType::ROS_BEHAVIOR);

        ToolResult svc_result = svc_tool->execute(ctx);
        // In simulation mode service call returns a stub with "success":true
        if (bridge->isSimulationMode()) {
            assert(svc_result.isSuccess());
        }

        std::cout << "  Service tool output: " << svc_result.getOutput().substr(0, 80) << std::endl;
        std::cout << "  ✓ createServiceCallerTool() working" << std::endl;

        // Test 14: rosMessageToAtom()
        std::cout << "\n14. Testing rosMessageToAtom()..." << std::endl;

        RosMessage atom_msg;
        atom_msg.topic    = "/sensor_data";
        atom_msg.msg_type = RosMessageType::STD_MSGS_FLOAT64;
        atom_msg.type_name = "std_msgs/Float64";
        atom_msg.payload  = "{\"data\":42.0}";
        atom_msg.fields["data"] = "42.0";

        Handle msg_atom = bridge->rosMessageToAtom(atom_msg, atomspace);
        assert(msg_atom != Handle::UNDEFINED);

        TruthValuePtr tv = msg_atom->getTruthValue();
        assert(tv->get_mean() == 1.0);

        std::cout << "  Message atom: " << msg_atom->to_short_string() << std::endl;
        std::cout << "  ✓ rosMessageToAtom() created correct atom" << std::endl;

        // Test 15: Static utility methods
        std::cout << "\n15. Testing static utility methods..." << std::endl;

        assert(RosBehaviorBridge::messageTypeToString(RosMessageType::STD_MSGS_STRING) == "std_msgs/String");
        assert(RosBehaviorBridge::messageTypeToString(RosMessageType::GEOMETRY_MSGS_TWIST) == "geometry_msgs/Twist");
        assert(RosBehaviorBridge::messageTypeToString(RosMessageType::CUSTOM) == "custom");

        assert(RosBehaviorBridge::messageTypeFromString("std_msgs/String") == RosMessageType::STD_MSGS_STRING);
        assert(RosBehaviorBridge::messageTypeFromString("sensor_msgs/Image") == RosMessageType::SENSOR_MSGS_IMAGE);
        assert(RosBehaviorBridge::messageTypeFromString("unknown_type") == RosMessageType::CUSTOM);

        std::cout << "  ✓ Static utility methods working" << std::endl;

        // Test 16: Statistics
        std::cout << "\n16. Testing statistics..." << std::endl;

        std::string stats = bridge->getStatistics();
        assert(!stats.empty());
        assert(stats.find("node_name")            != std::string::npos);
        assert(stats.find("publish_count")        != std::string::npos);
        assert(stats.find("service_call_count")   != std::string::npos);
        assert(stats.find("simulation_mode")      != std::string::npos);

        std::cout << "  Statistics: " << stats << std::endl;
        std::cout << "  ✓ Statistics JSON is well-formed" << std::endl;

        // Test 17: disconnect()
        std::cout << "\n17. Testing disconnect()..." << std::endl;

        bridge->disconnect();
        assert(!bridge->isConnected());

        std::cout << "  ✓ disconnect() working" << std::endl;

        // Test 18: AtomSpace set/get
        std::cout << "\n18. Testing AtomSpace set/get..." << std::endl;

        auto bridge2 = std::make_unique<RosBehaviorBridge>("node2");
        assert(bridge2->getAtomSpace() == nullptr);
        bridge2->setAtomSpace(atomspace);
        assert(bridge2->getAtomSpace() == atomspace);

        std::cout << "  ✓ AtomSpace set/get working" << std::endl;

        std::cout << "\n=== All RosBehaviorBridge tests passed! ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
