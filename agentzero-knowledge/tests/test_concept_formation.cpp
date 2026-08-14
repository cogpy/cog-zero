#include "test_runner.h"

#include <opencog/agentzero/knowledge/ConceptFormation.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::knowledge;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ConceptFormation_InitializeAndShutdown)
{
    auto as = make_as();
    ConceptFormation cf(as);
    ASSERT_FALSE(cf.isInitialized());
    ASSERT_TRUE(cf.initialize());
    ASSERT_TRUE(cf.isHealthy());
    ASSERT_TRUE(cf.shutdown());
}

TEST(ConceptFormation_ObserveExemplars)
{
    auto as = make_as();
    ConceptFormation cf(as);
    ASSERT_TRUE(cf.initialize());

    Handle e1 = as->add_node(CONCEPT_NODE, "fluffy");
    Handle e2 = as->add_node(CONCEPT_NODE, "whiskers");
    Handle counter = as->add_node(CONCEPT_NODE, "rover");

    ASSERT_TRUE(cf.observeExemplar(e1, "cat"));
    ASSERT_TRUE(cf.observeExemplar(e2, "cat"));
    ASSERT_TRUE(cf.observeCounterExemplar(counter, "cat"));
    ASSERT_EQ(cf.exemplarCount("cat"), static_cast<size_t>(2));
    ASSERT_EQ(cf.exemplarCount("missing"), static_cast<size_t>(0));

    auto labels = cf.getCandidateLabels();
    ASSERT_EQ(labels.size(), static_cast<size_t>(1));
    ASSERT_EQ(labels[0], std::string("cat"));

    auto cand = cf.getCandidate("cat");
    ASSERT_TRUE(cand.has_value());
    ASSERT_EQ(cand->exemplars.size(), static_cast<size_t>(2));
    ASSERT_EQ(cand->counter_exemplars.size(), static_cast<size_t>(1));
}

TEST(ConceptFormation_FormAndRetrieveConcepts)
{
    auto as = make_as();
    ConceptFormation cf(as);
    ASSERT_TRUE(cf.initialize());

    // Same-type exemplars → high structural similarity/coherence
    Handle e1 = as->add_node(CONCEPT_NODE, "siamese");
    Handle e2 = as->add_node(CONCEPT_NODE, "persian");
    Handle e3 = as->add_node(CONCEPT_NODE, "tabby");
    cf.observeExemplar(e1, "cat");
    cf.observeExemplar(e2, "cat");
    cf.observeExemplar(e3, "cat");

    ConceptFormationConfig cfg;
    cfg.min_coherence = 0.1;
    cfg.min_coverage = 0.0;
    cfg.novelty_threshold = 0.0;
    cfg.min_exemplars = 2;
    cfg.auto_merge = false;
    cfg.auto_split = false;

    size_t changes = cf.formConcepts(cfg);
    ASSERT_GE(changes, static_cast<size_t>(1));

    Handle concept = cf.getConceptHandle("cat");
    ASSERT_TRUE(static_cast<bool>(concept));
    ASSERT_EQ(concept->get_name(), std::string("cat"));

    auto all = cf.getAllConcepts();
    ASSERT_GE(all.size(), static_cast<size_t>(1));
}

TEST(ConceptFormation_FormSingleAndMergeSplit)
{
    auto as = make_as();
    ConceptFormation cf(as);
    ASSERT_TRUE(cf.initialize());

    Handle a1 = as->add_node(CONCEPT_NODE, "a1");
    Handle a2 = as->add_node(CONCEPT_NODE, "a2");
    Handle b1 = as->add_node(CONCEPT_NODE, "b1");
    Handle b2 = as->add_node(CONCEPT_NODE, "b2");

    cf.observeExemplar(a1, "alpha");
    cf.observeExemplar(a2, "alpha");
    cf.observeExemplar(b1, "beta");
    cf.observeExemplar(b2, "beta");

    ASSERT_TRUE(cf.formConcept("alpha"));
    ASSERT_TRUE(cf.formConcept("beta"));
    ASSERT_TRUE(static_cast<bool>(cf.getConceptHandle("alpha")));
    ASSERT_TRUE(static_cast<bool>(cf.getConceptHandle("beta")));

    Handle merged = cf.mergeConcepts("alpha", "beta", "alphabet");
    ASSERT_TRUE(static_cast<bool>(merged));
    ASSERT_EQ(merged->get_name(), std::string("alphabet"));

    // Split alphabet into two
    ASSERT_TRUE(cf.splitConcept("alphabet", "left", "right"));
    ASSERT_GE(cf.getCandidateLabels().size(), static_cast<size_t>(2));

    auto hist = cf.getRefinementHistory();
    ASSERT_GE(hist.size(), static_cast<size_t>(1));
    ASSERT_FALSE(cf.getStatsSummary().empty());
}

TEST(ConceptFormation_BuildHierarchy)
{
    auto as = make_as();
    ConceptFormation cf(as);
    ASSERT_TRUE(cf.initialize());

    Handle x = as->add_node(CONCEPT_NODE, "x");
    Handle y = as->add_node(CONCEPT_NODE, "y");
    Handle z = as->add_node(CONCEPT_NODE, "z");

    // child uses subset of parent exemplars
    cf.observeExemplar(x, "child");
    cf.observeExemplar(x, "parent");
    cf.observeExemplar(y, "parent");
    cf.observeExemplar(z, "parent");

    cf.formConcept("child");
    cf.formConcept("parent");

    size_t links = cf.buildConceptHierarchy();
    // child exemplars {x} ⊂ parent {x,y,z} → InheritanceLink expected
    ASSERT_GE(links, static_cast<size_t>(1));
}
