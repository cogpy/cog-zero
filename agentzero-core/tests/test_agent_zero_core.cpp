#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/AgentZeroCore.h>
#include <opencog/agentzero/CognitiveLoop.h>
#include <opencog/agentzero/TaskManager.h>
#include <opencog/agentzero/KnowledgeIntegrator.h>
#include <opencog/agentzero/ReasoningEngine.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(AgentZeroCore_ConstructAndInitialize)
{
    auto as = createAtomSpace();
    AgentZeroCore core("TestAgent", as);
    ASSERT_EQ(core.getAgentName(), std::string("TestAgent"));
    ASSERT_FALSE(core.isRunning());
    ASSERT_FALSE(core.isInitialized());

    ASSERT_TRUE(core.initialize("TestAgent", as));
    ASSERT_TRUE(core.isInitialized());
    ASSERT_TRUE(core.getAtomSpace() != nullptr);
    ASSERT_NE(core.getAgentSelfAtom(), Handle::UNDEFINED);
    ASSERT_TRUE(core.getCognitiveLoop() != nullptr);
    ASSERT_TRUE(core.getTaskManager() != nullptr);
    ASSERT_TRUE(core.getKnowledgeIntegrator() != nullptr);
    ASSERT_TRUE(core.getReasoningEngine() != nullptr);
}

TEST(AgentZeroCore_StartStop)
{
    auto as = createAtomSpace();
    AgentZeroCore core("Runner", as);
    ASSERT_TRUE(core.initialize("Runner", as));
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(core.isRunning());
    ASSERT_TRUE(core.stop());
    ASSERT_FALSE(core.isRunning());
}

TEST(AgentZeroCore_AtomSpaceState)
{
    auto as = createAtomSpace();
    AgentZeroCore core("StateAgent", as);
    ASSERT_TRUE(core.initialize("StateAgent", as));

    // Agent self + working memory + goal anchors should be present
    ASSERT_GT(as->get_size(), 0u);
    ASSERT_NE(core.getAgentSelfAtom(), Handle::UNDEFINED);
    ASSERT_NE(core.getCurrentGoal(), Handle::UNDEFINED);

    // Set a goal atom and verify
    Handle goal = as->add_node(CONCEPT_NODE, "ExploreEnvironment");
    ASSERT_TRUE(core.setGoal(goal));
    ASSERT_EQ(core.getCurrentGoal(), goal);

    // Task + knowledge + reasoning side effects via cognitive step
    auto* tm = core.getTaskManager();
    Handle task = tm->createTask("look-around", "scan surroundings");
    ASSERT_NE(task, Handle::UNDEFINED);
    ASSERT_GT(tm->getPendingTaskCount(), 0u);

    auto* ki = core.getKnowledgeIntegrator();
    Handle fact = ki->addFact("Agent", "is_a", "CognitiveSystem");
    ASSERT_NE(fact, Handle::UNDEFINED);

    ASSERT_TRUE(core.processCognitiveStep());

    // AtomSpace holds goals, tasks, knowledge
    ASSERT_GT(as->get_size(), 5u);
    auto concepts = as->get_handles_by_type(CONCEPT_NODE);
    ASSERT_GT(concepts.size(), 3u);
    auto evals = as->get_handles_by_type(EVALUATION_LINK);
    ASSERT_GT(evals.size(), 0u);
    auto states = as->get_handles_by_type(STATE_LINK);
    ASSERT_GT(states.size(), 0u);

    std::string status = core.getStatusInfo();
    ASSERT_TRUE(status.find("StateAgent") != std::string::npos);
    ASSERT_TRUE(status.find("initialized") != std::string::npos);
}
