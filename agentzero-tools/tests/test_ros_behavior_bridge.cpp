#include "test_runner.h"

#include <opencog/agentzero/tools/RosBehaviorBridge.h>
#include <opencog/agentzero/tools/ToolWrapper.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(RosBehavior_ConstructAndConnect)
{
    auto as = make_as();
    RosBehaviorBridge bridge("test_node", as);
    ASSERT_FALSE(bridge.isConnected());
    ASSERT_EQ(bridge.getPublishCount(), 0);
    ASSERT_EQ(bridge.getReceiveCount(), 0);

    ASSERT_TRUE(bridge.connect());
    ASSERT_TRUE(bridge.isConnected());
    // Without a live ROS master we expect simulation mode.
    ASSERT_TRUE(bridge.isSimulationMode());
}

TEST(RosBehavior_PublishSubscribeSimulate)
{
    auto as = make_as();
    RosBehaviorBridge bridge("pub_sub_node", as);
    ASSERT_TRUE(bridge.connect());

    ASSERT_TRUE(bridge.publish("/chatter", RosMessageType::STD_MSGS_STRING,
                               "{\"data\":\"hello\"}"));
    ASSERT_EQ(bridge.getPublishCount(), 1);
    ASSERT_FALSE(bridge.publish("", RosMessageType::STD_MSGS_STRING, "x"));

    int callbacks = 0;
    std::string payload;
    std::string sid = bridge.subscribe("/chatter", RosMessageType::STD_MSGS_STRING,
        [&](const RosMessage& m) {
            ++callbacks;
            payload = m.payload;
        });
    ASSERT_FALSE(sid.empty());

    RosMessage msg;
    msg.topic = "/chatter";
    msg.msg_type = RosMessageType::STD_MSGS_STRING;
    msg.payload = "{\"data\":\"sim\"}";
    ASSERT_EQ(bridge.simulateMessage(msg), 1);
    ASSERT_EQ(callbacks, 1);
    ASSERT_EQ(payload, std::string("{\"data\":\"sim\"}"));

    ASSERT_TRUE(bridge.unsubscribe(sid));
    ASSERT_EQ(bridge.simulateMessage(msg), 0);
    ASSERT_EQ(callbacks, 1);
}

TEST(RosBehavior_ServiceAndBehaviorTree)
{
    auto as = make_as();
    RosBehaviorBridge bridge("svc_node", as);
    ASSERT_TRUE(bridge.connect());

    std::string resp = bridge.callService("/get_status", "{\"q\":1}");
    ASSERT_FALSE(resp.empty());
    ASSERT_EQ(bridge.getServiceCallCount(), 1);

    std::string bt = R"(
BehaviorTree:
  Sequence:
    - Action: MoveForward
)";
    ToolResult ok = bridge.executeBehaviorTree(bt);
    ASSERT_TRUE(ok.isSuccess());

    ToolResult empty = bridge.executeBehaviorTree("");
    ASSERT_FALSE(empty.isSuccess());

    ToolResult bad = bridge.executeBehaviorTree("not a tree");
    ASSERT_FALSE(bad.isSuccess());
}

TEST(RosBehavior_ScriptAndToolWrappers)
{
    auto as = make_as();
    RosBehaviorBridge bridge("tools_node", as);
    ASSERT_TRUE(bridge.connect());

    ToolResult script = bridge.executeBehaviorScript(
        "/tmp/nonexistent_agentzero_script.py", {{"a", "1"}});
    ASSERT_EQ(bridge.getScriptExecutionCount(), 1);
    // Non-existent script should not succeed
    ASSERT_FALSE(script.isSuccess());

    auto pub_tool = bridge.createTopicPublisherTool(
        "cmd_pub", "/cmd_vel", RosMessageType::GEOMETRY_MSGS_TWIST);
    ASSERT_TRUE(pub_tool != nullptr);
    ASSERT_EQ(pub_tool->getToolName(), std::string("cmd_pub"));
    ASSERT_EQ(pub_tool->getToolType(), ToolType::ROS_BEHAVIOR);

    auto svc_tool = bridge.createServiceCallerTool("status_svc", "/get_status");
    ASSERT_TRUE(svc_tool != nullptr);

    RosMessage msg;
    msg.topic = "/pose";
    msg.msg_type = RosMessageType::GEOMETRY_MSGS_POSE;
    msg.payload = "{\"x\":1}";
    msg.fields["x"] = "1";
    Handle atom = bridge.rosMessageToAtom(msg, as);
    ASSERT_TRUE(atom != Handle::UNDEFINED);

    ASSERT_EQ(RosBehaviorBridge::messageTypeToString(RosMessageType::STD_MSGS_BOOL),
              std::string("std_msgs/Bool"));
    ASSERT_EQ(RosBehaviorBridge::messageTypeFromString("std_msgs/String"),
              RosMessageType::STD_MSGS_STRING);

    ASSERT_FALSE(bridge.getStatistics().empty());
    bridge.disconnect();
    ASSERT_FALSE(bridge.isConnected());
}
