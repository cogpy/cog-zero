#include "test_runner.h"

#include <opencog/agentzero/planning/TemporalReasoner.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::planning;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(Temporal_Initialize)
{
    auto as = make_as();
    TemporalReasoner tr(as);
    ASSERT_TRUE(tr.initialize());
    ASSERT_TRUE(tr.isInitialized());
}

TEST(Temporal_IntervalsAndRelations)
{
    auto as = make_as();
    TemporalReasoner tr(as);
    tr.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "event_a");
    Handle b = as->add_node(CONCEPT_NODE, "event_b");
    auto t0 = std::chrono::steady_clock::now();
    auto t1 = t0 + std::chrono::seconds(1);
    auto t2 = t0 + std::chrono::seconds(2);
    auto t3 = t0 + std::chrono::seconds(3);

    ASSERT_TRUE(tr.addTemporalInterval(a, t0, t1, true));
    ASSERT_TRUE(tr.addTemporalInterval(b, t2, t3, true));
    ASSERT_EQ(tr.getTemporalRelation(a, b), TemporalReasoner::TemporalRelation::BEFORE);
    ASSERT_EQ(tr.intervalCount(), static_cast<size_t>(2));
}

TEST(Temporal_DeadlineConstraint)
{
    auto as = make_as();
    TemporalReasoner tr(as);
    tr.initialize();

    Handle e = as->add_node(CONCEPT_NODE, "deadline_event");
    auto t0 = std::chrono::steady_clock::now();
    tr.addTemporalInterval(e, t0, t0 + std::chrono::seconds(5), true);
    tr.addDeadlineConstraint(e, t0 + std::chrono::seconds(10));
    ASSERT_TRUE(tr.areConstraintsSatisfiable({e}));
    tr.addDeadlineConstraint(e, t0 + std::chrono::seconds(1));
    // second hard deadline violated
    ASSERT_FALSE(tr.areConstraintsSatisfiable({e}));
}

TEST(Temporal_OptimizeSchedule)
{
    auto as = make_as();
    TemporalReasoner tr(as);
    tr.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "s1");
    Handle b = as->add_node(CONCEPT_NODE, "s2");
    Handle c = as->add_node(CONCEPT_NODE, "s3");
    auto ordered = tr.optimizeTemporalSchedule({a, b, c});
    ASSERT_EQ(ordered.size(), static_cast<size_t>(3));
    ASSERT_EQ(tr.intervalCount(), static_cast<size_t>(3));
    ASSERT_TRUE(tr.validateTemporalSchedule(ordered));
}

TEST(Temporal_RelativeOrdering)
{
    auto as = make_as();
    TemporalReasoner tr(as);
    tr.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "ra");
    Handle b = as->add_node(CONCEPT_NODE, "rb");
    auto t0 = std::chrono::steady_clock::now();
    tr.addTemporalInterval(a, t0, t0 + std::chrono::milliseconds(100), false);
    tr.addTemporalInterval(b, t0 + std::chrono::milliseconds(200),
                           t0 + std::chrono::milliseconds(300), false);
    ASSERT_TRUE(tr.addRelativeConstraint(a, b, TemporalReasoner::TemporalRelation::BEFORE));
    ASSERT_TRUE(tr.areConstraintsSatisfiable({a, b}));
}
