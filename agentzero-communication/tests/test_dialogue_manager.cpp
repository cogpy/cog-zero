#include "test_runner.h"

#include <opencog/agentzero/communication/DialogueManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(DialogueManager_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        DialogueManager dm(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(DialogueManager_StartEndConversation)
{
    auto as = make_as();
    DialogueManager dm(as, "AgentA");

    ASSERT_TRUE(dm.startConversation("c1", {"user1", "AgentA"}));
    ASSERT_TRUE(dm.isConversationActive("c1"));
    ASSERT_FALSE(dm.startConversation("c1", {"user1"})); // duplicate
    ASSERT_EQ(dm.getActiveCount(), static_cast<size_t>(1));

    auto parts = dm.getParticipants("c1");
    ASSERT_GE(parts.size(), static_cast<size_t>(1));

    ASSERT_TRUE(dm.endConversation("c1"));
    ASSERT_FALSE(dm.isConversationActive("c1"));
}

TEST(DialogueManager_MultiTurnProcess)
{
    auto as = make_as();
    DialogueManager dm(as, "AgentA");
    ASSERT_TRUE(dm.startConversation("chat", {"alice", "AgentA"}));

    std::string r1 = dm.processMessage("chat", "alice", "Hello AgentA");
    ASSERT_FALSE(r1.empty());

    std::string r2 = dm.processMessage("chat", "alice", "What can you do?");
    ASSERT_FALSE(r2.empty());

    auto hist = dm.getHistory("chat");
    // user + agent per turn => 4 turns
    ASSERT_EQ(hist.size(), static_cast<size_t>(4));
    ASSERT_STREQ(hist[0].speaker_id.c_str(), "alice");
    ASSERT_STREQ(hist[1].speaker_id.c_str(), "AgentA");

    auto limited = dm.getHistory("chat", 2);
    ASSERT_EQ(limited.size(), static_cast<size_t>(2));
}

TEST(DialogueManager_ContextAndTopic)
{
    auto as = make_as();
    DialogueManager dm(as, "AgentB");
    ASSERT_TRUE(dm.startConversation("c2", {"u"}));

    dm.setConversationTopic("c2", "navigation");
    ASSERT_STREQ(dm.getConversationTopic("c2").c_str(), "navigation");

    dm.setConversationContext("c2", "locale", "en");
    ASSERT_STREQ(dm.getConversationContext("c2", "locale").c_str(), "en");
    ASSERT_TRUE(dm.getConversationContext("c2", "missing").empty());

    Handle goal = as->add_node(CONCEPT_NODE, "goal:reach-base");
    dm.addConversationGoal("c2", goal);
    auto goals = dm.getConversationGoals("c2");
    ASSERT_EQ(goals.size(), static_cast<size_t>(1));
    ASSERT_TRUE(goals[0] == goal);
}

TEST(DialogueManager_SendAndLazyCreate)
{
    auto as = make_as();
    DialogueManager dm(as, "AgentC");

    // Lazy create via processMessage
    std::string reply = dm.processMessage("lazy", "bob", "Hi there");
    ASSERT_FALSE(reply.empty());
    ASSERT_TRUE(dm.isConversationActive("lazy"));

    ASSERT_TRUE(dm.sendMessage("lazy", "bob", "Proactive ping"));
    auto hist = dm.getHistory("lazy");
    ASSERT_GE(hist.size(), static_cast<size_t>(3));
}

TEST(DialogueManager_ActiveList)
{
    auto as = make_as();
    DialogueManager dm(as, "AgentD");
    dm.startConversation("a", {"u"});
    dm.startConversation("b", {"u"});
    auto ids = dm.getActiveConversations();
    ASSERT_EQ(ids.size(), static_cast<size_t>(2));
}
