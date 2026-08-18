#include "test_runner.h"

#include <opencog/agentzero/communication/AgentComms.h>
#include <opencog/atomspace/AtomSpace.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(AgentComms_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        AgentComms ac(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(AgentComms_PeerRegistry)
{
    auto as = make_as();
    AgentComms ac(as, "alpha");

    ASSERT_TRUE(ac.registerPeer("beta"));
    ASSERT_TRUE(ac.isPeerRegistered("beta"));
    ASSERT_FALSE(ac.registerPeer("alpha")); // self
    ASSERT_FALSE(ac.registerPeer(""));

    auto peers = ac.listPeers();
    ASSERT_EQ(peers.size(), static_cast<size_t>(1));
    ASSERT_TRUE(ac.unregisterPeer("beta"));
    ASSERT_FALSE(ac.isPeerRegistered("beta"));
}

TEST(AgentComms_SendReceiveLoopback)
{
    auto as = make_as();
    AgentComms ac(as, "alpha");

    ASSERT_TRUE(ac.send("alpha", "ping", MessageType::INFO));
    ASSERT_EQ(ac.inboxSize(), static_cast<size_t>(1));
    ASSERT_EQ(ac.getSentCount(), static_cast<size_t>(1));
    ASSERT_EQ(ac.getReceivedCount(), static_cast<size_t>(1));

    AgentMessage msg;
    ASSERT_TRUE(ac.receive(msg));
    ASSERT_STREQ(msg.content.c_str(), "ping");
    ASSERT_STREQ(msg.sender_id.c_str(), "alpha");
    ASSERT_EQ(msg.type, MessageType::INFO);
    ASSERT_FALSE(ac.receive(msg));
}

TEST(AgentComms_BroadcastAndDispatch)
{
    auto as = make_as();
    AgentComms ac(as, "alpha");
    ac.registerPeer("beta");

    int seen = 0;
    ac.setHandler(MessageType::NOTIFICATION, [&](const AgentMessage& m) {
        if (m.content == "hello-all") ++seen;
    });

    ASSERT_TRUE(ac.broadcast("hello-all", MessageType::NOTIFICATION));
    size_t n = ac.dispatch(10);
    ASSERT_GE(n, static_cast<size_t>(1));
    ASSERT_EQ(seen, 1);
}

TEST(AgentComms_DeliverAndTypes)
{
    auto as = make_as();
    AgentComms ac(as, "alpha");

    AgentMessage inbound;
    inbound.id = AgentComms::generateMessageId();
    inbound.sender_id = "beta";
    inbound.recipient_id = "alpha";
    inbound.type = MessageType::REQUEST;
    inbound.priority = MessagePriority::HIGH;
    inbound.content = "need-help";
    inbound.timestamp = std::chrono::system_clock::now();

    ASSERT_TRUE(ac.deliver(inbound));
    ASSERT_EQ(ac.inboxSize(), static_cast<size_t>(1));

    ASSERT_STREQ(AgentComms::messageTypeToString(MessageType::QUERY).c_str(), "QUERY");
    ASSERT_EQ(AgentComms::stringToMessageType("HEARTBEAT"), MessageType::HEARTBEAT);
    ASSERT_STREQ(AgentComms::priorityToString(MessagePriority::CRITICAL).c_str(), "CRITICAL");

    ac.clearInbox();
    ASSERT_EQ(ac.inboxSize(), static_cast<size_t>(0));
}

TEST(AgentComms_EmptySendFails)
{
    auto as = make_as();
    AgentComms ac(as, "alpha");
    ASSERT_FALSE(ac.send("beta", ""));
    ASSERT_FALSE(ac.broadcast(""));
}
