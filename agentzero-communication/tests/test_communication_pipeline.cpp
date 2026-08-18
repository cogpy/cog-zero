#include "test_runner.h"

#include <opencog/agentzero/communication/LanguageProcessor.h>
#include <opencog/agentzero/communication/DialogueManager.h>
#include <opencog/agentzero/communication/AgentComms.h>
#include <opencog/agentzero/communication/HumanInterface.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

/**
 * End-to-end pipeline on one AtomSpace:
 * human input → dialogue/NLU → agent reply → inter-agent notify.
 */
TEST(Pipeline_HumanDialogueAndAgentComms)
{
    auto as = createAtomSpace();

    HumanInterface human(as, "AgentZero");
    AgentComms bus(as, "AgentZero");
    bus.registerPeer("Observer");

    int notifications = 0;
    bus.setHandler(MessageType::NOTIFICATION, [&](const AgentMessage& m) {
        if (m.content.find("dialogue-turn") != std::string::npos)
            ++notifications;
    });

    std::string sid = human.startSession("researcher");
    ASSERT_FALSE(sid.empty());

    std::string r1 = human.processInput(sid, "Hello AgentZero");
    ASSERT_FALSE(r1.empty());
    ASSERT_NE(r1.find("AgentZero:"), std::string::npos);

    std::string r2 = human.processInput(sid, "What can you do?");
    ASSERT_FALSE(r2.empty());

    // Dialogue history should contain both user and agent turns
    auto& dm = human.getDialogueManager();
    auto active = dm.getActiveConversations();
    ASSERT_EQ(active.size(), static_cast<size_t>(1));
    auto hist = dm.getHistory(active.front());
    ASSERT_GE(hist.size(), static_cast<size_t>(4));

    // Language processor stats moved through dialogue's own processor;
    // human's processor remains available for direct use.
    auto direct = human.getLanguageProcessor().parseText("Plan a route to Base");
    ASSERT_TRUE(direct.success);
    ASSERT_FALSE(direct.parsed_atoms.empty());

    // Notify peers that a dialogue turn completed
    ASSERT_TRUE(bus.broadcast("dialogue-turn:" + active.front(),
                              MessageType::NOTIFICATION));
    size_t dispatched = bus.dispatch(8);
    ASSERT_GE(dispatched, static_cast<size_t>(1));
    ASSERT_EQ(notifications, 1);

    // AtomSpace should hold utterances, sessions, messages
    ASSERT_GT(as->get_size(), static_cast<size_t>(10));

    ASSERT_TRUE(human.endSession(sid));
    ASSERT_EQ(human.getActiveSessionCount(), static_cast<size_t>(0));
}

TEST(Pipeline_TwoAgentMessageExchange)
{
    auto as = createAtomSpace();
    AgentComms a(as, "A");
    AgentComms b(as, "B");

    a.registerPeer("B");
    b.registerPeer("A");

    // Simulate A → B by delivering into B's inbox
    AgentMessage msg;
    msg.id = AgentComms::generateMessageId();
    msg.sender_id = "A";
    msg.recipient_id = "B";
    msg.type = MessageType::TASK_ASSIGNMENT;
    msg.priority = MessagePriority::HIGH;
    msg.content = "survey-sector-7";
    msg.timestamp = std::chrono::system_clock::now();

    ASSERT_TRUE(a.sendMessage(msg)); // logs to AtomSpace
    ASSERT_TRUE(b.deliver(msg));

    std::string accepted;
    b.setHandler(MessageType::TASK_ASSIGNMENT, [&](const AgentMessage& m) {
        accepted = m.content;
    });
    ASSERT_EQ(b.dispatch(1), static_cast<size_t>(1));
    ASSERT_STREQ(accepted.c_str(), "survey-sector-7");

    // B responds
    AgentMessage reply;
    reply.id = AgentComms::generateMessageId();
    reply.sender_id = "B";
    reply.recipient_id = "A";
    reply.type = MessageType::RESPONSE;
    reply.content = "ack:survey-sector-7";
    reply.timestamp = std::chrono::system_clock::now();
    ASSERT_TRUE(a.deliver(reply));

    AgentMessage got;
    ASSERT_TRUE(a.receive(got));
    ASSERT_STREQ(got.content.c_str(), "ack:survey-sector-7");
}
