/*
 * standalone/tests/test_cognitive_loop.cpp
 */
#include <chrono>
#include <thread>

#include "test_runner.h"
#include "cog0/AtomStore.h"
#include "cog0/CognitiveLoop.h"
#include "cog0/ReasoningEngine.h"
#include "cog0/TaskManager.h"

using namespace cog0;

TEST(single_cycle_runs) {
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning = std::make_shared<ReasoningEngine>(store);
    CognitiveLoop loop(store, taskMgr, reasoning);
    loop.runSingleCycle();
    ASSERT_EQ(loop.cycleCount(), 1u);
}

TEST(percept_added_to_store) {
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning = std::make_shared<ReasoningEngine>(store);
    CognitiveLoop loop(store, taskMgr, reasoning);

    loop.addPercept(PerceptInput{"sensor", "text", "hello world", 0.7});
    loop.runSingleCycle();

    auto h = store->getNode(AtomType::CONCEPT, "Percept:hello world");
    ASSERT_TRUE(h != nullptr);
    ASSERT_GE(h->sti(), 0.7 - 1e-9);
}

TEST(task_executed_during_action_phase) {
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning = std::make_shared<ReasoningEngine>(store);
    CognitiveLoop loop(store, taskMgr, reasoning);

    bool ran = false;
    auto t = taskMgr->createTask("t", "", Priority::NORMAL, [&ran]() { ran = true; return true; });
    taskMgr->enqueue(t);

    loop.runSingleCycle();
    ASSERT_TRUE(ran);
}

TEST(reflection_hook_called) {
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning = std::make_shared<ReasoningEngine>(store);
    CognitiveLoop loop(store, taskMgr, reasoning);

    size_t hookCalls = 0;
    loop.setReflectionHook([&hookCalls](const CycleStats& s) {
        ASSERT_GT(s.cycleNumber, 0u);
        ++hookCalls;
    });
    loop.runSingleCycle();
    loop.runSingleCycle();
    ASSERT_EQ(hookCalls, 2u);
}

TEST(perception_hook_called_for_each_percept) {
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning = std::make_shared<ReasoningEngine>(store);
    CognitiveLoop loop(store, taskMgr, reasoning);

    size_t perceptsSeen = 0;
    loop.setPerceptionHook([&perceptsSeen](const PerceptInput&, AtomStore&) {
        ++perceptsSeen;
    });
    loop.addPercept({"s", "text", "a", 0.5});
    loop.addPercept({"s", "text", "b", 0.5});
    loop.runSingleCycle();
    ASSERT_EQ(perceptsSeen, 2u);
}

TEST(background_thread_runs_and_stops) {
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning = std::make_shared<ReasoningEngine>(store);
    CognitiveLoop loop(store, taskMgr, reasoning);

    loop.setCycleInterval(std::chrono::milliseconds(10));
    loop.setMaxCycles(3);
    loop.start();

    // Wait for loop to finish (max 2 seconds)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (loop.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ASSERT_FALSE(loop.isRunning());
    ASSERT_EQ(loop.cycleCount(), 3u);
}
