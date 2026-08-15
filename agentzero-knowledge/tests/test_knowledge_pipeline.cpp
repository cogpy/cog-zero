#include "test_runner.h"

#include <opencog/agentzero/knowledge/KnowledgeBase.h>
#include <opencog/agentzero/knowledge/PatternDiscovery.h>
#include <opencog/agentzero/knowledge/ConceptFormation.h>
#include <opencog/agentzero/knowledge/PLNRuleLibrary.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

using namespace opencog;
using namespace opencog::agentzero::knowledge;

TEST(Pipeline_KnowledgeEndToEnd)
{
    auto as = createAtomSpace();

    // 1. Bulk-load domain knowledge
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    auto load = kb.loadFromTriples({
        {"Dog", "isa", "Animal"},
        {"Cat", "isa", "Animal"},
        {"Animal", "isa", "LivingThing"},
        {"Rover", "isa", "Dog"},
        {"Whiskers", "isa", "Cat"}
    });
    ASSERT_TRUE(load.success);

    // 2. Query animals
    auto q = kb.query({QueryTriple{"?x", "isa", "Animal"}});
    ASSERT_TRUE(q.success);
    ASSERT_EQ(q.total_matches, static_cast<size_t>(2));

    // 3. Form concepts from query results
    ConceptFormation cf(as);
    ASSERT_TRUE(cf.initialize());
    for (const auto& row : q.bindings) {
        cf.observeExemplar(row.at("?x"), "animal-instance");
    }
    // Also observe living things
    Handle dog = as->get_node(CONCEPT_NODE, "Dog");
    Handle cat = as->get_node(CONCEPT_NODE, "Cat");
    cf.observeExemplar(dog, "pet");
    cf.observeExemplar(cat, "pet");
    cf.formConcept("pet");
    ASSERT_TRUE(static_cast<bool>(cf.getConceptHandle("pet")));

    // 4. Mine patterns over episode-like groupings
    PatternDiscovery pd(as);
    ASSERT_TRUE(pd.initialize());
    Handle ep1 = as->add_link(AND_LINK, {
        as->add_node(CONCEPT_NODE, "perceive-dog"),
        as->add_node(CONCEPT_NODE, "label-animal")
    });
    Handle ep2 = as->add_link(AND_LINK, {
        as->add_node(CONCEPT_NODE, "perceive-cat"),
        as->add_node(CONCEPT_NODE, "label-animal")
    });
    pd.recordEpisode(ep1, "ep-dog");
    pd.recordEpisode(ep2, "ep-cat");
    MiningConfig mcfg;
    mcfg.min_support = 2;
    auto patterns = pd.minePatterns(mcfg);
    ASSERT_GT(patterns.size(), static_cast<size_t>(0));

    // 5. Reason with PLN over isa chain
    PLNRuleLibrary pln(as);
    ASSERT_TRUE(pln.initialize());
    ASSERT_EQ(pln.loadBuiltinRules(), static_cast<size_t>(7));

    Handle Animal = as->get_node(CONCEPT_NODE, "Animal");
    Handle Living = as->get_node(CONCEPT_NODE, "LivingThing");
    Handle Dog = as->get_node(CONCEPT_NODE, "Dog");
    Handle AB = as->add_link(IMPLICATION_LINK, {Dog, Animal});
    Handle BC = as->add_link(IMPLICATION_LINK, {Animal, Living});
    SimpleTruthValue::setTV(AB, 0.95, 0.9);
    SimpleTruthValue::setTV(BC, 0.9, 0.9);

    auto inferred = pln.applyRule("deduction", {AB, BC});
    ASSERT_TRUE(inferred.success);
    ASSERT_TRUE(static_cast<bool>(inferred.conclusion));

    // Health checks across the stack
    ASSERT_TRUE(kb.isHealthy());
    ASSERT_TRUE(pd.isHealthy());
    ASSERT_TRUE(cf.isHealthy());
    ASSERT_TRUE(pln.isHealthy());
}
