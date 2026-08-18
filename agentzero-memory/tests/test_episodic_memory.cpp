#include "test_runner.h"

#include <opencog/agentzero/memory/EpisodicMemory.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::memory;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(EpisodicMemory_Initialize)
{
    auto as = make_as();
    EpisodicMemory mem(as, /*max=*/100);
    ASSERT_TRUE(mem.initialize());
    ASSERT_EQ(mem.getEpisodeCount(), static_cast<size_t>(0));
}

TEST(EpisodicMemory_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        EpisodicMemory mem(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(EpisodicMemory_StoreAndRetrieve)
{
    auto as = make_as();
    EpisodicMemory mem(as, 100);
    mem.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "event_a");
    Handle b = as->add_node(CONCEPT_NODE, "event_b");

    std::string id1 = mem.storeEpisode(a, 0.8, "nav");
    std::string id2 = mem.storeEpisodeMulti({a, b}, 0.6, "nav");

    ASSERT_FALSE(id1.empty());
    ASSERT_FALSE(id2.empty());
    ASSERT_EQ(mem.getEpisodeCount(), static_cast<size_t>(2));
    ASSERT_TRUE(mem.hasEpisode(id1));

    auto ep = mem.retrieveEpisode(id1);
    ASSERT_TRUE(ep != nullptr);
    ASSERT_EQ(ep->root_atom, a);
    ASSERT_EQ(ep->context, std::string("nav"));
    ASSERT_NEAR(ep->importance, 0.8, 1e-9);
}

TEST(EpisodicMemory_TemporalAndContext)
{
    auto as = make_as();
    EpisodicMemory mem(as, 100);
    mem.initialize();

    Handle x = as->add_node(CONCEPT_NODE, "x");
    Handle y = as->add_node(CONCEPT_NODE, "y");
    mem.storeEpisode(x, 0.9, "A");
    mem.storeEpisode(y, 0.4, "B");
    mem.storeEpisode(x, 0.7, "A");

    auto by_a = mem.getEpisodesByContext("A");
    ASSERT_EQ(by_a.size(), static_cast<size_t>(2));

    auto recent = mem.getRecentEpisodes(2);
    ASSERT_EQ(recent.size(), static_cast<size_t>(2));

    auto seq = mem.getTemporalSequence(10);
    ASSERT_EQ(seq.size(), static_cast<size_t>(3));

    auto important = mem.getEpisodesByImportance(0.75);
    ASSERT_EQ(important.size(), static_cast<size_t>(1));

    auto now = std::chrono::system_clock::now();
    auto ranged = mem.getEpisodesByTimeRange(now - std::chrono::hours(1), now + std::chrono::hours(1));
    ASSERT_EQ(ranged.size(), static_cast<size_t>(3));
}

TEST(EpisodicMemory_QueryUpdateRemove)
{
    auto as = make_as();
    EpisodicMemory mem(as, 100);
    mem.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "q");
    std::string id = mem.storeEpisode(a, 0.5, "ctx");
    ASSERT_TRUE(mem.updateEpisodeImportance(id, 0.95));

    EpisodicQuery q;
    q.context_filter = "ctx";
    q.min_importance = 0.9;
    auto hits = mem.query(q);
    ASSERT_EQ(hits.size(), static_cast<size_t>(1));

    ASSERT_TRUE(mem.removeEpisode(id));
    ASSERT_FALSE(mem.hasEpisode(id));
    ASSERT_TRUE(mem.clearAll());
    ASSERT_EQ(mem.getEpisodeCount(), static_cast<size_t>(0));
}

TEST(EpisodicMemory_CapacityEnforcement)
{
    auto as = make_as();
    EpisodicMemory mem(as, /*max_episodes=*/3);
    mem.initialize();

    for (int i = 0; i < 5; ++i) {
        Handle h = as->add_node(CONCEPT_NODE, "e" + std::to_string(i));
        mem.storeEpisode(h, 0.5 + 0.1 * i, "cap");
    }
    ASSERT_LE(mem.getEpisodeCount(), static_cast<size_t>(3));
}
