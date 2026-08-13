#include "test_runner.h"

#include <thread>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/AgentZeroCore.h>
#include <opencog/agentzero/CognitiveLoop.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(CognitiveLoop_SingleCycle)
{
    auto as = createAtomSpace();
    AgentZeroCore core("LoopAgent", as);
    ASSERT_TRUE(core.initialize("LoopAgent", as));
    auto* loop = core.getCognitiveLoop();
    ASSERT_TRUE(loop != nullptr);

    loop->addPercept("sensor", "hello world", "text", 0.8);
    loop->runSingleCycle();

    ASSERT_GE(loop->getCycleCount(), 1u);
    ASSERT_NE(loop->getLastPerceptAtom(), Handle::UNDEFINED);

    auto stats = loop->lastStats();
    ASSERT_EQ(stats.cycle_number, 1u);
    ASSERT_EQ(stats.percepts_processed, 1u);

    // Percept encoded as EvaluationLink in AtomSpace
    auto evals = as->get_handles_by_type(EVALUATION_LINK);
    ASSERT_GT(evals.size(), 0u);
}

TEST(CognitiveLoop_StartStop)
{
    auto as = createAtomSpace();
    AgentZeroCore core("BgLoop", as);
    ASSERT_TRUE(core.initialize("BgLoop", as));
    auto* loop = core.getCognitiveLoop();
    loop->setCycleInterval(std::chrono::milliseconds(10));
    loop->setMaxCycles(3);
    ASSERT_TRUE(loop->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    ASSERT_TRUE(loop->stop());
    ASSERT_GE(loop->getCycleCount(), 1u);
}
