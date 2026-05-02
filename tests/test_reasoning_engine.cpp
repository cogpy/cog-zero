/*
 * standalone/tests/test_reasoning_engine.cpp
 */
#include "test_runner.h"
#include "cog0/AtomStore.h"
#include "cog0/ReasoningEngine.h"

using namespace cog0;

TEST(rule_fires_when_condition_true) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    bool fired = false;
    re.addRule("always-fire",
               [](const AtomStore&) { return true; },
               [&fired](AtomStore&) { fired = true; });
    re.runCycle();
    ASSERT_TRUE(fired);
}

TEST(rule_does_not_fire_when_condition_false) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    bool fired = false;
    re.addRule("never-fire",
               [](const AtomStore&) { return false; },
               [&fired](AtomStore&) { fired = true; });
    re.runCycle();
    ASSERT_FALSE(fired);
}

TEST(forward_chaining_terminates) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    // Rule fires only first time (adds a node that disables it)
    re.addRule("once",
               [](const AtomStore& s) {
                   return s.getNode(AtomType::CONCEPT, "once-done") == nullptr;
               },
               [](AtomStore& s) {
                   s.addNode(AtomType::CONCEPT, "once-done");
               });
    size_t fired = re.runForwardChaining(10);
    ASSERT_EQ(fired, 1u);
    ASSERT_TRUE(store->getNode(AtomType::CONCEPT, "once-done") != nullptr);
}

TEST(query_exists) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    ASSERT_FALSE(re.queryExists(AtomType::CONCEPT, "X"));
    store->addNode(AtomType::CONCEPT, "X");
    ASSERT_TRUE(re.queryExists(AtomType::CONCEPT, "X"));
}

TEST(query_inherits) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    auto child  = store->addNode(AtomType::CONCEPT, "Dog");
    auto parent = store->addNode(AtomType::CONCEPT, "Animal");
    ASSERT_FALSE(re.queryInherits("Dog", "Animal"));
    auto lnk = store->addLink(AtomType::INHERITANCE, {child, parent});
    lnk->setTV(TruthValue{0.9, 0.9});
    ASSERT_TRUE(re.queryInherits("Dog", "Animal"));
}

TEST(inheritance_transitivity_rule) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    // Add transitivity rule
    re.addRule("transitivity",
               [](const AtomStore& s) {
                   auto links = s.getByType(AtomType::INHERITANCE);
                   for (const auto& ab : links) {
                       if (ab->out().size() < 2) continue;
                       const auto& B = ab->out()[1];
                       for (const auto& bc : s.getByType(AtomType::INHERITANCE)) {
                           if (bc->out().size() < 2) continue;
                           if (bc->out()[0] == B) return true;
                       }
                   }
                   return false;
               },
               [](AtomStore& s) {
                   auto links = s.getByType(AtomType::INHERITANCE);
                   for (const auto& ab : links) {
                       if (ab->out().size() < 2) continue;
                       const auto& A = ab->out()[0];
                       const auto& B = ab->out()[1];
                       for (const auto& bc : s.getByType(AtomType::INHERITANCE)) {
                           if (bc->out().size() < 2) continue;
                           if (bc->out()[0] != B) continue;
                           const auto& C = bc->out()[1];
                           auto existing = s.getLink(AtomType::INHERITANCE, {A, C});
                           if (!existing) {
                               auto link = s.addLink(AtomType::INHERITANCE, {A, C});
                               link->setTV(TruthValue{
                                   ab->tv().strength * bc->tv().strength,
                                   ab->tv().confidence * bc->tv().confidence * 0.9});
                           }
                       }
                   }
               });

    auto A = store->addNode(AtomType::CONCEPT, "Poodle");
    auto B = store->addNode(AtomType::CONCEPT, "Dog");
    auto C = store->addNode(AtomType::CONCEPT, "Animal");
    store->addLink(AtomType::INHERITANCE, {A, B})->setTV({0.9, 0.9});
    store->addLink(AtomType::INHERITANCE, {B, C})->setTV({0.9, 0.9});

    re.runForwardChaining(5);

    ASSERT_TRUE(re.queryInherits("Poodle", "Animal"));
}

TEST(remove_rule) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    bool fired = false;
    re.addRule("r", [](const AtomStore&) { return true; },
               [&fired](AtomStore&) { fired = true; });
    re.removeRule("r");
    re.runCycle();
    ASSERT_FALSE(fired);
}
