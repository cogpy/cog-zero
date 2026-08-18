#include "test_runner.h"

#include <opencog/agentzero/memory/LongTermMemory.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

#include <filesystem>

using namespace opencog;
using namespace opencog::agentzero::memory;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(LongTermMemory_Initialize)
{
    auto as = make_as();
    MemoryConfig cfg;
    cfg.persistence_directory = "/tmp/az-memory-test-ltm";
    LongTermMemory ltm(as, cfg);
    ASSERT_TRUE(ltm.initialize());
    ASSERT_TRUE(ltm.isHealthy());
    ASSERT_TRUE(ltm.shutdown());
}

TEST(LongTermMemory_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        LongTermMemory ltm(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(LongTermMemory_StoreRetrieveContains)
{
    auto as = make_as();
    MemoryConfig cfg;
    cfg.persistence_directory = "/tmp/az-memory-test-ltm2";
    LongTermMemory ltm(as, cfg);
    ltm.initialize();

    Handle skill = as->add_node(CONCEPT_NODE, "SkillX");
    ASSERT_TRUE(ltm.store(skill, MemoryImportance::HIGH,
                          PersistenceLevel::LONG_TERM,
                          {ContextType::TASK}));
    ASSERT_TRUE(ltm.contains(skill));
    ASSERT_EQ(ltm.retrieve(skill), skill);

    auto by_imp = ltm.findByImportance(MemoryImportance::MEDIUM);
    ASSERT_GE(by_imp.size(), static_cast<size_t>(1));

    auto by_ctx = ltm.findByContext(ContextType::TASK);
    ASSERT_GE(by_ctx.size(), static_cast<size_t>(1));

    ASSERT_TRUE(ltm.updateImportance(skill, MemoryImportance::CRITICAL));
    ASSERT_TRUE(ltm.remove(skill));
    ASSERT_FALSE(ltm.contains(skill));
}

TEST(LongTermMemory_SearchAndConsolidate)
{
    auto as = make_as();
    MemoryConfig cfg;
    cfg.persistence_directory = "/tmp/az-memory-test-ltm3";
    LongTermMemory ltm(as, cfg);
    ltm.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "alpha_pattern");
    Handle b = as->add_node(CONCEPT_NODE, "beta_pattern");
    ltm.store(a, MemoryImportance::MEDIUM, PersistenceLevel::MEDIUM_TERM, {ContextType::COGNITIVE});
    ltm.store(b, MemoryImportance::LOW, PersistenceLevel::SHORT_TERM, {ContextType::TEMPORAL});

    auto hits = ltm.search({"alpha_pattern"});
    ASSERT_GE(hits.size(), static_cast<size_t>(1));

    auto similar = ltm.findSimilar(a, 5, 0.1);
    ASSERT_GE(similar.size(), static_cast<size_t>(1));

    auto status = ltm.consolidate(true);
    ASSERT_GE(status.total_memories, static_cast<size_t>(0));

    auto usage = ltm.getMemoryUsage();
    ASSERT_TRUE(usage.count("active_records") > 0);

    ASSERT_TRUE(ltm.flushToPersistence() >= 0);
}

TEST(LongTermMemory_BackupPath)
{
    auto as = make_as();
    MemoryConfig cfg;
    cfg.persistence_directory = "/tmp/az-memory-test-ltm4";
    std::filesystem::create_directories(cfg.persistence_directory);
    LongTermMemory ltm(as, cfg);
    ltm.initialize();

    Handle h = as->add_node(CONCEPT_NODE, "persist_me");
    ltm.store(h, MemoryImportance::HIGH, PersistenceLevel::PERMANENT);

    std::string bak = cfg.persistence_directory + "/test.backup";
    ASSERT_TRUE(ltm.backup(bak));
}
