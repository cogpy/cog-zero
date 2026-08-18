#include "test_runner.h"

#include <opencog/agentzero/memory/ContextManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::memory;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ContextManager_Initialize)
{
    auto as = make_as();
    ContextManager cm(as);
    ASSERT_TRUE(cm.initialize());
    ASSERT_EQ(cm.getContextCount(), static_cast<size_t>(0));
    ASSERT_TRUE(cm.shutdown());
}

TEST(ContextManager_CreateSwitchAndAtoms)
{
    auto as = make_as();
    ContextManager cm(as);
    cm.initialize();

    ASSERT_TRUE(cm.createContext("task_nav", ContextType::TASK,
                                 {{"goal", "reach_door"}}, 0.8));
    ASSERT_TRUE(cm.createContext("social", ContextType::SOCIAL, {}, 0.4));
    ASSERT_TRUE(cm.hasContext("task_nav"));
    ASSERT_FALSE(cm.createContext("task_nav")); // duplicate

    Handle door = as->add_node(CONCEPT_NODE, "Door");
    Handle map = as->add_node(CONCEPT_NODE, "Map");
    ASSERT_TRUE(cm.addAtomToContext("task_nav", door));
    ASSERT_TRUE(cm.addAtomToContext("task_nav", map));
    ASSERT_EQ(cm.getAtomsInContext("task_nav").size(), static_cast<size_t>(2));

    ASSERT_TRUE(cm.setActiveContext("task_nav"));
    ASSERT_EQ(cm.getActiveContext(), std::string("task_nav"));

    auto ctxs = cm.getContextsForAtom(door);
    ASSERT_EQ(ctxs.count("task_nav"), static_cast<size_t>(1));

    ASSERT_TRUE(cm.removeAtomFromContext("task_nav", map));
    ASSERT_EQ(cm.getAtomsInContext("task_nav").size(), static_cast<size_t>(1));
}

TEST(ContextManager_RelevanceScoring)
{
    auto as = make_as();
    ContextManager cm(as);
    cm.initialize();

    cm.createContext("focus", ContextType::COGNITIVE, {}, 0.9);
    cm.createContext("other", ContextType::TEMPORAL, {}, 0.2);
    cm.setActiveContext("focus");

    Handle in_ctx = as->add_node(CONCEPT_NODE, "InFocus");
    Handle outsider = as->add_node(CONCEPT_NODE, "Outsider");
    cm.addAtomToContext("focus", in_ctx);
    cm.addAtomToContext("other", outsider);

    double s_in = cm.scoreAtomRelevance("focus", in_ctx);
    double s_out = cm.scoreAtomRelevance("focus", outsider);
    ASSERT_GT(s_in, s_out);
    ASSERT_GT(s_in, 0.5);

    auto ranked = cm.retrieveRelevantAtoms("focus", 10);
    ASSERT_EQ(ranked.size(), static_cast<size_t>(1));
    ASSERT_EQ(ranked[0].first, in_ctx);

    auto cand = cm.retrieveRelevantAtoms("focus", {in_ctx, outsider}, 10);
    ASSERT_EQ(cand.size(), static_cast<size_t>(2));
    ASSERT_EQ(cand[0].first, in_ctx);

    auto relevant_ctx = cm.findRelevantContexts(in_ctx, 5);
    ASSERT_GE(relevant_ctx.size(), static_cast<size_t>(1));
    ASSERT_EQ(relevant_ctx[0], std::string("focus"));
}

TEST(ContextManager_ImportanceAndMerge)
{
    auto as = make_as();
    ContextManager cm(as);
    cm.initialize();

    cm.createContext("src", ContextType::TASK, {}, 0.5);
    cm.createContext("dst", ContextType::TASK, {}, 0.6);
    Handle h = as->add_node(CONCEPT_NODE, "MergedAtom");
    cm.addAtomToContext("src", h);

    ASSERT_TRUE(cm.boostContextImportance("src", 1.5));
    ASSERT_GT(cm.getContextImportance("src"), 0.5);

    ASSERT_TRUE(cm.mergeContexts("src", "dst", true));
    ASSERT_FALSE(cm.hasContext("src"));
    ASSERT_TRUE(cm.getAtomsInContext("dst").count(h) > 0);

    auto most = cm.getMostImportantContexts(1);
    ASSERT_EQ(most.size(), static_cast<size_t>(1));
}

TEST(ContextManager_SnapshotAndStats)
{
    auto as = make_as();
    ContextManager cm(as);
    cm.initialize();
    cm.createContext("snap", ContextType::ENVIRONMENTAL, {{"loc", "lab"}}, 0.7);
    Handle a = as->add_node(CONCEPT_NODE, "Sensor");
    cm.addAtomToContext("snap", a);
    cm.setActiveContext("snap");

    Handle snap = cm.createContextSnapshot();
    ASSERT_TRUE(static_cast<bool>(snap));

    auto stats = cm.getStatistics();
    ASSERT_EQ(stats.total_contexts, static_cast<size_t>(1));
    ASSERT_TRUE(cm.clearAllContexts());
    ASSERT_EQ(cm.getContextCount(), static_cast<size_t>(0));
}
