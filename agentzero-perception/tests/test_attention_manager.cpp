#include "test_runner.h"

#include <stdexcept>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/agentzero/AttentionManager.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(Attention_ConstructorDefault)
{
    auto as = createAtomSpace();
    AttentionManager mgr(as);
    ASSERT_EQ(mgr.trackedAtomCount(), 0u);
    ASSERT_NEAR(mgr.getConfig().base_sti, 0.5, 1e-9);
    ASSERT_NEAR(mgr.getConfig().decay_rate, 0.05, 1e-9);
    ASSERT_NEAR(mgr.getConfig().focus_boundary, 0.7, 1e-9);
}

TEST(Attention_ConstructorCustomConfig)
{
    auto as = createAtomSpace();
    AttentionConfig cfg;
    cfg.base_sti = 0.3;
    cfg.decay_rate = 0.1;
    cfg.focus_boundary = 0.6;
    AttentionManager mgr(as, cfg);
    ASSERT_NEAR(mgr.getConfig().base_sti, 0.3, 1e-9);
    ASSERT_NEAR(mgr.getConfig().decay_rate, 0.1, 1e-9);
    ASSERT_NEAR(mgr.getConfig().focus_boundary, 0.6, 1e-9);
}

TEST(Attention_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        AttentionManager bad(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Attention_AllocateAndScale)
{
    auto as = createAtomSpace();
    AttentionManager mgr(as);

    Handle atom = as->add_node(CONCEPT_NODE, "test_percept");
    double sti = mgr.allocateAttention(atom, 0.8);
    ASSERT_TRUE(sti > 0.0);
    ASSERT_TRUE(sti <= 1.0);
    ASSERT_EQ(mgr.trackedAtomCount(), 1u);
    ASSERT_NEAR(mgr.getSTI(atom), sti, 1e-9);

    Handle low = as->add_node(CONCEPT_NODE, "low");
    Handle high = as->add_node(CONCEPT_NODE, "high");
    double sti_low = mgr.allocateAttention(low, 0.1);
    double sti_high = mgr.allocateAttention(high, 0.9);
    ASSERT_TRUE(sti_high > sti_low);

    ASSERT_NEAR(mgr.allocateAttention(Handle::UNDEFINED, 0.5), 0.0, 1e-9);

    Handle clamped = as->add_node(CONCEPT_NODE, "clamped");
    double sti_c = mgr.allocateAttention(clamped, 2.0);
    ASSERT_TRUE(sti_c >= 0.0 && sti_c <= 1.0);
}

TEST(Attention_UpdateAndGet)
{
    auto as = createAtomSpace();
    AttentionManager mgr(as);
    Handle atom = as->add_node(CONCEPT_NODE, "u");
    mgr.allocateAttention(atom, 0.5);
    mgr.updateAttention(atom, 0.9);
    ASSERT_NEAR(mgr.getSTI(atom), 0.9, 1e-9);

    Handle untracked = as->add_node(CONCEPT_NODE, "x");
    ASSERT_NEAR(mgr.getSTI(untracked), 0.0, 1e-9);
}

TEST(Attention_Decay)
{
    auto as = createAtomSpace();
    AttentionConfig cfg;
    cfg.decay_rate = 0.5;
    cfg.base_sti = 0.8;
    cfg.min_sti = 0.0;
    AttentionManager mgr(as, cfg);

    Handle atom = as->add_node(CONCEPT_NODE, "d");
    double initial = mgr.allocateAttention(atom, 1.0);
    ASSERT_TRUE(initial > 0.0);
    mgr.decayAttention();
    double decayed = mgr.getSTI(atom);
    ASSERT_TRUE(decayed < initial);
    ASSERT_NEAR(decayed, initial * 0.5, 1e-6);
}

TEST(Attention_DecayRemovesLowSTI)
{
    auto as = createAtomSpace();
    AttentionConfig cfg;
    cfg.decay_rate = 0.99;
    cfg.min_sti = 0.0;
    AttentionManager mgr(as, cfg);

    Handle atom = as->add_node(CONCEPT_NODE, "low");
    mgr.allocateAttention(atom, 0.01);
    ASSERT_EQ(mgr.trackedAtomCount(), 1u);
    for (int i = 0; i < 20; ++i) mgr.decayAttention();
    ASSERT_EQ(mgr.trackedAtomCount(), 0u);
}

TEST(Attention_FocusSet)
{
    auto as = createAtomSpace();
    AttentionConfig cfg;
    cfg.focus_boundary = 0.7;
    AttentionManager mgr(as, cfg);

    ASSERT_TRUE(mgr.getAttentionFocus().empty());

    Handle above = as->add_node(CONCEPT_NODE, "above");
    Handle below = as->add_node(CONCEPT_NODE, "below");
    mgr.allocateAttention(above, 1.0);
    mgr.updateAttention(above, 0.9);
    mgr.allocateAttention(below, 0.1);
    mgr.updateAttention(below, 0.3);

    auto focus = mgr.getAttentionFocus();
    bool found_above = false, found_below = false;
    for (const auto& h : focus) {
        if (h == above) found_above = true;
        if (h == below) found_below = true;
    }
    ASSERT_TRUE(found_above);
    ASSERT_FALSE(found_below);

    ASSERT_TRUE(mgr.isInAttentionFocus(above));
    mgr.updateAttention(above, 0.2);
    ASSERT_FALSE(mgr.isInAttentionFocus(above));
}

TEST(Attention_Salience)
{
    auto as = createAtomSpace();
    AttentionManager mgr(as);

    SensoryInput input("visual", "camera", {0.1, 0.2, 0.3}, 0.8);
    SalienceScore score = mgr.calculateSalience(input);
    ASSERT_TRUE(score.signal_quality >= 0.0 && score.signal_quality <= 1.0);
    ASSERT_TRUE(score.novelty >= 0.0 && score.novelty <= 1.0);
    ASSERT_TRUE(score.overall >= 0.0 && score.overall <= 1.0);

    SensoryInput same("audio", "mic", {1.0}, 0.9);
    auto first = mgr.calculateSalience(same);
    auto second = mgr.calculateSalience(same);
    auto third = mgr.calculateSalience(same);
    ASSERT_TRUE(first.novelty >= second.novelty);
    ASSERT_TRUE(second.novelty >= third.novelty);

    mgr.reset();
    auto high = mgr.calculateSalience(SensoryInput("tactile", "p", {0.5}, 1.0));
    mgr.reset();
    auto low = mgr.calculateSalience(SensoryInput("tactile", "p", {0.5}, 0.1));
    ASSERT_TRUE(high.signal_quality > low.signal_quality);
}

TEST(Attention_Spread)
{
    auto as = createAtomSpace();
    AttentionManager mgr(as);

    Handle dst1 = as->add_node(CONCEPT_NODE, "dest1");
    Handle dst2 = as->add_node(CONCEPT_NODE, "dest2");
    Handle link = as->add_link(LIST_LINK, HandleSeq{dst1, dst2});

    mgr.spreadAttention(link, 0.2);
    ASSERT_TRUE(mgr.getSTI(dst1) > 0.0 || mgr.getSTI(dst2) > 0.0);

    // Should not throw
    mgr.spreadAttention(Handle::UNDEFINED, 0.1);
}

TEST(Attention_StatsAndReset)
{
    auto as = createAtomSpace();
    AttentionManager mgr(as);
    std::string stats = mgr.getStats();
    ASSERT_TRUE(stats.find("allocations") != std::string::npos);
    ASSERT_TRUE(stats.find("decay_cycles") != std::string::npos);
    ASSERT_TRUE(stats.find("tracked_atoms") != std::string::npos);
    ASSERT_TRUE(stats.find("focus_size") != std::string::npos);

    Handle atom = as->add_node(CONCEPT_NODE, "r");
    mgr.allocateAttention(atom, 0.8);
    ASSERT_EQ(mgr.trackedAtomCount(), 1u);
    mgr.reset();
    ASSERT_EQ(mgr.trackedAtomCount(), 0u);
}
