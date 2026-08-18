/*
 * standalone/tests/test_task_manager.cpp
 */
#include "test_runner.h"
#include "cog0/AtomStore.h"
#include "cog0/TaskManager.h"

using namespace cog0;

TEST(set_goal_creates_goal) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    auto g = tm.setGoal("learn-opencog", "Explore OpenCog", 0.8);
    ASSERT_TRUE(g != nullptr);
    ASSERT_EQ(g->name, "learn-opencog");
    ASSERT_EQ(tm.goals().size(), 1u);
    ASSERT_EQ(tm.currentGoal(), g);
}

TEST(create_and_enqueue_task) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    auto t = tm.createTask("my-task", "do something", Priority::HIGH);
    tm.enqueue(t);
    ASSERT_EQ(tm.pendingCount(), 1u);
}

TEST(execute_next_calls_action) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    bool called = false;
    auto t = tm.createTask("act", "", Priority::NORMAL,
                            [&called]() { called = true; return true; });
    tm.enqueue(t);
    bool ran = tm.executeNext();
    ASSERT_TRUE(ran);
    ASSERT_TRUE(called);
    ASSERT_TRUE(t->completed);
    ASSERT_EQ(tm.pendingCount(), 0u);
}

TEST(execute_next_marks_failed_on_false_return) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    auto t = tm.createTask("fail", "", Priority::NORMAL, []() { return false; });
    tm.enqueue(t);
    tm.executeNext();
    ASSERT_TRUE(t->failed);
    ASSERT_FALSE(t->completed);
}

TEST(priority_ordering) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    std::vector<std::string> order;
    auto tLow  = tm.createTask("low",      "", Priority::LOW,
                                [&]() { order.push_back("low");  return true; });
    auto tHigh = tm.createTask("high",     "", Priority::HIGH,
                                [&]() { order.push_back("high"); return true; });
    auto tNorm = tm.createTask("normal",   "", Priority::NORMAL,
                                [&]() { order.push_back("normal"); return true; });
    tm.enqueue(tLow);
    tm.enqueue(tHigh);
    tm.enqueue(tNorm);
    tm.executeAll();
    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], "high");
    ASSERT_EQ(order[1], "normal");
    ASSERT_EQ(order[2], "low");
}

TEST(achieve_goal) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    auto g = tm.setGoal("g");
    ASSERT_FALSE(g->achieved);
    tm.achieveGoal(g->id);
    ASSERT_TRUE(g->achieved);
    ASSERT_EQ(tm.currentGoal(), nullptr);
}

TEST(attach_task_to_goal) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    auto g = tm.setGoal("big-goal");
    auto t = tm.createTask("sub-task");
    tm.attachToGoal(g->id, t);
    ASSERT_EQ(g->tasks.size(), 1u);
    ASSERT_EQ(tm.pendingCount(), 1u);
}

TEST(task_atom_created_in_store) {
    auto store = std::make_shared<AtomStore>();
    TaskManager tm(store);
    tm.createTask("atom-task");
    // TaskManager creates a concept node for the task
    auto h = store->getNode(AtomType::CONCEPT, "Task:atom-task");
    ASSERT_TRUE(h != nullptr);
}
