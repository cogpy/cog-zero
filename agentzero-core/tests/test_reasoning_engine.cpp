#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/ReasoningEngine.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(ReasoningEngine_ForwardChaining)
{
    auto as = createAtomSpace();
    ReasoningEngine engine(nullptr, as);

    Handle a = as->add_node(CONCEPT_NODE, "A");
    Handle b = as->add_node(CONCEPT_NODE, "B");
    SimpleTruthValue::setTV(a, 1.0, 0.9);
    Handle impl = as->add_link(IMPLICATION_LINK, a, b);
    SimpleTruthValue::setTV(impl, 0.9, 0.9);

    auto results = engine.reason({a}, ReasoningEngine::ReasoningMode::FORWARD_CHAINING);
    ASSERT_GT(results.size(), 0u);
    ASSERT_NE(results[0].conclusion, Handle::UNDEFINED);
    ASSERT_GT(results[0].confidence, 0.0);

    ASSERT_TRUE(engine.processReasoningCycle());
    ASSERT_GT(engine.ruleCount(), 0u);
}

TEST(ReasoningEngine_Hypotheses)
{
    auto as = createAtomSpace();
    ReasoningEngine engine(nullptr, as);
    Handle obs = as->add_node(CONCEPT_NODE, "Observation");
    auto hyps = engine.generateHypotheses({obs});
    ASSERT_EQ(hyps.size(), 1u);
    ASSERT_NE(hyps[0].atom, Handle::UNDEFINED);
}
