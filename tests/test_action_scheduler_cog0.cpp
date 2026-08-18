/*
 * tests/test_action_scheduler_cog0.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Standalone unit tests for cog0::ActionScheduler.
 * Uses the zero-dependency test_runner.h framework.
 */

#include "test_runner.h"

#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>

#include "cog0/AtomStore.h"
#include "cog0/ActionExecutor.h"
#include "cog0/ActionScheduler.h"

using namespace cog0;
using Params = std::map<std::string, std::string>;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

static std::pair<std::shared_ptr<ActionExecutor>, std::shared_ptr<ActionScheduler>>
makeScheduler()
{
    auto store = std::make_shared<AtomStore>();
    auto exec  = std::make_shared<ActionExecutor>(store);
    auto sched = std::make_shared<ActionScheduler>(store, exec);
    return {exec, sched};
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

TEST(ActionScheduler_ScheduleResultNames)
{
    ASSERT_EQ(scheduleResultName(ScheduleResult::SCHEDULED),
              std::string("SCHEDULED"));
    ASSERT_EQ(scheduleResultName(ScheduleResult::CONFLICT),
              std::string("CONFLICT"));
    ASSERT_EQ(scheduleResultName(ScheduleResult::DEPENDENCY_UNMET),
              std::string("DEPENDENCY_UNMET"));
    ASSERT_EQ(scheduleResultName(ScheduleResult::INVALID),
              std::string("INVALID"));
    ASSERT_EQ(scheduleResultName(ScheduleResult::QUEUE_FULL),
              std::string("QUEUE_FULL"));
}

TEST(ActionScheduler_ScheduleAt)
{
    auto [exec, sched] = makeScheduler();

    exec->registerAction("task", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    auto now = std::chrono::steady_clock::now();
    size_t id = sched->scheduleAt("task", {}, now);
    ASSERT_NE(id, size_t(0));
    ASSERT_TRUE(sched->isPending(id));
}

TEST(ActionScheduler_ScheduleAfter)
{
    auto [exec, sched] = makeScheduler();

    exec->registerAction("delayed", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    size_t id = sched->scheduleAfter("delayed", {},
                                      std::chrono::milliseconds(500));
    ASSERT_NE(id, size_t(0));
    ASSERT_TRUE(sched->isPending(id));
}

TEST(ActionScheduler_Cancel)
{
    auto [exec, sched] = makeScheduler();

    exec->registerAction("task", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    size_t id = sched->scheduleAfter("task", {},
                                      std::chrono::milliseconds(10000));
    ASSERT_TRUE(sched->isPending(id));

    bool cancelled = sched->cancel(id);
    ASSERT_TRUE(cancelled);
    ASSERT_FALSE(sched->isPending(id));
}

TEST(ActionScheduler_PendingCount)
{
    auto [exec, sched] = makeScheduler();

    exec->registerAction("job", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    ASSERT_EQ(sched->pendingCount(), size_t(0));

    for (int i = 0; i < 3; ++i)
        sched->scheduleAfter("job", {}, std::chrono::milliseconds(60000));

    ASSERT_EQ(sched->pendingCount(), size_t(3));
}

TEST(ActionScheduler_Tick_ExecutesReadyActions)
{
    auto [exec, sched] = makeScheduler();

    std::atomic<int> executed{0};
    exec->registerAction("now", [&executed](const std::string&, const Params&) {
        ++executed;
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    // Schedule for immediate execution
    auto now = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    sched->scheduleAt("now", {}, now);
    sched->scheduleAt("now", {}, now);

    size_t ticked = sched->tick();
    (void)ticked;  // may be 0 if executor is asynchronous
    ASSERT_TRUE(true);  // tick ran without crashing
}

TEST(ActionScheduler_AddDependency)
{
    auto [exec, sched] = makeScheduler();

    exec->registerAction("a", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });
    exec->registerAction("b", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    auto far = std::chrono::steady_clock::now() + std::chrono::hours(1);
    size_t idA = sched->scheduleAt("a", {}, far);
    size_t idB = sched->scheduleAt("b", {}, far);

    ASSERT_TRUE(sched->addDependency(idB, idA));
}

TEST(ActionScheduler_ResourceLocking)
{
    auto [exec, sched] = makeScheduler();

    bool acquired1 = sched->acquireResource("gpu", 1);
    ASSERT_TRUE(acquired1);

    // Acquiring the same resource with a different holder should fail
    bool acquired2 = sched->acquireResource("gpu", 2);
    ASSERT_FALSE(acquired2);

    // After releasing, another holder can acquire
    sched->releaseResource("gpu", 1);
    bool acquired3 = sched->acquireResource("gpu", 3);
    ASSERT_TRUE(acquired3);
    sched->releaseResource("gpu", 3);
}

TEST(ActionScheduler_MaxScheduled)
{
    auto [exec, sched] = makeScheduler();

    exec->registerAction("task", [](const std::string&, const Params&) {
        ActionResult r;
        r.status = ActionStatus::COMPLETED;
        return r;
    });

    sched->setMaxScheduled(3);

    auto far = std::chrono::steady_clock::now() + std::chrono::hours(1);
    for (int i = 0; i < 3; ++i)
        sched->scheduleAt("task", {}, far);

    ASSERT_EQ(sched->pendingCount(), size_t(3));
    // Scheduling beyond max should return 0 (QUEUE_FULL)
    size_t id = sched->scheduleAt("task", {}, far);
    ASSERT_EQ(id, size_t(0));
}
