#include "test_runner.h"

#include <opencog/agentzero/communication/HumanInterface.h>
#include <opencog/atomspace/AtomSpace.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(HumanInterface_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        HumanInterface hi(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(HumanInterface_SessionLifecycle)
{
    auto as = make_as();
    HumanInterface hi(as, "Guide");

    std::string sid = hi.startSession("alice");
    ASSERT_FALSE(sid.empty());
    ASSERT_TRUE(hi.isSessionActive(sid));
    ASSERT_EQ(hi.getActiveSessionCount(), static_cast<size_t>(1));

    auto sessions = hi.listSessions();
    ASSERT_EQ(sessions.size(), static_cast<size_t>(1));

    ASSERT_TRUE(hi.endSession(sid));
    ASSERT_FALSE(hi.isSessionActive(sid));
    ASSERT_EQ(hi.getActiveSessionCount(), static_cast<size_t>(0));
}

TEST(HumanInterface_ProcessInput)
{
    auto as = make_as();
    HumanInterface hi(as, "Guide");
    std::string sid = hi.startSession("bob");

    std::string reply = hi.processInput(sid, "Hello!");
    ASSERT_FALSE(reply.empty());
    ASSERT_NE(reply.find("Guide:"), std::string::npos);

    std::string empty = hi.processInput(sid, "   ");
    ASSERT_NE(empty.find("non-empty"), std::string::npos);

    std::string dead = hi.processInput("no-such-session", "Hi");
    ASSERT_NE(dead.find("not active"), std::string::npos);
}

TEST(HumanInterface_DefaultSessionAndReset)
{
    auto as = make_as();
    HumanInterface hi(as, "Buddy");

    std::string r1 = hi.processHumanInput("Hi there");
    ASSERT_FALSE(r1.empty());
    ASSERT_EQ(hi.getActiveSessionCount(), static_cast<size_t>(1));

    hi.resetSession();
    ASSERT_EQ(hi.getActiveSessionCount(), static_cast<size_t>(1));

    std::string proactive = hi.generateResponse("topic:status");
    ASSERT_NE(proactive.find("Buddy:"), std::string::npos);

    ASSERT_STREQ(hi.getAgentName().c_str(), "Buddy");
}
