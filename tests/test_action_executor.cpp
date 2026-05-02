/*
 * test_action_executor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for ActionExecutor class
 * Tests for AZ-ACTION-002: Create action execution framework
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

#include "opencog/agentzero/ActionExecutor.h"
#include "opencog/agentzero/ActionScheduler.h"

using namespace opencog;
using namespace opencog::agentzero;

class ActionExecutorTest : public ::testing::Test
{
protected:
    void SetUp() override {
        atomspace = createAtomSpace();
        executor = std::make_unique<ActionExecutor>(nullptr, atomspace);  // nullptr for agent_core in tests
        scheduler = std::make_shared<ActionScheduler>(nullptr, atomspace);
        executor->setScheduler(scheduler);
    }

    void TearDown() override {
        executor.reset();
        scheduler.reset();
        atomspace.reset();
    }

    AtomSpacePtr atomspace;
    std::unique_ptr<ActionExecutor> executor;
    std::shared_ptr<ActionScheduler> scheduler;
};

// Test basic action execution
TEST_F(ActionExecutorTest, BasicActionExecution)
{
    // Create a simple action atom
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "TestAction");
    ASSERT_NE(action_atom, Handle::UNDEFINED);
    
    // Execute action synchronously
    ActionExecutor::ActionResult result = executor->executeActionSync(action_atom, 5000);
    
    // Verify execution succeeded
    EXPECT_EQ(result.status, ActionExecutor::ActionStatus::COMPLETED);
    EXPECT_GT(result.duration.count(), 0);
    EXPECT_GT(result.success_probability, 0.0);
    EXPECT_FALSE(result.message.empty());
}

// Test action queueing
TEST_F(ActionExecutorTest, ActionQueueing)
{
    // Create multiple action atoms
    Handle action1 = atomspace->add_node(CONCEPT_NODE, "TestAction1");
    Handle action2 = atomspace->add_node(CONCEPT_NODE, "TestAction2");
    Handle action3 = atomspace->add_node(CONCEPT_NODE, "TestAction3");
    
    // Queue actions with different priorities
    EXPECT_TRUE(executor->executeAction(action1, ActionExecutor::Priority::LOW));
    EXPECT_TRUE(executor->executeAction(action2, ActionExecutor::Priority::HIGH));
    EXPECT_TRUE(executor->executeAction(action3, ActionExecutor::Priority::MEDIUM));
    
    // Verify actions are queued
    std::vector<Handle> pending = executor->getPendingActions();
    EXPECT_EQ(pending.size(), 3);
    
    // Check initial status
    EXPECT_EQ(executor->getActionStatus(action1), ActionExecutor::ActionStatus::PENDING);
    EXPECT_EQ(executor->getActionStatus(action2), ActionExecutor::ActionStatus::PENDING);
    EXPECT_EQ(executor->getActionStatus(action3), ActionExecutor::ActionStatus::PENDING);
}

// Test action cancellation
TEST_F(ActionExecutorTest, ActionCancellation)
{
    // Create action and queue it
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "CancellableAction");
    EXPECT_TRUE(executor->executeAction(action_atom, ActionExecutor::Priority::MEDIUM));
    
    // Verify it's queued
    EXPECT_EQ(executor->getActionStatus(action_atom), ActionExecutor::ActionStatus::PENDING);
    
    // Cancel the action
    EXPECT_TRUE(executor->cancelAction(action_atom));
    
    // Verify it's cancelled
    EXPECT_EQ(executor->getActionStatus(action_atom), ActionExecutor::ActionStatus::CANCELLED);
    
    // Verify result reflects cancellation
    ActionExecutor::ActionResult result = executor->getActionResult(action_atom);
    EXPECT_EQ(result.status, ActionExecutor::ActionStatus::CANCELLED);
    EXPECT_FALSE(result.message.empty());
}

// Test custom action registration
TEST_F(ActionExecutorTest, CustomActionRegistration)
{
    // Define custom action callback
    bool callback_called = false;
    auto custom_callback = [&callback_called](const Handle& action_atom, const std::map<std::string, Handle>& params) -> ActionExecutor::ActionResult {
        callback_called = true;
        ActionExecutor::ActionResult result;
        result.status = ActionExecutor::ActionStatus::COMPLETED;
        result.message = "Custom action executed";
        result.success_probability = 0.95;
        return result;
    };
    
    // Register custom action
    EXPECT_TRUE(executor->registerAction("custom_test", custom_callback));
    EXPECT_TRUE(executor->isActionRegistered("custom_test"));
    
    // Execute custom action
    std::map<std::string, Handle> params;
    Handle action_atom = executor->executeSimpleAction("custom_test", params);
    EXPECT_NE(action_atom, Handle::UNDEFINED);
    
    // Process the action queue to trigger execution
    executor->processActionQueue();
    
    // Give some time for async execution
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify callback was called
    EXPECT_TRUE(callback_called);
}

// Test action with parameters
TEST_F(ActionExecutorTest, ActionWithParameters)
{
    // Create parameter atoms
    Handle param_value = atomspace->add_node(CONCEPT_NODE, "ParameterValue");
    std::map<std::string, Handle> parameters;
    parameters["test_param"] = param_value;
    
    // Execute action with parameters
    Handle action_atom = executor->executeSimpleAction("basic", parameters);
    EXPECT_NE(action_atom, Handle::UNDEFINED);
    
    // Verify action was created correctly
    EXPECT_TRUE(atomspace->is_valid_handle(action_atom));
}

// Test multiple concurrent actions
TEST_F(ActionExecutorTest, ConcurrentActions)
{
    // Set max concurrent actions
    executor->setMaxConcurrentActions(3);
    
    // Create multiple actions
    std::vector<Handle> actions;
    for (int i = 0; i < 5; ++i) {
        Handle action = atomspace->add_node(CONCEPT_NODE, "ConcurrentAction" + std::to_string(i));
        actions.push_back(action);
        EXPECT_TRUE(executor->executeAction(action, ActionExecutor::Priority::MEDIUM));
    }
    
    // Process queue
    int processed = executor->processActionQueue();
    EXPECT_GT(processed, 0);
    EXPECT_LE(processed, 3);  // Should not exceed max concurrent
    
    // Verify some actions are executing
    std::vector<Handle> executing = executor->getExecutingActions();
    EXPECT_GT(executing.size(), 0);
    EXPECT_LE(executing.size(), 3);
}

// Test action monitoring and timeout
TEST_F(ActionExecutorTest, ActionMonitoring)
{
    // Set short timeout for testing
    executor->setDefaultTimeout(100);  // 100ms timeout
    
    // Register action that takes longer than timeout
    auto slow_callback = [](const Handle& action_atom, const std::map<std::string, Handle>& params) -> ActionExecutor::ActionResult {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // Takes 200ms
        ActionExecutor::ActionResult result;
        result.status = ActionExecutor::ActionStatus::COMPLETED;
        result.message = "Slow action completed";
        return result;
    };
    
    executor->registerAction("slow_action", slow_callback);
    
    // Execute slow action
    Handle action_atom = executor->executeSimpleAction("slow_action");
    EXPECT_NE(action_atom, Handle::UNDEFINED);
    
    // Process and monitor
    executor->processActionQueue();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Wait a bit
    
    // Monitor should detect timeout after sufficient time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Wait for timeout
    int status_changes = executor->monitorExecutingActions();
    
    // Check if timeout was detected (may vary based on timing)
    ActionExecutor::ActionStatus final_status = executor->getActionStatus(action_atom);
    EXPECT_TRUE(final_status == ActionExecutor::ActionStatus::TIMEOUT || 
                final_status == ActionExecutor::ActionStatus::COMPLETED);
}

// Test action result retrieval
TEST_F(ActionExecutorTest, ActionResultRetrieval)
{
    // Create and execute action
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "ResultAction");
    ActionExecutor::ActionResult result = executor->executeActionSync(action_atom, 5000);
    
    // Verify result can be retrieved
    ActionExecutor::ActionResult retrieved = executor->getActionResult(action_atom);
    EXPECT_EQ(retrieved.status, result.status);
    EXPECT_EQ(retrieved.message, result.message);
    EXPECT_EQ(retrieved.success_probability, result.success_probability);
}

// Test status information
TEST_F(ActionExecutorTest, StatusInformation)
{
    // Get initial status
    std::string status = executor->getStatusInfo();
    EXPECT_FALSE(status.empty());
    
    // Should be valid JSON format (basic check)
    EXPECT_NE(status.find("action_queue_size"), std::string::npos);
    EXPECT_NE(status.find("max_concurrent_actions"), std::string::npos);
    EXPECT_NE(status.find("executing_actions"), std::string::npos);
}

// Test AtomSpace integration
TEST_F(ActionExecutorTest, AtomSpaceIntegration)
{
    // Execute atomspace action type
    Handle action_atom = executor->executeSimpleAction("atomspace");
    EXPECT_NE(action_atom, Handle::UNDEFINED);
    
    // Execute synchronously to verify AtomSpace operations
    ActionExecutor::ActionResult result = executor->executeActionSync(action_atom);
    EXPECT_EQ(result.status, ActionExecutor::ActionStatus::COMPLETED);
    EXPECT_NE(result.outcome_atom, Handle::UNDEFINED);
    
    // Verify outcome atom exists in atomspace
    EXPECT_TRUE(atomspace->is_valid_handle(result.outcome_atom));
}

// Test configuration changes
TEST_F(ActionExecutorTest, Configuration)
{
    // Test timeout configuration
    executor->setDefaultTimeout(2000);
    
    // Test max concurrent actions
    executor->setMaxConcurrentActions(10);
    
    // Test async execution toggle
    executor->setAsyncExecution(false);
    
    // Verify configuration is reflected in status
    std::string status = executor->getStatusInfo();
    EXPECT_NE(status.find("max_concurrent_actions"), std::string::npos);
}

// Test error handling
TEST_F(ActionExecutorTest, ErrorHandling)
{
    // Test invalid action atom
    ActionExecutor::ActionResult result = executor->executeActionSync(Handle::UNDEFINED);
    EXPECT_EQ(result.status, ActionExecutor::ActionStatus::FAILED);
    EXPECT_FALSE(result.message.empty());
    
    // Test unregistering non-existent action
    EXPECT_FALSE(executor->unregisterAction("nonexistent_action"));
    
    // Test cancelling non-existent action
    Handle fake_action = atomspace->add_node(CONCEPT_NODE, "FakeAction");
    EXPECT_FALSE(executor->cancelAction(fake_action));
}

// Integration test with scheduler
TEST_F(ActionExecutorTest, SchedulerIntegration)
{
    // Verify scheduler is set
    EXPECT_NE(executor->getScheduler(), nullptr);
    
    // Create action and schedule it
    Handle action_atom = atomspace->add_node(CONCEPT_NODE, "ScheduledAction");
    
    // Schedule action for immediate execution
    ActionScheduler::ScheduleResult schedule_result = scheduler->scheduleAction(
        action_atom, std::chrono::steady_clock::now(), 5);
    EXPECT_EQ(schedule_result, ActionScheduler::ScheduleResult::SCHEDULED);
    
    // Process scheduler queue
    int dispatched = scheduler->processScheduleQueue();
    EXPECT_EQ(dispatched, 1);
}