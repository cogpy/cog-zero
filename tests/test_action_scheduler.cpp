/*
 * test_action_scheduler.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for ActionScheduler class
 * Tests for AZ-ACTION-001: Implement ActionScheduler for temporal coordination
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/atoms/atom_types/types.h>

#include "opencog/agentzero/ActionScheduler.h"
#include "opencog/agentzero/ActionExecutor.h"

using namespace opencog;
using namespace opencog::agentzero;

class ActionSchedulerTest : public ::testing::Test
{
protected:
    void SetUp() override {
        atomspace = createAtomSpace();
        scheduler = std::make_unique<ActionScheduler>(nullptr, atomspace);
        executor = std::make_shared<ActionExecutor>(nullptr, atomspace);
        scheduler->setExecutor(executor);
    }

    void TearDown() override {
        scheduler.reset();
        executor.reset();
        atomspace.reset();
    }

    AtomSpacePtr atomspace;
    std::unique_ptr<ActionScheduler> scheduler;
    std::shared_ptr<ActionExecutor> executor;
};

// Test basic action scheduling
TEST_F(ActionSchedulerTest, BasicActionScheduling)
{
    // Create action atom
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "ScheduledTestAction");
    ASSERT_NE(action_atom, Handle::UNDEFINED);
    
    // Schedule action for immediate execution
    auto schedule_time = std::chrono::steady_clock::now();
    ActionScheduler::ScheduleResult result = scheduler->scheduleAction(action_atom, schedule_time, 5);
    
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify action appears in scheduled actions list
    std::vector<ActionScheduler::ScheduledAction> scheduled = scheduler->getScheduledActions();
    EXPECT_EQ(scheduled.size(), 1);
    EXPECT_EQ(scheduled[0].action_atom, action_atom);
    EXPECT_EQ(scheduled[0].priority, 5);
}

// Test scheduling with delay
TEST_F(ActionSchedulerTest, ScheduleActionAfter)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "DelayedAction");
    
    // Schedule action with 100ms delay
    ActionScheduler::ScheduleResult result = scheduler->scheduleActionAfter(action_atom, 100, 7);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify scheduling time is in the future
    auto next_time = scheduler->getNextActionTime();
    auto now = std::chrono::steady_clock::now();
    EXPECT_GT(next_time, now);
}

// Test scheduling with deadline
TEST_F(ActionSchedulerTest, ScheduleActionBefore)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "DeadlineAction");
    
    // Schedule action with deadline 1 second from now
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    ActionScheduler::ScheduleResult result = scheduler->scheduleActionBefore(action_atom, deadline, 8);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify action has deadline set
    std::vector<ActionScheduler::ScheduledAction> scheduled = scheduler->getScheduledActions();
    EXPECT_EQ(scheduled.size(), 1);
    EXPECT_NE(scheduled[0].deadline, std::chrono::steady_clock::time_point{});
}

// Test periodic action scheduling
TEST_F(ActionSchedulerTest, PeriodicActionScheduling)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "PeriodicAction");
    
    // Schedule periodic action with 50ms period, 3 repetitions
    ActionScheduler::ScheduleResult result = scheduler->schedulePeriodicAction(action_atom, 50, 3, 6);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify periodic action properties
    std::vector<ActionScheduler::ScheduledAction> scheduled = scheduler->getScheduledActions();
    EXPECT_EQ(scheduled.size(), 1);
    EXPECT_TRUE(scheduled[0].is_periodic);
    EXPECT_EQ(scheduled[0].repeat_count, 3);
    EXPECT_EQ(scheduled[0].period.count(), 50);
}

// Test action scheduling with dependencies
TEST_F(ActionSchedulerTest, ScheduleWithDependencies)
{
    // Create action atoms
    Handle dep_action = atomspace->add_node(CONCEPT_NODE, "DependencyAction");
    Handle main_action = atomspace->add_node(CONCEPT_NODE, "MainAction");
    
    // Schedule main action with dependency
    std::vector<Handle> dependencies = {dep_action};
    ActionScheduler::ScheduleResult result = scheduler->scheduleActionWithDependencies(main_action, dependencies, 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify dependencies are recorded
    std::vector<ActionScheduler::ScheduledAction> scheduled = scheduler->getScheduledActions();
    EXPECT_EQ(scheduled.size(), 1);
    EXPECT_EQ(scheduled[0].dependencies.size(), 1);
    EXPECT_EQ(scheduled[0].dependencies[0], dep_action);
}

// Test action scheduling with resources
TEST_F(ActionSchedulerTest, ScheduleWithResources)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "ResourceAction");
    
    // Schedule action requiring CPU resource
    std::vector<std::string> resources = {"cpu", "memory"};
    ActionScheduler::ScheduleResult result = scheduler->scheduleActionWithResources(action_atom, resources, 7);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify resources are recorded
    std::vector<ActionScheduler::ScheduledAction> scheduled = scheduler->getScheduledActions();
    EXPECT_EQ(scheduled.size(), 1);
    EXPECT_EQ(scheduled[0].required_resources.size(), 2);
    EXPECT_EQ(scheduled[0].required_resources[0], "cpu");
    EXPECT_EQ(scheduled[0].required_resources[1], "memory");
}

// Test action cancellation
TEST_F(ActionSchedulerTest, ActionCancellation)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "CancellableScheduledAction");
    
    // Schedule action
    ActionScheduler::ScheduleResult result = scheduler->scheduleActionAfter(action_atom, 500, 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify it's scheduled
    EXPECT_EQ(scheduler->getScheduledActions().size(), 1);
    
    // Cancel the action
    EXPECT_TRUE(scheduler->cancelScheduledAction(action_atom));
    
    // Verify it's removed from schedule
    EXPECT_EQ(scheduler->getScheduledActions().size(), 0);
}

// Test action rescheduling
TEST_F(ActionSchedulerTest, ActionRescheduling)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "ReschedulableAction");
    
    // Initial scheduling
    auto initial_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    ActionScheduler::ScheduleResult result = scheduler->scheduleAction(action_atom, initial_time, 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Reschedule to different time
    auto new_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    result = scheduler->rescheduleAction(action_atom, new_time);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Verify new time is set
    std::vector<ActionScheduler::ScheduledAction> scheduled = scheduler->getScheduledActions();
    EXPECT_EQ(scheduled.size(), 1);
    EXPECT_GE(scheduled[0].scheduled_time, new_time - std::chrono::milliseconds(10));  // Allow small tolerance
}

// Test schedule queue processing
TEST_F(ActionSchedulerTest, ScheduleQueueProcessing)
{
    // Schedule multiple actions for immediate execution
    std::vector<Handle> actions;
    for (int i = 0; i < 3; ++i) {
        Handle action = atomspace->add_node(CONCEPT_NODE, "QueueAction" + std::to_string(i));
        actions.push_back(action);
        
        ActionScheduler::ScheduleResult result = scheduler->scheduleAction(
            action, std::chrono::steady_clock::now(), 5 + i);
        EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    }
    
    // Process the schedule queue
    int dispatched = scheduler->processScheduleQueue();
    EXPECT_EQ(dispatched, 3);
    
    // Actions should be removed from schedule after dispatch
    EXPECT_EQ(scheduler->getScheduledActions().size(), 0);
}

// Test resource management
TEST_F(ActionSchedulerTest, ResourceManagement)
{
    // Register custom resource
    EXPECT_TRUE(scheduler->registerResource("custom_resource"));
    
    // Verify resource is available
    EXPECT_TRUE(scheduler->isResourceAvailable("custom_resource"));
    
    // Get available resources list
    std::vector<std::string> resources = scheduler->getAvailableResources();
    EXPECT_GT(resources.size(), 0);
    
    // Should include default resources plus custom one
    bool found_custom = false;
    for (const std::string& resource : resources) {
        if (resource == "custom_resource") {
            found_custom = true;
            break;
        }
    }
    EXPECT_TRUE(found_custom);
    
    // Unregister resource
    EXPECT_TRUE(scheduler->unregisterResource("custom_resource"));
    EXPECT_FALSE(scheduler->isResourceAvailable("custom_resource"));
}

// Test resource conflicts
TEST_F(ActionSchedulerTest, ResourceConflicts)
{
    // Create two actions requiring the same resource
    Handle action1 = atomspace->add_node(CONCEPT_NODE, "ResourceAction1");
    Handle action2 = atomspace->add_node(CONCEPT_NODE, "ResourceAction2");
    
    std::vector<std::string> resources = {"cpu"};
    
    // Schedule first action
    ActionScheduler::ScheduleResult result1 = scheduler->scheduleActionWithResources(action1, resources, 5);
    EXPECT_EQ(result1, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Schedule second action with same resource - should succeed in scheduling
    // Resource conflicts are handled during dispatch, not scheduling
    ActionScheduler::ScheduleResult result2 = scheduler->scheduleActionWithResources(action2, resources, 5);
    EXPECT_EQ(result2, ActionScheduler::ScheduleResult::SCHEDULED);
}

// Test scheduler updating
TEST_F(ActionSchedulerTest, SchedulerUpdate)
{
    // Schedule action with past deadline to test deadline checking
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "ExpiredAction");
    auto past_deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(100);
    
    ActionScheduler::ScheduleResult result = scheduler->scheduleActionBefore(action_atom, past_deadline, 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Update scheduler - should remove expired actions
    int status_changes = scheduler->updateScheduler();
    EXPECT_GT(status_changes, 0);
    
    // Expired action should be removed
    EXPECT_EQ(scheduler->getScheduledActions().size(), 0);
}

// Test next action time calculation
TEST_F(ActionSchedulerTest, NextActionTime)
{
    // Initially no actions scheduled
    auto next_time = scheduler->getNextActionTime();
    EXPECT_EQ(next_time, std::chrono::steady_clock::time_point::max());
    
    // Schedule action
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "TimedAction");
    auto schedule_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    scheduler->scheduleAction(action_atom, schedule_time, 5);
    
    // Next action time should be approximately the scheduled time
    next_time = scheduler->getNextActionTime();
    EXPECT_LE(next_time, schedule_time + std::chrono::milliseconds(10));  // Small tolerance
    EXPECT_GE(next_time, schedule_time - std::chrono::milliseconds(10));
}

// Test configuration
TEST_F(ActionSchedulerTest, Configuration)
{
    // Test max scheduled actions
    scheduler->setMaxScheduledActions(50);
    
    // Test scheduling resolution
    scheduler->setSchedulingResolution(10);
    
    // Test feature configuration
    scheduler->configureFeatures(true, true, false);  // resources, dependencies, no temporal
    
    // Configuration should be reflected in status
    std::string status = scheduler->getStatusInfo();
    EXPECT_NE(status.find("max_scheduled_actions"), std::string::npos);
    EXPECT_NE(status.find("resource_management_enabled"), std::string::npos);
}

// Test status information
TEST_F(ActionSchedulerTest, StatusInformation)
{
    // Get status info
    std::string status = scheduler->getStatusInfo();
    EXPECT_FALSE(status.empty());
    
    // Should contain expected fields
    EXPECT_NE(status.find("scheduled_actions"), std::string::npos);
    EXPECT_NE(status.find("available_resources"), std::string::npos);
    EXPECT_NE(status.find("dependency_entries"), std::string::npos);
    
    // Schedule some actions to populate status
    Handle action1 = atomspace->add_node(CONCEPT_NODE, "StatusAction1");
    Handle action2 = atomspace->add_node(CONCEPT_NODE, "StatusAction2");
    
    scheduler->scheduleActionAfter(action1, 100, 5);
    scheduler->scheduleActionAfter(action2, 200, 7);
    
    // Status should reflect scheduled actions
    status = scheduler->getStatusInfo();
    EXPECT_NE(status.find("\"scheduled_actions\": 2"), std::string::npos);
}

// Test error handling
TEST_F(ActionSchedulerTest, ErrorHandling)
{
    // Test scheduling invalid action
    ActionScheduler::ScheduleResult result = scheduler->scheduleAction(
        Handle::UNDEFINED, std::chrono::steady_clock::now(), 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::INVALID_CONSTRAINT);
    
    // Test cancelling non-existent action
    Handle fake_action = atomspace->add_node(CONCEPT_NODE, "FakeAction");
    EXPECT_FALSE(scheduler->cancelScheduledAction(fake_action));
    
    // Test rescheduling non-existent action
    result = scheduler->rescheduleAction(fake_action, std::chrono::steady_clock::now());
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::INVALID_CONSTRAINT);
}

// Test periodic action execution
TEST_F(ActionSchedulerTest, PeriodicExecution)
{
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "PeriodicTestAction");
    
    // Schedule periodic action with short period for testing
    ActionScheduler::ScheduleResult result = scheduler->schedulePeriodicAction(action_atom, 50, 2, 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Process first iteration
    int dispatched1 = scheduler->processScheduleQueue();
    EXPECT_EQ(dispatched1, 1);
    
    // Should still have action scheduled for next iteration
    EXPECT_EQ(scheduler->getScheduledActions().size(), 1);
    
    // Wait for next period and process again
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    int dispatched2 = scheduler->processScheduleQueue();
    EXPECT_EQ(dispatched2, 1);
    
    // After 2 iterations, should be removed
    EXPECT_EQ(scheduler->getScheduledActions().size(), 0);
}

// Integration test with executor
TEST_F(ActionSchedulerTest, ExecutorIntegration)
{
    // Verify executor is set
    EXPECT_NE(scheduler->getExecutor(), nullptr);
    
    // Schedule action and verify it gets executed
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "IntegrationAction");
    ActionScheduler::ScheduleResult result = scheduler->scheduleAction(
        action_atom, std::chrono::steady_clock::now(), 5);
    EXPECT_EQ(result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Process should dispatch to executor
    int dispatched = scheduler->processScheduleQueue();
    EXPECT_EQ(dispatched, 1);
    
    // Action should appear in executor's queue
    std::vector<Handle> pending = executor->getPendingActions();
    EXPECT_EQ(pending.size(), 1);
    EXPECT_EQ(pending[0], action_atom);
}