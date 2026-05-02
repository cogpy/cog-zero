/*
 * standalone/tests/test_atom_store.cpp
 */
#include "test_runner.h"
#include "cog0/AtomStore.h"

using namespace cog0;

TEST(add_and_get_node) {
    AtomStore store;
    auto h = store.addNode(AtomType::CONCEPT, "foo");
    ASSERT_TRUE(h != nullptr);
    ASSERT_EQ(h->name(), "foo");
    ASSERT_EQ(h->type(), AtomType::CONCEPT);
    ASSERT_TRUE(h->isNode());
    ASSERT_EQ(store.size(), 1u);
}

TEST(duplicate_node_returns_same_handle) {
    AtomStore store;
    auto h1 = store.addNode(AtomType::CONCEPT, "bar");
    auto h2 = store.addNode(AtomType::CONCEPT, "bar");
    ASSERT_EQ(h1, h2);
    ASSERT_EQ(store.size(), 1u);
}

TEST(distinct_types_are_different_nodes) {
    AtomStore store;
    auto h1 = store.addNode(AtomType::CONCEPT,   "x");
    auto h2 = store.addNode(AtomType::PREDICATE, "x");
    ASSERT_NE(h1, h2);
    ASSERT_EQ(store.size(), 2u);
}

TEST(add_and_get_link) {
    AtomStore store;
    auto a = store.addNode(AtomType::CONCEPT, "A");
    auto b = store.addNode(AtomType::CONCEPT, "B");
    auto lnk = store.addLink(AtomType::INHERITANCE, {a, b});
    ASSERT_TRUE(lnk != nullptr);
    ASSERT_TRUE(lnk->isLink());
    ASSERT_EQ(lnk->out().size(), 2u);
    ASSERT_EQ(lnk->out()[0], a);
    ASSERT_EQ(lnk->out()[1], b);
}

TEST(duplicate_link_returns_same_handle) {
    AtomStore store;
    auto a = store.addNode(AtomType::CONCEPT, "A");
    auto b = store.addNode(AtomType::CONCEPT, "B");
    auto l1 = store.addLink(AtomType::INHERITANCE, {a, b});
    auto l2 = store.addLink(AtomType::INHERITANCE, {a, b});
    ASSERT_EQ(l1, l2);
}

TEST(incoming_index) {
    AtomStore store;
    auto a = store.addNode(AtomType::CONCEPT, "A");
    auto b = store.addNode(AtomType::CONCEPT, "B");
    store.addLink(AtomType::INHERITANCE, {a, b});
    auto inc = store.getIncoming(a);
    ASSERT_EQ(inc.size(), 1u);
    auto incB = store.getIncoming(b);
    ASSERT_EQ(incB.size(), 1u);
}

TEST(remove_node) {
    AtomStore store;
    auto h = store.addNode(AtomType::CONCEPT, "tmp");
    ASSERT_EQ(store.size(), 1u);
    store.remove(h);
    ASSERT_EQ(store.size(), 0u);
    ASSERT_EQ(store.getNode(AtomType::CONCEPT, "tmp"), nullptr);
}

TEST(truth_value) {
    AtomStore store;
    auto h = store.addNode(AtomType::CONCEPT, "tv-test");
    h->setTV(TruthValue{0.8, 0.9});
    ASSERT_EQ(h->tv().strength,   0.8);
    ASSERT_EQ(h->tv().confidence, 0.9);
}

TEST(sti_and_lti) {
    AtomStore store;
    auto h = store.addNode(AtomType::CONCEPT, "av-test");
    h->setSTI(0.7);
    h->setLTI(0.3);
    ASSERT_EQ(h->sti(), 0.7);
    ASSERT_EQ(h->lti(), 0.3);
}

TEST(get_by_type) {
    AtomStore store;
    store.addNode(AtomType::CONCEPT, "c1");
    store.addNode(AtomType::CONCEPT, "c2");
    store.addNode(AtomType::PREDICATE, "p1");
    auto concepts = store.getByType(AtomType::CONCEPT);
    ASSERT_EQ(concepts.size(), 2u);
    auto predicates = store.getByType(AtomType::PREDICATE);
    ASSERT_EQ(predicates.size(), 1u);
}

TEST(clear) {
    AtomStore store;
    store.addNode(AtomType::CONCEPT, "c");
    store.addNode(AtomType::PREDICATE, "p");
    store.clear();
    ASSERT_EQ(store.size(), 0u);
}
