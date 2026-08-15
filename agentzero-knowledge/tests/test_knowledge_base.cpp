#include "test_runner.h"

#include <opencog/agentzero/knowledge/KnowledgeBase.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include <fstream>
#include <cstdio>

using namespace opencog;
using namespace opencog::agentzero::knowledge;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(KnowledgeBase_InitializeAndShutdown)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_FALSE(kb.isInitialized());
    ASSERT_TRUE(kb.initialize());
    ASSERT_TRUE(kb.isInitialized());
    ASSERT_TRUE(kb.isHealthy());
    ASSERT_TRUE(kb.shutdown());
    ASSERT_FALSE(kb.isInitialized());
}

TEST(KnowledgeBase_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        KnowledgeBase kb(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(KnowledgeBase_BulkLoadTriples)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    std::vector<std::tuple<std::string, std::string, std::string>> triples = {
        {"Dog", "isa", "Animal"},
        {"Cat", "isa", "Animal"},
        {"Fluffy", "isa", "Cat"}
    };

    auto result = kb.loadFromTriples(triples);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.atoms_loaded, static_cast<size_t>(3));
    ASSERT_GT(kb.size(), static_cast<size_t>(0));
    ASSERT_FALSE(kb.queryByName("Dog").empty());
    ASSERT_FALSE(kb.queryByName("Animal").empty());
}

TEST(KnowledgeBase_BulkLoadAdjacency)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    std::map<std::string, std::vector<std::string>> adj = {
        {"A", {"B", "C"}},
        {"B", {"C"}}
    };
    auto result = kb.loadFromAdjacency(adj, "related-to");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.atoms_loaded, static_cast<size_t>(3));
}

TEST(KnowledgeBase_LoadFromFile)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    const char* path = "/tmp/az_kb_test_triples.txt";
    {
        std::ofstream f(path);
        f << "; comment\n";
        f << "Bird can-fly True\n";
        f << "Fish can-fly False\n";
    }
    auto result = kb.loadFromFile(path);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.atoms_loaded, static_cast<size_t>(2));
    std::remove(path);
}

TEST(KnowledgeBase_QueryByTypeAndName)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    kb.loadFromTriples({{"X", "rel", "Y"}});

    auto concepts = kb.queryByType(CONCEPT_NODE);
    ASSERT_GE(concepts.size(), static_cast<size_t>(2));

    auto named = kb.queryByName("X");
    ASSERT_EQ(named.size(), static_cast<size_t>(1));
    ASSERT_EQ(named[0]->get_name(), std::string("X"));
}

TEST(KnowledgeBase_SPARQLLikeQuery)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    kb.loadFromTriples({
        {"Dog", "isa", "Animal"},
        {"Cat", "isa", "Animal"},
        {"Car", "isa", "Vehicle"}
    });

    QueryTriple pat{"?x", "isa", "Animal"};
    auto result = kb.query({pat});
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.total_matches, static_cast<size_t>(2));
    for (const auto& row : result.bindings) {
        ASSERT_TRUE(row.count("?x") > 0);
        auto name = row.at("?x")->get_name();
        ASSERT_TRUE(name == "Dog" || name == "Cat");
    }
}

TEST(KnowledgeBase_QueryNeighborsAndTV)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    Handle dog = as->add_node(CONCEPT_NODE, "Dog");
    Handle animal = as->add_node(CONCEPT_NODE, "Animal");
    Handle link = as->add_link(INHERITANCE_LINK, {dog, animal});
    SimpleTruthValue::setTV(link, 0.9, 0.8);

    auto neighbors = kb.queryNeighbors(dog, INHERITANCE_LINK, /*incoming=*/false);
    ASSERT_GE(neighbors.size(), static_cast<size_t>(1));

    auto strong = kb.queryByTruthValue(0.5, 0.5);
    ASSERT_GE(strong.size(), static_cast<size_t>(1));
}

TEST(KnowledgeBase_AddRemoveContainsClear)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    Handle h = as->add_node(CONCEPT_NODE, "Temp");
    ASSERT_TRUE(kb.contains(h));
    size_t before = kb.size();
    ASSERT_TRUE(kb.removeAtom(h));
    ASSERT_LT(kb.size(), before);

    kb.loadFromTriples({{"A", "r", "B"}});
    ASSERT_GT(kb.size(), static_cast<size_t>(0));
    kb.clear();
    ASSERT_EQ(kb.size(), static_cast<size_t>(0));
}

TEST(KnowledgeBase_StatsSummary)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    kb.loadFromTriples({{"A", "r", "B"}});
    auto summary = kb.getStatsSummary();
    ASSERT_FALSE(summary.empty());
    auto counts = kb.getTypeCounts();
    ASSERT_FALSE(counts.empty());
}
