/*
 * standalone/tests/test_persistence.cpp
 *
 * Tests for AtomStore save/load persistence.
 */
#include <cstdio>
#include <string>

#include "test_runner.h"
#include "cog0/AtomStore.h"

using namespace cog0;

// Use a unique temp path for each test to avoid inter-test interference.
// Write to the current working directory (build dir) for cross-platform compat.
static std::string tmpPath(const char* suffix) {
    return std::string("cog0_test_") + suffix + ".snap";
}

TEST(save_and_load_nodes) {
    const std::string path = tmpPath("nodes");
    std::remove(path.c_str());

    AtomStore store;
    auto a = store.addNode(AtomType::CONCEPT, "alpha");
    a->setTV({0.8, 0.9});
    a->setSTI(0.5);
    auto b = store.addNode(AtomType::PREDICATE, "beta");

    ASSERT_TRUE(store.saveToFile(path));

    AtomStore store2;
    ASSERT_TRUE(store2.loadFromFile(path));
    ASSERT_EQ(store2.size(), 2u);

    auto ra = store2.getNode(AtomType::CONCEPT, "alpha");
    ASSERT_TRUE(ra != nullptr);
    ASSERT_EQ(ra->name(), "alpha");
    ASSERT_EQ(ra->type(), AtomType::CONCEPT);
    ASSERT_EQ(ra->tv().strength,   0.8);
    ASSERT_EQ(ra->tv().confidence, 0.9);
    ASSERT_EQ(ra->sti(),           0.5);

    auto rb = store2.getNode(AtomType::PREDICATE, "beta");
    ASSERT_TRUE(rb != nullptr);
    ASSERT_EQ(rb->name(), "beta");

    std::remove(path.c_str());
}

TEST(save_and_load_links) {
    const std::string path = tmpPath("links");
    std::remove(path.c_str());

    AtomStore store;
    auto a = store.addNode(AtomType::CONCEPT, "A");
    auto b = store.addNode(AtomType::CONCEPT, "B");
    auto lnk = store.addLink(AtomType::INHERITANCE, {a, b});
    lnk->setTV({0.7, 0.85});

    ASSERT_TRUE(store.saveToFile(path));

    AtomStore store2;
    ASSERT_TRUE(store2.loadFromFile(path));
    ASSERT_EQ(store2.size(), 3u);

    auto ra = store2.getNode(AtomType::CONCEPT, "A");
    auto rb = store2.getNode(AtomType::CONCEPT, "B");
    ASSERT_TRUE(ra != nullptr);
    ASSERT_TRUE(rb != nullptr);

    auto rl = store2.getLink(AtomType::INHERITANCE, {ra, rb});
    ASSERT_TRUE(rl != nullptr);
    ASSERT_EQ(rl->tv().strength,   0.7);
    ASSERT_EQ(rl->tv().confidence, 0.85);
    ASSERT_EQ(rl->out().size(), 2u);
    ASSERT_EQ(rl->out()[0], ra);
    ASSERT_EQ(rl->out()[1], rb);

    std::remove(path.c_str());
}

TEST(incoming_index_restored_after_load) {
    const std::string path = tmpPath("incoming");
    std::remove(path.c_str());

    AtomStore store;
    auto a = store.addNode(AtomType::CONCEPT, "X");
    auto b = store.addNode(AtomType::CONCEPT, "Y");
    store.addLink(AtomType::INHERITANCE, {a, b});

    ASSERT_TRUE(store.saveToFile(path));

    AtomStore store2;
    ASSERT_TRUE(store2.loadFromFile(path));

    auto ra = store2.getNode(AtomType::CONCEPT, "X");
    ASSERT_EQ(store2.getIncoming(ra).size(), 1u);

    std::remove(path.c_str());
}

TEST(load_clears_previous_content) {
    const std::string path = tmpPath("clear");
    std::remove(path.c_str());

    AtomStore store;
    store.addNode(AtomType::CONCEPT, "foo");
    ASSERT_TRUE(store.saveToFile(path));

    AtomStore store2;
    store2.addNode(AtomType::CONCEPT, "old-node-1");
    store2.addNode(AtomType::CONCEPT, "old-node-2");
    ASSERT_EQ(store2.size(), 2u);

    ASSERT_TRUE(store2.loadFromFile(path));
    ASSERT_EQ(store2.size(), 1u);
    ASSERT_TRUE(store2.getNode(AtomType::CONCEPT, "foo") != nullptr);
    ASSERT_EQ(store2.getNode(AtomType::CONCEPT, "old-node-1"), nullptr);

    std::remove(path.c_str());
}

TEST(save_load_name_with_special_chars) {
    const std::string path = tmpPath("special");
    std::remove(path.c_str());

    AtomStore store;
    // Name with spaces, colons, and backslash-escaped characters
    store.addNode(AtomType::CONCEPT, "some concept with spaces");
    store.addNode(AtomType::CONCEPT, "key:value");
    store.addNode(AtomType::CONCEPT, "path\\file");

    ASSERT_TRUE(store.saveToFile(path));

    AtomStore store2;
    ASSERT_TRUE(store2.loadFromFile(path));
    ASSERT_EQ(store2.size(), 3u);
    ASSERT_TRUE(store2.getNode(AtomType::CONCEPT, "some concept with spaces") != nullptr);
    ASSERT_TRUE(store2.getNode(AtomType::CONCEPT, "key:value") != nullptr);
    ASSERT_TRUE(store2.getNode(AtomType::CONCEPT, "path\\file") != nullptr);

    std::remove(path.c_str());
}

TEST(save_to_nonexistent_dir_fails) {
    AtomStore store;
    store.addNode(AtomType::CONCEPT, "x");
    ASSERT_FALSE(store.saveToFile("/nonexistent/dir/file.snap"));
}

TEST(load_missing_file_fails) {
    AtomStore store;
    ASSERT_FALSE(store.loadFromFile("/tmp/no_such_file_cog0_xyz.snap"));
}

TEST(next_id_preserved_after_roundtrip) {
    const std::string path = tmpPath("nextid");
    std::remove(path.c_str());

    AtomStore store;
    store.addNode(AtomType::CONCEPT, "a");
    store.addNode(AtomType::CONCEPT, "b");
    // 2 atoms, so next_id == 3

    ASSERT_TRUE(store.saveToFile(path));

    AtomStore store2;
    ASSERT_TRUE(store2.loadFromFile(path));

    // Adding a new atom after load should get a fresh id (>= 3)
    auto newAtom = store2.addNode(AtomType::CONCEPT, "c");
    ASSERT_TRUE(newAtom->id() >= 3u);

    std::remove(path.c_str());
}
