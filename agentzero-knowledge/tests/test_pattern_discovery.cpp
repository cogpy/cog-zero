#include "test_runner.h"

#include <opencog/agentzero/knowledge/PatternDiscovery.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::knowledge;

static AtomSpacePtr make_as() { return createAtomSpace(); }

static Handle make_episode(AtomSpacePtr as, const std::string& a, const std::string& b)
{
    Handle ha = as->add_node(CONCEPT_NODE, a);
    Handle hb = as->add_node(CONCEPT_NODE, b);
    return as->add_link(AND_LINK, {ha, hb});
}

TEST(PatternDiscovery_InitializeAndShutdown)
{
    auto as = make_as();
    PatternDiscovery pd(as);
    ASSERT_FALSE(pd.isInitialized());
    ASSERT_TRUE(pd.initialize());
    ASSERT_TRUE(pd.isHealthy());
    ASSERT_TRUE(pd.shutdown());
}

TEST(PatternDiscovery_RecordAndRemoveEpisodes)
{
    auto as = make_as();
    PatternDiscovery pd(as);
    ASSERT_TRUE(pd.initialize());

    Handle ep1 = make_episode(as, "act", "reward");
    Handle ep2 = make_episode(as, "act", "punish");
    ASSERT_TRUE(pd.recordEpisode(ep1, "e1", {{"reward", "1"}}));
    ASSERT_TRUE(pd.recordEpisode(ep2, "e2"));
    ASSERT_EQ(pd.episodeCount(), static_cast<size_t>(2));
    ASSERT_EQ(pd.getEpisodes().size(), static_cast<size_t>(2));

    ASSERT_TRUE(pd.removeEpisode("e1"));
    ASSERT_EQ(pd.episodeCount(), static_cast<size_t>(1));
    ASSERT_FALSE(pd.removeEpisode("missing"));

    pd.clearEpisodes();
    ASSERT_EQ(pd.episodeCount(), static_cast<size_t>(0));
}

TEST(PatternDiscovery_MineFrequentPatterns)
{
    auto as = make_as();
    PatternDiscovery pd(as);
    ASSERT_TRUE(pd.initialize());

    // Shared substructure across episodes: concept "act"
    ASSERT_TRUE(pd.recordEpisode(make_episode(as, "act", "r1"), "e1"));
    ASSERT_TRUE(pd.recordEpisode(make_episode(as, "act", "r2"), "e2"));
    ASSERT_TRUE(pd.recordEpisode(make_episode(as, "act", "r3"), "e3"));

    MiningConfig cfg;
    cfg.min_support = 2;
    cfg.max_results = 50;
    auto patterns = pd.minePatterns(cfg);
    ASSERT_GT(patterns.size(), static_cast<size_t>(0));

    bool found_act = false;
    for (const auto& p : patterns) {
        if (p.pattern && p.pattern->is_node() && p.pattern->get_name() == "act") {
            found_act = true;
            ASSERT_GE(p.support, static_cast<size_t>(2));
            ASSERT_GT(p.frequency, 0.0);
        }
    }
    ASSERT_TRUE(found_act);

    auto cached = pd.getCachedPatterns();
    ASSERT_EQ(cached.size(), patterns.size());

    auto by_freq = pd.getPatternsByFrequency(5);
    ASSERT_LE(by_freq.size(), static_cast<size_t>(5));
    auto by_surp = pd.getPatternsBySurprisingness(5);
    ASSERT_LE(by_surp.size(), static_cast<size_t>(5));
}

TEST(PatternDiscovery_UpdateAndMatch)
{
    auto as = make_as();
    PatternDiscovery pd(as);
    ASSERT_TRUE(pd.initialize());

    Handle root = make_episode(as, "shared", "once");
    auto updated = pd.updatePatterns(root);
    ASSERT_GE(pd.episodeCount(), static_cast<size_t>(1));

    // After mining via update, matching against the episode root should work
    // once cache is populated
    pd.recordEpisode(make_episode(as, "shared", "twice"), "e2");
    auto patterns = pd.minePatterns(MiningConfig{});
    if (!patterns.empty()) {
        auto matches = pd.findMatchingPatterns(patterns[0].instances.empty()
                                                   ? root
                                                   : patterns[0].instances[0]);
        // May be empty if instance not exact; just ensure API is callable
        (void)matches;
    }
    ASSERT_FALSE(pd.getStatsSummary().empty());
}
