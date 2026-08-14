#include "test_runner.h"

#include <opencog/agentzero/knowledge/PLNRuleLibrary.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

using namespace opencog;
using namespace opencog::agentzero::knowledge;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(PLN_InitializeAndBuiltinRules)
{
    auto as = make_as();
    PLNRuleLibrary lib(as);
    ASSERT_FALSE(lib.isInitialized());
    ASSERT_TRUE(lib.initialize());
    ASSERT_TRUE(lib.isHealthy());

    size_t n = lib.loadBuiltinRules();
    ASSERT_EQ(n, static_cast<size_t>(7));
    // Idempotent
    ASSERT_EQ(lib.loadBuiltinRules(), static_cast<size_t>(0));

    auto rules = lib.getRules();
    ASSERT_EQ(rules.size(), static_cast<size_t>(7));

    auto deduction = lib.getRulesByCategory("deduction");
    ASSERT_GE(deduction.size(), static_cast<size_t>(2));

    auto found = lib.findRule("modus-ponens");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, std::string("modus-ponens"));
}

TEST(PLN_TruthValueFormulas)
{
    double s, c;
    PLNRuleLibrary::deductionTV(0.8, 0.9, 0.7, 0.8, s, c);
    ASSERT_NEAR(s, 0.56, 1e-9);
    ASSERT_GT(c, 0.0);
    ASSERT_LE(c, 1.0);

    PLNRuleLibrary::modusPonensTV(0.9, 0.9, 0.8, 0.8, s, c);
    ASSERT_NEAR(s, 0.72, 1e-9);
    ASSERT_GT(c, 0.0);

    PLNRuleLibrary::inversionTV(0.8, 0.9, 0.5, 0.9, 0.4, 0.9, s, c);
    ASSERT_NEAR(s, 1.0, 1e-9); // (0.8*0.5)/0.4 = 1.0
    ASSERT_GT(c, 0.0);
}

TEST(PLN_ApplyDeductionAndModusPonens)
{
    auto as = make_as();
    PLNRuleLibrary lib(as);
    ASSERT_TRUE(lib.initialize());
    lib.loadBuiltinRules();

    Handle A = as->add_node(CONCEPT_NODE, "A");
    Handle B = as->add_node(CONCEPT_NODE, "B");
    Handle C = as->add_node(CONCEPT_NODE, "C");

    Handle AB = as->add_link(IMPLICATION_LINK, {A, B});
    Handle BC = as->add_link(IMPLICATION_LINK, {B, C});
    SimpleTruthValue::setTV(AB, 0.9, 0.9);
    SimpleTruthValue::setTV(BC, 0.8, 0.8);
    SimpleTruthValue::setTV(A, 0.95, 0.9);

    auto ded = lib.applyRule("deduction", {AB, BC});
    ASSERT_TRUE(ded.success);
    ASSERT_TRUE(static_cast<bool>(ded.conclusion));
    ASSERT_EQ(ded.conclusion->get_type(), IMPLICATION_LINK);
    ASSERT_GT(ded.strength, 0.0);

    auto mp = lib.applyRule("modus-ponens", {A, AB});
    ASSERT_TRUE(mp.success);
    ASSERT_TRUE(static_cast<bool>(mp.conclusion));
    ASSERT_EQ(mp.conclusion, B);
}

TEST(PLN_ForwardAndBackwardChain)
{
    auto as = make_as();
    PLNRuleLibrary lib(as);
    ASSERT_TRUE(lib.initialize());
    lib.loadBuiltinRules();

    Handle A = as->add_node(CONCEPT_NODE, "P");
    Handle B = as->add_node(CONCEPT_NODE, "Q");
    Handle C = as->add_node(CONCEPT_NODE, "R");
    Handle AB = as->add_link(IMPLICATION_LINK, {A, B});
    Handle BC = as->add_link(IMPLICATION_LINK, {B, C});
    SimpleTruthValue::setTV(AB, 0.9, 0.9);
    SimpleTruthValue::setTV(BC, 0.9, 0.9);
    SimpleTruthValue::setTV(A, 1.0, 1.0);

    auto fwd = lib.forwardChain({A, AB, BC}, 3);
    ASSERT_GE(fwd.size(), static_cast<size_t>(1));

    auto bwd = lib.backwardChain(C, 2);
    // Must find at least the BC implication whose consequent is C
    ASSERT_GE(bwd.size(), static_cast<size_t>(1));
    bool found = false;
    for (const auto& r : bwd) {
        if (r.conclusion == C) found = true;
    }
    ASSERT_TRUE(found);
}

TEST(PLN_RegisterUnregisterActivate)
{
    auto as = make_as();
    PLNRuleLibrary lib(as);
    ASSERT_TRUE(lib.initialize());

    Handle custom = as->add_link(IMPLICATION_LINK, {
        as->add_node(VARIABLE_NODE, "$X"),
        as->add_node(VARIABLE_NODE, "$Y")
    });
    ASSERT_TRUE(lib.registerRule(custom, "custom-rule", "custom", "test"));
    ASSERT_TRUE(lib.findRule("custom-rule").has_value());
    ASSERT_TRUE(lib.setRuleActive("custom-rule", false));
    auto inactive = lib.findRule("custom-rule");
    ASSERT_TRUE(inactive.has_value());
    ASSERT_FALSE(inactive->active);
    ASSERT_TRUE(lib.unregisterRule("custom-rule"));
    ASSERT_FALSE(lib.findRule("custom-rule").has_value());
    ASSERT_FALSE(lib.getStatsSummary().empty());
}
