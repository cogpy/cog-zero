#include "test_runner.h"

#include <opencog/agentzero/WorkingMemory.h>
#include <opencog/agentzero/memory/EpisodicMemory.h>
#include <opencog/agentzero/memory/LongTermMemory.h>
#include <opencog/agentzero/memory/ContextManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;
using namespace opencog::agentzero::memory;

TEST(MemoryPipeline_EndToEnd)
{
    auto as = createAtomSpace();

    WorkingMemory wm(as, 100, 0.1, std::chrono::seconds(3600));
    EpisodicMemory episodic(as, 1000);
    LongTermMemory ltm(as, MemoryConfig{});
    ContextManager ctx(as);

    ASSERT_TRUE(episodic.initialize());
    ASSERT_TRUE(ltm.initialize());
    ASSERT_TRUE(ctx.initialize());

    // Perceive -> working memory
    Handle percept = as->add_node(CONCEPT_NODE, "RedBall");
    Handle action = as->add_node(CONCEPT_NODE, "Grasp");
    ASSERT_TRUE(wm.addItem(percept, 0.85, "perception"));
    ASSERT_TRUE(wm.addItem(action, 0.7, "planning"));

    // Context tracks the task
    ASSERT_TRUE(ctx.createContext("grasp_task", ContextType::TASK, {{"obj", "ball"}}, 0.8));
    ASSERT_TRUE(ctx.addAtomToContext("grasp_task", percept));
    ASSERT_TRUE(ctx.addAtomToContext("grasp_task", action));
    ASSERT_TRUE(ctx.setActiveContext("grasp_task"));

    // Episodic record of the sequence
    std::string eid = episodic.storeEpisodeMulti({percept, action}, 0.9, "grasp_task");
    ASSERT_FALSE(eid.empty());

    // Promote high-importance working items into long-term memory
    for (const auto& item : wm.getImportantItems(0.6)) {
        ASSERT_TRUE(ltm.store(item->atom, MemoryImportance::HIGH,
                              PersistenceLevel::LONG_TERM,
                              {ContextType::TASK}));
    }

    // Relevance: action and percept should score highly in grasp_task
    auto ranked = ctx.retrieveRelevantAtoms("grasp_task", 5);
    ASSERT_EQ(ranked.size(), static_cast<size_t>(2));
    ASSERT_GT(ranked[0].second, 0.5);

    // Cross-check episodic context retrieval
    auto eps = episodic.getEpisodesByContext("grasp_task");
    ASSERT_EQ(eps.size(), static_cast<size_t>(1));

    // LTM still holds promoted atoms
    ASSERT_TRUE(ltm.contains(percept));
    ASSERT_TRUE(ltm.contains(action));

    auto stats = ctx.getStatistics();
    ASSERT_EQ(stats.total_contexts, static_cast<size_t>(1));
    ASSERT_EQ(episodic.getEpisodeCount(), static_cast<size_t>(1));
    ASSERT_GE(wm.getCurrentSize(), static_cast<size_t>(2));
}
